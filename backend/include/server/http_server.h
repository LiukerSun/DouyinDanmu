#pragma once

#include <string>
#include <memory>
#include <functional>
#include <atomic>

namespace douyin {

class WsServer;
class CaptureEngine;
class MessageStore;
struct AppConfig;

// HTTP REST API server using cpp-httplib.
// Also serves static files for the frontend.
class HttpServer {
public:
    HttpServer();
    ~HttpServer();

    // Non-copyable
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    // Initialize the server with config. Must be called before start().
    void init(const AppConfig& config, WsServer* ws_server,
              CaptureEngine* engine, MessageStore* msg_store);

    // Start listening (blocking). Runs on the calling thread.
    void start();

    // Stop the server from another thread.
    void stop();

    // Get the bound port (useful if port 0 was specified).
    int port() const;

private:
    void setup_routes();
    void setup_static_files(const std::string& static_dir);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace douyin
