#include "server/ws_server.h"
#include "utils/logger.h"

#include <ixwebsocket/IXWebSocketServer.h>
#include <set>

namespace douyin {

struct WsServer::Impl {
    std::unique_ptr<ix::WebSocketServer> server;
    std::string host;
    int port = 0;
    std::string path = "/ws";

    // Client tracking: connection -> subscribed room_ids
    struct ClientInfo {
        std::set<std::string> subscribed_rooms;
        ix::WebSocket* ws = nullptr;  // Raw pointer to the WebSocket for sending
    };

    std::mutex clients_mutex;
    std::map<void*, ClientInfo> clients;
};

WsServer::WsServer()
    : m_impl(std::make_unique<Impl>()) {
}

WsServer::~WsServer() {
    stop();
}

void WsServer::init(const std::string& host, int port, const std::string& path) {
    m_impl->host = host;
    m_impl->port = port;
    m_impl->path = path;

    m_impl->server = std::make_unique<ix::WebSocketServer>(port, host);

    m_impl->server->setOnClientMessageCallback(
        [this](std::shared_ptr<ix::ConnectionState> connectionState,
               ix::WebSocket& webSocket,
               const ix::WebSocketMessagePtr& msg) {
            auto* conn_ptr = connectionState.get();

            if (msg->type == ix::WebSocketMessageType::Open) {
                LOG_INFO("WS client connected: {}", connectionState->getId());
                std::lock_guard<std::mutex> lock(m_impl->clients_mutex);
                Impl::ClientInfo info;
                info.ws = &webSocket;
                m_impl->clients[conn_ptr] = info;
            }
            else if (msg->type == ix::WebSocketMessageType::Close) {
                LOG_INFO("WS client disconnected: {}", connectionState->getId());
                std::lock_guard<std::mutex> lock(m_impl->clients_mutex);
                m_impl->clients.erase(conn_ptr);
            }
            else if (msg->type == ix::WebSocketMessageType::Message) {
                // Handle subscription control messages
                try {
                    auto j = nlohmann::json::parse(msg->str);
                    std::string action = j.value("action", "");
                    std::string room_id = j.value("room_id", "");

                    if (action == "subscribe" && !room_id.empty()) {
                        std::lock_guard<std::mutex> lock(m_impl->clients_mutex);
                        if (m_impl->clients.count(conn_ptr)) {
                            m_impl->clients[conn_ptr].subscribed_rooms.insert(room_id);
                            LOG_DEBUG("WS client subscribed to room: {}", room_id);
                        }
                    } else if (action == "unsubscribe" && !room_id.empty()) {
                        std::lock_guard<std::mutex> lock(m_impl->clients_mutex);
                        if (m_impl->clients.count(conn_ptr)) {
                            m_impl->clients[conn_ptr].subscribed_rooms.erase(room_id);
                            LOG_DEBUG("WS client unsubscribed from room: {}", room_id);
                        }
                    } else if (action == "subscribe_all") {
                        std::lock_guard<std::mutex> lock(m_impl->clients_mutex);
                        if (m_impl->clients.count(conn_ptr)) {
                            m_impl->clients[conn_ptr].subscribed_rooms.clear(); // empty = all
                            LOG_DEBUG("WS client subscribed to all rooms");
                        }
                    }
                } catch (const std::exception&) {
                    // Ignore invalid JSON messages
                }
            }
        });
}

void WsServer::start() {
    if (!m_impl->server) {
        LOG_ERROR("WS server not initialized");
        return;
    }

    auto res = m_impl->server->listen();
    if (!res.first) {
        LOG_ERROR("WS server failed to listen: {}", res.second);
        return;
    }

    m_impl->server->start();
    LOG_INFO("WebSocket server started on {}:{}", m_impl->host, m_impl->port);
}

void WsServer::stop() {
    if (m_impl->server) {
        m_impl->server->stop();
        LOG_INFO("WebSocket server stopped");
    }
}

void WsServer::broadcast(const nlohmann::json& msg, const std::string& room_id) {
    if (!m_impl->server) return;

    std::string payload = msg.dump();
    std::lock_guard<std::mutex> lock(m_impl->clients_mutex);

    for (auto& [conn_ptr, info] : m_impl->clients) {
        // If room_id is specified, only send to clients subscribed to that room
        // (empty subscribed_rooms set means "subscribe all" - always send)
        if (!room_id.empty() &&
            !info.subscribed_rooms.empty() &&
            info.subscribed_rooms.find(room_id) == info.subscribed_rooms.end()) {
            continue;
        }

        // Send directly to this client's WebSocket
        if (info.ws) {
            try {
                info.ws->send(payload);
            } catch (const std::exception& e) {
                LOG_WARN("WS broadcast send failed: {}", e.what());
            }
        }
    }
}

size_t WsServer::client_count() const {
    std::lock_guard<std::mutex> lock(m_impl->clients_mutex);
    return m_impl->clients.size();
}

} // namespace douyin
