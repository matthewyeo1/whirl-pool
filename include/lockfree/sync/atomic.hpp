#pragma once

#include <atomic>

namespace lockfree {

template <typename T>
T load_relaxed(const std::atomic<T>& value) noexcept {
    return value.load(std::memory_order_relaxed);
}

template <typename T>
T load_relaxed(const volatile std::atomic<T>& value) noexcept {
    return value.load(std::memory_order_relaxed);
}

template <typename T>
T load_acquire(const std::atomic<T>& value) noexcept {
    return value.load(std::memory_order_acquire);
}

template <typename T>
T load_acquire(const volatile std::atomic<T>& value) noexcept {
    return value.load(std::memory_order_acquire);
}

template <typename T>
void store_relaxed(std::atomic<T>& target, T value) noexcept {
    target.store(value, std::memory_order_relaxed);
}

template <typename T>
void store_relaxed(volatile std::atomic<T>& target, T value) noexcept {
    target.store(value, std::memory_order_relaxed);
}

template <typename T>
void store_release(std::atomic<T>& target, T value) noexcept {
    target.store(value, std::memory_order_release);
}

template <typename T>
void store_release(volatile std::atomic<T>& target, T value) noexcept {
    target.store(value, std::memory_order_release);
}

#define WHIRLPOOL_DEFINE_CAS_HELPER(name, operation, success_order, failure_order) \
    template <typename T>                                                        \
    bool name(std::atomic<T>& target, T& expected, T desired) noexcept {          \
        return target.operation(expected, desired, success_order, failure_order); \
    }                                                                             \
    template <typename T>                                                        \
    bool name(volatile std::atomic<T>& target, T& expected, T desired) noexcept { \
        return target.operation(expected, desired, success_order, failure_order); \
    }

WHIRLPOOL_DEFINE_CAS_HELPER(compare_exchange_weak_relaxed,
                            compare_exchange_weak,
                            std::memory_order_relaxed,
                            std::memory_order_relaxed)
WHIRLPOOL_DEFINE_CAS_HELPER(compare_exchange_weak_acquire,
                            compare_exchange_weak,
                            std::memory_order_acquire,
                            std::memory_order_acquire)
WHIRLPOOL_DEFINE_CAS_HELPER(compare_exchange_weak_release,
                            compare_exchange_weak,
                            std::memory_order_release,
                            std::memory_order_relaxed)
WHIRLPOOL_DEFINE_CAS_HELPER(compare_exchange_weak_acq_rel,
                            compare_exchange_weak,
                            std::memory_order_acq_rel,
                            std::memory_order_acquire)
WHIRLPOOL_DEFINE_CAS_HELPER(compare_exchange_strong_relaxed,
                            compare_exchange_strong,
                            std::memory_order_relaxed,
                            std::memory_order_relaxed)
WHIRLPOOL_DEFINE_CAS_HELPER(compare_exchange_strong_acquire,
                            compare_exchange_strong,
                            std::memory_order_acquire,
                            std::memory_order_acquire)
WHIRLPOOL_DEFINE_CAS_HELPER(compare_exchange_strong_release,
                            compare_exchange_strong,
                            std::memory_order_release,
                            std::memory_order_relaxed)
WHIRLPOOL_DEFINE_CAS_HELPER(compare_exchange_strong_acq_rel,
                            compare_exchange_strong,
                            std::memory_order_acq_rel,
                            std::memory_order_acquire)

#undef WHIRLPOOL_DEFINE_CAS_HELPER

inline void acquire_fence() noexcept {
    std::atomic_thread_fence(std::memory_order_acquire);
}

inline void release_fence() noexcept {
    std::atomic_thread_fence(std::memory_order_release);
}

inline void full_fence() noexcept {
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

} // namespace lockfree
