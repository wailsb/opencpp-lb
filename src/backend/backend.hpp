#pragma once

#include <asio/ip/tcp.hpp>
#include <atomic>
#include <cstdint>
#include <string>
#include <utility>

namespace lb {

enum class HealthState : std::uint8_t { Healthy, Unhealthy };

// A backend server. Identity is immutable; live stats are atomics shared by all workers (ADR-0005)
struct Backend {
    Backend(std::string id_, asio::ip::tcp::endpoint endpoint_, std::uint32_t weight_)
        : id(std::move(id_)), endpoint(std::move(endpoint_)), weight(weight_) {}

    const std::string id;
    const asio::ip::tcp::endpoint endpoint;
    const std::uint32_t weight;

    std::atomic<HealthState> health{HealthState::Healthy};
    std::atomic<std::uint32_t> active_conns{0};
    std::atomic<std::uint64_t> connect_failures{0};

    bool is_healthy() const { return health.load(std::memory_order_relaxed) == HealthState::Healthy; }
};

// RAII guard that counts one connection against a backend for its lifetime
class BackendLease {
public:
    BackendLease() = default;
    explicit BackendLease(Backend& backend) : backend_(&backend) {
        backend_->active_conns.fetch_add(1, std::memory_order_relaxed);
    }
    BackendLease(BackendLease&& other) noexcept : backend_(std::exchange(other.backend_, nullptr)) {}
    BackendLease& operator=(BackendLease&& other) noexcept {
        if (this != &other) {
            reset();
            backend_ = std::exchange(other.backend_, nullptr);
        }
        return *this;
    }
    BackendLease(const BackendLease&) = delete;
    BackendLease& operator=(const BackendLease&) = delete;
    ~BackendLease() { reset(); }

    void reset() {
        if (backend_ != nullptr) {
            backend_->active_conns.fetch_sub(1, std::memory_order_relaxed);
            backend_ = nullptr;
        }
    }
    Backend* get() const { return backend_; }

private:
    Backend* backend_ = nullptr;
};

}  // namespace lb
