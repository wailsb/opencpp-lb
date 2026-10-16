#pragma once

#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/signal_set.hpp>
#include <atomic>
#include <memory>
#include <optional>
#include <thread>

#include "backend/runtime_state.hpp"
#include "config/config.hpp"
#include "net/listener.hpp"
#include "net/worker_pool.hpp"

namespace lb {

// Owns the whole load balancer: control thread (accept + signals) and the worker pool
class Application {
public:
    // Validates config and resolves backends; throws std::invalid_argument / std::system_error
    explicit Application(Config config);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void start();  // Binds the listener and starts all threads; returns immediately
    void stop();   // Thread-safe and idempotent
    void wait();   // Blocks until all threads have exited

    // start() + stop on SIGINT/SIGTERM + wait(); returns the process exit code
    int run();

    asio::ip::tcp::endpoint listen_endpoint() const { return listener_->local_endpoint(); }
    std::shared_ptr<const RuntimeState> state() const { return state_.load(); }

private:
    void spawn_session(asio::ip::tcp::socket socket);

    Config config_;
    StateHolder state_;
    WorkerPool workers_;

    // Declared before listener_/signals_ so it outlives the I/O objects bound to it
    asio::io_context control_{1};
    asio::executor_work_guard<asio::io_context::executor_type> control_guard_;
    std::unique_ptr<Listener> listener_;
    std::optional<asio::signal_set> signals_;
    std::thread control_thread_;
    std::atomic<bool> stopped_{false};
};

}  // namespace lb
