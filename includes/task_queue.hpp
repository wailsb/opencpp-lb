#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include "platform.hpp"

// Represents a queued client connection task
struct ClientTask {
    SOCKET clientSocket;
};

class TaskQueue {
private:
    std::queue<ClientTask> queue;
    std::mutex mtx;
    std::condition_variable cv;
    bool closed = false;

public:
    // Pushes a new client connection into the queue
    void push(SOCKET socket) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push({socket});
        }
        cv.notify_one(); // Wake up a waiting worker thread
    }

    // Pulls a client connection from the queue (blocks if empty)
    // Returns false once the queue is closed and drained
    bool pop(ClientTask& task) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return closed || !queue.empty(); });
        if (queue.empty()) return false;
        task = queue.front();
        queue.pop();
        return true;
    }

    // Wakes all workers so they can exit once remaining tasks are drained
    void close() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            closed = true;
        }
        cv.notify_all();
    }
};
