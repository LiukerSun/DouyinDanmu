#include "models/message.h"

namespace douyin {

std::string message_type_to_string(MessageType type) {
    switch (type) {
        case MessageType::Chat:        return "chat";
        case MessageType::Gift:        return "gift";
        case MessageType::Enter:       return "enter";
        case MessageType::Like:        return "like";
        case MessageType::Social:      return "social";
        case MessageType::OnlineCount: return "online_count";
        case MessageType::System:      return "system";
        case MessageType::Stats:       return "stats";
        default:                       return "unknown";
    }
}

MessageType message_type_from_string(const std::string& s) {
    if (s == "chat")         return MessageType::Chat;
    if (s == "gift")         return MessageType::Gift;
    if (s == "enter")        return MessageType::Enter;
    if (s == "like")         return MessageType::Like;
    if (s == "social")       return MessageType::Social;
    if (s == "online_count") return MessageType::OnlineCount;
    if (s == "system")       return MessageType::System;
    if (s == "stats")        return MessageType::Stats;
    return MessageType::Unknown;
}

nlohmann::json Message::to_json() const {
    return {
        {"id",         id},
        {"type",       message_type_to_string(type)},
        {"room_id",    room_id},
        {"timestamp",  timestamp},
        {"user_id",    user_id},
        {"user_name",  user_name},
        {"content",    content},
        {"gift_count", gift_count},
        {"extra",      extra},
        {"trace_id",   trace_id}
    };
}

Message Message::from_json(const nlohmann::json& j) {
    Message m;
    m.id         = j.value("id", int64_t(0));
    m.type       = message_type_from_string(j.value("type", "unknown"));
    m.room_id    = j.value("room_id", "");
    m.timestamp  = j.value("timestamp", int64_t(0));
    m.user_id    = j.value("user_id", "");
    m.user_name  = j.value("user_name", "");
    m.content    = j.value("content", "");
    m.gift_count = j.value("gift_count", int64_t(0));
    m.extra      = j.value("extra", "");
    m.trace_id   = j.value("trace_id", "");
    return m;
}

} // namespace douyin
