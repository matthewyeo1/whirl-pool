#pragma once
#include <atomic>
#include <memory>
#include <vector>
#include <type_traits>
#include <new>
#include "utils.hpp"

namespace lockfree {

template<typename T, size_t Capacity>
class PooledPtr;

/**
 * Lock-Free Object Pool
 * 
 * Pre-allocates N objects and provides lock-free acquire/release
 * 
 * obj auto-returns to pool on destruction
 */
template<typename T, size_t Capacity = 65536>

class ObjectPool {
    static_assert(Capacity > 0, "Capacity must be positive");

public:
    struct Block {
        alignas(alignof(T)) unsigned char storage[sizeof(T)];
        std::atomic<Block*> next;
        bool constructed = false;
    };

    ObjectPool() : m_free_list(nullptr), m_used(0) {
        // Preallocate all objects in one contiguous block
        m_memory = static_cast<char*>(::operator new(Capacity * sizeof(Block)));

        // Construct free list
        for (size_t i = 0; i < Capacity; i++) {
            Block* block = reinterpret_cast<Block*>(m_memory + i * sizeof(Block));
            block->constructed = false;
            block->next.store(m_free_list.load(std::memory_order_relaxed),
                                std::memory_order_relaxed);
            m_free_list.store(block, std::memory_order_relaxed);
        }
    }

    ~ObjectPool() {
        // Destroy any remaining objects
        Block* block = m_free_list.load(std::memory_order_relaxed);

        for (size_t i = 0; i < Capacity; i++) {
            Block* block = reinterpret_cast<Block*>(m_memory + i * sizeof(Block));
            if (block->constructed) {
                reinterpret_cast<T*>(block->storage)->~T();
            }
        }
        ::operator delete(m_memory);
    }

    // Non-copyable
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;
    
    // Acquire an object (returns RAII wrapper)
    auto acquire() {
        Block* block = pop_free_list();
        
        if (!block) {
            return PooledPtr<T, Capacity>(nullptr, *this);
        }

        // Construct object in place
        T* ptr = new (block->storage) T();
        block->constructed = true;
        m_used.fetch_add(1, std::memory_order_relaxed);

        return PooledPtr<T, Capacity>(ptr, *this, block);
    }
    
    // Release object back to pool (called automatically)
    void release(T* ptr, Block* block) {
        if (!ptr) return;

        // Destroy object
        ptr->~T();
        block->constructed = false;

        // Push back to free list
        push_free_list(block);
        m_used.fetch_sub(1, std::memory_order_relaxed);
    }
    
    // Statistics
    size_t used_count() const {
        return m_used.load(std::memory_order_relaxed);
    }

    size_t free_count() const {
        return Capacity - used_count();
    }

    constexpr size_t capacity() const {
        return Capacity;
    }

private:

    // Friend declaratio so PooledPtr can access Block
    friend class PooledPtr<T, Capacity>;
    
    Block* pop_free_list() {
    Block* block = m_free_list.load(std::memory_order_acquire);
        while (block) {
            Block* next = block->next.load(std::memory_order_relaxed);
            if (m_free_list.compare_exchange_weak(block, next,
                                                std::memory_order_acq_rel)) {
                return block;
            }
            // Block is automatically updated to current m_free_list value
            // If compare_exchange fails just continue the loop
        }
        return nullptr;
    }
        
    void push_free_list(Block* block) {
        Block* old_head = m_free_list.load(std::memory_order_relaxed);
        do {
            block->next.store(old_head, std::memory_order_relaxed);
        } while (!m_free_list.compare_exchange_weak(old_head, block,
                                                        std::memory_order_release));
    }
    
    char* m_memory;
    std::atomic<Block*> m_free_list;
    std::atomic<size_t> m_used;
};

// RAII wrapper that auto-returns to pool
template<typename T, size_t Capacity>
class PooledPtr {
public:
    explicit PooledPtr(T* ptr, ObjectPool<T, Capacity>& pool, 
        typename ObjectPool<T, Capacity>::Block* block = nullptr)
        : m_ptr(ptr), m_pool(pool), m_block(block) {}
    
    ~PooledPtr() {
        if (m_ptr) m_pool.release(m_ptr, m_block);
    }
    
    // Move constructor
    PooledPtr(PooledPtr&& other) noexcept
        : m_ptr(other.m_ptr), m_pool(other.m_pool), m_block(other.m_block) {
        other.m_ptr = nullptr;
        other.m_block = nullptr;
    }

    // Move assignment
    PooledPtr& operator=(PooledPtr&& other) noexcept {
        if (this != &other) {
            if (m_ptr) m_pool.release(m_ptr, m_block);

            m_ptr = other.m_ptr;
            m_pool = other.m_pool;
            m_block = other.m_block;
            other.m_ptr = nullptr;
            other.m_block = nullptr;
        }
        return *this;
    }
    
    // No copying
    PooledPtr(const PooledPtr&) = delete;
    PooledPtr& operator=(const PooledPtr&) = delete;
    
    T* operator->() { return m_ptr; }
    const T* operator->() const { return m_ptr; }
    T& operator*() { return *m_ptr; }
    const T& operator*() const { return *m_ptr; }
    T* get() { return m_ptr; }
    explicit operator bool() const { return m_ptr != nullptr; }
    
private:
    T* m_ptr;
    ObjectPool<T, Capacity>& m_pool;
    typename ObjectPool<T, Capacity>::Block* m_block;
};

}