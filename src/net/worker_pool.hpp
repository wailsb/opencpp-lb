#pragma once

#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

namespace lb {

// N single-threaded io_contexts; each session lives entirely on one of them (ADR-0003)
class WorkerPool {
public:
    explicit WorkerPool(std::size_t count);
    ~WorkerPool();

    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;

    void start();
    void stop();  // Thread-safe; makes every worker's run() return
    void join();

    // Round-robin choice of worker context for the next accepted connection
    asio::io_context& next();
    std::size_t size() const { return contexts_.size(); }

private:
    using WorkGuard = asio::executor_work_guard<asio::io_context::executor_type>;

    std::vector<std::unique_ptr<asio::io_context>> contexts_;
    std::vector<WorkGuard> guards_;
    std::vector<std::thread> threads_;
    std::atomic<std::size_t> next_{0};
};

}  // namespace lb
