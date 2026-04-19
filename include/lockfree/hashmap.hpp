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
        // Allocate storage on the heap to avoid large stack/ABI objects
        m_occupied = std::make_unique<std::atomic<bool>[]>(Capacity);
        m_keys = std::make_unique<Key[]>(Capacity);
        m_values = std::make_unique<std::atomic<Value>[]>(Capacity);

        // Initialize all entries as empty
        for (size_t i = 0; i < Capacity; i++) {
            m_occupied[i].store(false, std::memory_order_relaxed);
        }
    }
    
    // Insert KV pair
    bool insert(const Key& key, const Value& value) {
        size_t idx = hash(key);
        size_t start = idx;
        
        while (true) {
            bool occupied = m_occupied[idx].load(std::memory_order_acquire);
            
            if (!occupied) {
                // Try to claim this slot
                bool expected = false;
                if (m_occupied[idx].compare_exchange_weak(expected, true,
                    std::memory_order_release, std::memory_order_relaxed)) {
                    m_keys[idx] = key;
                    m_values[idx].store(value, std::memory_order_release);
                    m_size.fetch_add(1, std::memory_order_relaxed);
                    return true;
                }
            }
            else {
                // Slot occupied, check if key matches
                Key current_key = m_keys[idx];
                if (current_key == key) {
                    m_values[idx].store(value, std::memory_order_release);
                    return true;
                }
            }
            
            idx = (idx + 1) & (Capacity - 1);
            if (idx == start) {
                return false;  // Table full
            }
        }
    }
    
    // Retrieve KV pair if inside hashmap
    std::optional<Value> find(const Key& key) const {
        size_t idx = hash(key);
        size_t start = idx;
        
        while (true) {
            bool occupied = m_occupied[idx].load(std::memory_order_acquire);
            
            if (!occupied) {
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
    
    // Remove KV pair
    bool erase(const Key& key) {
        size_t idx = hash(key);
        size_t start = idx;
        
        while (true) {
            bool occupied = m_occupied[idx].load(std::memory_order_acquire);
            
            if (!occupied) {
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
    static constexpr Key EMPTY_KEY = Key(-1);
    static constexpr Key TOMBSTONE_KEY = Key(-2);
    
    size_t hash(const Key& key) const {
        return std::hash<Key>{}(key) & (Capacity - 1);
    }
    
    // Heap-allocated arrays to avoid huge stack objects
    std::unique_ptr<std::atomic<bool>[]> m_occupied;
    std::unique_ptr<Key[]> m_keys;
    std::unique_ptr<std::atomic<Value>[]> m_values;
    std::atomic<size_t> m_size{0};
};

} 