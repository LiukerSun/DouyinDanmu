#pragma once

#include <functional>
#include <thread>
#include <atomic>
#include <chrono>

namespace douyin {

// Simple periodic timer that runs a callback at a fixed interval.
// Thread-safe: the callback runs on an internal background thread.
class Timer {
public:
    using Callback = std::function<void()>;

    Timer();
    ~Timer();

    // Non-copyable, non-movable
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    // Start the timer with the given interval and callback.
    // If repeat is true, the callback fires repeatedly; otherwise once.
    void start(std::chrono::milliseconds interval, Callback cb, bool repeat = true);

    // Stop the timer and join the thread.
    void stop();

    // Check if the timer is running.
    bool is_running() const;

private:
    std::atomic<bool> m_running{false};
    std::thread m_thread;
};

} // namespace douyin
