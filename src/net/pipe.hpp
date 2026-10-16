#pragma once

#include <asio/awaitable.hpp>
#include <asio/ip/tcp.hpp>

#include "net/idle_timer.hpp"

namespace lb {

// Copies bytes from `from` to `to` until EOF or error. On EOF the write side of `to` is shut down
// (propagating half-close); on error both sockets are closed so the opposite direction stops too.
// The referenced objects must outlive the returned awaitable.
asio::awaitable<void> pipe(asio::ip::tcp::socket& from,  // NOLINT(cppcoreguidelines-avoid-reference-coroutine-parameters)
                           asio::ip::tcp::socket& to, IdleTimer& idle);

}  // namespace lb
