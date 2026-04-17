#pragma once
#include <atomic>
#include <memory>
#include <vector>
#include <type_traits>
#include "utils.hpp"

namespace lockfree {

/**
 * Lock-Free Object Pool
 * 
 * Pre-allocates N objects and provides lock-free acquire/release
 * 
 * Usage:
 *   ObjectPool<MyClass, 10000> pool;
 *   auto obj = pool.acquire();
 *   obj->doSomething();
 * 
 * obj auto-returns to pool on destruction
 */
template<typename T, size_t Capacity = 65536>
class ObjectPool {
    static_assert(Capacity > 0, "Capacity must be positive");
    
public:
    ObjectPool();
    ~ObjectPool();
    
    // Acquire an object (returns RAII wrapper)
    auto acquire();
    
    // Release object back to pool (called automatically)
    void release(T* ptr);
    
    // Statistics
    size_t used_count() const;
    size_t free_count() const;
    
private:
    struct Block {
        alignas(T) unsigned char storage[sizeof(T)];
        std::atomic<Block*> next;
    };
    
    Block* m_free_list;
    std::vector<Block> m_blocks;
    std::atomic<size_t> m_used{0};
};

// RAII wrapper that auto-returns to pool
template<typename T, size_t Capacity>
class PooledPtr {
public:
    explicit PooledPtr(T* ptr, ObjectPool<T, Capacity>& pool)
        : m_ptr(ptr), m_pool(pool) {}
    
    ~PooledPtr() {
        if (m_ptr) m_pool.release(m_ptr);
    }
    
    // Move semantics
    PooledPtr(PooledPtr&& other) noexcept
        : m_ptr(other.m_ptr), m_pool(other.m_pool) {
        other.m_ptr = nullptr;
    }
    
    // No copying
    PooledPtr(const PooledPtr&) = delete;
    PooledPtr& operator=(const PooledPtr&) = delete;
    
    T* operator->() { return m_ptr; }
    T& operator*() { return *m_ptr; }
    T* get() { return m_ptr; }
    
private:
    T* m_ptr;
    ObjectPool<T, Capacity>& m_pool;
};

}