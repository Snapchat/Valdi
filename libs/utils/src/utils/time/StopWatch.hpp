#pragma once

#include <array>
#include <chrono>
#include <fmt/format.h>
#include <string>

#include "utils/time/Duration.hpp"

namespace snap::utils::time {

// Helper class to measure time intervals. Use the StopWatch alias for monotonic
// clocks by default.
//
// Basic usage:
//
//   StopWatch sw(StopWatch::STARTED);
//   doSomething();
//   LOG("Operation took {}ms", sw.elapsedMs());
//
// If you need more manual control of your stop watch you can explicitly call
// start() and stop().  A stopwatch will by default start in a stopped state,
// requiring you to call start() to initiate timing.
//
//   StopWatch sw;
//   sw.start();
//   doSomething();
//   sw.stop();
//   LOG("Operation took {}ms", sw.elapsedMs());
//
// This class is NOT thread-safe for changes to the running state. If you
// intend to share this object across threads, such as an async callback,
// to update the StopWatch running state then you must ensure that only
// one thread updates the class at a time, including readers.  Concurrent
// reads to the StopWatch are always thread-safe.
template<class Clock>
class StopWatchGeneric {
public:
    enum Mode {
        STOPPED = 0,
        STARTED = 1,
    };

    // Messaging test code relies on implicit constructor below for shorter code.
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr StopWatchGeneric(const Mode startMode = StopWatchGeneric::STOPPED) {
        if (startMode == STARTED) {
            _startingTime = now();
            _running = true;
        }
    }

    // Starts or resumes measuring time. If it has been started/stopped before,
    // the newly measured elapsed time will be summed with the previous elapsed value.
    // Trying to start if it's already started will do nothing.
    void start() {
        if (_running) {
            return;
        }

        _startingTime = now();
        _running = true;
    }

    // Stops measuring elapsed time. It does not reset already accumulated elapsed time.
    void stop() {
        if (!_running) {
            return;
        }
        _elapsedTotal += fromStart();
        _running = false;
    }

    // Stops time interval measurement and resets the elapsed time to zero.
    void reset() {
        _elapsedTotal = {};
        _running = false;
    }

    // Helper method that does reset() and then start()
    void restart() {
        _elapsedTotal = {};
        _startingTime = now();
        _running = true;
    }

    int64_t elapsedSeconds() const {
        return static_cast<int64_t>(elapsed().seconds());
    }

    int64_t elapsedMs() const {
        return static_cast<int64_t>(elapsed().milliseconds());
    }

    int64_t elapsedUs() const {
        return static_cast<int64_t>(elapsed().microseconds());
    }

    // Total elapsed time while this stopwatch was running.
    Duration<Clock> elapsed() const {
        auto elapsed = _elapsedTotal;

        if (_running)
            elapsed += fromStart();

        return Duration<Clock>(elapsed);
    }

    const typename Clock::time_point& getStartTime() const {
        return _startingTime;
    }

private:
    typename Clock::duration fromStart() const {
        return now() - _startingTime;
    }

    static typename Clock::time_point now() noexcept {
        return Clock::now();
    }

    typename Clock::duration _elapsedTotal{};
    typename Clock::time_point _startingTime{};
    bool _running = false;
};

// StopWatch for monotonic clock. Guarantees that elapsed time is never negative.
using StopWatch = StopWatchGeneric<std::chrono::steady_clock>;

} // namespace snap::utils::time
