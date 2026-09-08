#include "storage/database.h"
#include "utils/logger.h"

#include <sqlite3.h>

namespace douyin {

struct Database::Impl {
    sqlite3* db = nullptr;
    std::string path;
};

Database::Database()
    : m_impl(std::make_unique<Impl>()) {
}

Database::~Database() {
    close();
}

bool Database::open(const std::string& path) {
    close();

    m_impl->path = path;
    int rc = sqlite3_open(path.c_str(), &m_impl->db);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to open database '{}': {}", path, sqlite3_errmsg(m_impl->db));
        m_impl->db = nullptr;
        return false;
    }

    // Enable WAL mode for better concurrent read performance
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA foreign_keys=ON;");

    LOG_INFO("Database opened: {}", path);
    return true;
}

void Database::close() {
    if (m_impl->db) {
        sqlite3_close(m_impl->db);
        m_impl->db = nullptr;
    }
}

bool Database::is_open() const {
    return m_impl->db != nullptr;
}

bool Database::exec(const std::string& sql) {
    if (!m_impl->db) {
        LOG_ERROR("Database not open");
        return false;
    }

    char* err_msg = nullptr;
    int rc = sqlite3_exec(m_impl->db, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        LOG_ERROR("SQL error: {}", err_msg ? err_msg : "unknown");
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

bool Database::migrate() {
    if (!m_impl->db) {
        LOG_ERROR("Database not open, cannot migrate");
        return false;
    }

    // Create messages table
    const char* create_messages = R"SQL(
        CREATE TABLE IF NOT EXISTS messages (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            type        TEXT NOT NULL,
            room_id     TEXT NOT NULL,
            timestamp   INTEGER NOT NULL,
            user_id     TEXT DEFAULT '',
            user_name   TEXT DEFAULT '',
            content     TEXT DEFAULT '',
            gift_count  INTEGER DEFAULT 0,
            extra       TEXT DEFAULT '',
            trace_id    TEXT DEFAULT ''
        );
        CREATE INDEX IF NOT EXISTS idx_messages_room_id ON messages(room_id);
        CREATE INDEX IF NOT EXISTS idx_messages_type ON messages(type);
        CREATE INDEX IF NOT EXISTS idx_messages_timestamp ON messages(timestamp);
        CREATE INDEX IF NOT EXISTS idx_messages_room_type ON messages(room_id, type);
    )SQL";

    if (!exec(create_messages)) {
        LOG_ERROR("Failed to create messages table");
        return false;
    }

    // Create rooms table
    const char* create_rooms = R"SQL(
        CREATE TABLE IF NOT EXISTS rooms (
            room_id     TEXT PRIMARY KEY,
            live_id     TEXT NOT NULL,
            title       TEXT DEFAULT '',
            anchor_name TEXT DEFAULT '',
            status      TEXT DEFAULT 'offline',
            created_at  INTEGER DEFAULT 0,
            updated_at  INTEGER DEFAULT 0
        );
    )SQL";

    if (!exec(create_rooms)) {
        LOG_ERROR("Failed to create rooms table");
        return false;
    }

    LOG_INFO("Database migration completed");
    return true;
}

sqlite3* Database::handle() const {
    return m_impl->db;
}

} // namespace douyin
