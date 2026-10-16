# OpenCPP-LB: Next-Generation Architecture Plan

**Status:** Proposal · **Branch:** `wail/feat/new-architecture` · **Baseline commit:** `b347620`

This document says where OpenCPP-LB goes next. It covers the architecture decisions, why each was made, the technologies chosen and the alternatives rejected, and a phased implementation plan where every phase has acceptance criteria.

---

## 1. Where we are today

```
            accept() loop (main thread)
                     │  push(SOCKET)
                     ▼
        TaskQueue (mutex + condition_variable)
                     │  pop()
     ┌───────────┬───┴───────┬───────────┐
  worker 1    worker 2    worker 3    worker 4     (blocking I/O)
     │
     └─► BackendPool::getBestBackend()  ──► (TODO: proxy)
```

| Area | Current state | Problem |
|---|---|---|
| I/O model | Blocking sockets, 4 fixed workers | One slow client holds a whole thread. Concurrency is capped at 4 connections. |
| Proxying | Not implemented | The product is not a load balancer yet. |
| Backend state | `std::vector<BackendServer>` with raw pointers handed out | Data race: workers read while others could write. Pointers are invalidated when backends are added. |
| Load metric | Fixed mock `current_load = 0.2` | Selection is effectively "always the first backend". |
| Health | Fixed `is_healthy = true` | A dead backend keeps getting traffic. |
| Config | Hard-coded port, backend and pool size | The program must be recompiled to change a backend. |
| Observability | `std::cout` | No metrics, no log levels, no structured logs. Output from different threads interleaves. |
| Lifecycle | Infinite loop, killed with `taskkill` | No graceful drain. In-flight requests are lost. |
| Testing | 6 unit checks, manual curl | No integration, load or sanitizer coverage, and no CI. |

The baseline was right to separate accepting connections from handling them, and to put backend selection behind an interface (`BackendPool`). We keep both ideas and replace the mechanism underneath them.

---

## 2. Goals and non-goals

**Goals (v1.0)**
1. A production-grade **L4 (TCP) reverse proxy / load balancer** that runs on Windows, Linux and macOS.
2. **10k+ concurrent connections** per instance on commodity hardware, with p99 added latency < 1 ms at moderate load.
3. **Pluggable balancing strategies**: round-robin, least-connections, and power-of-two-choices (P2C) with latency-weighted EWMA.
4. **Active and passive health checking**, with automatic ejection and recovery of backends.
5. **Declarative config file** with **hot reload** that drops no connections.
6. **Observability**: structured logs, Prometheus metrics, and an admin HTTP endpoint.
7. **Graceful shutdown** with a bounded connection-drain timeout.

**Stretch (v1.x):** L7 HTTP/1.1 routing (host/path rules, header injection), TLS termination, sticky sessions.

**Non-goals (for now):** HTTP/2 and HTTP/3, a service-discovery integration (Consul/K8s), a clustered control plane, a WAF, or caching.

---

## 3. Target architecture

```
                         ┌──────────────────── Control plane ────────────────────┐
  lb.toml ──► ConfigLoader ──► validated Config ──► atomic swap of RuntimeState    │
     ▲  (SIGHUP / admin POST /reload)                       │                      │
     │                                                      ▼                      │
     │                    HealthChecker (coroutines on its own io_context)         │
     │                       └─ updates Backend::health / ewma (atomics)           │
     │                                                                             │
     │                    Admin server :9090  ── /metrics /health /backends /reload│
                         └─────────────────────────────────────────────────────────┘

                         ┌───────────────────── Data plane ─────────────────────┐
  clients ──► Listener(s) :8080                                                 │
                 │ async_accept (one acceptor per worker on Linux via            │
                 │               SO_REUSEPORT; shared acceptor elsewhere)        │
                 ▼                                                               │
     ┌──────────────┬──────────────┬──────────────┐                              │
   Worker 0       Worker 1  ...  Worker N-1         (1 io_context + 1 thread each)│
     │                                                                           │
     └─► Session (coroutine per connection)                                      │
           1. snapshot = runtime.load()            (lock-free read)              │
           2. backend  = snapshot->balancer->pick(ctx)                           │
           3. co_await connect(backend)  ── retry on next pick if it fails       │
           4. co_await (pipe(client→up) && pipe(up→client))                      │
           5. record latency / errors → backend stats (atomics)                  │
                         └───────────────────────────────────────────────────────┘
```

### 3.1 Module layout

```
opencpp-lb/
├── CMakeLists.txt                # Top level: options, deps, subdirs
├── cmake/
│   ├── Dependencies.cmake        # FetchContent declarations (pinned)
│   ├── CompilerWarnings.cmake
│   └── Sanitizers.cmake
├── config/
│   └── lb.example.toml
├── src/
│   ├── main.cpp                  # CLI parsing, wiring, signal handling
│   ├── app/        Application.{hpp,cpp}     # Lifecycle: start / reload / drain / stop
│   ├── config/     Config.hpp, ConfigLoader.cpp, Validation.cpp
│   ├── net/        Listener, WorkerPool, Session, Pipe, Timeouts
│   ├── backend/    Backend.hpp, BackendSet.hpp, HealthChecker.cpp
│   ├── balance/    Balancer.hpp, RoundRobin, LeastConn, P2CEwma, Factory
│   ├── admin/      AdminServer.cpp (tiny HTTP/1.1 on Asio + llhttp)
│   └── observe/    Logging.cpp, Metrics.{hpp,cpp}
├── tests/
│   ├── unit/                     # GoogleTest: balancers, config, metrics
│   ├── integration/              # In-process: real sockets, mock backends
│   └── load/                     # oha / wrk scripts + baseline results
└── docs/
    ├── ARCHITECTURE_PLAN.md      # This file
    └── adr/                      # One Architecture Decision Record per decision
```

