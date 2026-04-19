#pragma once
#include <atomic>
#include <cstdint>
#include "utils.hpp"

namespace lockfree {

/**
 * Lock-Free Atomic Counter
 * 
 * Thread-safe counter for sequence numbers, order IDs, etc.
 */
template<typename T = uint64_t>
class AtomicCounter {
    static_assert(std::is_integral_v<T>, "T must be integral type");
    
public:
    AtomicCounter() : m_value(0) {}
    explicit AtomicCounter(T initial) : m_value(initial) {}
    
    // Get next value (increment and return)
    T next() {
        return m_value.fetch_add(1, std::memory_order_relaxed);
    }
    
    // Get current value without incrementing
    T current() const {
        return m_value.load(std::memory_order_acquire);
    }
    
    // Reset to zero (use only when no threads are using it)
    void reset() {
        m_value.store(0, std::memory_order_release);
    }
    
    // Reset to specific value
    void reset(T value) {
        m_value.store(value, std::memory_order_release);
    }
    
    // Add arbitrary delta
    T add(T delta) {
        return m_value.fetch_add(delta, std::memory_order_relaxed);
    }
    
    // Compare and set 
    bool compare_set(T expected, T desired) {
        return m_value.compare_exchange_strong(expected, desired,
            std::memory_order_acq_rel, std::memory_order_relaxed);
    }
    
private:
    alignas(CACHE_LINE_SIZE) std::atomic<T> m_value;
};

using AtomicU64 = AtomicCounter<uint64_t>;
using AtomicU32 = AtomicCounter<uint32_t>;
using OrderIDGenerator = AtomicCounter<uint64_t>;

}