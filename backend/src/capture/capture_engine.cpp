#include "capture/capture_engine.h"
#include "capture/douyin_client.h"
#include "capture/auth_manager.h"
#include "capture/message_parser.h"
#include "capture/signature.h"
#include "capture/room_manager.h"
#include "models/message.h"
#include "models/room.h"
#include "utils/logger.h"
#include "utils/timer.h"

#include <nlohmann/json.hpp>

#include <map>
#include <mutex>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <thread>

#ifdef HAS_PROTOBUF
#include "douyin.pb.h"
#endif

// Per-room session holding all connection state
struct RoomSession {
    std::unique_ptr<douyin::DouyinClient> client;
    douyin::AuthManager auth;
    douyin::MessageParser parser;
    douyin::Timer heartbeat;
    std::string room_id;
    std::string cursor;
    std::string internal_ext;
    bool connected = false;
    int reconnect_attempts = 0;
    int max_reconnect_attempts = 10;
    std::chrono::milliseconds reconnect_interval{5000};
};

namespace douyin {

class DouyinCaptureEngine : public CaptureEngine {
public:
    DouyinCaptureEngine(const std::string& sign_js_path,
                        const std::string& a_bogus_js_path,
                        const std::string& user_agent)
        : m_user_agent(user_agent) {
        m_room_manager = std::make_unique<RoomManager>();

        m_signature.set_user_agent(user_agent);
        if (!sign_js_path.empty()) {
            if (!m_signature.load_sign_js(sign_js_path)) {
                LOG_ERROR("Failed to load sign.js from: {}", sign_js_path);
            }
        }
        if (!a_bogus_js_path.empty()) {
            if (!m_signature.load_a_bogus_js(a_bogus_js_path)) {
                LOG_WARN("Failed to load a_bogus.js from: {}", a_bogus_js_path);
            }
        }
    }

