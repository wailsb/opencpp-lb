# ADR-0004: L4 TCP proxy first, L7 HTTP later

- **Status:** Accepted
- **Date:** 2026-10-16
- **Details:** [ARCHITECTURE_PLAN.md §4](../ARCHITECTURE_PLAN.md#4-architecture-decisions-adrs)

## Decision

v1.0 is a protocol-agnostic TCP proxy. HTTP-aware routing comes in Phase 6 using llhttp.

## Key reasons

- L4 serves any TCP protocol and is the shortest path to a usable product.
- L7 needs a hardened parser (request smuggling) and per-request balancing, both built on a solid L4 core.
