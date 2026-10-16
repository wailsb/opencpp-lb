#pragma once

#include <memory>
#include <span>

#include "backend/backend.hpp"

namespace lb {

struct PickContext {
    std::span<const Backend* const> exclude;  // Backends already tried for this connection
};

// Strategy for choosing a backend; implementations must be safe to call from any worker thread
class Balancer {
public:
    virtual ~Balancer() = default;
    virtual Backend* pick(std::span<const std::shared_ptr<Backend>> backends, const PickContext& ctx) const = 0;
};

// True when the backend is healthy and has not already been tried
bool is_eligible(const Backend& backend, const PickContext& ctx);

}  // namespace lb
