# ADR-0009: Observability: spdlog and a native Prometheus endpoint

- **Status:** Accepted
- **Date:** 2026-10-16
- **Details:** [ARCHITECTURE_PLAN.md §4](../ARCHITECTURE_PLAN.md#4-architecture-decisions-adrs)

## Decision

Async spdlog 1.14 for logs. A hand-rolled per-worker metrics registry exposed in Prometheus text format on the Asio admin server.

## Key reasons

- Async logging keeps I/O off worker threads.
- Thread-local counters avoid contention on the hot path.
- Avoids prometheus-cpp's extra HTTP server dependency.
