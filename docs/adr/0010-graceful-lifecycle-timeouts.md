# ADR-0010: Graceful lifecycle and bounded timeouts

- **Status:** Accepted
- **Date:** 2026-10-16
- **Details:** [ARCHITECTURE_PLAN.md §4](../ARCHITECTURE_PLAN.md#4-architecture-decisions-adrs)

## Decision

Signal-driven drain (stop accepting, fail `/ready`, wait up to `drain_timeout`, then cancel). Every network wait is bounded by connect or idle timeouts using `awaitable_operators`.

## Key reasons

- No dropped in-flight connections on deploy.
- Defends against slowloris-style resource exhaustion.
