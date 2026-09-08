#include "storage/message_store.h"
#include "storage/database.h"
#include "models/message.h"
#include "utils/logger.h"

#include <sqlite3.h>
#include <sstream>
#include <chrono>
#include <ctime>

namespace douyin {

MessageStore::MessageStore(Database* db)
    : m_db(db) {
}

MessageStore::~MessageStore() = default;

int64_t MessageStore::insert(const Message& msg) {
    if (!m_db || !m_db->is_open()) {
        LOG_ERROR("MessageStore: database not open");
        return 0;
    }

    const char* sql = R"SQL(
        INSERT INTO messages (type, room_id, timestamp, user_id, user_name, content, gift_count, extra, trace_id)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);
    )SQL";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db->handle(), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        LOG_ERROR("MessageStore: prepare failed: {}", sqlite3_errmsg(m_db->handle()));
        return 0;
    }

    std::string type_str = message_type_to_string(msg.type);

    sqlite3_bind_text(stmt, 1, type_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, msg.room_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, msg.timestamp);
    sqlite3_bind_text(stmt, 4, msg.user_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, msg.user_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, msg.content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 7, msg.gift_count);
    sqlite3_bind_text(stmt, 8, msg.extra.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, msg.trace_id.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        LOG_ERROR("MessageStore: insert failed: {}", sqlite3_errmsg(m_db->handle()));
        return 0;
    }

    return sqlite3_last_insert_rowid(m_db->handle());
}

std::vector<Message> MessageStore::query(const std::string& room_id,
                                          const std::string& type_filter,
                                          const std::string& keyword,
                                          int offset,
                                          int limit) {
    std::vector<Message> results;
    if (!m_db || !m_db->is_open()) return results;

    std::ostringstream sql;
    sql << "SELECT id, type, room_id, timestamp, user_id, user_name, content, gift_count, extra, trace_id "
        << "FROM messages WHERE room_id = ?";

    if (!type_filter.empty()) {
        sql << " AND type = ?";
    }
    if (!keyword.empty()) {
        sql << " AND content LIKE ?";
    }

    sql << " ORDER BY timestamp DESC LIMIT ? OFFSET ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db->handle(), sql.str().c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        LOG_ERROR("MessageStore: query prepare failed: {}", sqlite3_errmsg(m_db->handle()));
        return results;
    }

    int param_idx = 1;
    sqlite3_bind_text(stmt, param_idx++, room_id.c_str(), -1, SQLITE_TRANSIENT);

    if (!type_filter.empty()) {
        sqlite3_bind_text(stmt, param_idx++, type_filter.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (!keyword.empty()) {
        std::string like_pattern = "%" + keyword + "%";
        sqlite3_bind_text(stmt, param_idx++, like_pattern.c_str(), -1, SQLITE_TRANSIENT);
    }

    sqlite3_bind_int(stmt, param_idx++, limit);
    sqlite3_bind_int(stmt, param_idx++, offset);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Message msg;
        msg.id         = sqlite3_column_int64(stmt, 0);
        msg.type       = message_type_from_string(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        msg.room_id    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        msg.timestamp  = sqlite3_column_int64(stmt, 3);
        msg.user_id    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        msg.user_name  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        msg.content    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        msg.gift_count = sqlite3_column_int64(stmt, 7);
        msg.extra      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        msg.trace_id   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        results.push_back(std::move(msg));
    }

    sqlite3_finalize(stmt);
    return results;
}

int64_t MessageStore::count(const std::string& room_id, const std::string& type_filter) {
    if (!m_db || !m_db->is_open()) return 0;

    std::string sql = "SELECT COUNT(*) FROM messages WHERE room_id = ?";
    if (!type_filter.empty()) {
        sql += " AND type = ?";
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db->handle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return 0;

    sqlite3_bind_text(stmt, 1, room_id.c_str(), -1, SQLITE_TRANSIENT);
    if (!type_filter.empty()) {
        sqlite3_bind_text(stmt, 2, type_filter.c_str(), -1, SQLITE_TRANSIENT);
    }

    int64_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = sqlite3_column_int64(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return result;
}

nlohmann::json MessageStore::today_stats(const std::string& room_id) {
    nlohmann::json stats;
    stats["room_id"] = room_id;
    stats["chat_count"] = 0;
    stats["gift_count"] = 0;
    stats["enter_count"] = 0;
    stats["like_count"] = 0;
    stats["social_count"] = 0;
    stats["total_count"] = 0;
    stats["online_count"] = 0;

    if (!m_db || !m_db->is_open()) return stats;

    // Get today's start timestamp (midnight, local time)
    auto now = std::chrono::system_clock::now();
    auto now_t = std::chrono::system_clock::to_time_t(now);
    struct tm* local_tm = std::localtime(&now_t);
    local_tm->tm_hour = 0;
    local_tm->tm_min = 0;
    local_tm->tm_sec = 0;
    auto midnight = std::mktime(local_tm);
    int64_t today_start_ms = static_cast<int64_t>(midnight) * 1000;

    // Query counts by type for today
    const char* sql = R"SQL(
        SELECT type, COUNT(*) as cnt
        FROM messages
        WHERE room_id = ? AND timestamp >= ?
        GROUP BY type
    )SQL";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db->handle(), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        LOG_ERROR("MessageStore: today_stats prepare failed: {}", sqlite3_errmsg(m_db->handle()));
        return stats;
    }

    sqlite3_bind_text(stmt, 1, room_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, today_start_ms);

    int64_t total = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* type_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        int64_t cnt = sqlite3_column_int64(stmt, 1);
        total += cnt;

        std::string type(type_str);
        if (type == "chat")         stats["chat_count"] = cnt;
        else if (type == "gift")    stats["gift_count"] = cnt;
        else if (type == "enter")   stats["enter_count"] = cnt;
        else if (type == "like")    stats["like_count"] = cnt;
        else if (type == "social")  stats["social_count"] = cnt;
    }

    sqlite3_finalize(stmt);
    stats["total_count"] = total;

    // Get latest online count from stats messages
    const char* online_sql = R"SQL(
        SELECT gift_count FROM messages
        WHERE room_id = ? AND type = 'online_count'
        ORDER BY timestamp DESC LIMIT 1
    )SQL";

    rc = sqlite3_prepare_v2(m_db->handle(), online_sql, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, room_id.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats["online_count"] = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    return stats;
}

} // namespace douyin
