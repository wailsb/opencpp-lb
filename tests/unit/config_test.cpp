#include <gtest/gtest.h>
#include <stdexcept>

#include "config/config.hpp"

using lb::parse_host_port;

TEST(ParseHostPort, ParsesIpv4) {
    const auto result = parse_host_port("127.0.0.1:9001");
    EXPECT_EQ(result.host, "127.0.0.1");
    EXPECT_EQ(result.port, 9001);
}

TEST(ParseHostPort, ParsesHostname) {
    const auto result = parse_host_port("localhost:80");
    EXPECT_EQ(result.host, "localhost");
    EXPECT_EQ(result.port, 80);
}

TEST(ParseHostPort, ParsesBracketedIpv6) {
    const auto result = parse_host_port("[::1]:8080");
    EXPECT_EQ(result.host, "::1");
    EXPECT_EQ(result.port, 8080);
}

TEST(ParseHostPort, RejectsMalformedInput) {
    EXPECT_THROW(parse_host_port("127.0.0.1"), std::invalid_argument);
    EXPECT_THROW(parse_host_port(":80"), std::invalid_argument);
    EXPECT_THROW(parse_host_port("host:"), std::invalid_argument);
    EXPECT_THROW(parse_host_port("host:abc"), std::invalid_argument);
    EXPECT_THROW(parse_host_port("host:65536"), std::invalid_argument);
    EXPECT_THROW(parse_host_port("::1:80"), std::invalid_argument);
    EXPECT_THROW(parse_host_port("[::1]80"), std::invalid_argument);
}

namespace {
lb::Config valid_config() {
    lb::Config config;
    config.backends.push_back({"a", "127.0.0.1", 9001, 1});
    return config;
}
}  // namespace

TEST(ValidateConfig, AcceptsValidConfig) { EXPECT_NO_THROW(lb::validate(valid_config())); }

TEST(ValidateConfig, RejectsInvalidSettings) {
    auto no_backends = valid_config();
    no_backends.backends.clear();
    EXPECT_THROW(lb::validate(no_backends), std::invalid_argument);

    auto duplicate = valid_config();
    duplicate.backends.push_back({"a", "127.0.0.1", 9002, 1});
    EXPECT_THROW(lb::validate(duplicate), std::invalid_argument);

    auto zero_port = valid_config();
    zero_port.backends[0].port = 0;
    EXPECT_THROW(lb::validate(zero_port), std::invalid_argument);

    auto zero_attempts = valid_config();
    zero_attempts.max_connect_attempts = 0;
    EXPECT_THROW(lb::validate(zero_attempts), std::invalid_argument);
}
