#include "models/config.h"
#include <toml++/toml.hpp>
#include <fstream>

namespace douyin {

AppConfig AppConfig::load_from_file(const std::string& path) {
    AppConfig config;
    try {
        auto tbl = toml::parse_file(path);

        // [server]
        if (auto server = tbl["server"].as_table()) {
            config.server.host = server->get("host")->value_or<std::string>("0.0.0.0");
            config.server.port = server->get("port")->value_or(8080);
        }

        // [database]
        if (auto db = tbl["database"].as_table()) {
            config.database.path = db->get("path")->value_or<std::string>("./data/douyin_danmu.db");
        }

        // [capture]
        if (auto cap = tbl["capture"].as_table()) {
            config.capture.heartbeat_interval = cap->get("heartbeat_interval")->value_or(5);
            config.capture.reconnect_interval = cap->get("reconnect_interval")->value_or(5);
            config.capture.max_reconnect_attempts = cap->get("max_reconnect_attempts")->value_or(10);
            config.capture.sign_js_path = cap->get("sign_js_path")->value_or<std::string>("./scripts/sign.js");
            config.capture.a_bogus_js_path = cap->get("a_bogus_js_path")->value_or<std::string>("./scripts/a_bogus.js");
            config.capture.user_agent = cap->get("user_agent")->value_or<std::string>("Mozilla/5.0");

            if (auto rooms = cap->get("rooms")->as_array()) {
                for (auto& room : *rooms) {
                    if (auto s = room.value<std::string>()) {
                        config.capture.rooms.push_back(*s);
                    }
                }
            }
        }

        // [notification]
        if (auto notif = tbl["notification"].as_table()) {
            if (auto types = notif->get("trigger_types")->as_array()) {
                config.notification.trigger_types.clear();
                for (auto& t : *types) {
                    if (auto s = t.value<std::string>()) {
                        config.notification.trigger_types.push_back(*s);
                    }
                }
            }
        }

        // [log]
        if (auto log = tbl["log"].as_table()) {
            config.log.level = log->get("level")->value_or<std::string>("info");
            config.log.file = log->get("file")->value_or<std::string>("./logs/app.log");
            config.log.max_size_mb = log->get("max_size_mb")->value_or(50);
            config.log.max_files = log->get("max_files")->value_or(5);
        }
    } catch (const toml::parse_error& e) {
        // Return default config on parse error
        (void)e;
    }
    return config;
}

nlohmann::json AppConfig::to_json() const {
    nlohmann::json j;
    j["server"]["host"] = server.host;
    j["server"]["port"] = server.port;
    j["database"]["path"] = database.path;
    j["capture"]["rooms"] = capture.rooms;
    j["capture"]["heartbeat_interval"] = capture.heartbeat_interval;
    j["capture"]["reconnect_interval"] = capture.reconnect_interval;
    j["capture"]["max_reconnect_attempts"] = capture.max_reconnect_attempts;
    j["notification"]["trigger_types"] = notification.trigger_types;
    j["log"]["level"] = log.level;
    j["log"]["file"] = log.file;
    return j;
}

void AppConfig::from_json(const nlohmann::json& j) {
    if (j.contains("server")) {
        if (j["server"].contains("host")) server.host = j["server"]["host"];
        if (j["server"].contains("port")) server.port = j["server"]["port"];
    }
    if (j.contains("capture")) {
        if (j["capture"].contains("rooms")) capture.rooms = j["capture"]["rooms"].get<std::vector<std::string>>();
        if (j["capture"].contains("heartbeat_interval")) capture.heartbeat_interval = j["capture"]["heartbeat_interval"];
    }
    if (j.contains("notification")) {
        if (j["notification"].contains("trigger_types"))
            notification.trigger_types = j["notification"]["trigger_types"].get<std::vector<std::string>>();
    }
    if (j.contains("log")) {
        if (j["log"].contains("level")) log.level = j["log"]["level"];
    }
}

} // namespace douyin
