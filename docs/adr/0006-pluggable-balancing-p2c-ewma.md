# ADR-0006: Pluggable balancing, P2C + EWMA as default

- **Status:** Accepted
- **Date:** 2026-10-16
- **Details:** [ARCHITECTURE_PLAN.md §4](../ARCHITECTURE_PLAN.md#4-architecture-decisions-adrs)

## Decision

A `Balancer` interface with round-robin (smooth weighted), least-connections and power-of-two-choices weighted by EWMA latency (default).

## Key reasons

- Picking the global minimum sends every worker to the same backend at once (herding).
- P2C is O(1), needs no coordination, and is proven in Envoy, Linkerd and Finagle.
- EWMA moves traffic away from slow backends before they fail.
