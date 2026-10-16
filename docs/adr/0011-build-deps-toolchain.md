# ADR-0011: Build, dependencies and toolchain

- **Status:** Accepted
- **Date:** 2026-10-16
- **Details:** [ARCHITECTURE_PLAN.md §4](../ARCHITECTURE_PLAN.md#4-architecture-decisions-adrs)

## Decision

CMake ≥ 3.24 with presets. Dependencies via FetchContent, pinned by tag and SHA256 (`FIND_PACKAGE_ARGS` allows system/vcpkg copies). Strict warnings, `-Werror` in CI, ASan/UBSan/TSan presets, clang-format/clang-tidy, and a CI matrix over Linux, macOS, Windows MSVC and MinGW.

## Key reasons

- A fresh clone builds with only CMake and a compiler.
- Sanitizers verify the concurrency design (ADR-0003, ADR-0005).