`includes/`, `TaskQueue` and the `platform.hpp` socket shims are **retired**. Asio provides portable sockets, and the queue is replaced by the event loop (see ADR-1).

---

## 4. Architecture decisions (ADRs)

Each decision below is written as context → decision → why → alternatives rejected → consequences. During Phase 0 each one becomes its own file in `docs/adr/`.

### ADR-1: Asynchronous, event-driven I/O instead of a blocking thread pool

**Context.** The current design dedicates one thread to each connection while that connection is handled. A load balancer's workload is almost entirely *waiting* on the network: idle keep-alive clients, slow uploads, backend latency. With blocking I/O the number of concurrent connections equals the number of threads. Threads cost about 1 MB of stack each, plus context-switch overhead.

**Decision.** Use a **proactor/reactor event loop**. Each OS thread runs an event loop that multiplexes thousands of sockets through the OS's native readiness or completion mechanism: `epoll` on Linux, `kqueue` on macOS/BSD, **IOCP** on Windows.

**Why.**
- Concurrency is bounded by memory and file descriptors, not threads. A connection costs a few KB (a coroutine frame plus buffers) instead of a thread.
- Every high-performance proxy (HAProxy, Envoy, NGINX) uses this model.
- With the right library it stays portable, which matters because development happens on Windows and deployment is likely on Linux.

**Alternatives rejected.**
- *A bigger blocking thread pool*: still caps concurrency, and a few slow clients can starve the pool (head-of-line blocking).
- *Raw epoll/IOCP by hand*: two to three completely different code paths, and a lot of subtle edge cases (EAGAIN, partial writes, IOCP buffer lifetime).

**Consequences.** Code must never block inside the event loop. Disk I/O (config reload) and DNS resolution go through async APIs or a separate thread.

---

### ADR-2: Standalone Asio with C++20 coroutines

**Decision.** Use **standalone Asio** (header-only, not the Boost version), pinned to **1.30.2**, and write session logic as **C++20 coroutines** (`asio::awaitable<T>`, `co_await`).

**Why Asio.**
- One API on top of IOCP, epoll and kqueue, and it is mature. It is the basis of the C++ Networking TS.
- Its proactor model maps natively to IOCP, so Windows is a first-class platform and not a compatibility layer.
- It includes everything a proxy needs: timers, signal handling (`asio::signal_set`), strands, cancellation slots, SSL (`asio::ssl`, used for TLS in Phase 5) and `awaitable_operators` (`&&`, `||`) for composing concurrent operations.
- The standalone build avoids pulling in Boost (~150 MB).

**Why coroutines instead of callbacks.** The proxy logic is a sequence: pick, connect, retry, pipe, record. With callbacks it becomes nested lambdas with manual lifetime management (`shared_from_this` everywhere). With coroutines it reads top to bottom, uses plain `try/catch` for errors, and keeps lifetimes inside the coroutine frame. Throughput is the same.

**Why C++20.** Coroutines, `std::atomic<std::shared_ptr<T>>` (ADR-5), `std::span`, concepts for the balancer interface, and designated initializers in tests. Our toolchains support it: GCC 15, MSVC 19.3x and Clang 17+.

**Alternatives rejected.**

| Option | Why not |
|---|---|
| **libuv** | A C API with callback-only style and manual memory management. Integrates poorly with RAII and coroutines. |
| **io_uring (liburing)** | Linux only. Worth adding as an optional backend later, but cannot be the base. |
| **Boost.Asio** | The same engine with a heavy dependency. Standalone has the same features. |
| **Seastar** | Linux only, and it takes over the whole process model. Overkill. |
| **libevent / libev** | Callback C APIs, and IOCP support is weak or second-class. |

**Consequences.** Raises `CMAKE_CXX_STANDARD` to 20. The team needs to learn coroutine lifetime rules: never capture a reference to a local that outlives the `co_await` point.

---

### ADR-3: One `io_context` per thread (shared-nothing data plane)

**Context.** Asio supports either one `io_context` run by N threads, or N `io_context`s with one thread each.

**Decision.** Run **N workers, each with its own `io_context` and one thread**, where N = `std::thread::hardware_concurrency()` (configurable). A session lives entirely on the worker that accepted it.

**Why.**
- No locks or strands are needed for per-connection state, because one connection never runs on two threads at once.
- Better CPU cache locality: a connection's buffers stay on one core.
- **Linux:** each worker opens its own listening socket with `SO_REUSEPORT`, and the kernel spreads incoming connections across them. No central acceptor and no thundering herd.
- **Windows/macOS:** there is no load-spreading `SO_REUSEPORT`, so one acceptor hands accepted sockets round-robin to worker contexts. The cost is a single `asio::post` per connection, which is negligible.

**Alternative rejected.** *One shared `io_context` with N threads*: simpler, but handlers for one connection can run on different threads, which forces strands and atomics on every piece of session state.

**Consequences.** Shared state (backends, metrics, config) must be **lock-free to read** from any worker (ADR-5, ADR-9).

---

### ADR-4: L4 TCP proxy first, L7 HTTP second

**Decision.** v1.0 is a **protocol-agnostic TCP proxy**. HTTP-aware routing (L7) is a later phase, built on the same session pipeline.

**Why.**
- L4 works for *any* TCP service (HTTP, gRPC, Postgres, Redis) and has the shortest path to a working product.
- L7 needs a correct HTTP parser, keep-alive handling, request framing and per-request (not per-connection) balancing. That is a separate body of work and depends on the L4 foundation being solid.

