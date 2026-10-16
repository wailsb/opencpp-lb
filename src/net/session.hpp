#pragma once

#include <asio/awaitable.hpp>
#include <asio/ip/tcp.hpp>
#include <memory>

#include "backend/runtime_state.hpp"

namespace lb {

// Proxies one client connection: picks a backend (retrying on connect failure), then pipes
// bytes both ways until both sides close or the idle timeout fires.
asio::awaitable<void> run_session(asio::ip::tcp::socket client, std::shared_ptr<const RuntimeState> state);

}  // namespace lb
