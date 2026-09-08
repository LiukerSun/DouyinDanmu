#pragma once

#include <string>
#include <memory>
#include <functional>

struct sqlite3;

namespace douyin {

// SQLite database wrapper.
// Manages the database connection and provides basic CRUD operations.
class Database {
public:
    Database();
    ~Database();

    // Non-copyable
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Open (or create) the database at the given path.
    bool open(const std::string& path);

    // Close the database.
    void close();

    // Check if the database is open.
    bool is_open() const;

    // Execute a raw SQL statement (no results).
    bool exec(const std::string& sql);

    // Run the schema migration (create tables if they don't exist).
    bool migrate();

    // Get the underlying sqlite3 handle (for advanced use).
    sqlite3* handle() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace douyin