**L7 plan (Phase 6).** Parse with **llhttp** (v9.x), the parser Node.js uses. It is a C library generated from a formal state machine, fuzzed, very fast, and has no dependencies. We rejected writing our own parser (HTTP framing has many security-critical edge cases, such as request smuggling via `Content-Length`/`Transfer-Encoding` conflicts) and Boost.Beast (it needs Boost).

---

### ADR-5: Immutable snapshots for shared runtime state (RCU-style)

**Context.** Every new connection reads the backend list and balancer config. Hot reload and health checks change them occasionally. Reads happen orders of magnitude more often than writes.

**Decision.**
- *Topology* (which backends exist, their weights, the strategy) lives in an immutable `RuntimeState` object, published through `std::atomic<std::shared_ptr<const RuntimeState>>`. A reload builds a new state and swaps it in atomically.
- *Volatile per-backend stats* (active connections, health, EWMA latency) are `std::atomic` fields on `Backend` objects, which are themselves held by `shared_ptr`. A reload carries over the existing `Backend` object for any address that is unchanged, so its stats and health history survive.

```cpp
struct Backend {
    const std::string     id;          // "api-1"
    const asio::ip::tcp::endpoint endpoint;
    const uint32_t        weight;

    std::atomic<HealthState> health{HealthState::Unknown};
    std::atomic<uint32_t>    active_conns{0};
    std::atomic<uint64_t>    ewma_latency_us{0};   // fixed-point EWMA
    std::atomic<uint32_t>    consecutive_failures{0};
};

struct RuntimeState {
    std::vector<std::shared_ptr<Backend>> backends;
    std::unique_ptr<Balancer>             balancer;
    Timeouts                              timeouts;
    uint64_t                              generation;
};

class StateHolder {
    std::atomic<std::shared_ptr<const RuntimeState>> current_;
public:
    std::shared_ptr<const RuntimeState> load() const { return current_.load(std::memory_order_acquire); }
    void publish(std::shared_ptr<const RuntimeState> s) { current_.store(std::move(s), std::memory_order_release); }
};
```

**Why.**
- Readers never block and never see a half-updated list. A session that started under generation 7 keeps its snapshot alive, through `shared_ptr` ownership, until it finishes. This is how reload avoids dropping connections.
- It removes the current raw-pointer-into-vector bug by design.

**Alternatives rejected.** A `std::shared_mutex` around the vector works, but puts lock traffic on every new connection, and the writer can starve. Hand-written hazard pointers or real RCU are more complex than we need.

**Caveat.** On some standard libraries `atomic<shared_ptr>` is implemented with an internal spinlock. It is still correct, and contention is only one atomic per *connection*, not per byte. If profiling shows it as hot, swap in a per-worker cached copy that is refreshed on a generation counter.

---

### ADR-6: Pluggable balancing, with P2C + EWMA as the default

**Interface.**

```cpp
struct PickContext {
    const asio::ip::tcp::endpoint& client;
    std::span<const Backend* const> exclude;   // already tried on this request
};

class Balancer {
public:
    virtual ~Balancer() = default;
    virtual Backend* pick(std::span<const std::shared_ptr<Backend>> backends,
                          const PickContext& ctx) = 0;
};
```

**Strategies.**

| Strategy | Algorithm | When to use |
|---|---|---|
| `round_robin` | Atomic counter mod the healthy count, weight-expanded with smooth weighted RR (the NGINX algorithm) | Homogeneous backends, predictable request cost |
| `least_conn` | Lowest `active_conns / weight`; ties broken by RR | Long-lived connections (WebSocket, DB) |
| `p2c_ewma` (default) | Pick 2 random healthy backends and choose the lower `(active_conns + 1) * ewma_latency` | Mixed or unknown workloads |
| `ip_hash` (Phase 6) | Consistent hashing (Maglev or jump hash) on the client IP | Sticky sessions without cookies |

**Why P2C is the default.** "Always choose the global best" (what `getBestBackend()` does today) causes **herding**: every worker sees the same "best" backend in the same instant and overloads it, especially with stale stats. Choosing randomly between two candidates gives an exponential improvement in maximum load over pure random (Mitzenmacher, *The Power of Two Choices*). It needs O(1) work and no global coordination. Linkerd, Envoy and Finagle use it for this reason. Weighting by EWMA latency moves traffic away from slow backends before they fail outright.

**EWMA detail.** `ewma = α·sample + (1-α)·ewma`, with α derived from a decay half-life (default 10 s) and the time since the last sample, so it adapts at any traffic rate. It is stored as fixed-point `uint64_t` microseconds so a single atomic CAS updates it without a lock.

---

### ADR-7: Active and passive health checking, with outlier ejection

**Active checks** run as coroutines on a dedicated control-plane `io_context` thread, so they never compete with the data plane.
- `type = "tcp"`: connect and close. `type = "http"`: `GET /health`, expecting a 2xx response.
- `interval` (5 s), `timeout` (1 s), `rise` (2 successes to become healthy), `fall` (3 failures to become unhealthy). Hysteresis stops a backend from flapping between states.
- Each check's start is randomized by ±10% so checks to many backends don't all fire in sync.

**Passive checks (outlier detection)** come from real traffic. A connect failure or reset increments `consecutive_failures`. At 5 failures the backend is **ejected** for `base_ejection * 2^n` (capped at 5 min), and then re-admitted on a trial basis.
- **Max ejection percent** (default 50%) is a safety valve. If most backends look bad, the problem is probably the network or the LB itself, so we keep sending traffic instead of ejecting the whole fleet.

**Connect retry.** If connecting to the chosen backend fails, the session excludes it and picks again, up to `max_connect_attempts` (2). This is safe at L4 because no client bytes have been sent upstream yet.

