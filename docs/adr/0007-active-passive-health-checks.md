# ADR-0007: Active and passive health checks with outlier ejection

- **Status:** Accepted
- **Date:** 2026-10-16
- **Details:** [ARCHITECTURE_PLAN.md §4](../ARCHITECTURE_PLAN.md#4-architecture-decisions-adrs)

## Decision

Active TCP/HTTP checks with rise/fall hysteresis and jitter on a control-plane thread. Passive ejection on consecutive failures, with exponential backoff and a max-ejection-percent safety valve. Retry connects on the next backend.

## Key reasons

- Active checks find dead idle backends; passive checks react within milliseconds.
- The ejection cap prevents an LB-side fault from ejecting the whole fleet.
