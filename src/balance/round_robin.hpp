#pragma once

#include <atomic>
#include <cstddef>

#include "balance/balancer.hpp"

namespace lb {

// Cycles through backends in order, skipping ineligible ones
class RoundRobinBalancer final : public Balancer {
public:
    Backend* pick(std::span<const std::shared_ptr<Backend>> backends, const PickContext& ctx) const override;

private:
    mutable std::atomic<std::size_t> next_{0};
};

}  // namespace lb