**Why both kinds.** Active checks find backends that are dead but idle. Passive checks react within milliseconds to failures that happen between check intervals. Envoy and HAProxy combine them the same way.

---

### ADR-8: TOML configuration with validated hot reload

**Technology.** Use **toml++** (v3.4.0): header-only, C++17/20, with good error messages that include line and column.

**Why TOML.** It is unambiguous and has no indentation pitfalls (YAML's "Norway problem", tabs vs spaces). It is comfortable to write by hand and supports comments, which JSON doesn't. We rejected yaml-cpp (a compiled dependency, and YAML's implicit typing causes bugs) and JSON (no comments, so a poor fit for ops-written config).

```toml
# config/lb.example.toml
[server]
listen         = "0.0.0.0:8080"
workers        = 0            # 0 = hardware_concurrency
drain_timeout  = "30s"

[admin]
listen = "127.0.0.1:9090"

[timeouts]
connect = "2s"
idle    = "60s"

[balancer]
strategy = "p2c_ewma"
max_connect_attempts = 2

[health]
type = "http"
path = "/health"
interval = "5s"
timeout  = "1s"
rise = 2
fall = 3

[[backend]]
id = "api-1"
address = "127.0.0.1:9001"
weight = 1

[[backend]]
id = "api-2"
address = "127.0.0.1:9002"
weight = 2
```

**Reload pipeline.** A trigger arrives (`SIGHUP` on POSIX, `POST /reload` on admin, which is the only option on Windows). Then: parse → validate (unique IDs, resolvable addresses, durations > 0, a known strategy) → build the new `RuntimeState` (reusing existing `Backend` objects that match by address) → `publish()`.
- If any step fails, the **old config stays live** and the error is logged and returned through the admin API.
- Changing `listen` needs a restart in v1. We report this clearly instead of silently ignoring it.

---

### ADR-9: Observability with spdlog and a native Prometheus endpoint

**Logging: spdlog (v1.14.x).**
- Async mode: a background thread does the formatting and I/O, so workers never block on stdout or disk.
- Built on `{fmt}`, so formatting is fast and type-safe.
- Levels are configurable at runtime, with an optional JSON pattern for log shippers.
- *Rejected:* Boost.Log (heavy), glog (abseil dependency, less flexible sinks), `iostream` (what we have now: mutex-free interleaving, no levels).

**Metrics: a hand-rolled registry exposed in the Prometheus text format** on the admin port.
- The hot path holds only per-worker (thread-local) `uint64_t` counters with no atomics or sharing. A scrape sums them across workers.
- Histograms (connect latency, session duration) use fixed log-scale buckets.
- *Rejected:* prometheus-cpp. It brings in civetweb plus its own HTTP server, and we already have an Asio admin server. The text format is about 50 lines to implement.

**Key metrics.**
```
lb_connections_accepted_total
lb_connections_active
lb_backend_connections_active{backend="api-1"}
lb_backend_connect_failures_total{backend="api-1"}
lb_backend_health{backend="api-1"}              # 0/1
lb_backend_connect_latency_seconds_bucket{...}
lb_bytes_total{direction="upstream|downstream"}
lb_config_reloads_total{result="ok|error"}
```

**Admin API** (bound to localhost by default):

| Endpoint | Purpose |
|---|---|
| `GET /health` | Liveness of the LB itself |
| `GET /ready` | 200 only once listeners are bound and at least one backend is healthy (for K8s readiness) |
| `GET /metrics` | Prometheus scrape |
| `GET /backends` | JSON: health, active connections, EWMA and ejection state |
| `POST /reload` | Trigger a config reload |
| `POST /drain` | Begin graceful shutdown |

---

### ADR-10: Graceful lifecycle and timeouts

**Shutdown sequence** (`SIGINT`/`SIGTERM` via `asio::signal_set`, or a Windows console Ctrl handler, which Asio also maps):
1. Close the listening sockets, so new connections go to other instances or are refused.
2. `/ready` returns 503 so orchestrators stop routing to us.
3. Let active sessions finish, up to `drain_timeout`.
4. Cancel what remains through Asio cancellation slots, stop the `io_context`s, and join the threads.

**Timeouts.** Every `co_await` on the network is bounded:
- `connect`: the upstream connect deadline.
- `idle`: reset on every byte in either direction. This is what kills slowloris-style clients.
- Implementation: `co_await (op || timer.async_wait())` using `awaitable_operators`; the loser is cancelled automatically.

---

### ADR-11: Build, dependency management and toolchain

**Dependencies via CMake `FetchContent`, each pinned to an exact tag** (with `FIND_PACKAGE_ARGS` so a system or vcpkg copy is used when present).

| Library | Version | Purpose | Header-only |
|---|---|---|---|
| Asio (standalone) | 1.30.2 | Networking, timers, signals, coroutines | ✅ |
| toml++ | 3.4.0 | Config parsing | ✅ |
| spdlog (+ bundled fmt) | 1.14.1 | Logging | ✅ (compiled mode optional) |
| CLI11 | 2.4.2 | CLI flags (`--config`, `--check`, `--log-level`) | ✅ |
| llhttp | 9.2.1 | HTTP parsing (admin now, L7 later) | ❌ (small C lib) |
| GoogleTest | 1.15.2 | Unit and integration tests (test builds only) | ❌ |

**Why FetchContent instead of vcpkg/Conan.** It needs no extra tool to install, so a fresh clone builds with only CMake and a compiler on all three OSes. It also works the same on the MinGW setup used today. Every dependency is header-only or tiny, so build time stays small. A `vcpkg.json` manifest can be added later for packagers without changing the CMake files, because `FIND_PACKAGE_ARGS` already supports it.

**Why GoogleTest instead of the current hand-rolled `CHECK`.** It gives test discovery (`gtest_discover_tests` → CTest), fixtures for integration tests, gMock to fake a `Balancer` in session tests, and output that CI understands. Catch2 v3 would be equally good; we chose GoogleTest because it is more widely recognized.

**Toolchain and quality gates.**
- Compilers: GCC ≥ 12, Clang ≥ 16, MSVC 19.34+ (VS 2022), and MinGW-w64 GCC 15 (current dev setup).
- `-Wall -Wextra -Wpedantic -Wconversion -Wshadow` (with `/W4 /permissive-` on MSVC). CI uses `-Werror`.
- **Sanitizers** (`-DLB_SANITIZE=address,undefined` and `thread`) in Linux CI. TSan is essential for ADR-3 and ADR-5 correctness.
- **clang-format** (committed `.clang-format`) and **clang-tidy** (bugprone-\*, performance-\*, modernize-\*, concurrency-\*) checked in CI.
- `.gitattributes` with `* text=auto eol=lf` and `*.bat eol=crlf`, which fixes the CRLF warnings seen today.

**CI (GitHub Actions) matrix.**

| OS | Compiler | Jobs |
|---|---|---|
| ubuntu-24.04 | GCC 14, Clang 18 | build, unit, integration, ASan+UBSan, TSan, clang-tidy |
| windows-2022 | MSVC, MinGW GCC | build, unit, integration |
| macos-14 | Apple Clang | build, unit, integration |

The pipeline also runs a nightly **load-test smoke** (oha, 30 s), which fails if p99 regresses more than 20% against the stored baseline.

---

### ADR-12: Testing strategy

| Layer | Tool | Examples |
|---|---|---|
| Unit | GoogleTest | Balancer distribution (chi-squared on 100k picks), smooth-WRR sequence, EWMA decay, config validation errors, metrics text format |
| Integration (in-process) | GoogleTest + Asio | Start the LB on an ephemeral port plus 3 mock echo/HTTP backends in the test process. Covers routing, failover when a backend is killed mid-test, health eject/recover, hot reload without dropping open connections, half-close, idle timeout, and drain on shutdown |
| Fuzz (Phase 6) | libFuzzer | Config parser and HTTP parser glue |
| Load | **oha** (Rust; a single binary that also runs on Windows) and **wrk** (Linux) | Throughput, latency percentiles, 10k concurrent connections |
| Soak | 24 h run under load | FD and memory leaks (track RSS and `lb_connections_active`) |

**Why in-process integration tests instead of a Python harness.** They need no extra runtime, they are deterministic (ephemeral ports, no `sleep`), they debug in the same IDE, and they run under sanitizers.

---

## 5. Data-plane implementation details

### 5.1 Session coroutine

```cpp
asio::awaitable<void> Session::run(tcp::socket client) {
    auto state   = runtime_.load();                     // pin snapshot for this session
    auto ex      = co_await asio::this_coro::executor;
    tcp::socket upstream(ex);
    BackendLease lease;                                 // RAII: ++active_conns / --active_conns
    small_vector<const Backend*, 4> tried;

    for (int attempt = 0; attempt < state->max_connect_attempts; ++attempt) {
        Backend* b = state->balancer->pick(state->backends, {client.remote_endpoint(), tried});
        if (!b) break;
        lease = BackendLease(*b);
        auto t0 = Clock::now();
        auto ec = co_await connect_with_timeout(upstream, b->endpoint, state->timeouts.connect);
        if (!ec) { b->record_success(Clock::now() - t0); break; }
        b->record_failure();                             // feeds passive outlier detection
        tried.push_back(b);
        lease.reset();
        upstream = tcp::socket(ex);
    }

    if (!upstream.is_open()) { metrics_.no_backend.inc(); co_return; } // L4: just close

    IdleTimer idle(ex, state->timeouts.idle);
    using namespace asio::experimental::awaitable_operators;
    co_await ((pipe(client, upstream, idle) && pipe(upstream, client, idle)) || idle.expired());
}
```

### 5.2 Pipe with half-close and zero extra copies

```cpp
asio::awaitable<void> pipe(tcp::socket& from, tcp::socket& to, IdleTimer& idle) {
    std::array<std::byte, 16 * 1024> buf;               // lives in the coroutine frame
    for (;;) {
        auto [ec, n] = co_await from.async_read_some(asio::buffer(buf), as_tuple(use_awaitable));
        if (ec) { to.shutdown(tcp::socket::shutdown_send, ignore_ec); co_return; } // propagate FIN
        idle.touch();
        co_await asio::async_write(to, asio::buffer(buf, n), use_awaitable);
    }
}
```

- **`&&` instead of `||` for the two pipes**: TCP is full-duplex. A client can finish sending (FIN) and still be waiting for the response. `||` would cut it off.
- **16 KB buffer per direction**: matches typical socket buffer chunks. That is about 32 KB per connection, so 10k connections ≈ 320 MB worst case. A later optimization is to allocate buffers from a pool only while a read is pending.
- **Backpressure is built in**: we only read again after the write finishes, so a slow reader slows its writer and memory stays bounded.
- **Future (Linux)**: `splice(2)` through a pipe for zero-copy, behind a compile flag.

### 5.3 Socket options

| Option | Where | Why |
|---|---|---|
| `TCP_NODELAY` | client and upstream sockets | Avoid Nagle delay on small request/response messages |
| `SO_REUSEPORT` | Linux listeners | Kernel spreads accepts across workers (ADR-3) |
| `SO_EXCLUSIVEADDRUSE` | Windows listener | Stops another process from taking over the port, which `SO_REUSEADDR` allows on Windows |
| `SO_KEEPALIVE` | upstream sockets | Detect dead backends on long-idle connections |
| listen backlog | configurable, default 1024 | The current `10` drops connections under a burst |

---

## 6. Phased implementation plan

Each phase is one or more PRs into `main`. Every phase ends with green CI and no new sanitizer findings.

### Phase 0: Foundations (≈ 3 days)
- [x] Restructure the folders (§3.1). Add the `cmake/` modules: warnings, sanitizers, dependencies.
- [x] Switch to C++20. Add the FetchContent dependencies. Add `.clang-format`, `.clang-tidy`, `.gitattributes`.
- [x] Port the existing unit tests to GoogleTest.
- [~] Set up GitHub Actions CI (build + unit tests) on all three OSes. *(Workflow written; not yet run, so it needs a push to verify.)*
- [x] Write the ADRs in `docs/adr/0001-…md` through `0012-…md`.

**Acceptance:** `cmake -B build && cmake --build build && ctest --test-dir build` passes on Windows (MSVC and MinGW), Linux and macOS in CI.

### Phase 1: Async L4 proxy core (≈ 1 week)
- [x] `WorkerPool` (N `io_context`s plus threads) and `Listener` (with the per-platform accept strategy).
- [x] `Session` coroutine, `pipe` with half-close, connect and idle timeouts.
- [x] Static backend list and round-robin `Balancer`.
- [x] spdlog integration and the CLI (`--config`, `--log-level`).
- [x] Integration tests: echo-through-proxy, large payload (100 MB) integrity, half-close, idle timeout.

**Status (2026-10-16):** done on Windows/MinGW. curl → LB → Python `http.server` returns 200. 1,000 concurrent connections held open and echoed correctly. The large-payload test uses 8 MiB. Per-listener `SO_REUSEPORT` and the `--config` flag move to Phases 5 and 4. TSan is pending the first CI run.

**Acceptance:**
- `curl` through the LB to a real HTTP server returns the right response.
- 1k concurrent connections are held open stably.
- TSan is clean.

### Phase 2: Balancing and backend state (≈ 4 days)
- [ ] `Backend` with atomics, the `RuntimeState` snapshot and `StateHolder` (ADR-5).
- [ ] `least_conn`, smooth weighted RR, and `p2c_ewma` balancers, plus a factory.
- [ ] Connect retry on the next backend.
- [ ] Unit tests: distribution fairness and weight adherence. Integration test: a slow backend receives less traffic under P2C.

**Acceptance:** with backends at 1 ms and 50 ms latency, P2C sends at least 80% of requests to the 1 ms backend.

### Phase 3: Health checking (≈ 4 days)
- [ ] A control-plane `io_context` and the `HealthChecker` (TCP and HTTP checks, rise/fall, jitter).
- [ ] Passive outlier ejection with exponential backoff and a max-ejection percent.
- [ ] Integration test: kill a backend → ejected within `fall × interval`; restart it → back within `rise × interval`, and no client errors once the ejection has completed.

### Phase 4: Config, hot reload, admin and metrics (≈ 1 week)
- [ ] toml++ loader, validation with useful error messages, and `--check` (validate and exit).
- [ ] Reload via SIGHUP and `POST /reload`, with `Backend` reuse on reload.
- [ ] Admin server (Asio + llhttp), `/metrics`, `/backends`, `/ready`.
- [ ] Per-worker metrics registry and the Prometheus text encoder.
- [ ] Graceful drain (ADR-10).
- [ ] Integration test: reload adds a backend and removes another while 100 long-lived connections stay open, with zero resets.

### Phase 5: Hardening and performance (≈ 1 week)
- [ ] Load-test baseline with oha. Profile with `perf` and flamegraphs on Linux, and VS Profiler on Windows.
- [ ] Buffer pooling, a per-worker cached snapshot if `atomic<shared_ptr>` shows up as hot, and `TCP_NODELAY` / backlog tuning.
- [ ] Connection limits: global `max_connections` and per-backend `max_conns`, with queuing or 503 when exceeded.
- [ ] Optional TLS termination with `asio::ssl` + OpenSSL 3.x (behind `LB_ENABLE_TLS`).
- [ ] A 24 h soak test.

**Acceptance:**
- ≥ 10k concurrent connections.
- ≥ 50k req/s of small HTTP responses on 4 cores (Linux).
- p99 added latency < 1 ms at 50% of max throughput.
- Flat RSS over the 24 h soak.

### Phase 6: L7 HTTP (v1.x, ≈ 2 weeks)
- [ ] llhttp-based request framing and keep-alive.
- [ ] Per-request balancing on the same client connection.
- [ ] Routing rules (host, path prefix → backend pool) and `X-Forwarded-For` / `X-Request-Id` injection.
- [ ] Defenses against request smuggling: reject a request that has both `Content-Length` and `Transfer-Encoding`, and reject duplicate `Content-Length` headers.
- [ ] `ip_hash` (Maglev) and cookie-based sticky sessions.
- [ ] Fuzzing of the HTTP glue.

---

## 7. Risks and mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Coroutine lifetime bugs (dangling references across `co_await`) | Crashes and use-after-free | ASan in CI. A code-review rule: take parameters by value into coroutines. clang-tidy `cppcoreguidelines-avoid-reference-coroutine-parameters`. |
| Platform behaviour differences (IOCP vs epoll, `SO_REUSEPORT`) | Bugs that only appear on one OS | Integration tests run on all three OSes in CI. The accept strategy sits behind one interface. |
| MinGW / Asio quirks on Windows | Build breaks on the dev machine | MinGW job in CI. Define `_WIN32_WINNT=0x0A00` and `ASIO_STANDALONE` centrally. |
| `atomic<shared_ptr>` implemented with locks on some standard libraries | Contention at high connection rates | Measure in Phase 5. Fall back to a per-worker cached snapshot plus a generation counter. |
| Scope creep into L7 before L4 is solid | Delays v1.0 | L7 stays strictly in Phase 6, and the Phase 5 acceptance gate must pass first. |
| Health-check storms against large fleets | Backends DoS'd by the LB | Jittered start, per-check timeout, and checks paced over time instead of all at once. |

---

## 8. Migration from the current code

| Current | Fate | Replacement |
|---|---|---|
| `includes/platform.hpp` | Removed | Asio portable sockets |
| `includes/task_queue.hpp` | Removed | Per-worker `io_context` (the event loop *is* the queue) |
| `BackendServer` / `BackendPool` | Evolved | `Backend` (atomics) + `RuntimeState` + `Balancer` |
| `getBestBackend()` (global min) | Replaced | `P2CEwmaBalancer::pick()` (see ADR-6 on herding) |
| `workerThreadFunction` | Replaced | `Session::run()` coroutine |
| Hard-coded `PORT`, backend | Replaced | `lb.toml` + CLI11 |
| `tests/unit_tests.cpp` | Ported | GoogleTest in `tests/unit/` |
| `scripts/tests.bat`, `build.sh` | Simplified | `ctest`; scripts become thin wrappers around CMake presets (`CMakePresets.json`) |

---

## 9. Estimated timeline

Estimates assume **one developer** who knows modern C++ but is new to Asio and coroutines. A *dev-day* is about 6 hours of focused work. The pessimistic column covers the learning curve and debugging that only shows up on one OS.

| Phase | Scope | Optimistic | Expected | Pessimistic | Cumulative (expected) |
|---|---|---|---|---|---|
| 0 | Foundations: restructure, deps, CI, GoogleTest | 2 d | 3 d | 5 d | 3 d |
| 0.5 | **Asio/coroutine spike** (de-risk, throwaway) | 1 d | 2 d | 3 d | 5 d |
| 1 | Async L4 proxy core | 4 d | 6 d | 9 d | 11 d |
| 2 | Balancers and backend state | 3 d | 4 d | 6 d | 15 d |
| 3 | Health checking | 3 d | 4 d | 6 d | 19 d |
| 4 | Config, hot reload, admin, metrics, drain | 4 d | 6 d | 9 d | 25 d |
| 5 | Hardening and performance (TLS excluded) | 4 d | 5 d | 8 d | 30 d |
| **v1.0 total** | | **21 d** | **30 d** | **46 d** | |
| 5b | TLS termination (optional) | 2 d | 3 d | 5 d | |
| 6 | L7 HTTP (v1.x) | 8 d | 10 d | 15 d | |

**Calendar time for v1.0 (expected 30 dev-days):**

| Availability | Calendar estimate |
|---|---|
| Full-time (5 d/week) | **≈ 6–7 weeks** (≈ 9 weeks pessimistic) |
| Half-time (~20 h/week) | **≈ 3 months** |
| Side project (~10 h/week) | **≈ 5–6 months** |

**Milestones that can be demonstrated along the way:**
- **M1 (end of Phase 1, ~day 11):** real traffic proxied through the LB with curl, a browser or `psql`.
- **M2 (end of Phase 3, ~day 19):** kill a backend and traffic heals automatically. This is the first "feels like a real LB" demo.
- **M3 (end of Phase 4, ~day 25):** edit `lb.toml`, reload, and watch Grafana update.
- **v1.0 (end of Phase 5, ~day 30):** performance numbers published.

**Biggest schedule risks:** coroutine lifetime bugs (Phase 1), and getting IOCP and epoll to behave the same in integration tests (Phases 1 and 3). Phase 0.5 exists to find out about these early.

---

## 10. Getting started: step-by-step workflow

### 10.1 Branching and PR workflow

```
main ─────●─────────────●────────────●──────────► (always green, releasable)
           \           / \          /
            feat/p0-foundations    feat/p1-async-core   ...
                 (PR, CI green, squash-merge)
```

- This branch (`wail/feat/new-architecture`) holds the **plan and ADRs only**. Merge it first so the decisions are recorded on `main`.
- One branch per phase (`wail/feat/p0-foundations`, `wail/feat/p1-async-core`, …) cut from the latest `main`. Phases larger than about 500 changed lines get split into several PRs.
- Use conventional commits (`feat:`, `fix:`, `refactor:`, `test:`, `docs:`, `ci:`, `build:`).
- Definition of done for every PR: CI green on all three OSes, no sanitizer findings, tests added for new behaviour, and the docs/ADR updated if a decision changed.

### 10.2 Day 1: tooling and the plan

1. **Install the missing tools** (Windows dev box):
   ```powershell
   winget install LLVM.LLVM          # clang-format, clang-tidy
   winget install hatoo.oha          # load testing
   winget install GitHub.cli         # PRs from the terminal
   ```
2. **Review this plan** and answer the open questions in §11, because they change the Phase 5 and 6 scope.
3. **Merge the plan:** `gh pr create --base main --head wail/feat/new-architecture`, then squash-merge.

### 10.3 Days 1–3: Phase 0 (Foundations)

```bash
git checkout main && git pull
git checkout -b wail/feat/p0-foundations
```

**Step 1: restructure the folders** (behaviour unchanged, so the code still builds):
```bash
mkdir -p cmake config docs/adr src/{app,config,net,backend,balance,admin,observe} tests/{unit,integration,load}
git mv tests/unit_tests.cpp tests/unit/legacy_tests.cpp
```
Commit: `refactor: adopt target directory layout`.

**Step 2: add `.gitattributes`, `.clang-format` and `.clang-tidy`:**
```gitattributes
* text=auto eol=lf
*.bat text eol=crlf
```
Run `clang-format -i` over the sources once, in **its own commit** (`style: apply clang-format`), so later diffs stay readable.

**Step 3: add the CMake modules.**
- `cmake/CompilerWarnings.cmake`: the warning flags from ADR-11, plus an `LB_WARNINGS_AS_ERRORS` option.
- `cmake/Sanitizers.cmake`: an `LB_SANITIZE` option (`address,undefined` or `thread`).
- `cmake/Dependencies.cmake`, for example:
  ```cmake
  include(FetchContent)
  FetchContent_Declare(asio
      GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
      GIT_TAG        asio-1-30-2
      GIT_SHALLOW    TRUE)
  FetchContent_Declare(googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
      FIND_PACKAGE_ARGS NAMES GTest)
  # spdlog, tomlplusplus, CLI11 follow the same pattern
  FetchContent_MakeAvailable(asio googletest)

  add_library(asio INTERFACE)
  target_include_directories(asio SYSTEM INTERFACE ${asio_SOURCE_DIR}/asio/include)
  target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED
                                            $<$<PLATFORM_ID:Windows>:_WIN32_WINNT=0x0A00>)
  target_link_libraries(asio INTERFACE Threads::Threads $<$<PLATFORM_ID:Windows>:ws2_32 mswsock>)
  ```
- Raise `CMAKE_CXX_STANDARD` to 20.

**Step 4: add `CMakePresets.json`** so everyone builds the same way:
```
cmake --preset dev          # Debug, Ninja, warnings
cmake --preset asan         # Debug + ASan/UBSan (Linux/Clang)
cmake --preset release      # RelWithDebInfo
cmake --build --preset dev && ctest --preset dev
```
Replace the bodies of `scripts/build.sh` and `scripts/tests.bat` with these preset calls.

**Step 5: port the tests to GoogleTest.** Convert the 6 `CHECK`s into `TEST()` cases and register them with `gtest_discover_tests`.

**Step 6: set up CI.** Add `.github/workflows/ci.yml` with the OS/compiler matrix from ADR-11 (build + `ctest` only at this stage). Push and iterate until all jobs are green.

**Step 7: write the ADRs.** Copy each ADR from §4 into `docs/adr/NNNN-title.md`.

**Phase 0 is done when:** CI is green on Windows (MSVC and MinGW), Linux and macOS, and the existing behaviour is unchanged.

### 10.4 Days 4–5: Phase 0.5 (Asio/coroutine spike)

**The goal is to prove the riskiest technology works on every target before building on it.** The code is throwaway, kept in `spikes/echo_proxy.cpp` and excluded from the main build.

1. Write a minimal coroutine **echo server** (accept → read → write back) on one `io_context`. Check it builds with **MinGW GCC 15** (the known risk) and with MSVC.
2. Turn it into a **hard-coded TCP proxy**: listen on 8080, forward to 9001 using `pipe() && pipe()`.
3. Point it at a real backend (`python -m http.server 9001`) and `curl` through it.
4. Run `oha -c 500 -z 20s http://localhost:8080`, and check that nothing crashes and the open-connection count stays stable.
5. On Linux (WSL is fine), run it under **TSan** and **ASan**.
6. Write down what you learned (compiler flags, MinGW quirks, IOCP surprises) in `docs/adr/0002-…md`.

**Go/no-go:** if the spike works on MinGW and MSVC, continue with Phase 1. If MinGW coroutines turn out to be unstable, make MSVC the main Windows toolchain and keep MinGW as best-effort (this needs an ADR amendment).

### 10.5 From day 6: Phases 1–5

Each phase follows the same loop:

```
1. Cut branch from main            git checkout -b wail/feat/pN-<name>
2. Write integration test first    (it fails: red)
3. Implement the smallest slice    (unit tests alongside)
4. Make it green locally           cmake --build --preset dev && ctest --preset dev
5. Run sanitizers (WSL/Linux)      cmake --preset tsan && ctest --preset tsan
6. Open PR, CI green on all OSes, self-review the diff
7. Squash-merge, tag milestone     git tag v0.N.0  (M1 = v0.1.0 ... v1.0.0)
8. Update this doc's checkboxes and any ADR whose decision changed
```

Suggested order of work *within* Phase 1, which is the largest:
1. `WorkerPool` (start/stop N threads) with a unit test that shuts down cleanly.
2. `Listener`, using a single shared acceptor on all OSes at first. Add `SO_REUSEPORT` afterwards.
3. `pipe()` and `Session` with a hard-coded backend. Integration test: echo through the proxy.
4. Connect and idle timeouts. Integration test: an idle client is dropped after the timeout.
5. Round-robin over a static list, and the CLI flags.
6. Large-payload, half-close and 1k-connection tests. Then tag **v0.1.0 (M1)**.

---

## 11. Open questions

1. **Deployment target**: is Linux the production OS, which would prioritize `SO_REUSEPORT`, io_uring and splice work, or is Windows a first-class deployment target?
2. **TLS**: should it be in v1.0 (Phase 5) or deferred? It adds OpenSSL as the only non-trivial dependency.
3. **Config format**: TOML is proposed. Is YAML preferred for consistency with an existing K8s/ops toolchain?
4. **Licence**: all chosen libraries are permissive (BSL-1.0, MIT, BSD-3, Apache-2.0). The project licence should be chosen before the first release.
