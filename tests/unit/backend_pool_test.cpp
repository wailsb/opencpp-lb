#include <gtest/gtest.h>
#include "backend.hpp"

TEST(BackendPool, EmptyPoolHasNoBackend) {
    BackendPool pool;
    EXPECT_TRUE(pool.isEmpty());
    EXPECT_EQ(pool.getBestBackend(), nullptr);
}

TEST(BackendPool, PicksSingleBackend) {
    BackendPool pool;
    pool.addBackend("127.0.0.1", 9001);
    ASSERT_FALSE(pool.isEmpty());
    BackendServer* best = pool.getBestBackend();
    ASSERT_NE(best, nullptr);
    EXPECT_EQ(best->port, 9001);
}

TEST(BackendPool, PicksLeastSaturated) {
    BackendPool pool;
    pool.addBackend("127.0.0.1", 9001);
    pool.addBackend("127.0.0.1", 9002);
    pool.addBackend("127.0.0.1", 9003);

    pool.getBestBackend()->current_load = 0.9f; // 9001 becomes busy
    BackendServer* best = pool.getBestBackend();
    ASSERT_NE(best, nullptr);
    EXPECT_EQ(best->port, 9002);

    best->current_load = 0.95f;
    best = pool.getBestBackend();
    ASSERT_NE(best, nullptr);
    EXPECT_EQ(best->port, 9003);
}

TEST(BackendPool, SkipsUnhealthyBackends) {
    BackendPool pool;
    pool.addBackend("127.0.0.1", 9001);
    pool.getBestBackend()->is_healthy = false;
    EXPECT_FALSE(pool.isEmpty());
    EXPECT_EQ(pool.getBestBackend(), nullptr);
}
