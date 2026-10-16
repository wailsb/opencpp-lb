#include "net/session.hpp"

#include <asio/experimental/awaitable_operators.hpp>
#include <asio/steady_timer.hpp>
#include <asio/this_coro.hpp>
#include <spdlog/spdlog.h>
#include <variant>
#include <vector>

#include "net/endpoint.hpp"
#include "net/idle_timer.hpp"
#include "net/pipe.hpp"

namespace lb {

namespace {
using asio::ip::tcp;
using namespace asio::experimental::awaitable_operators;

// Connects `socket` to `endpoint`, giving up after `timeout`. as_tuple keeps a fast failure
// (e.g. connection refused) from losing the race to the timer (see ADR-0002 spike findings).
asio::awaitable<asio::error_code> connect_with_timeout(
    tcp::socket& socket,  // NOLINT(cppcoreguidelines-avoid-reference-coroutine-parameters)
    tcp::endpoint endpoint, std::chrono::milliseconds timeout) {
    asio::steady_timer timer(co_await asio::this_coro::executor, timeout);
    const auto token = asio::as_tuple(asio::use_awaitable);

    auto result = co_await (socket.async_connect(endpoint, token) || timer.async_wait(token));
    if (result.index() == 1) co_return asio::error::timed_out;
    co_return std::get<0>(std::get<0>(result));
}
}  // namespace

asio::awaitable<void> run_session(tcp::socket client, std::shared_ptr<const RuntimeState> state) {
    const auto executor = client.get_executor();
    asio::error_code ec;
    const auto peer = client.remote_endpoint(ec);
    client.set_option(tcp::no_delay(true), ec);

    tcp::socket upstream(executor);
    BackendLease lease;
    std::vector<const Backend*> tried;
    tried.reserve(static_cast<std::size_t>(state->max_connect_attempts));

    for (int attempt = 0; attempt < state->max_connect_attempts; ++attempt) {
        Backend* backend = state->balancer->pick(state->backends, PickContext{tried});
        if (backend == nullptr) break;

        lease = BackendLease(*backend);
        const auto connect_ec = co_await connect_with_timeout(upstream, backend->endpoint, state->timeouts.connect);
        if (!connect_ec) break;

        backend->connect_failures.fetch_add(1, std::memory_order_relaxed);
        spdlog::warn("connect to backend {} ({}) failed: {}", backend->id, to_string(backend->endpoint),
                     connect_ec.message());
        tried.push_back(backend);
        lease.reset();
        upstream = tcp::socket(executor);
    }

    if (lease.get() == nullptr) {
        spdlog::warn("no backend available for client {}, closing", to_string(peer));
        co_return;
    }

    upstream.set_option(tcp::no_delay(true), ec);
    spdlog::debug("client {} -> backend {}", to_string(peer), lease.get()->id);

    IdleTimer idle(executor, state->timeouts.idle);
    co_await ((pipe(client, upstream, idle) && pipe(upstream, client, idle)) || idle.expired());
    spdlog::debug("client {} session closed", to_string(peer));
}

}  // namespace lb
