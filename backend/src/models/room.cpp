#include "models/room.h"

namespace douyin {

std::string room_status_to_string(RoomStatus status) {
    switch (status) {
        case RoomStatus::Offline:    return "offline";
        case RoomStatus::Connecting: return "connecting";
        case RoomStatus::Live:       return "live";
        default:                     return "unknown";
    }
}

RoomStatus room_status_from_string(const std::string& s) {
    if (s == "live")       return RoomStatus::Live;
    if (s == "connecting") return RoomStatus::Connecting;
    return RoomStatus::Offline;
}

nlohmann::json Room::to_json() const {
    return {
        {"room_id",       room_id},
        {"live_id",       live_id},
        {"title",         title},
        {"anchor_name",   anchor_name},
        {"status",        room_status_to_string(status)},
        {"online_count",  online_count},
        {"created_at",    created_at},
        {"updated_at",    updated_at}
    };
}

Room Room::from_json(const nlohmann::json& j) {
    Room r;
    r.room_id      = j.value("room_id", "");
    r.live_id      = j.value("live_id", "");
    r.title        = j.value("title", "");
    r.anchor_name  = j.value("anchor_name", "");
    r.status       = room_status_from_string(j.value("status", "offline"));
    r.online_count = j.value("online_count", int64_t(0));
    r.created_at   = j.value("created_at", int64_t(0));
    r.updated_at   = j.value("updated_at", int64_t(0));
    return r;
}

} // namespace douyin
