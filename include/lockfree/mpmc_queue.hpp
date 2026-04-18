#pragma once
#include <atomic>
#include <optional>
#include <memory>
#include "utils.hpp"

namespace lockfree {

/**
 * Michael-Scott Lock-Free MPMC Queue
 * 
 * Multi-Producer Multi-Consumer queue using atomic compare-and-swap.
 * 
 * Usage:
 *   MPMCQueue<int> queue;
 *   queue.push(42);
 *   auto value = queue.pop();  // std::optional<int>
 * 
 * Reference: Michael & Scott, PODC 1996
 */
template<typename T>
class MPMCQueue {
private:
    struct Node {
        T data;
        std::atomic<Node*> next;

        Node() : next(nullptr) {}
        Node(const T& value) : data(value), next(nullptr) {}
    };

    alignas(CACHE_LINE_SIZE) std::atomic<Node*> m_head;
    alignas(CACHE_LINE_SIZE) std::atomic<Node*> m_tail;

public:
    MPMCQueue() {
        Node* node = new Node();
        m_head.store(node, std::memory_order_relaxed);
        m_tail.store(node, std::memory_order_relaxed);
    }

    ~MPMCQueue() {
        Node* node = m_head.load();

        while (node) {
            Node* next = node->next.load(std::memory_order_relaxed);
            delete node;
            node = next;
        }
    }

    // Non-copyable
    MPMCQueue(const MPMCQueue&) = delete;
    MPMCQueue& operator=(const MPMCQueue&) = delete;

    // Thread-safe push to queue
    void push(const T& value) {
        Node* node = new Node(value);
        
        while (true) {
            Node* tail = m_tail.load(std::memory_order_acquire);
            Node* next = tail->next.load(std::memory_order_acquire);
            
            if (tail != m_tail.load(std::memory_order_acquire)) {
                continue;  // Tail changed, retry
            }
            
            if (next != nullptr) {
                // Tail is not pointing to last node, advance it
                m_tail.compare_exchange_weak(tail, next, 
                    std::memory_order_release, std::memory_order_relaxed);
                continue;
            }
            
            // Try to link new node at the end
            if (tail->next.compare_exchange_weak(next, node,
                std::memory_order_release, std::memory_order_relaxed)) {
                break;  // Success
            }
        }
        
        // Advance tail to new node
        Node* tail = m_tail.load();
        m_tail.compare_exchange_strong(tail, node,
            std::memory_order_release, std::memory_order_relaxed);
    }

    // Thread-safe pop from queue
    std::optional<T> pop() {
         while (true) {
            Node* head = m_head.load(std::memory_order_acquire);
            Node* tail = m_tail.load(std::memory_order_acquire);
            Node* next = head->next.load(std::memory_order_acquire);
            
            if (head != m_head.load(std::memory_order_acquire)) {
                continue;  // Head changed, retry
            }
            
            if (head == tail) {
                if (next == nullptr) {
                    return std::nullopt;  // Queue empty
                }
                // Tail is behind, advance it
                m_tail.compare_exchange_weak(tail, next,
                    std::memory_order_release, std::memory_order_relaxed);
                continue;
            }
            
            // Read value before CAS (to avoid ABA issues)
            T value = next->data;
            
            // Try to move head forward
            if (m_head.compare_exchange_weak(head, next,
                std::memory_order_release, std::memory_order_relaxed)) {
                // Delete old head (optional, could be deferred)
                delete head;
                return value;
            }
        }
    }

    // Check if queue is empty (not thread-safe)
    bool empty() const {
        Node* head = m_head.load(std::memory_order_acquire);
        Node* tail = m_tail.load(std::memory_order_acquire);
        Node* next = head->next.load(std::memory_order_acquire);
        return (head == tail) && (next == nullptr);
    }
};

}
