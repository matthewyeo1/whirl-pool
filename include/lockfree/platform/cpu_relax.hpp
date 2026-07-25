#pragma once

#include "lockfree/platform/features.hpp"
#include "lockfree/sync/compiler_barrier.hpp"

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__x86_64__)
#include <immintrin.h>
#endif

namespace lockfree {

inline void cpu_relax() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    _mm_pause();
#elif defined(__aarch64__)
    __asm__ __volatile__("yield");
#elif defined(_M_ARM64) && defined(_MSC_VER)
    __yield();
#else
    compiler_barrier();
#endif
}

} // namespace lockfree
