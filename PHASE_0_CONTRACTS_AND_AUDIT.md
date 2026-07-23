# Phase 0 — Contracts and Audit

## Part A — Current Implementation Audit

Project 3 will extend Whirl-Pool directly through small `lockfree` components and enhancements. It will not create a separate LowLatencyLib layer, namespace, or package.

Audit baseline: commit `a8b9993`, Apple Clang 21, macOS arm64.

### Decisions

| Area | Decision and reason |
|---|---|
| `SPSCQueue` | Retain and enhance. It currently has `Capacity - 1` usable slots and lacks batch, move, explicit-lifetime, capacity, and approximate-size APIs. |
| Legacy `RingBuffer` | Preserve as bounded MPMC. It allocates storage at construction, default-constructs payloads, and exposes reservation-inclusive size semantics. |
| `ObjectPool` | Remediate before extension. Its raw block lifetime/alignment, untagged free-list ABA exposure, constructor-failure handling, and public release path are unsafe. |
| `MPMCQueue` | Keep as legacy, not a bounded hot-path queue. Push and reclamation allocate, hazard-slot exhaustion throws, and reclamation state is shared per `T`. |
| `TStack` | Keep experimental. Allocation, cross-instance hazard state, reclamation, and move/destruction behavior need redesign. |
| `HashMap` | Defer for correctness work. Occupancy is published before key/value initialization, deletion breaks probe chains, and concurrent key access can race. |
| RCU variants | Keep experimental. Shared-pointer reads are not wait-free; `SimpleRCU` has non-atomic reads; `EpochRCU` lacks safe reader registration/reclamation. |
| `AtomicCounter` | Retain as a legacy utility; document overflow and quiescent reset. Add separate atomic/fence helpers for new algorithms. |
| Cache utilities | Replace incrementally with typed portable helpers. Current padding fails for some sizes and cache/prefetch behavior is hardcoded. |
| Build/package | Extend `whirlpool`; later export `whirlpool::whirlpool`. Current CMake has no install/export package and its test option is ineffective. |
| Tests/CI | Insufficient concurrency proof. Invariants, lifetime, wrap, allocation, and weak-memory coverage are missing; Linux CI's colon filter selects zero tests. |
| Benchmarks/docs | Preliminary only. Setup is timed, metadata is missing, performance claims are unscoped, changelog and source disagree, and no license file is tracked. |

### Evidence and limitations

- Reviewed all public headers, umbrella header, tests, benchmarks, CMake, CI, scripts, README, changelog, versioning, and tracked license files.
- All public headers compiled as C++17; strict compilation reported existing warnings.
- All 47 existing tests passed in a normal direct Apple Clang build.
- ASan/UBSan exposed undefined behavior in value-returning worker lambdas created by the test assertion macro, so the suite is not sanitizer-clean evidence.
- Passing tests do not prove lock-free correctness; the decisions above also use source-level lifetime, ordering, allocation, and reclamation analysis.
- CMake execution was unavailable locally because `cmake` is not installed; package validation is deferred to a later Phase 0 part.

### Changes

- Revised `PROJECT_3_IMPLEMENTATION_PLAN.md` so Project 3 extends Whirl-Pool directly.
- Added this cumulative audit record to preserve decisions and blockers.
- Built no components and changed no C++, CMake, CI, tests, benchmarks, scripts, or public behavior because Part A is investigative.

## Part B — Frozen Public Contracts

Whirl-Pool keeps the `lockfree` namespace, in-tree `whirlpool` target, and installed `whirlpool::whirlpool` target. Existing APIs remain source-compatible unless deprecated before a major release.

| Contract | Frozen decision |
|---|---|
| Configuration | `WHIRLPOOL_CACHE_LINE_SIZE` defaults to 64 and must be a nonzero power of two; `cache_line_size` is canonical and `CACHE_LINE_SIZE` remains compatible. |
| Errors | Hot paths use Boolean, pointer/null, or count results. Value-plus-status APIs use the allocation-free `ErrorCode` and `Result<T>` contracts. |
| SPSC capacity | `SPSCQueue<T, Capacity>` owns `Capacity` storage slots and exposes `Capacity - 1` usable positions. |
| Queue identity | `RingBuffer` remains legacy bounded MPMC; new bounded fan-in uses `MPSCQueue`. Concurrent size values are diagnostic where reservations exist. |
| Pool ownership | `ObjectPool::acquire` and move-only `PooledPtr` remain; future remediation preserves return-to-origin ownership. |
| Hot paths | New hot operations do not grow, allocate implicitly, block on hidden syscalls, or silently fall back to heap storage. |
| Concurrency | Each stable concurrent API documents roles, lifetime, capacity, linearization, memory ordering, progress, and quiescence. |
| Compatibility | New stable APIs require neither exceptions nor RTTI. Existing throwing or reclamation-sensitive structures remain provisional. |
| Platforms | Unsupported or privileged operations return explicit `unsupported`, `permission_denied`, or `platform_error` status. |
| Version | Phase 0 retains version `0.1.0`; functional enhancements trigger the next release version. |

### Changes

- Added `config.hpp` to centralize the cache-line contract while preserving `CACHE_LINE_SIZE`.
- Added `error.hpp` for the common allocation-free status contract.
- Exposed both contracts through `lockfree.h`.
- Made compile-hygiene-only corrections; no data-structure algorithm changed.

## Part C — Build and Contract Checks

Whirl-Pool now has an installable CMake interface package, canonical
`WHIRLPOOL_*` options, sanitizer modes, presets, and dependency-free contract
fixtures. In-tree consumers use `whirlpool`; build-tree and installed consumers
use `whirlpool::whirlpool`.

### Built and changed

| Change | Reason |
|---|---|
| Install/export package and external-consumer fixtures | Prove `find_package(whirlpool)` and `add_subdirectory` consumption without a second library layer. |
| Header, umbrella, multi-TU, no-exception/no-RTTI, and cache configuration fixtures | Make the frozen source and build contracts executable. |
| Canonical CMake options and sanitizer module | Remove global flags and make checks reproducible and target-scoped. |
| Test failure/filter repairs | Remove worker-lambda UB and prevent zero-test CI success. |
| GCC, Clang, AppleClang, MSVC, and ASan/UBSan CI jobs | Cover the supported compiler surface and package smoke tests. |
| MIT license, README, scripts, presets, and changelog updates | Make installation and developer workflows match the frozen package contract. |

No queue, pool, hashmap, stack, RCU, or ring-buffer algorithm changed.

### Validation

- Apple Clang 21 compiled all public headers and contract fixtures as C++17 with strict warnings treated as errors.
- The umbrella header passed a two-translation-unit link check.
- Stable contract fixtures passed with exceptions and RTTI disabled.
- Cache-line size 128 compiled; invalid sizes 0 and 63 failed as required.
- All 47 existing functional tests passed.
- The repaired colon-separated filter selected and passed 18 supported tests; an empty match returned failure.
- ASan/UBSan passed the contract API and the 18 supported pool/SPSC/ring/counter tests.
- CMake configure, install/export, preset, and external-consumer execution remain unverified locally because `cmake` is unavailable; CI is the next executable verification point.
