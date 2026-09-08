#include "utils/timer.h"

namespace douyin {

Timer::Timer() = default;

Timer::~Timer() {
    stop();
}

void Timer::start(std::chrono::milliseconds interval, Callback cb, bool repeat) {
    if (m_running.load()) {
        stop();
    }
    m_running.store(true);
    m_thread = std::thread([this, interval, cb = std::move(cb), repeat]() {
        while (m_running.load()) {
            std::this_thread::sleep_for(interval);
            if (!m_running.load()) break;
            cb();
            if (!repeat) break;
        }
        m_running.store(false);
    });
}

void Timer::stop() {
    m_running.store(false);
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

bool Timer::is_running() const {
    return m_running.load();
}

} // namespace douyin
