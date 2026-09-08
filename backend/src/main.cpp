// DouyinDanmu - Douyin Live Stream Capture Tool
// Backend entry point

#include "server/http_server.h"
#include "server/ws_server.h"
#include "capture/capture_engine.h"
#include "storage/database.h"
#include "storage/message_store.h"
#include "models/config.h"
#include "models/message.h"
#include "models/room.h"
#include "utils/logger.h"

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace douyin;

// Version info
static constexpr int VERSION_MAJOR = 0;
static constexpr int VERSION_MINOR = 1;
static constexpr int VERSION_PATCH = 0;

static std::atomic<bool> g_running{true};

static void signal_handler(int sig) {
    (void)sig;
    g_running.store(false);
}

static void print_usage(const char* program) {
    std::cout << "DouyinDanmu v" << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_PATCH
              << " - Douyin Live Stream Capture Tool\n\n"
              << "Usage: " << program << " [options]\n\n"
              << "Options:\n"
              << "  -c, --config <file>    Config file path (default: config.toml)\n"
              << "  -p, --port <port>      HTTP/WebSocket port (default: 8080)\n"
              << "  --no-web               Disable web UI, data capture only\n"
              << "  --interactive          Enter interactive command mode\n"
              << "  -h, --help             Show this help message\n"
              << "  -v, --version          Show version\n";
}

static void print_version() {
    std::cout << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_PATCH << std::endl;
}

static void run_interactive(CaptureEngine* engine, Database* db, const AppConfig& config) {
    std::cout << "\nDouyinDanmu Interactive Mode\n"
              << "Type 'help' for available commands, 'quit' to exit.\n\n";

    std::string line;
    while (g_running.load()) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;

        if (line.empty()) continue;

        if (line == "help" || line == "h") {
            std::cout << "Commands:\n"
                      << "  add <live_id>     - Add and monitor a room\n"
                      << "  remove <live_id>  - Stop monitoring a room\n"
                      << "  list              - List monitored rooms\n"
                      << "  status            - Show connection status and stats\n"
                      << "  open              - Open Web UI in browser\n"
                      << "  help              - Show this help\n"
                      << "  quit / exit       - Exit gracefully\n";
        }
        else if (line.substr(0, 4) == "add ") {
            std::string live_id = line.substr(4);
            // Trim whitespace
            while (!live_id.empty() && live_id.front() == ' ') live_id.erase(0, 1);
            if (!live_id.empty()) {
                LOG_INFO("Interactive: adding room {}", live_id);
                engine->connect(live_id);
            }
        }
        else if (line.substr(0, 7) == "remove ") {
            std::string live_id = line.substr(7);
            while (!live_id.empty() && live_id.front() == ' ') live_id.erase(0, 1);
            if (!live_id.empty()) {
                LOG_INFO("Interactive: removing room {}", live_id);
                engine->disconnect(live_id);
            }
        }
        else if (line == "list") {
            auto rooms = engine->connected_rooms();
            if (rooms.empty()) {
                std::cout << "No rooms being monitored.\n";
            } else {
                std::cout << "Monitored rooms:\n";
                for (const auto& r : rooms) {
                    std::cout << "  - " << r << "\n";
                }
            }
        }
        else if (line == "status") {
            auto rooms = engine->connected_rooms();
            std::cout << "Connected rooms: " << rooms.size() << "\n"
                      << "Database: " << (db->is_open() ? "open" : "closed") << "\n";
        }
        else if (line == "open") {
#ifdef _WIN32
            std::string url = "http://localhost:" + std::to_string(config.server.port);
            ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
            std::cout << "Please open http://localhost:" << config.server.port << " in your browser.\n";
#endif
        }
        else if (line == "quit" || line == "exit") {
            g_running.store(false);
        }
        else {
            std::cout << "Unknown command: " << line << ". Type 'help' for commands.\n";
        }
    }
}

