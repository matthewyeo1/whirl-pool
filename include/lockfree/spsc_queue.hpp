#pragma once
#include <atomic>
#include <optional>
#include <memory>
#include "utils.hpp"

namespace lockfree {

/**
 * Single-Producer Single-Consumer Lock-Free Queue
 * 
 * Design: Circular ring buffer with atomic head/tail
 * Wait-free for both push and pop (no retry loops)
 * 
*/
template<typename T, size_t Capacity>
class SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    
public:
    SPSCQueue() : m_head(0), m_tail(0) {}
    
    // Push to queue (producer only)
    bool push(const T& value) {
        size_t head = m_head.load(std::memory_order_relaxed);
        size_t next_head = (head + 1) & (Capacity - 1);
        
        // Queue full
        if (next_head == m_tail.load(std::memory_order_acquire)) {
            return false; 
        }
        
        m_buffer[head] = value;
        m_head.store(next_head, std::memory_order_release);
        return true;
    }
    
    // Pop from queue (consumer only)
    std::optional<T> pop() {
        size_t tail = m_tail.load(std::memory_order_relaxed);
        
        // Queue empty
        if (tail == m_head.load(std::memory_order_acquire)) {
            return std::nullopt; 
        }
        
        T value = std::move(m_buffer[tail]);
        m_tail.store((tail + 1) & (Capacity - 1), std::memory_order_release);
        return value;
    }
    
    bool empty() const {
        return m_head.load(std::memory_order_acquire) == 
               m_tail.load(std::memory_order_acquire);
    }
    
    size_t size() const {
        size_t head = m_head.load(std::memory_order_acquire);
        size_t tail = m_tail.load(std::memory_order_acquire);
        return (head - tail) & (Capacity - 1);
    }
    
private:
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> m_head;
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> m_tail;
    T m_buffer[Capacity];
};

}