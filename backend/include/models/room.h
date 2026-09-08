#pragma once

#include <string>
#include <ctime>
#include <nlohmann/json.hpp>

namespace douyin {

enum class RoomStatus {
    Offline,    // Not connected / stream ended
    Connecting, // Connection in progress
    Live        // Connected and receiving data
};

std::string room_status_to_string(RoomStatus status);
RoomStatus room_status_from_string(const std::string& s);

struct Room {
    std::string room_id;       // Douyin room ID (numeric string)
    std::string live_id;       // The live_id from URL (may differ)
    std::string title;         // Stream title (populated after connection)
    std::string anchor_name;   // Streamer name
    RoomStatus status = RoomStatus::Offline;
    int64_t online_count = 0;
    int64_t created_at = 0;    // Unix timestamp ms
    int64_t updated_at = 0;    // Unix timestamp ms

    nlohmann::json to_json() const;
    static Room from_json(const nlohmann::json& j);
};

} // namespace douyin
