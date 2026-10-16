#include "net/worker_pool.hpp"

#include <spdlog/spdlog.h>

namespace lb {

WorkerPool::WorkerPool(std::size_t count) {
    if (count == 0) count = 1;
    contexts_.reserve(count);
    guards_.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        // Concurrency hint 1: each context is only ever run by its own thread
        contexts_.push_back(std::make_unique<asio::io_context>(1));
        guards_.push_back(asio::make_work_guard(*contexts_.back()));
    }
}

WorkerPool::~WorkerPool() {
    stop();
    join();
}

void WorkerPool::start() {
    threads_.reserve(contexts_.size());
    for (auto& context : contexts_) {
        threads_.emplace_back([&io = *context]() {
            try {
                io.run();
            } catch (const std::exception& e) {
                spdlog::critical("worker event loop crashed: {}", e.what());
            }
        });
    }
}

void WorkerPool::stop() {
    for (auto& context : contexts_) context->stop();
}

void WorkerPool::join() {
    for (auto& thread : threads_) {
        if (thread.joinable()) thread.join();
    }
}

asio::io_context& WorkerPool::next() {
    return *contexts_[next_.fetch_add(1, std::memory_order_relaxed) % contexts_.size()];
}

}  // namespace lb
