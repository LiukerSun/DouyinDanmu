#include "utils/logger.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <mutex>

namespace douyin {

std::shared_ptr<spdlog::logger> Logger::s_logger = nullptr;

// Thread-local trace ID storage
static thread_local std::string t_trace_id;

void Logger::init(const std::string& level,
                  const std::string& log_file,
                  int max_size_mb,
                  int max_files) {
    std::vector<spdlog::sink_ptr> sinks;

    // Console sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [trace:%8t] %v");
    sinks.push_back(console_sink);

    // File sink (if log_file is specified)
    if (!log_file.empty()) {
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_file,
            static_cast<size_t>(max_size_mb) * 1024 * 1024,
            static_cast<size_t>(max_files));
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [trace:%8t] %v");
        sinks.push_back(file_sink);
    }

    s_logger = std::make_shared<spdlog::logger>("douyin", sinks.begin(), sinks.end());

    // Set log level
    s_logger->set_level(spdlog::level::from_str(level));
    s_logger->flush_on(spdlog::level::warn);
}

std::shared_ptr<spdlog::logger> Logger::get() {
    if (!s_logger) {
        // Default initialization if init() was not called
        init("info");
    }
    return s_logger;
}

std::string Logger::generate_trace_id() {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(8) << dist(rng);
    return oss.str();
}

void Logger::set_trace_id(const std::string& trace_id) {
    t_trace_id = trace_id;
}

std::string Logger::get_trace_id() {
    return t_trace_id;
}

} // namespace douyin