    ~DouyinCaptureEngine() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [live_id, session] : m_sessions) {
            session->heartbeat.stop();
            if (session->client) {
                session->client->disconnect();
            }
        }
        m_sessions.clear();
    }

    bool connect(const std::string& live_id) override {
        LOG_INFO("Connecting to live_id={}", live_id);

        auto trace_id = Logger::generate_trace_id();
        Logger::set_trace_id(trace_id);

        if (m_status_cb) {
            m_status_cb(live_id, RoomStatus::Connecting);
        }

        m_room_manager->add_room(live_id);
        m_room_manager->update_status(live_id, RoomStatus::Connecting);

        auto session = std::make_unique<RoomSession>();
        session->max_reconnect_attempts = 10;
        session->reconnect_interval = std::chrono::milliseconds(5000);

        // Step 1: Auth - fetch ttwid (with retries)
        bool ttwid_ok = false;
        for (int attempt = 0; attempt < 3; ++attempt) {
            if (session->auth.fetch_ttwid(m_user_agent)) {
                ttwid_ok = true;
                break;
            }
            LOG_WARN("ttwid fetch attempt {} failed, retrying...", attempt + 1);
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        if (!ttwid_ok) {
            LOG_ERROR("Failed to fetch ttwid for live_id={}", live_id);
            m_room_manager->update_status(live_id, RoomStatus::Offline);
            if (m_status_cb) m_status_cb(live_id, RoomStatus::Offline);
            return false;
        }

        // Step 2: Fetch room_id (with retries)
        std::string room_id;
        for (int attempt = 0; attempt < 3; ++attempt) {
            room_id = session->auth.fetch_room_id(live_id, m_user_agent);
            if (!room_id.empty()) break;
            LOG_WARN("room_id fetch attempt {} failed, retrying...", attempt + 1);
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        if (room_id.empty()) {
            LOG_ERROR("Failed to fetch room_id for live_id={}", live_id);
            m_room_manager->update_status(live_id, RoomStatus::Offline);
            if (m_status_cb) m_status_cb(live_id, RoomStatus::Offline);
            return false;
        }
        session->room_id = room_id;
        m_room_manager->update_room_id(live_id, room_id);

        // Step 3: Fetch __ac_nonce
        session->auth.fetch_ac_nonce(m_user_agent);

        // Step 4: Generate user_unique_id (19-digit random number)
        std::string user_unique_id = generate_user_unique_id();

        // Step 5: Build initial cursor and internal_ext
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::ostringstream cursor_ss;
        cursor_ss << "d-1_u-1_fh-" << user_unique_id << "_t-" << now_ms << "_r-1";
        session->cursor = cursor_ss.str();

        std::ostringstream ext_ss;
        ext_ss << "internal_src:dim|wss_push_room_id:" << room_id
               << "|wss_push_did:" << user_unique_id
               << "|dim_log_id:" << trace_id;
        session->internal_ext = ext_ss.str();

        // Step 6: Build WSS URL parameters
        // Note: browser_version must be URL-encoded (spaces -> %20)
        std::string browser_version = "5.0%20(Windows%20NT%2010.0;%20Win64;%20x64)%20AppleWebKit/537.36%20(KHTML,%20like%20Gecko)%20Chrome/140.0.0.0%20Safari/537.36%20Edg/140.0.0.0";

        std::map<std::string, std::string> wss_params;
        wss_params["app_name"] = "douyin_web";
        wss_params["version_code"] = "180800";
        wss_params["webcast_sdk_version"] = "1.0.14-beta.0";
        wss_params["update_version_code"] = "1.0.14-beta.0";
        wss_params["compress"] = "gzip";
        wss_params["device_platform"] = "web";
        wss_params["cookie_enabled"] = "true";
        wss_params["screen_width"] = "1536";
        wss_params["screen_height"] = "864";
        wss_params["browser_language"] = "zh-CN";
        wss_params["browser_platform"] = "Win32";
        wss_params["browser_name"] = "Mozilla";
        wss_params["browser_version"] = browser_version;
        wss_params["browser_online"] = "true";
        wss_params["tz_name"] = "Asia/Shanghai";
        wss_params["cursor"] = session->cursor;
        wss_params["internal_ext"] = session->internal_ext;
        wss_params["host"] = "https://live.douyin.com";
        wss_params["aid"] = "6383";
        wss_params["live_id"] = "1";
        wss_params["did_rule"] = "3";
        wss_params["endpoint"] = "live_pc";
        wss_params["support_wrds"] = "1";
        wss_params["user_unique_id"] = user_unique_id;
        wss_params["im_path"] = "/webcast/im/fetch/";
        wss_params["identity"] = "audience";
        wss_params["need_persist_msg_count"] = "15";
        wss_params["room_id"] = room_id;
        wss_params["sub_room_id"] = "";
        wss_params["sub_channel_id"] = "";
        wss_params["device_type"] = "";
        wss_params["ac"] = "";
        wss_params["heartbeatDuration"] = "0";

        // Step 7: Generate X-Bogus signature
        std::string x_bogus = m_signature.generate_x_bogus(wss_params);
        if (x_bogus.empty()) {
            LOG_ERROR("Failed to generate X-Bogus signature for live_id={}", live_id);
            m_room_manager->update_status(live_id, RoomStatus::Offline);
            if (m_status_cb) m_status_cb(live_id, RoomStatus::Offline);
            return false;
        }

        // Step 8: Use local Node.js proxy for WebSocket connection
        // The proxy handles ttwid, signature, and connection to Douyin
        std::string proxy_url = "ws://localhost:8090";

        // Step 9: Create DouyinClient and set up callbacks
        session->client = std::make_unique<DouyinClient>();

        // Message callback: handle JSON messages from proxy
        session->client->on_message([this, live_id](const std::vector<uint8_t>& data) {
            std::string msg(data.begin(), data.end());

            // Debug: log received message
            LOG_INFO("Received message from proxy: {} bytes, starts with: {}", msg.size(), msg.substr(0, 50));

            // All messages from proxy are JSON
            if (!msg.empty() && msg[0] == '{') {
                try {
                    auto json = nlohmann::json::parse(msg);
                    std::string type = json.value("type", "");

                    if (type == "connected") {
                        LOG_INFO("Proxy connected to room {}", json.value("room_id", ""));
                        std::lock_guard<std::mutex> lock(m_mutex);
                        auto it = m_sessions.find(live_id);
                        if (it != m_sessions.end()) {
                            it->second->connected = true;
                            start_heartbeat_locked(live_id);
                            m_room_manager->update_status(live_id, RoomStatus::Live);
                            if (m_status_cb) m_status_cb(live_id, RoomStatus::Live);
                        }
                        return;
                    }

                    if (type == "error") {
                        LOG_ERROR("Proxy error: {}", json.value("message", ""));
                        return;
                    }

                    if (type == "disconnected") {
                        LOG_WARN("Proxy disconnected for room {}", live_id);
                        std::lock_guard<std::mutex> lock(m_mutex);
                        auto it = m_sessions.find(live_id);
                        if (it != m_sessions.end()) {
                            it->second->connected = false;
                            it->second->heartbeat.stop();
                            m_room_manager->update_status(live_id, RoomStatus::Connecting);
                            if (m_status_cb) m_status_cb(live_id, RoomStatus::Connecting);
                        }
                        return;
                    }

                    // Parse actual message types
                    Message parsed_msg;
                    parsed_msg.room_id = json.value("room_id", live_id);

                    // Handle timestamp - might be number or string
                    if (json.contains("timestamp") && json["timestamp"].is_number()) {
                        parsed_msg.timestamp = json["timestamp"].get<int64_t>();
                    } else {
                        parsed_msg.timestamp = std::time(nullptr) * 1000;
                    }

                    parsed_msg.user_id = json.value("user_id", "");
                    parsed_msg.user_name = json.value("user_name", "");
                    parsed_msg.content = json.value("content", "");
                    parsed_msg.trace_id = Logger::generate_trace_id();

                    if (type == "chat") {
                        parsed_msg.type = MessageType::Chat;
                    } else if (type == "gift") {
                        parsed_msg.type = MessageType::Gift;
                        if (json.contains("gift_count") && json["gift_count"].is_number()) {
                            parsed_msg.gift_count = json["gift_count"].get<int64_t>();
                        } else {
                            parsed_msg.gift_count = 1;
                        }
                    } else if (type == "enter") {
                        parsed_msg.type = MessageType::Enter;
                    } else if (type == "like") {
                        parsed_msg.type = MessageType::Like;
                    } else if (type == "social") {
                        parsed_msg.type = MessageType::Social;
                    } else if (type == "system") {
                        parsed_msg.type = MessageType::System;
                    } else if (type == "online_count") {
                        parsed_msg.type = MessageType::OnlineCount;
                    } else {
                        return; // Unknown type, skip
                    }

                    // Call message callback
                    if (m_message_cb) {
                        m_message_cb(parsed_msg);
                    }

                } catch (const std::exception& e) {
                    LOG_ERROR("Failed to parse proxy message: {}", e.what());
                }
            }
        });

        // Status callback
        session->client->on_status([this, live_id](bool connected) {
            std::unique_lock<std::mutex> lock(m_mutex);
            auto it = m_sessions.find(live_id);
            if (it == m_sessions.end()) return;

            if (connected) {
                LOG_INFO("WebSocket connected for live_id={}", live_id);
                it->second->connected = true;
                it->second->reconnect_attempts = 0;
                start_heartbeat_locked(live_id);
                lock.unlock();
                m_room_manager->update_status(live_id, RoomStatus::Live);
                if (m_status_cb) m_status_cb(live_id, RoomStatus::Live);
            } else {
                LOG_WARN("WebSocket disconnected for live_id={}", live_id);
                it->second->connected = false;
                it->second->heartbeat.stop();
                lock.unlock();
                m_room_manager->update_status(live_id, RoomStatus::Connecting);
                if (m_status_cb) m_status_cb(live_id, RoomStatus::Connecting);
                attempt_reconnect(live_id);
            }
        });

        // Set up MessageParser
        session->parser.set_room_id(room_id);

        session->parser.on_parsed([this](const Message& msg) {
            if (m_message_cb) {
                m_message_cb(msg);
            }
        });

        session->parser.on_ack([this, live_id](int64_t log_id, const std::string& internal_ext) {
#ifdef HAS_PROTOBUF
            ::PushFrame ack;
            ack.set_logid(static_cast<uint64_t>(log_id));
            ack.set_payloadtype("ack");
            ack.set_payload(internal_ext);

            std::string data = ack.SerializeAsString();
#else
            // Simple ACK frame: log_id (8 bytes, big-endian) + internal_ext
            std::string data;
            data.reserve(8 + internal_ext.size());
            for (int i = 7; i >= 0; --i) {
                data += static_cast<char>((static_cast<uint64_t>(log_id) >> (i * 8)) & 0xFF);
            }
            data += internal_ext;
#endif
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_sessions.find(live_id);
            if (it != m_sessions.end() && it->second->client) {
                it->second->client->send_binary(
                    reinterpret_cast<const uint8_t*>(data.data()), data.size());
            }
        });

        // Store session
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_sessions[live_id] = std::move(session);
        }

        // Connect to local proxy (proxy handles Douyin connection)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_sessions[live_id]->client->connect(proxy_url, "")) {
                LOG_ERROR("Failed to connect to proxy for live_id={}", live_id);
                m_sessions.erase(live_id);
                m_room_manager->update_status(live_id, RoomStatus::Offline);
                if (m_status_cb) m_status_cb(live_id, RoomStatus::Offline);
                return false;
            }
        }

        // Wait for connection to be established, then send connect command
        std::thread([this, live_id]() {
            // Wait for connection to be ready
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_sessions.find(live_id);
            if (it != m_sessions.end() && it->second->client) {
                std::string connect_cmd = "connect:" + live_id;
                it->second->client->send_binary(
                    reinterpret_cast<const uint8_t*>(connect_cmd.data()), connect_cmd.size());
                LOG_INFO("Sent connect command to proxy for live_id={}", live_id);
            }
        }).detach();

        LOG_INFO("Connection to live_id={} initiated", live_id);
        return true;
    }

    void disconnect(const std::string& live_id) override {
        LOG_INFO("Disconnecting from live_id={}", live_id);
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_sessions.find(live_id);
        if (it != m_sessions.end()) {
            it->second->heartbeat.stop();
            if (it->second->client) {
                it->second->client->disconnect();
            }
            m_sessions.erase(it);
        }

        m_room_manager->remove_room(live_id);
        if (m_status_cb) {
            m_status_cb(live_id, RoomStatus::Offline);
        }
    }

    void on_message(MessageCallback cb) override {
        m_message_cb = std::move(cb);
    }

    void on_status(StatusCallback cb) override {
        m_status_cb = std::move(cb);
    }

    bool is_connected(const std::string& live_id) const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessions.find(live_id);
        return it != m_sessions.end() && it->second->connected;
    }

    std::vector<std::string> connected_rooms() const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::string> rooms;
        for (const auto& [live_id, session] : m_sessions) {
            if (session->connected) {
                rooms.push_back(live_id);
            }
        }
        return rooms;
    }

    RoomManager* room_manager() override {
        return m_room_manager.get();
    }

