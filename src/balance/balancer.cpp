#include "balance/balancer.hpp"
#include "balance/round_robin.hpp"

#include <algorithm>

namespace lb {

bool is_eligible(const Backend& backend, const PickContext& ctx) {
    return backend.is_healthy() &&
           std::find(ctx.exclude.begin(), ctx.exclude.end(), &backend) == ctx.exclude.end();
}

Backend* RoundRobinBalancer::pick(std::span<const std::shared_ptr<Backend>> backends,
                                  const PickContext& ctx) const {
    const std::size_t count = backends.size();
    if (count == 0) return nullptr;

    const std::size_t start = next_.fetch_add(1, std::memory_order_relaxed);
    for (std::size_t i = 0; i < count; ++i) {
        Backend& candidate = *backends[(start + i) % count];
        if (is_eligible(candidate, ctx)) return &candidate;
    }
    return nullptr;
}

}  // namespace lb
