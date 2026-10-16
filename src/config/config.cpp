#include "config/config.hpp"

#include <charconv>
#include <set>
#include <stdexcept>

namespace lb {

HostPort parse_host_port(const std::string& text) {
    std::string host;
    std::string port;

    if (!text.empty() && text.front() == '[') {
        const auto close = text.find(']');
        if (close == std::string::npos || close + 1 >= text.size() || text[close + 1] != ':') {
            throw std::invalid_argument("expected [host]:port, got '" + text + "'");
        }
        host = text.substr(1, close - 1);
        port = text.substr(close + 2);
    } else {
        const auto colon = text.rfind(':');
        if (colon == std::string::npos || text.find(':') != colon) {
            throw std::invalid_argument("expected host:port, got '" + text + "'");
        }
        host = text.substr(0, colon);
        port = text.substr(colon + 1);
    }

    if (host.empty()) throw std::invalid_argument("missing host in '" + text + "'");

    unsigned long value = 0;
    const char* end = port.data() + port.size();
    const auto [ptr, ec] = std::from_chars(port.data(), end, value);
    if (port.empty() || ec != std::errc{} || ptr != end || value > 65535) {
        throw std::invalid_argument("invalid port in '" + text + "'");
    }
    return {host, static_cast<std::uint16_t>(value)};
}

void validate(const Config& config) {
    if (config.backends.empty()) throw std::invalid_argument("at least one backend is required");
    if (config.backlog <= 0) throw std::invalid_argument("backlog must be positive");
    if (config.max_connect_attempts < 1) throw std::invalid_argument("max_connect_attempts must be >= 1");
    if (config.connect_timeout.count() <= 0) throw std::invalid_argument("connect timeout must be positive");
    if (config.idle_timeout.count() <= 0) throw std::invalid_argument("idle timeout must be positive");

    std::set<std::string> ids;
    for (const auto& backend : config.backends) {
        if (backend.port == 0) throw std::invalid_argument("backend '" + backend.id + "' has port 0");
        if (backend.weight == 0) throw std::invalid_argument("backend '" + backend.id + "' has weight 0");
        if (!ids.insert(backend.id).second) {
            throw std::invalid_argument("duplicate backend id '" + backend.id + "'");
        }
    }
}

}  // namespace lb
