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

Validated on 2026-10-16 with MinGW-w64 GCC 15.2: a coroutine echo backend plus a proxy using `pipe() && pipe()` moved 1 MiB intact over loopback.

- **Required defines:** `ASIO_STANDALONE`, plus `_WIN32_WINNT=0x0A00` on Windows. Link `ws2_32` and `mswsock`.
- **GCC false positive:** `-Wmismatched-new-delete` fires on Asio's awaitable frame allocator (inlined into user coroutines, so `-isystem` doesn't hide it). It is disabled for GCC in `cmake/CompilerWarnings.cmake`.
- **`||` semantics:** `operator||` waits for the first operation to *succeed*. An operation that throws does not win. Wrap socket operations in `as_tuple(use_awaitable)` so a fast failure (e.g. connection refused) completes the race immediately instead of waiting for the timeout timer.
- **Windows loopback:** connecting to a closed localhost port takes about 2 s on Windows (SYN retries) instead of failing at once. Connect timeouts must therefore be enforced by us, not by the OS.

**Go decision:** proceed with Asio coroutines on MinGW and MSVC.
