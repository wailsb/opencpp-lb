# ADR-0008: TOML configuration with validated hot reload

- **Status:** Accepted
- **Date:** 2026-10-16
- **Details:** [ARCHITECTURE_PLAN.md §4](../ARCHITECTURE_PLAN.md#4-architecture-decisions-adrs)

## Decision

Configuration in TOML parsed with toml++ 3.4. Reload via SIGHUP or `POST /reload`: parse, validate, build the new state, then swap atomically. On any error the old config stays live.

## Key reasons

- TOML is unambiguous, supports comments and is easy to write by hand.
- toml++ is header-only with precise error locations.
- Rejected: YAML (implicit typing pitfalls), JSON (no comments).
