#pragma once

#include <atomic>

namespace lockfree {

inline void compiler_acquire_barrier() noexcept {
    std::atomic_signal_fence(std::memory_order_acquire);
}

inline void compiler_release_barrier() noexcept {
    std::atomic_signal_fence(std::memory_order_release);
}

inline void compiler_barrier() noexcept {
    std::atomic_signal_fence(std::memory_order_seq_cst);
}

} // namespace lockfree
