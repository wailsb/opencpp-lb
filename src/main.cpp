#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

#include "app/application.hpp"
#include "config/config.hpp"
#include "observe/logging.hpp"

int main(int argc, char** argv) {
    std::string listen = "0.0.0.0:8080";
    std::vector<std::string> backends;
    std::string log_level = "info";
    long connect_timeout_ms = 2000;
    long idle_timeout_ms = 60000;
    lb::Config config;

    CLI::App cli{"OpenCPP-LB: a cross-platform TCP load balancer"};
    cli.add_option("-l,--listen", listen, "Address to listen on (host:port)")->capture_default_str();
    cli.add_option("-b,--backend", backends, "Backend address host:port (repeatable)")->required();
    cli.add_option("-w,--workers", config.workers, "Worker threads (0 = one per CPU core)")
        ->capture_default_str();
    cli.add_option("--connect-timeout", connect_timeout_ms, "Backend connect timeout (ms)")
        ->check(CLI::PositiveNumber)
        ->capture_default_str();
    cli.add_option("--idle-timeout", idle_timeout_ms, "Close sessions idle for this long (ms)")
        ->check(CLI::PositiveNumber)
        ->capture_default_str();
    cli.add_option("--max-connect-attempts", config.max_connect_attempts, "Backends to try per connection")
        ->check(CLI::Range(1, 16))
        ->capture_default_str();
    cli.add_option("--log-level", log_level, "trace|debug|info|warn|error|critical|off")
        ->check(CLI::IsMember({"trace", "debug", "info", "warn", "error", "critical", "off"}))
        ->capture_default_str();
    CLI11_PARSE(cli, argc, argv);

    try {
        lb::init_logging(log_level);

        const auto listen_address = lb::parse_host_port(listen);
        config.listen_host = listen_address.host;
        config.listen_port = listen_address.port;
        config.connect_timeout = std::chrono::milliseconds(connect_timeout_ms);
        config.idle_timeout = std::chrono::milliseconds(idle_timeout_ms);
        for (std::size_t i = 0; i < backends.size(); ++i) {
            const auto address = lb::parse_host_port(backends[i]);
            config.backends.push_back({"backend-" + std::to_string(i + 1), address.host, address.port, 1});
        }

        lb::Application app(std::move(config));
        return app.run();
    } catch (const std::exception& e) {
        spdlog::critical("fatal: {}", e.what());
        return 1;
    }
}
