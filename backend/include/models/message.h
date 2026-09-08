#pragma once

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace douyin {

enum class MessageType {
    Chat,         // Danmu (chat message)
    Gift,         // Gift
    Enter,        // User entered room
    Like,         // User liked
    Social,       // Follow / share
    OnlineCount,  // Online viewer count update
    System,       // System announcement / control
    Stats,        // Room stats
    Unknown
};

std::string message_type_to_string(MessageType type);
MessageType message_type_from_string(const std::string& s);

struct Message {
    int64_t id = 0;              // DB row id (0 if not persisted)
    MessageType type = MessageType::Unknown;
    std::string room_id;
    int64_t timestamp = 0;       // Unix timestamp ms
    std::string user_id;
    std::string user_name;
    std::string content;         // Text content or gift name
    int64_t gift_count = 0;      // For gifts
    std::string extra;           // JSON string for type-specific fields
    std::string trace_id;        // For log tracing

    nlohmann::json to_json() const;
    static Message from_json(const nlohmann::json& j);
};

} // namespace douyin
