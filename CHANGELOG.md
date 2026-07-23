## Unreleased

- Added frozen cache-line and error/result contracts under `lockfree`.
- Added install/export packaging through `whirlpool::whirlpool`.
- Added header, multi-translation-unit, no-exception/no-RTTI, cache
  configuration, and external-consumer contract checks.
- Added canonical `WHIRLPOOL_*` CMake options and sanitizer/preset support.
- Fixed test worker-lambda failure reporting and colon-separated test filters.
- Added the MIT license and documented provisional legacy components.
- 2026-04-19: Recorded the intent to move `HashMap` arrays to the heap;
  the audited implementation still uses inline storage and remains provisional.
