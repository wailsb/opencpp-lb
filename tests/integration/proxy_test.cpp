#include <gtest/gtest.h>
#include <asio.hpp>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "app/application.hpp"
#include "test_backend.hpp"

using namespace std::chrono_literals;
using asio::ip::tcp;
using lb::test::TestBackend;

namespace {

lb::Config make_config(const std::vector<std::uint16_t>& backend_ports) {
    lb::Config config;
    config.listen_host = "127.0.0.1";
    config.listen_port = 0;  // Ephemeral: tests never collide
    config.workers = 2;
    config.connect_timeout = 500ms;
    config.idle_timeout = 5s;
    for (std::size_t i = 0; i < backend_ports.size(); ++i) {
        config.backends.push_back({"b" + std::to_string(i), "127.0.0.1", backend_ports[i], 1});
    }
    return config;
}

class ProxyTest : public ::testing::Test {
protected:
    void start(lb::Config config) {
        app_ = std::make_unique<lb::Application>(std::move(config));
        app_->start();
    }

    tcp::socket connect() {
        tcp::socket socket(io_);
        socket.connect({asio::ip::make_address("127.0.0.1"), app_->listen_endpoint().port()});
        return socket;
    }

    // Reads until the peer closes; returns everything received
    static std::string read_all(tcp::socket& socket) {
        std::string data;
        std::array<char, 16 * 1024> buffer;
        asio::error_code ec;
        for (;;) {
            const std::size_t n = socket.read_some(asio::buffer(buffer), ec);
            if (ec) break;
            data.append(buffer.data(), n);
        }
        return data;
    }

    asio::io_context io_;
    std::unique_ptr<lb::Application> app_;
};

}  // namespace

TEST_F(ProxyTest, EchoesThroughProxy) {
    TestBackend backend(TestBackend::Mode::Echo);
    start(make_config({backend.port()}));

    auto socket = connect();
    asio::write(socket, asio::buffer(std::string("hello")));
    std::string reply(5, '\0');
    asio::read(socket, asio::buffer(reply));
    EXPECT_EQ(reply, "hello");
}

TEST_F(ProxyTest, HalfCloseStillDeliversResponse) {
    TestBackend backend(TestBackend::Mode::Echo);
    start(make_config({backend.port()}));

    auto socket = connect();
    asio::write(socket, asio::buffer(std::string("ping")));
    socket.shutdown(tcp::socket::shutdown_send);
    EXPECT_EQ(read_all(socket), "ping");
}

TEST_F(ProxyTest, TransfersLargePayloadIntact) {
    TestBackend backend(TestBackend::Mode::Echo);
    start(make_config({backend.port()}));

    std::string payload(8 * 1024 * 1024, '\0');
    for (std::size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<char>((i * 31 + 7) % 251);

    auto socket = connect();
    // Write concurrently with reading, otherwise both directions' buffers fill and deadlock
    std::thread writer([&]() {
        asio::write(socket, asio::buffer(payload));
        socket.shutdown(tcp::socket::shutdown_send);
    });
    const std::string received = read_all(socket);
    writer.join();

    ASSERT_EQ(received.size(), payload.size());
    EXPECT_TRUE(received == payload);
}

TEST_F(ProxyTest, DistributesRoundRobin) {
    TestBackend a(TestBackend::Mode::Identify, "A");
    TestBackend b(TestBackend::Mode::Identify, "B");
    TestBackend c(TestBackend::Mode::Identify, "C");
    start(make_config({a.port(), b.port(), c.port()}));

    std::map<std::string, int> hits;
    for (int i = 0; i < 6; ++i) {
        auto socket = connect();
        ++hits[read_all(socket)];
    }
    EXPECT_EQ(hits["A"], 2);
    EXPECT_EQ(hits["B"], 2);
    EXPECT_EQ(hits["C"], 2);
}

TEST_F(ProxyTest, FailsOverToNextBackend) {
    TestBackend live(TestBackend::Mode::Identify, "live");
    start(make_config({lb::test::unused_port(), live.port()}));

    for (int i = 0; i < 4; ++i) {
        auto socket = connect();
        EXPECT_EQ(read_all(socket), "live") << "connection " << i;
    }
    EXPECT_GE(app_->state()->backends[0]->connect_failures.load(), 1u);
}

TEST_F(ProxyTest, ClosesClientWhenNoBackendReachable) {
    start(make_config({lb::test::unused_port()}));

    const auto begin = std::chrono::steady_clock::now();
    auto socket = connect();
    EXPECT_EQ(read_all(socket), "");
    EXPECT_LT(std::chrono::steady_clock::now() - begin, 3s);
}

TEST_F(ProxyTest, ClosesIdleSessions) {
    TestBackend backend(TestBackend::Mode::Echo);
    auto config = make_config({backend.port()});
    config.idle_timeout = 300ms;
    start(std::move(config));

    const auto begin = std::chrono::steady_clock::now();
    auto socket = connect();
    EXPECT_EQ(read_all(socket), "");  // Blocks until the proxy closes the idle session
    const auto elapsed = std::chrono::steady_clock::now() - begin;
    EXPECT_GE(elapsed, 250ms);
    EXPECT_LT(elapsed, 3s);
}

TEST_F(ProxyTest, ReleasesBackendLeaseWhenSessionEnds) {
    TestBackend backend(TestBackend::Mode::Identify, "A");
    start(make_config({backend.port()}));

    {
        auto socket = connect();
        EXPECT_EQ(read_all(socket), "A");
    }
    const auto& counted = app_->state()->backends[0]->active_conns;
    for (int i = 0; i < 100 && counted.load() != 0; ++i) std::this_thread::sleep_for(10ms);
    EXPECT_EQ(counted.load(), 0u);
}
