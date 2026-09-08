#pragma once

#include <string>
#include <memory>
#include <functional>
#include <set>
#include <mutex>
#include <nlohmann/json.hpp>

namespace douyin {

// WebSocket server using IXWebSocket (server mode).
// Handles client connections and broadcasts real-time messages.
class WsServer {
public:
    using ClientId = std::string;

    WsServer();
    ~WsServer();

    // Non-copyable
    WsServer(const WsServer&) = delete;
    WsServer& operator=(const WsServer&) = delete;

    // Initialize and start the WebSocket server on the given port.
    // path: the WebSocket endpoint path (default "/ws").
    void init(const std::string& host, int port, const std::string& path = "/ws");

    // Start the server (non-blocking, runs internally).
    void start();

    // Stop the server.
    void stop();

    // Broadcast a JSON message to all connected clients.
    // If room_id is non-empty, only send to clients subscribed to that room.
    void broadcast(const nlohmann::json& msg, const std::string& room_id = "");

    // Get the number of connected clients.
    size_t client_count() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace douyin
