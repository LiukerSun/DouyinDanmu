#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace douyin {

struct ServerConfig {
    std::string host = "0.0.0.0";
    int port = 8080;
};

struct DatabaseConfig {
    std::string path = "./data/douyin_danmu.db";
};

struct CaptureConfig {
    std::vector<std::string> rooms;
    int heartbeat_interval = 5;
    int reconnect_interval = 5;
    int max_reconnect_attempts = 10;
    std::string sign_js_path = "./scripts/sign.js";
    std::string a_bogus_js_path = "./scripts/a_bogus.js";
    std::string user_agent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                             "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";
};

struct StorageConfig {
    // Placeholder for future sampling/aggregation settings
};

struct NotificationConfig {
    std::vector<std::string> trigger_types = {"gift", "social"};
};

struct LogConfig {
    std::string level = "info";
    std::string file = "./logs/app.log";
    int max_size_mb = 50;
    int max_files = 5;
};

struct AppConfig {
    ServerConfig server;
    DatabaseConfig database;
    CaptureConfig capture;
    StorageConfig storage;
    NotificationConfig notification;
    LogConfig log;

    // Load from a TOML file path
    static AppConfig load_from_file(const std::string& path);

    // Serialize to JSON (for GET /api/config)
    nlohmann::json to_json() const;

    // Deserialize from JSON (for PUT /api/config)
    void from_json(const nlohmann::json& j);
};

} // namespace douyin
