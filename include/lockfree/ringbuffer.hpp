#pragma once
#include <atomic>
#include <array>
#include <memory>
#include <optional>
#include "utils.hpp"

namespace lockfree {

/**
 * Lock-Free Ring Buffer (Bounded MPMC Queue)
 * 
 * Multi-producer multi-consumer ring buffer using sequential consistency.
 * Fixed capacity, no dynamic allocation after construction.
 * 
 * Reference: Dmitry Vyukov's bounded MPMC queue
 */
template<typename T, size_t Capacity>
class RingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    
private:
    static constexpr size_t MASK = Capacity - 1;
    
    // Cache line padded to prevent false sharing
    struct alignas(CACHE_LINE_SIZE) ProducerState {
        std::atomic<size_t> write_index{0};
    };
    
    struct alignas(CACHE_LINE_SIZE) ConsumerState {
        std::atomic<size_t> read_index{0};
    };
    
    // Each slot has a sequence number to track state
    struct Slot {
        alignas(CACHE_LINE_SIZE) std::atomic<size_t> sequence;
        T data;
        
        Slot() : sequence(0) {}
    };
    
    ProducerState m_producer;
    ConsumerState m_consumer;
    std::unique_ptr<Slot[]> m_slots;
    
public:
    RingBuffer() {
        // Allocate slots on the heap to avoid large stack usage
        m_slots = std::make_unique<Slot[]>(Capacity);
        
        for (size_t i = 0; i < Capacity; i++) {
            m_slots[i].sequence.store(i, std::memory_order_relaxed);
        }
    }
    
    // Non-copyable
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
    
    // Thread-safe push
    bool push(const T& value) {
        size_t pos = m_producer.write_index.load(std::memory_order_relaxed);
        while (true) {
            size_t slot_index = pos & MASK;
            size_t seq = m_slots[slot_index].sequence.load(std::memory_order_acquire);

            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            if (diff == 0) {
                if (m_producer.write_index.compare_exchange_weak(pos, pos + 1,
                    std::memory_order_relaxed, std::memory_order_relaxed)) {
                    m_slots[slot_index].data = value;
                    m_slots[slot_index].sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
                // CAS failed, pos has been updated with current tail, retry
            } else if (diff < 0) {
                return false; // full
            } else {
                pos = m_producer.write_index.load(std::memory_order_relaxed);
            }
        }
    }
    
    // Thread-safe pop
    std::optional<T> pop() {
        size_t pos = m_consumer.read_index.load(std::memory_order_relaxed);
        while (true) {
            size_t slot_index = pos & MASK;
            size_t seq = m_slots[slot_index].sequence.load(std::memory_order_acquire);

            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
            if (diff == 0) {
                if (m_consumer.read_index.compare_exchange_weak(pos, pos + 1,
                    std::memory_order_relaxed, std::memory_order_relaxed)) {
                    T value = std::move(m_slots[slot_index].data);
                    m_slots[slot_index].sequence.store(pos + Capacity, std::memory_order_release);
                    return value;
                }
                // CAS failed, pos updated, retry
            } else if (diff < 0) {
                return std::nullopt; // empty
            } else {
                pos = m_consumer.read_index.load(std::memory_order_relaxed);
            }
        }
    }

    size_t size() const {
        size_t write = m_producer.write_index.load(std::memory_order_acquire);
        size_t read = m_consumer.read_index.load(std::memory_order_acquire);
        return write - read;
    }
    
    // Check if empty
    bool empty() const {
        return size() == 0;
    }
    
    bool full() const {
        return size() == Capacity;
    }
    
    constexpr size_t capacity() const {
        return Capacity;
    }
};

} 