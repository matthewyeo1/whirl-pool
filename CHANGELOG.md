## Unreleased

## 0.2.0 - 2026-07-25

- Added cache-aligned wrappers, explicit padding, and compile-time alignment
  calculations without changing legacy cache macros or `padded<T>`.
- Added fixed-order atomic helpers plus hardware and compiler-only barriers.
- Added compile-time OS, architecture, and compiler detection with portable
  `cpu_relax` fallback behavior.
- Added allocation-free busy polling with pause, yield, no-op, and adaptive
  backoff strategies plus cancellation, attempt, and deadline results.
- Added recursive header checks, layout assertions, publication litmus tests,
  BusyPoll path coverage, package-consumer usage, and supported TSan coverage.
- Preserved all existing data-structure implementations and layouts.

- Added frozen cache-line and error/result contracts under `lockfree`.
- Added install/export packaging through `whirlpool::whirlpool`.
- Added header, multi-translation-unit, no-exception/no-RTTI, cache
  configuration, and external-consumer contract checks.
- Added canonical `WHIRLPOOL_*` CMake options and sanitizer/preset support.
- Fixed test worker-lambda failure reporting and colon-separated test filters.
- Added the MIT license and documented provisional legacy components.
- 2026-04-19: Recorded the intent to move `HashMap` arrays to the heap;
  the audited implementation still uses inline storage and remains provisional.
