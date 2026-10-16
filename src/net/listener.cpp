#include "net/listener.hpp"

#include <asio/as_tuple.hpp>
#include <asio/co_spawn.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/detached.hpp>
#include <asio/steady_timer.hpp>
#include <chrono>
#include <spdlog/spdlog.h>

namespace lb {

Listener::Listener(asio::io_context& context, const asio::ip::tcp::endpoint& endpoint, int backlog,
                   WorkerPool& workers, SessionHandler handler)
    : acceptor_(context), workers_(workers), handler_(std::move(handler)) {
    acceptor_.open(endpoint.protocol());
#ifdef _WIN32
    // SO_REUSEADDR on Windows lets other processes steal the port; use exclusive binding instead
    const int exclusive = 1;
    if (::setsockopt(acceptor_.native_handle(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                     reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) != 0) {
        spdlog::warn("failed to set SO_EXCLUSIVEADDRUSE on listener");
    }
#else
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
#endif
    acceptor_.bind(endpoint);
    acceptor_.listen(backlog);
}

void Listener::start() {
    asio::co_spawn(acceptor_.get_executor(), accept_loop(), asio::detached);
}

asio::awaitable<void> Listener::accept_loop() {
    const auto token = asio::as_tuple(asio::use_awaitable);
    for (;;) {
        auto [ec, socket] = co_await acceptor_.async_accept(workers_.next(), token);
        if (ec == asio::error::operation_aborted || !acceptor_.is_open()) co_return;
        if (ec) {
            // Typically fd exhaustion: back off briefly instead of spinning
            spdlog::warn("accept failed: {}", ec.message());
            asio::steady_timer backoff(acceptor_.get_executor(), std::chrono::milliseconds(10));
            co_await backoff.async_wait(token);
            continue;
        }
        handler_(std::move(socket));
    }
}

}  // namespace lb
