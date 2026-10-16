#include <gtest/gtest.h>
#include <thread>
#include "task_queue.hpp"

TEST(TaskQueue, PopsInFifoOrder) {
    TaskQueue queue;
    queue.push(static_cast<SOCKET>(1));
    queue.push(static_cast<SOCKET>(2));

    ClientTask task{};
    ASSERT_TRUE(queue.pop(task));
    EXPECT_EQ(task.clientSocket, static_cast<SOCKET>(1));
    ASSERT_TRUE(queue.pop(task));
    EXPECT_EQ(task.clientSocket, static_cast<SOCKET>(2));
}

TEST(TaskQueue, CloseDrainsPendingThenUnblocks) {
    TaskQueue queue;
    queue.push(static_cast<SOCKET>(7));

    int popped = 0;
    std::thread worker([&]() {
        ClientTask task{};
        while (queue.pop(task)) ++popped;
    });
    queue.close();
    worker.join();
    EXPECT_EQ(popped, 1);
}
