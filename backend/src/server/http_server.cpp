#include "server/http_server.h"
#include "server/ws_server.h"
#include "server/static_file.h"
#include "capture/capture_engine.h"
#include "capture/room_manager.h"
#include "storage/message_store.h"
#include "models/config.h"
#include "models/room.h"
#include "models/message.h"
#include "utils/logger.h"

#include <httplib.h>

#include <nlohmann/json.hpp>
#include <atomic>

namespace douyin {

struct HttpServer::Impl {
    std::unique_ptr<httplib::Server> server;
    AppConfig config;
    WsServer* ws_server = nullptr;
    CaptureEngine* engine = nullptr;
    MessageStore* msg_store = nullptr;
    std::atomic<bool> running{false};
    int bound_port = 0;
};

HttpServer::HttpServer()
    : m_impl(std::make_unique<Impl>()) {
    m_impl->server = std::make_unique<httplib::Server>();
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::init(const AppConfig& config, WsServer* ws_server,
                       CaptureEngine* engine, MessageStore* msg_store) {
    m_impl->config = config;
    m_impl->ws_server = ws_server;
    m_impl->engine = engine;
    m_impl->msg_store = msg_store;
    setup_routes();
    setup_static_files(StaticFileHandler::default_static_dir());
}

void HttpServer::start() {
    auto& cfg = m_impl->config.server;
    LOG_INFO("HTTP server starting on {}:{}", cfg.host, cfg.port);
    m_impl->running.store(true);

    // 设置线程池，支持并发请求
    m_impl->server->new_task_queue = [] {
        return new httplib::ThreadPool(8);  // 8个工作线程
    };

    if (!m_impl->server->listen(cfg.host, cfg.port)) {
        LOG_ERROR("HTTP server failed to listen on {}:{}", cfg.host, cfg.port);
        m_impl->running.store(false);
    }
}

void HttpServer::stop() {
    if (m_impl->running.load()) {
        m_impl->server->stop();
        m_impl->running.store(false);
        LOG_INFO("HTTP server stopped");
    }
}

int HttpServer::port() const {
    return m_impl->bound_port;
}

void HttpServer::setup_routes() {
    auto& srv = *m_impl->server;

    // Health check
    srv.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status":"ok"})", "application/json");
    });

    // GET /api/rooms - List all rooms
    srv.Get("/api/rooms", [this](const httplib::Request&, httplib::Response& res) {
        if (!m_impl->engine || !m_impl->engine->room_manager()) {
            res.set_content("[]", "application/json");
            return;
        }

        auto rooms = m_impl->engine->room_manager()->get_all_rooms();
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& room : rooms) {
            arr.push_back(room.to_json());
        }
        res.set_content(arr.dump(2), "application/json");
    });

    // POST /api/rooms - Add a room (async, returns immediately)
    srv.Post("/api/rooms", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string live_id = body.value("live_id", "");
            if (live_id.empty()) {
                // Also accept room_id as alias for live_id
                live_id = body.value("room_id", "");
            }
            if (live_id.empty()) {
                res.status = 400;
                res.set_content(R"({"error":"live_id is required"})", "application/json");
                return;
            }

            LOG_INFO("Add room request: live_id={}", live_id);

            if (!m_impl->engine) {
                res.status = 500;
                res.set_content(R"({"error":"capture engine not available"})", "application/json");
                return;
            }

            // 启动异步连接（不阻塞 HTTP 线程）
            std::thread([this, live_id]() {
                m_impl->engine->connect(live_id);
            }).detach();

            // 立即返回成功
            res.status = 201;
            nlohmann::json resp = {{"status", "connecting"}, {"live_id", live_id}};
            res.set_content(resp.dump(2), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            nlohmann::json err = {{"error", "invalid JSON"}, {"detail", e.what()}};
            res.set_content(err.dump(2), "application/json");
        }
    });

    // DELETE /api/rooms/:liveId - Remove a room
    srv.Delete(R"(/api/rooms/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string live_id = req.matches[1];
        LOG_INFO("Remove room request: live_id={}", live_id);

        if (!m_impl->engine) {
            res.status = 500;
            res.set_content(R"({"error":"capture engine not available"})", "application/json");
            return;
        }

        // 异步断开，不阻塞 HTTP 响应
        std::thread([this, live_id]() {
            m_impl->engine->disconnect(live_id);
        }).detach();

        nlohmann::json resp = {{"status", "removed"}, {"live_id", live_id}};
        res.set_content(resp.dump(2), "application/json");
    });

    // GET /api/rooms/:liveId/stats - Room statistics
    srv.Get(R"(/api/rooms/([^/]+)/stats)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string live_id = req.matches[1];

        if (!m_impl->msg_store) {
            res.status = 500;
            res.set_content(R"({"error":"message store not available"})", "application/json");
            return;
        }

        // Get room info to find room_id
        std::string room_id = live_id;
        if (m_impl->engine && m_impl->engine->room_manager()) {
            auto room = m_impl->engine->room_manager()->get_room(live_id);
            if (room && !room->room_id.empty()) {
                room_id = room->room_id;
            }
        }

        auto stats = m_impl->msg_store->today_stats(room_id);
        res.set_content(stats.dump(2), "application/json");
    });

    // GET /api/rooms/:liveId/messages - Query messages (paginated)
    srv.Get(R"(/api/rooms/([^/]+)/messages)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string live_id = req.matches[1];

        if (!m_impl->msg_store) {
            res.status = 500;
            res.set_content(R"({"error":"message store not available"})", "application/json");
            return;
        }

        // Get room info to find room_id
        std::string room_id = live_id;
        if (m_impl->engine && m_impl->engine->room_manager()) {
            auto room = m_impl->engine->room_manager()->get_room(live_id);
            if (room && !room->room_id.empty()) {
                room_id = room->room_id;
            }
        }

        // Parse query parameters
        std::string type_filter = req.get_param_value("type");
        std::string keyword = req.get_param_value("keyword");
        int limit = 50;
        int offset = 0;
        try {
            std::string limit_str = req.get_param_value("limit");
            if (!limit_str.empty()) limit = std::stoi(limit_str);
            std::string offset_str = req.get_param_value("offset");
            if (!offset_str.empty()) offset = std::stoi(offset_str);
        } catch (...) {}

        auto messages = m_impl->msg_store->query(room_id, type_filter, keyword, offset, limit);
        auto total = m_impl->msg_store->count(room_id, type_filter);

        nlohmann::json result;
        result["room_id"] = live_id;
        result["total"] = total;
        result["offset"] = offset;
        result["limit"] = limit;

        nlohmann::json msg_arr = nlohmann::json::array();
        for (const auto& msg : messages) {
            msg_arr.push_back(msg.to_json());
        }
        result["messages"] = msg_arr;

        res.set_content(result.dump(2), "application/json");
    });

    // GET /api/config - Get current configuration
    srv.Get("/api/config", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(m_impl->config.to_json().dump(2), "application/json");
    });

    // PUT /api/config - Update configuration
    srv.Put("/api/config", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            m_impl->config.from_json(body);
            LOG_INFO("Configuration updated via API");
            res.set_content(R"({"status":"updated"})", "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
        }
    });
}

void HttpServer::setup_static_files(const std::string& static_dir) {
    auto& srv = *m_impl->server;

    // Serve static files from the frontend dist directory
    if (!static_dir.empty()) {
        srv.set_mount_point("/", static_dir);
        LOG_INFO("Static files served from: {}", static_dir);
    }

    // Catch-all for SPA routing: serve index.html for non-API, non-file paths
    srv.Get(R"(/(.*))", [](const httplib::Request& req, httplib::Response& res) {
        // Only handle non-API routes
        if (req.path.substr(0, 4) == "/api") {
            res.status = 404;
            res.set_content(R"({"error":"not found"})", "application/json");
        }
        // For other routes, the mount_point handler will serve files or 404
    });
}

} // namespace douyin
