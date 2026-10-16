#pragma once

#include <asio/ip/tcp.hpp>
#include <cstdint>
#include <string>

namespace lb {

// Resolves host (IP literal or name) to the first matching endpoint; throws std::system_error on failure
asio::ip::tcp::endpoint resolve_endpoint(const std::string& host, std::uint16_t port);

// Formats as "1.2.3.4:80" or "[::1]:80"
std::string to_string(const asio::ip::tcp::endpoint& endpoint);

}  // namespace lb
