# ADR-0005: Immutable runtime snapshots (RCU-style)

- **Status:** Accepted
- **Date:** 2026-10-16
- **Details:** [ARCHITECTURE_PLAN.md §4](../ARCHITECTURE_PLAN.md#4-architecture-decisions-adrs)

## Decision

Topology lives in an immutable `RuntimeState` published through `std::atomic<std::shared_ptr<const RuntimeState>>`. Volatile per-backend stats are atomics on shared `Backend` objects, which are reused across reloads.

## Key reasons

- Lock-free reads on every new connection.
- Sessions keep the snapshot they started with alive, so reloads drop nothing.
- Eliminates the old raw-pointer-into-vector race.
