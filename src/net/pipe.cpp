#include "net/pipe.hpp"

#include <array>
#include <asio/write.hpp>

namespace lb {

namespace {
constexpr std::size_t kBufferSize = 16 * 1024;

void close_both(asio::ip::tcp::socket& a, asio::ip::tcp::socket& b) {
    asio::error_code ignored;
    a.close(ignored);
    b.close(ignored);
}
}  // namespace

asio::awaitable<void> pipe(asio::ip::tcp::socket& from,  // NOLINT(cppcoreguidelines-avoid-reference-coroutine-parameters)
                           asio::ip::tcp::socket& to, IdleTimer& idle) {
    std::array<char, kBufferSize> buffer;  // Lives in the coroutine frame
    const auto token = asio::as_tuple(asio::use_awaitable);

    for (;;) {
        auto [read_ec, n] = co_await from.async_read_some(asio::buffer(buffer), token);
        if (read_ec == asio::error::eof) {
            asio::error_code ignored;
            to.shutdown(asio::ip::tcp::socket::shutdown_send, ignored);
            co_return;
        }
        if (read_ec) {
            close_both(from, to);
            co_return;
        }
        idle.touch();

        // Next read only starts once this write completes, which gives natural backpressure
        auto [write_ec, written] = co_await asio::async_write(to, asio::buffer(buffer.data(), n), token);
        if (write_ec) {
            close_both(from, to);
            co_return;
        }
        idle.touch();
    }
}

}  // namespace lb
