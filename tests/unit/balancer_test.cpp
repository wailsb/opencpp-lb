#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "backend/runtime_state.hpp"
#include "balance/round_robin.hpp"

using lb::Backend;
using lb::PickContext;
using lb::RoundRobinBalancer;

namespace {
std::vector<std::shared_ptr<Backend>> make_backends(int count) {
    std::vector<std::shared_ptr<Backend>> backends;
    for (int i = 0; i < count; ++i) {
        const asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"),
                                               static_cast<unsigned short>(9001 + i));
        backends.push_back(std::make_shared<Backend>("b" + std::to_string(i), endpoint, 1));
    }
    return backends;
}
}  // namespace

TEST(RoundRobin, EmptyListReturnsNull) {
    RoundRobinBalancer balancer;
    EXPECT_EQ(balancer.pick({}, PickContext{}), nullptr);
}

TEST(RoundRobin, CyclesThroughBackendsInOrder) {
    const auto backends = make_backends(3);
    RoundRobinBalancer balancer;
    for (int round = 0; round < 2; ++round) {
        for (const auto& expected : backends) EXPECT_EQ(balancer.pick(backends, PickContext{}), expected.get());
    }
}

TEST(RoundRobin, SkipsExcludedBackends) {
    const auto backends = make_backends(3);
    RoundRobinBalancer balancer;
    const std::vector<const Backend*> exclude{backends[0].get(), backends[1].get()};
    for (int i = 0; i < 5; ++i) EXPECT_EQ(balancer.pick(backends, PickContext{exclude}), backends[2].get());
}

TEST(RoundRobin, SkipsUnhealthyBackends) {
    const auto backends = make_backends(2);
    backends[0]->health = lb::HealthState::Unhealthy;
    RoundRobinBalancer balancer;
    for (int i = 0; i < 4; ++i) EXPECT_EQ(balancer.pick(backends, PickContext{}), backends[1].get());
}

TEST(RoundRobin, ReturnsNullWhenNothingEligible) {
    const auto backends = make_backends(2);
    backends[0]->health = lb::HealthState::Unhealthy;
    const std::vector<const Backend*> exclude{backends[1].get()};
    RoundRobinBalancer balancer;
    EXPECT_EQ(balancer.pick(backends, PickContext{exclude}), nullptr);
}

TEST(BackendLease, TracksActiveConnections) {
    const auto backends = make_backends(1);
    {
        lb::BackendLease first(*backends[0]);
        lb::BackendLease second(*backends[0]);
        EXPECT_EQ(backends[0]->active_conns.load(), 2u);
        lb::BackendLease moved = std::move(first);
        EXPECT_EQ(backends[0]->active_conns.load(), 2u);
        second.reset();
        EXPECT_EQ(backends[0]->active_conns.load(), 1u);
    }
    EXPECT_EQ(backends[0]->active_conns.load(), 0u);
}

TEST(StateHolder, OldSnapshotStaysValidAfterPublish) {
    lb::StateHolder holder;
    auto first = std::make_shared<lb::RuntimeState>();
    first->generation = 1;
    holder.publish(first);

    const auto pinned = holder.load();
    auto second = std::make_shared<lb::RuntimeState>();
    second->generation = 2;
    holder.publish(second);

    EXPECT_EQ(pinned->generation, 1u);
    EXPECT_EQ(holder.load()->generation, 2u);
}
