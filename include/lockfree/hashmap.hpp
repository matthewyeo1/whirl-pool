#pragma once
#include <atomic>
#include <optional>
#include <vector>
#include <functional>
#include <memory>
#include "utils.hpp"

namespace lockfree {

/**
 * Lock-Free HashMap
 * 
 * Fixed-size hash map with linear probing and CAS updates.
 * Thread-safe for multiple producers/consumers.
 */
template<typename Key, typename Value, size_t Capacity = 1048576>
class HashMap {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    
public:
    HashMap() : m_size(0) {
        // Initialize all slots as empty
        for (size_t i = 0; i < Capacity; i++) {
            m_occupied[i].store(false, std::memory_order_relaxed);
        }
    }
    
    // Disable copy
    HashMap(const HashMap&) = delete;
    HashMap& operator=(const HashMap&) = delete;
    
    bool insert(const Key& key, const Value& value) {
        size_t idx = hash(key);
        size_t start = idx;
        
        while (true) {
            bool expected = false;
            
            // Try to claim this slot if empty
            if (m_occupied[idx].compare_exchange_weak(expected, true,
                std::memory_order_acquire, std::memory_order_relaxed)) {
                m_keys[idx] = key;
                m_values[idx].store(value, std::memory_order_release);
                m_size.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
            
            // Slot already occupied, check if it's our key
            if (m_keys[idx] == key) {
                m_values[idx].store(value, std::memory_order_release);
                return true;
            }
            
            // Move to next slot
            idx = (idx + 1) & (Capacity - 1);
            if (idx == start) {
                return false;  // Table full
            }
        }
    }
    
    std::optional<Value> find(const Key& key) const {
        size_t idx = hash(key);
        size_t start = idx;
        
        while (true) {
            if (!m_occupied[idx].load(std::memory_order_acquire)) {
                return std::nullopt;
            }
            
            if (m_keys[idx] == key) {
                return m_values[idx].load(std::memory_order_acquire);
            }
            
            idx = (idx + 1) & (Capacity - 1);
            if (idx == start) {
                return std::nullopt;
            }
        }
    }
    
    bool erase(const Key& key) {
        size_t idx = hash(key);
        size_t start = idx;
        
        while (true) {
            if (!m_occupied[idx].load(std::memory_order_acquire)) {
                return false;
            }
            
            if (m_keys[idx] == key) {
                m_occupied[idx].store(false, std::memory_order_release);
                m_size.fetch_sub(1, std::memory_order_relaxed);
                return true;
            }
            
            idx = (idx + 1) & (Capacity - 1);
            if (idx == start) {
                return false;
            }
        }
    }
    
    bool contains(const Key& key) const {
        return find(key).has_value();
    }
    
    size_t size() const {
        return m_size.load(std::memory_order_relaxed);
    }
    
    bool empty() const {
        return size() == 0;
    }
    
    constexpr size_t capacity() const {
        return Capacity;
    }
    
private:
    size_t hash(const Key& key) const {
        return std::hash<Key>{}(key) & (Capacity - 1);
    }
    
    alignas(CACHE_LINE_SIZE) std::atomic<bool> m_occupied[Capacity];
    Key m_keys[Capacity];
    alignas(CACHE_LINE_SIZE) std::atomic<Value> m_values[Capacity];
    std::atomic<size_t> m_size;
};

}