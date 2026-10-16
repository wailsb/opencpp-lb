# ADR-0012: Testing strategy

- **Status:** Accepted
- **Date:** 2026-10-16
- **Details:** [ARCHITECTURE_PLAN.md §4](../ARCHITECTURE_PLAN.md#4-architecture-decisions-adrs)

## Decision

GoogleTest unit tests, in-process integration tests with real sockets and mock backends, oha/wrk load tests, libFuzzer for parsers (Phase 6), and 24 h soak tests before v1.0.

## Key reasons

- In-process tests are deterministic, need no extra runtime, and run under sanitizers.
