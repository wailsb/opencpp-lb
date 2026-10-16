# ADR-0002: Standalone Asio 1.30 with C++20 coroutines

- **Status:** Accepted
- **Date:** 2026-10-16
- **Details:** [ARCHITECTURE_PLAN.md §4](../ARCHITECTURE_PLAN.md#4-architecture-decisions-adrs)

## Decision

Use header-only standalone Asio (`asio-1-30-2`) and write session logic as `asio::awaitable` coroutines. Raise the language standard to C++20.

## Key reasons

- One API over IOCP, epoll and kqueue, with Windows as a first-class target.
- Timers, signals, cancellation and `awaitable_operators` built in, plus SSL later.
- Coroutines keep the proxy logic linear and lifetimes in the coroutine frame.
- Rejected: libuv (C callbacks), io_uring (Linux only), Boost.Asio (heavy), Seastar (Linux only, invasive).

## Spike findings (Phase 0.5)

_To be filled in after the echo-proxy spike: MinGW/MSVC behaviour, flags, gotchas._
