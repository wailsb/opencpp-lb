#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace lb {

struct BackendConfig {
    std::string id;
    std::string host;
    std::uint16_t port = 0;
    std::uint32_t weight = 1;
};

struct Config {
    std::string listen_host = "0.0.0.0";
    std::uint16_t listen_port = 8080;
    unsigned workers = 0;  // 0 = one per hardware thread
    int backlog = 1024;
    int max_connect_attempts = 2;
    std::chrono::milliseconds connect_timeout{2000};
    std::chrono::milliseconds idle_timeout{60000};
    std::vector<BackendConfig> backends;
};

struct HostPort {
    std::string host;
    std::uint16_t port = 0;
};

// Parses "host:port" or "[ipv6]:port"; throws std::invalid_argument on malformed input
HostPort parse_host_port(const std::string& text);

// Throws std::invalid_argument describing the first invalid setting
void validate(const Config& config);

}  // namespace lb
