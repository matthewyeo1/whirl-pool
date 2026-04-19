## Unreleased

- 2026-04-19: Fix: `HashMap` now heap-allocates internal arrays to avoid stack/ABI overflow when using large capacities; resolves benchmark crashes on CI and local runs.