int main(int argc, char* argv[]) {
    // Default values
    std::string config_path = "config.toml";
    int port_override = -1;
    bool no_web = false;
    bool interactive = false;

    // Parse CLI arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            config_path = argv[++i];
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port_override = std::stoi(argv[++i]);
        } else if (arg == "--no-web") {
            no_web = true;
        } else if (arg == "--interactive") {
            interactive = true;
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            print_version();
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    // Set up signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Load configuration
    AppConfig config = AppConfig::load_from_file(config_path);
    if (port_override > 0) {
        config.server.port = port_override;
    }

    // Initialize logger
    Logger::init(config.log.level, config.log.file, config.log.max_size_mb, config.log.max_files);
    LOG_INFO("DouyinDanmu v{}.{}.{} starting...", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    LOG_INFO("Config loaded from: {}", config_path);

    // Initialize database
    Database db;
    if (!db.open(config.database.path)) {
        LOG_ERROR("Failed to open database: {}", config.database.path);
        return 1;
    }
    if (!db.migrate()) {
        LOG_ERROR("Database migration failed");
        return 1;
    }

    MessageStore msg_store(&db);

    // Initialize capture engine with signature scripts
    auto engine = create_capture_engine(
        config.capture.sign_js_path,
        config.capture.a_bogus_js_path,
        config.capture.user_agent
    );

    // Start WebSocket server (before connecting rooms, so broadcasts work)
    WsServer ws_server;
    if (!no_web) {
        ws_server.init(config.server.host, config.server.port + 1);
        ws_server.start();
    }

    // Wire up message callback: store + broadcast via WebSocket
    engine->on_message([&](const Message& msg) {
        // Store message in database
        int64_t row_id = msg_store.insert(msg);
        LOG_DEBUG("Message stored: type={}, room={}, user={}",
                  message_type_to_string(msg.type), msg.room_id, msg.user_name);

        // Broadcast to WebSocket clients
        if (!no_web) {
            nlohmann::json ws_msg;
            ws_msg["type"] = message_type_to_string(msg.type);
            ws_msg["room_id"] = msg.room_id;
            ws_msg["timestamp"] = msg.timestamp;
            ws_msg["data"] = {
                {"id",         row_id},
                {"user_id",    msg.user_id},
                {"user_name",  msg.user_name},
                {"content",    msg.content},
                {"gift_count", msg.gift_count},
                {"extra",      msg.extra.empty() ? nlohmann::json::object() : nlohmann::json::parse(msg.extra, nullptr, false)}
            };
            ws_server.broadcast(ws_msg, msg.room_id);
        }
    });

    engine->on_status([&](const std::string& live_id, RoomStatus status) {
        LOG_INFO("Room {} status: {}", live_id, room_status_to_string(status));

        // Broadcast status change to WebSocket clients
        if (!no_web) {
            nlohmann::json ws_msg;
            ws_msg["type"] = "status";
            ws_msg["live_id"] = live_id;
            ws_msg["status"] = room_status_to_string(status);
            ws_server.broadcast(ws_msg);
        }
    });

    // Don't auto-connect to configured rooms - let the frontend manage rooms via API
    // Users can add rooms through the web UI

    // Start HTTP server
    std::thread http_thread;
    HttpServer http_server;

    if (!no_web) {
        // HTTP server
        http_server.init(config, &ws_server, engine.get(), &msg_store);
        http_thread = std::thread([&http_server]() {
            http_server.start();
        });

        LOG_INFO("Web UI available at http://{}:{}", config.server.host, config.server.port);
    }

    // Run mode
    if (interactive) {
        run_interactive(engine.get(), &db, config);
    } else {
        LOG_INFO("Running in headless mode. Press Ctrl+C to exit.");
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    // Shutdown
    LOG_INFO("Shutting down...");
    engine.reset();

    if (!no_web) {
        http_server.stop();
        ws_server.stop();
        if (http_thread.joinable()) http_thread.join();
    }

    db.close();
    LOG_INFO("Shutdown complete.");

    return 0;
}
