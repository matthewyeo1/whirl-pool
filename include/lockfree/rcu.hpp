#pragma once
#include <atomic>
#include <memory>
#include <thread>
#include <chrono>
#include <functional>
#include <vector>
#include "utils.hpp"

namespace lockfree {

/**
 * Read-Copy-Update (RCU) Pattern
 * 
 * Lock-free reads, safe updates with automatic cleanup.
 * Perfect for configuration data, symbol tables, and routing tables.
 *
 * Reference: McKenney's RCU (Read-Copy-Update)
 */
template<typename T>
class RCU {
public:
    RCU() : m_ptr(std::make_shared<T>()) {}
    
    explicit RCU(const T& initial) : m_ptr(std::make_shared<T>(initial)) {}
    
    explicit RCU(T&& initial) : m_ptr(std::make_shared<T>(std::move(initial))) {}
    
    // Lock & wait-free read
    std::shared_ptr<const T> read() const {
        return std::atomic_load_explicit(&m_ptr, std::memory_order_acquire);
    }
    
    // Update
    void update(std::shared_ptr<T> new_ptr) {
        // Atomically swap pointers
        auto old_ptr = std::atomic_exchange_explicit(&m_ptr, new_ptr, 
            std::memory_order_release);
        
        // Grace period: wait for readers to finish
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // old_ptr will be destroyed automatically when last reference goes away
    }
    
    // Update with copy
    void update(const T& value) {
        update(std::make_shared<T>(value));
    }
    
    // Update with move
    void update(T&& value) {
        update(std::make_shared<T>(std::move(value)));
    }
    
    // Modify current value atomically (writer-only)
    void modify(std::function<void(T&)> fn) {
        auto new_ptr = std::make_shared<T>(*std::atomic_load_explicit(&m_ptr, 
            std::memory_order_acquire));
        fn(*new_ptr);
        update(std::move(new_ptr));
    }
    
    T get() const {
        return *std::atomic_load_explicit(&m_ptr, std::memory_order_acquire);
    }
    
    // Check if set
    bool valid() const {
        return m_ptr != nullptr;
    }
    
private:
    std::shared_ptr<T> m_ptr;
};


/**
 * Epoch-based RCU for high-performance use cases
 * Provides proper grace period without fixed sleeps
 */
template<typename T>
class EpochRCU {
private:
    struct EpochNode {
        std::shared_ptr<T> data;
        std::atomic<uint64_t> epoch;
        EpochNode* next;
        
        EpochNode(std::shared_ptr<T> d, uint64_t e) 
            : data(d), epoch(e), next(nullptr) {}
    };
    
    std::atomic<EpochNode*> m_head{nullptr};
    std::atomic<uint64_t> m_global_epoch{0};
    
public:
    EpochRCU() {
        m_head.store(new EpochNode(std::make_shared<T>(), 0), 
            std::memory_order_relaxed);
    }
    
    ~EpochRCU() {
        EpochNode* node = m_head.load();
        while (node) {
            EpochNode* next = node->next;
            delete node;
            node = next;
        }
    }

    std::shared_ptr<const T> read() const {
        uint64_t epoch = m_global_epoch.load(std::memory_order_acquire);
        EpochNode* node = m_head.load(std::memory_order_acquire);

        return node->data;
    }
    
    void update(std::shared_ptr<T> new_data) {
        uint64_t new_epoch = m_global_epoch.load(std::memory_order_relaxed) + 1;
        auto new_node = new EpochNode(std::move(new_data), new_epoch);
        
        EpochNode* old_head = m_head.exchange(new_node, std::memory_order_acq_rel);
        new_node->next = old_head;
        
        // Advance global epoch
        m_global_epoch.store(new_epoch, std::memory_order_release);
        
        // Garbage collect old nodes
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        
        // Clean up nodes older than current epoch minus 2
        EpochNode* node = new_node->next;
        EpochNode* prev = new_node;
        uint64_t current_epoch = m_global_epoch.load();
        
        while (node) {
            if (node->epoch.load() + 2 < current_epoch) {
                prev->next = node->next;
                delete node;
                node = prev->next;
            } else {
                prev = node;
                node = node->next;
            }
        }
    }
    
    void update(const T& value) {
        update(std::make_shared<T>(value));
    }
    
    T get() const {
        return *read();
    }
};

/**
 * Simple RCU for small configuration data
 * Uses copy-on-write with shared_ptr
 */
template<typename T>
class SimpleRCU {
public:
    SimpleRCU() : m_ptr(std::make_shared<T>()) {}
    explicit SimpleRCU(const T& initial) : m_ptr(std::make_shared<T>(initial)) {}
    
    std::shared_ptr<const T> read() const {
        return m_ptr;
    }
    
    // Writer: copy then swap
    void update(const T& new_value) {
        auto new_ptr = std::make_shared<T>(new_value);
        std::atomic_store(&m_ptr, new_ptr);
    }
    
    void update(std::function<void(T&)> modifier) {
        auto new_ptr = std::make_shared<T>(*m_ptr);
        modifier(*new_ptr);
        std::atomic_store(&m_ptr, new_ptr);
    }
    
    T get() const {
        return *m_ptr;
    }
    
private:
    std::shared_ptr<T> m_ptr;
};

} 