#pragma once
#include <atomic>
#include <cstddef>
#include "config.hpp"

namespace lockfree {

// Backward-compatible name. New code should prefer cache_line_size.
inline constexpr std::size_t CACHE_LINE_SIZE = cache_line_size;

// Force alignment to cache line to prevent false sharing
#define LOCKFREE_CACHE_ALIGN alignas(lockfree::CACHE_LINE_SIZE)

// Compiler hints
#ifdef __GNUC__
    #define LOCKFREE_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define LOCKFREE_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define LOCKFREE_PREFETCH(addr) __builtin_prefetch(addr)
#else
    #define LOCKFREE_LIKELY(x)   (x)
    #define LOCKFREE_UNLIKELY(x) (x)
    #define LOCKFREE_PREFETCH(addr)
#endif

// Padding to avoid false sharing
template<typename T>
struct padded {
    T value;
    char padding[CACHE_LINE_SIZE - sizeof(T) % CACHE_LINE_SIZE];
};

}
