#pragma once

#include <asio/awaitable.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <functional>

#include "net/worker_pool.hpp"

namespace lb {

// Called on the acceptor thread with a socket already bound to a worker's io_context
using SessionHandler = std::function<void(asio::ip::tcp::socket)>;

// Accepts connections and hands each one to the next worker
class Listener {
public:
    // Binds and listens immediately; throws std::system_error if the port is unavailable
    Listener(asio::io_context& context, const asio::ip::tcp::endpoint& endpoint, int backlog,
             WorkerPool& workers, SessionHandler handler);

    void start();
    asio::ip::tcp::endpoint local_endpoint() const { return acceptor_.local_endpoint(); }

private:
    asio::awaitable<void> accept_loop();

    asio::ip::tcp::acceptor acceptor_;
    WorkerPool& workers_;
    SessionHandler handler_;
};

}  // namespace lb
