#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>
#include <version>

#include "backend/backend.hpp"
#include "balance/balancer.hpp"

namespace lb {

struct Timeouts {
    std::chrono::milliseconds connect{2000};
    std::chrono::milliseconds idle{60000};
};

// Immutable snapshot of everything a session needs; replaced wholesale on reload (ADR-0005)
struct RuntimeState {
    std::vector<std::shared_ptr<Backend>> backends;
    std::unique_ptr<Balancer> balancer;
    Timeouts timeouts;
    int max_connect_attempts = 2;
    std::uint64_t generation = 0;
};

// Publishes RuntimeState snapshots to all workers; readers never observe a partial update
class StateHolder {
public:
    std::shared_ptr<const RuntimeState> load() const {
#if defined(__cpp_lib_atomic_shared_ptr)
        return current_.load(std::memory_order_acquire);
#else
        std::lock_guard<std::mutex> lock(mtx_);
        return current_;
#endif
    }

    void publish(std::shared_ptr<const RuntimeState> state) {
#if defined(__cpp_lib_atomic_shared_ptr)
        current_.store(std::move(state), std::memory_order_release);
#else
        std::lock_guard<std::mutex> lock(mtx_);
        current_ = std::move(state);
#endif
    }

private:
#if defined(__cpp_lib_atomic_shared_ptr)
    std::atomic<std::shared_ptr<const RuntimeState>> current_;
#else
    // Fallback for standard libraries without atomic<shared_ptr> (e.g. libc++)
    mutable std::mutex mtx_;
    std::shared_ptr<const RuntimeState> current_;
#endif
};

}  // namespace lb
