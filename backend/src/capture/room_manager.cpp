#include "capture/room_manager.h"
#include "models/room.h"
#include "utils/logger.h"

#include <chrono>

namespace douyin {

RoomManager::RoomManager() = default;
RoomManager::~RoomManager() = default;

bool RoomManager::add_room(const std::string& live_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_rooms.count(live_id)) {
        LOG_WARN("RoomManager: room already exists: {}", live_id);
        return false;
    }

    auto room = std::make_shared<Room>();
    room->live_id = live_id;
    room->status = RoomStatus::Offline;

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    room->created_at = now;
    room->updated_at = now;

    m_rooms[live_id] = room;
    LOG_INFO("RoomManager: added room {}", live_id);
    return true;
}

bool RoomManager::remove_room(const std::string& live_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_rooms.find(live_id);
    if (it == m_rooms.end()) {
        LOG_WARN("RoomManager: room not found: {}", live_id);
        return false;
    }
    m_rooms.erase(it);
    LOG_INFO("RoomManager: removed room {}", live_id);
    return true;
}

bool RoomManager::has_room(const std::string& live_id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rooms.count(live_id) > 0;
}

std::shared_ptr<Room> RoomManager::get_room(const std::string& live_id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_rooms.find(live_id);
    if (it != m_rooms.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<Room> RoomManager::get_all_rooms() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Room> result;
    result.reserve(m_rooms.size());
    for (const auto& [id, room] : m_rooms) {
        result.push_back(*room);
    }
    return result;
}

void RoomManager::update_status(const std::string& live_id, RoomStatus status) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_rooms.find(live_id);
    if (it != m_rooms.end()) {
        it->second->status = status;
        it->second->updated_at = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
}

void RoomManager::update_room_id(const std::string& live_id, const std::string& room_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_rooms.find(live_id);
    if (it != m_rooms.end()) {
        it->second->room_id = room_id;
        it->second->updated_at = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
}

void RoomManager::update_online_count(const std::string& live_id, int64_t count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_rooms.find(live_id);
    if (it != m_rooms.end()) {
        it->second->online_count = count;
        it->second->updated_at = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
}

void RoomManager::update_anchor_name(const std::string& live_id, const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_rooms.find(live_id);
    if (it != m_rooms.end()) {
        it->second->anchor_name = name;
    }
}

void RoomManager::update_title(const std::string& live_id, const std::string& title) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_rooms.find(live_id);
    if (it != m_rooms.end()) {
        it->second->title = title;
    }
}

size_t RoomManager::count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rooms.size();
}

} // namespace douyin
