# ADR-0003: One io_context per thread (shared-nothing data plane)

- **Status:** Accepted
- **Date:** 2026-10-16
- **Details:** [ARCHITECTURE_PLAN.md §4](../ARCHITECTURE_PLAN.md#4-architecture-decisions-adrs)

## Decision

Run N workers, each owning one `io_context` and one thread. A session lives entirely on the worker that accepted it. Linux uses `SO_REUSEPORT` per-worker listeners; other platforms use one acceptor that distributes sockets round-robin.

## Key reasons

- No locks or strands on per-connection state.
- Cache locality.
- No central acceptor bottleneck on Linux.
