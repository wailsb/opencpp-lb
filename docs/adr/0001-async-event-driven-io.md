# ADR-0001: Asynchronous, event-driven I/O

- **Status:** Accepted
- **Date:** 2026-10-16
- **Details:** [ARCHITECTURE_PLAN.md §4](../ARCHITECTURE_PLAN.md#4-architecture-decisions-adrs)

## Decision

Replace the blocking accept-queue-worker design with event loops multiplexing sockets via the OS-native mechanism (IOCP / epoll / kqueue).

## Key reasons

- Concurrency is bounded by memory, not threads (KBs per connection instead of ~1 MB stacks).
- Removes head-of-line blocking from slow clients.
- Industry-standard model for proxies (HAProxy, Envoy, NGINX).
