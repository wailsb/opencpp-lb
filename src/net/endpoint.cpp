#include "net/endpoint.hpp"

#include <asio/io_context.hpp>

namespace lb {

asio::ip::tcp::endpoint resolve_endpoint(const std::string& host, std::uint16_t port) {
    asio::error_code ec;
    const auto address = asio::ip::make_address(host, ec);
    if (!ec) return {address, port};

    // Not an IP literal: resolve once at startup (blocking is fine outside the event loops)
    asio::io_context io;
    asio::ip::tcp::resolver resolver(io);
    const auto results = resolver.resolve(host, std::to_string(port), asio::ip::resolver_base::numeric_service);
    return results.begin()->endpoint();
}

std::string to_string(const asio::ip::tcp::endpoint& endpoint) {
    const auto address = endpoint.address().to_string();
    const auto port = std::to_string(endpoint.port());
    return endpoint.address().is_v6() ? "[" + address + "]:" + port : address + ":" + port;
}

}  // namespace lb
