#pragma once

#include <cstddef>

#ifndef WHIRLPOOL_CACHE_LINE_SIZE
#define WHIRLPOOL_CACHE_LINE_SIZE 64
#endif

namespace lockfree {

inline constexpr std::size_t cache_line_size = WHIRLPOOL_CACHE_LINE_SIZE;

static_assert(cache_line_size != 0,
              "WHIRLPOOL_CACHE_LINE_SIZE must be nonzero");
static_assert((cache_line_size & (cache_line_size - 1)) == 0,
              "WHIRLPOOL_CACHE_LINE_SIZE must be a power of two");

} // namespace lockfree
