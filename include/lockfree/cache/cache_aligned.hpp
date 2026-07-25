#pragma once

#include "lockfree/config.hpp"

#include <cstddef>

namespace lockfree {

template <typename T, std::size_t Alignment = cache_line_size>
struct alignas(Alignment) CacheAligned {
    static_assert(Alignment != 0,
                  "CacheAligned alignment must be nonzero");
    static_assert((Alignment & (Alignment - 1)) == 0,
                  "CacheAligned alignment must be a power of two");
    static_assert(Alignment >= alignof(T),
                  "CacheAligned alignment must satisfy T's alignment");

    T value;

    constexpr T& get() noexcept { return value; }
    constexpr const T& get() const noexcept { return value; }

    constexpr T& operator*() noexcept { return value; }
    constexpr const T& operator*() const noexcept { return value; }

    constexpr T* operator->() noexcept { return &value; }
    constexpr const T* operator->() const noexcept { return &value; }
};

template <typename T, std::size_t Alignment = cache_line_size>
using Separated = CacheAligned<T, Alignment>;

} // namespace lockfree