private:
    std::string generate_user_unique_id() const {
        static thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<uint64_t> dist(1000000000000000000ULL, 9999999999999999999ULL);
        return std::to_string(dist(rng));
    }

    // Caller must hold m_mutex
    void start_heartbeat_locked(const std::string& live_id) {
        auto it = m_sessions.find(live_id);
        if (it == m_sessions.end()) return;

        auto& session = it->second;
        session->heartbeat.start(
            std::chrono::seconds(5),
            [this, live_id]() {
#ifdef HAS_PROTOBUF
                ::PushFrame hb;
                hb.set_payloadtype("hb");
                std::string data = hb.SerializeAsString();
#else
                // Simple heartbeat frame: just "hb" marker
                std::string data = "hb";
#endif
                std::lock_guard<std::mutex> lk(m_mutex);
                auto sit = m_sessions.find(live_id);
                if (sit != m_sessions.end() && sit->second->client && sit->second->connected) {
                    sit->second->client->send_binary(
                        reinterpret_cast<const uint8_t*>(data.data()), data.size());
                    LOG_TRACE("Heartbeat sent for live_id={}", live_id);
                }
            },
            true
        );
        LOG_INFO("Heartbeat timer started for live_id={}", live_id);
    }

    void attempt_reconnect(const std::string& live_id) {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto it = m_sessions.find(live_id);
        if (it == m_sessions.end()) return;

        auto& session = it->second;
        if (session->reconnect_attempts >= session->max_reconnect_attempts) {
            LOG_ERROR("Max reconnect attempts reached for room {}", live_id);
            session->connected = false;
            lock.unlock();
            m_room_manager->update_status(live_id, RoomStatus::Offline);
            if (m_status_cb) m_status_cb(live_id, RoomStatus::Offline);
            return;
        }

        session->reconnect_attempts++;
        int delay_ms = std::min(
            static_cast<int>(session->reconnect_interval.count()) * (1 << (session->reconnect_attempts - 1)),
            60000
        );

        LOG_INFO("Reconnecting to room {} in {}ms (attempt {})",
                 live_id, delay_ms, session->reconnect_attempts);

        std::string lid = live_id;
        int delay = delay_ms;
        lock.unlock();

        std::thread([this, lid, delay]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            connect(lid);
        }).detach();
    }

    MessageCallback m_message_cb;
    StatusCallback m_status_cb;

    mutable std::mutex m_mutex;
    std::map<std::string, std::unique_ptr<RoomSession>> m_sessions;

    Signature m_signature;
    std::string m_user_agent;
    std::unique_ptr<RoomManager> m_room_manager;
};

std::unique_ptr<CaptureEngine> create_capture_engine(const std::string& sign_js_path,
                                                      const std::string& a_bogus_js_path,
                                                      const std::string& user_agent) {
    return std::make_unique<DouyinCaptureEngine>(sign_js_path, a_bogus_js_path, user_agent);
}

} // namespace douyin
