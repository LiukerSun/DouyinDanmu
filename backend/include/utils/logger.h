#pragma once

#include <string>
#include <memory>
#include <spdlog/spdlog.h>

namespace douyin {

class Logger {
public:
    // Initialize spdlog with console + rotating file sinks
    static void init(const std::string& level = "info",
                     const std::string& log_file = "",
                     int max_size_mb = 50,
                     int max_files = 5);

    // Get the logger instance
    static std::shared_ptr<spdlog::logger> get();

    // Generate a short random trace ID (8 hex chars)
    static std::string generate_trace_id();

    // Set a thread-local trace ID that will be included in log format
    static void set_trace_id(const std::string& trace_id);
    static std::string get_trace_id();

private:
    static std::shared_ptr<spdlog::logger> s_logger;
};

// Convenience macros
#define LOG_TRACE(...) SPDLOG_LOGGER_TRACE(douyin::Logger::get(), __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(douyin::Logger::get(), __VA_ARGS__)
#define LOG_INFO(...)  SPDLOG_LOGGER_INFO(douyin::Logger::get(), __VA_ARGS__)
#define LOG_WARN(...)  SPDLOG_LOGGER_WARN(douyin::Logger::get(), __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_ERROR(douyin::Logger::get(), __VA_ARGS__)

} // namespace douyin
