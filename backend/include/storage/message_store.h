#pragma once

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

namespace douyin {

struct Message;
class Database;

// High-level message storage layer.
// Persists messages to SQLite and provides query methods.
class MessageStore {
public:
    explicit MessageStore(Database* db);
    ~MessageStore();

    // Non-copyable
    MessageStore(const MessageStore&) = delete;
    MessageStore& operator=(const MessageStore&) = delete;

    // Insert a message into the database. Returns the row id, or 0 on failure.
    int64_t insert(const Message& msg);

    // Query messages with optional filters.
    // type_filter: empty string means all types, or "chat"/"gift"/"enter"/etc.
    // keyword: search in content (empty means no filter).
    // offset/limit: pagination.
    std::vector<Message> query(const std::string& room_id,
                               const std::string& type_filter = "",
                               const std::string& keyword = "",
                               int offset = 0,
                               int limit = 50);

    // Get total message count for a room.
    int64_t count(const std::string& room_id, const std::string& type_filter = "");

    // Get today's stats for a room (chat count, gift count, enter count, etc.).
    // Returns a JSON object with the stats.
    nlohmann::json today_stats(const std::string& room_id);

private:
    Database* m_db; // Not owned
};

} // namespace douyin
