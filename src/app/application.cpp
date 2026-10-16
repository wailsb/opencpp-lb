#include "app/application.hpp"

#include <asio/co_spawn.hpp>
#include <csignal>
#include <spdlog/spdlog.h>

#include "balance/round_robin.hpp"
#include "net/endpoint.hpp"
#include "net/session.hpp"

namespace lb {

namespace {
std::size_t effective_workers(unsigned configured) {
    if (configured != 0) return configured;
    const unsigned hardware = std::thread::hardware_concurrency();
    return hardware == 0 ? 1 : hardware;
}

std::shared_ptr<const RuntimeState> build_state(const Config& config) {
    auto state = std::make_shared<RuntimeState>();
    for (const auto& backend : config.backends) {
        state->backends.push_back(std::make_shared<Backend>(
            backend.id, resolve_endpoint(backend.host, backend.port), backend.weight));
    }
    state->balancer = std::make_unique<RoundRobinBalancer>();
    state->timeouts = {config.connect_timeout, config.idle_timeout};
    state->max_connect_attempts = config.max_connect_attempts;
    state->generation = 1;
    return state;
}

void log_session_exception(std::exception_ptr error) {
    if (!error) return;
    try {
        std::rethrow_exception(error);
    } catch (const std::exception& e) {
        spdlog::error("session terminated by exception: {}", e.what());
    }
}
}  // namespace

Application::Application(Config config)
    : config_(std::move(config)),
      workers_(effective_workers(config_.workers)),
      control_guard_(asio::make_work_guard(control_)) {
    validate(config_);
    state_.publish(build_state(config_));
}

Application::~Application() {
    stop();
    wait();
}

void Application::start() {
    const auto endpoint = resolve_endpoint(config_.listen_host, config_.listen_port);
    listener_ = std::make_unique<Listener>(control_, endpoint, config_.backlog, workers_,
                                           [this](asio::ip::tcp::socket socket) { spawn_session(std::move(socket)); });
    listener_->start();
    workers_.start();
    control_thread_ = std::thread([this]() { control_.run(); });

    const auto state = state_.load();
    spdlog::info("listening on {} with {} workers and {} backends", to_string(listener_->local_endpoint()),
                 workers_.size(), state->backends.size());
    for (const auto& backend : state->backends) {
        spdlog::info("  backend {} -> {}", backend->id, to_string(backend->endpoint));
    }
}

void Application::stop() {
    if (stopped_.exchange(true)) return;
    // Abrupt stop for now; graceful connection draining arrives in Phase 4 (ADR-0010)
    control_.stop();
    workers_.stop();
}

void Application::wait() {
    if (control_thread_.joinable()) control_thread_.join();
    workers_.join();
}

int Application::run() {
    signals_.emplace(control_, SIGINT, SIGTERM);
    signals_->async_wait([this](const asio::error_code& ec, int signal) {
        if (ec) return;
        spdlog::info("received signal {}, shutting down", signal);
        stop();
    });
    start();
    wait();
    spdlog::info("stopped");
    return 0;
}

void Application::spawn_session(asio::ip::tcp::socket socket) {
    const auto executor = socket.get_executor();
    asio::co_spawn(executor, run_session(std::move(socket), state_.load()), log_session_exception);
}

}  // namespace lb
