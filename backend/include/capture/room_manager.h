#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <memory>
#include <nlohmann/json.hpp>

namespace douyin {

struct Room;
enum class RoomStatus;

// Manages the list of active rooms, bridging CaptureEngine and HTTP API.
class RoomManager {
public:
    RoomManager();
    ~RoomManager();

    // Non-copyable
    RoomManager(const RoomManager&) = delete;
    RoomManager& operator=(const RoomManager&) = delete;

    // Add a room. Returns true if added (not already present).
    bool add_room(const std::string& live_id);

    // Remove a room. Returns true if removed.
    bool remove_room(const std::string& live_id);

    // Check if a room exists.
    bool has_room(const std::string& live_id) const;

    // Get a room by live_id. Returns nullptr if not found.
    std::shared_ptr<Room> get_room(const std::string& live_id) const;

    // Get all rooms as a vector.
    std::vector<Room> get_all_rooms() const;

    // Update room status.
    void update_status(const std::string& live_id, RoomStatus status);

    // Update room_id (the numeric Douyin room ID, fetched after connection).
    void update_room_id(const std::string& live_id, const std::string& room_id);

    // Update online count.
    void update_online_count(const std::string& live_id, int64_t count);

    // Update anchor name.
    void update_anchor_name(const std::string& live_id, const std::string& name);

    // Update title.
    void update_title(const std::string& live_id, const std::string& title);

    // Get the count of rooms.
    size_t count() const;

private:
    mutable std::mutex m_mutex;
    std::map<std::string, std::shared_ptr<Room>> m_rooms;  // key = live_id
};

} // namespace douyin
