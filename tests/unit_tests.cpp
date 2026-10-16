#include <iostream>
#include <thread>
#include "backend.hpp"
#include "task_queue.hpp"

static int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << "  " #cond \
                      << std::endl;                                          \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void testEmptyPool() {
    BackendPool pool;
    CHECK(pool.isEmpty());
    CHECK(pool.getBestBackend() == nullptr);
}

static void testPicksSingleBackend() {
    BackendPool pool;
    pool.addBackend("127.0.0.1", 9001);
    CHECK(!pool.isEmpty());
    BackendServer* best = pool.getBestBackend();
    CHECK(best != nullptr);
    CHECK(best && best->port == 9001);
}

static void testPicksLeastSaturated() {
    BackendPool pool;
    pool.addBackend("127.0.0.1", 9001);
    pool.addBackend("127.0.0.1", 9002);
    pool.addBackend("127.0.0.1", 9003);
    pool.getBestBackend()->current_load = 0.9f; // 9001 becomes busy
    BackendServer* best = pool.getBestBackend();
    CHECK(best && best->port == 9002);
    best->current_load = 0.95f;
    best = pool.getBestBackend();
    CHECK(best && best->port == 9003);
}

static void testSkipsUnhealthy() {
    BackendPool pool;
    pool.addBackend("127.0.0.1", 9001);
    pool.getBestBackend()->is_healthy = false;
    CHECK(!pool.isEmpty());
    CHECK(pool.getBestBackend() == nullptr);
}

static void testQueueFifo() {
    TaskQueue queue;
    queue.push(static_cast<SOCKET>(1));
    queue.push(static_cast<SOCKET>(2));
    ClientTask task{};
    CHECK(queue.pop(task) && task.clientSocket == static_cast<SOCKET>(1));
    CHECK(queue.pop(task) && task.clientSocket == static_cast<SOCKET>(2));
}

static void testQueueCloseUnblocksWorkers() {
    TaskQueue queue;
    queue.push(static_cast<SOCKET>(7));
    int popped = 0;
    std::thread worker([&]() {
        ClientTask task{};
        while (queue.pop(task)) ++popped;
    });
    queue.close();
    worker.join();
    CHECK(popped == 1); // Pending task drained before exit
}

int main() {
    testEmptyPool();
    testPicksSingleBackend();
    testPicksLeastSaturated();
    testSkipsUnhealthy();
    testQueueFifo();
    testQueueCloseUnblocksWorkers();

    if (failures == 0) std::cout << "[PASS] All unit tests passed." << std::endl;
    return failures == 0 ? 0 : 1;
}
