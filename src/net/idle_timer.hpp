#pragma once

#include <asio/as_tuple.hpp>
#include <asio/awaitable.hpp>
#include <asio/steady_timer.hpp>
#include <asio/use_awaitable.hpp>
#include <chrono>

namespace lb {

// Tracks activity on a session; expired() completes once nothing happened for the whole timeout.
// Not thread-safe: all users must run on the session's (single-threaded) executor.
class IdleTimer {
public:
    using Clock = std::chrono::steady_clock;

    IdleTimer(const asio::any_io_executor& executor, Clock::duration timeout)
        : timer_(executor), timeout_(timeout), last_activity_(Clock::now()) {}

    void touch() { last_activity_ = Clock::now(); }

    asio::awaitable<void> expired() {
        for (;;) {
            const auto deadline = last_activity_ + timeout_;
            if (Clock::now() >= deadline) co_return;
            timer_.expires_at(deadline);
            auto [ec] = co_await timer_.async_wait(asio::as_tuple(asio::use_awaitable));
            if (ec) co_return;  // Cancelled: the session finished first
        }
    }

private:
    asio::steady_timer timer_;
    Clock::duration timeout_;
    Clock::time_point last_activity_;
};

}  // namespace lb
