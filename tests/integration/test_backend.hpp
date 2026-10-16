#pragma once

#include <asio.hpp>
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace lb::test {

// A real TCP server on an ephemeral loopback port, running on its own thread
class TestBackend {
public:
    enum class Mode {
        Echo,      // Echo bytes back until the client half-closes, then close
        Identify,  // Write the backend id, then close
    };

    explicit TestBackend(Mode mode, std::string id = "backend")
        : mode_(mode), id_(std::move(id)), acceptor_(io_, {asio::ip::make_address("127.0.0.1"), 0}) {
        asio::co_spawn(io_, accept_loop(), asio::detached);
        thread_ = std::thread([this]() { io_.run(); });
    }

    ~TestBackend() {
        io_.stop();
        thread_.join();
    }

    TestBackend(const TestBackend&) = delete;
    TestBackend& operator=(const TestBackend&) = delete;

    std::uint16_t port() const { return acceptor_.local_endpoint().port(); }
    int accepted() const { return accepted_.load(); }

private:
    asio::awaitable<void> accept_loop() {
        const auto token = asio::as_tuple(asio::use_awaitable);
        for (;;) {
            auto [ec, socket] = co_await acceptor_.async_accept(token);
            if (ec) co_return;
            accepted_.fetch_add(1);
            asio::co_spawn(io_, serve(std::move(socket)), asio::detached);
        }
    }

    asio::awaitable<void> serve(asio::ip::tcp::socket socket) {
        const auto token = asio::as_tuple(asio::use_awaitable);
        if (mode_ == Mode::Identify) {
            co_await asio::async_write(socket, asio::buffer(id_), token);
            co_return;
        }
        std::array<char, 16 * 1024> buffer;
        for (;;) {
            auto [read_ec, n] = co_await socket.async_read_some(asio::buffer(buffer), token);
            if (read_ec) co_return;
            auto [write_ec, written] = co_await asio::async_write(socket, asio::buffer(buffer.data(), n), token);
            if (write_ec) co_return;
        }
    }

    Mode mode_;
    std::string id_;
    asio::io_context io_;
    asio::ip::tcp::acceptor acceptor_;
    std::thread thread_;
    std::atomic<int> accepted_{0};
};

// Returns a loopback port that nothing is listening on
inline std::uint16_t unused_port() {
    asio::io_context io;
    asio::ip::tcp::acceptor acceptor(io, {asio::ip::make_address("127.0.0.1"), 0});
    return acceptor.local_endpoint().port();
}

}  // namespace lb::test
