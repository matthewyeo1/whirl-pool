#pragma once
#include <atomic>
#include <optional>
#include "utils.hpp"

namespace lockfree {

/**
 * Treiber Stack - Lock-free LIFO stack
 * 
 * Multi-producer multi-consumer lock-free stack using CAS.
 * 
 * Usage:
 *   TreiberStack<int> stack;
 *   stack.push(42);
 *   auto value = stack.pop();  // std::optional<int>
 * 
 * Reference: Treiber, 1986
 */
template<typename T>
class TStack {
private:
    struct Node {
        T data;
        std::atomic<Node*> next;

        Node(const T& value) : data(value), next(nullptr) {}
    };

    alignas(CACHE_LINE_SIZE) std::atomic<Node*> m_head;

public:
    TStack() : m_head(nullptr) {}

    ~TStack() {
        Node* node = m_head.load(std::memory_order_relaxed);

        while (node) {
            Node* next = node->next.load(std::memory_order_relaxed);
            delete node;
            node = next;
        }
    }

    // Non-copyable
    TStack(const TStack&) = delete;
    TStack& operator=(const TStack&) = delete;

    // Thread-safe push
    void push(const T& value) {
        Node* new_node = new Node(value);
        Node* old_head = m_head.load(std::memory_order_relaxed);

        do {
            new_node->next.store(old_head, std::memory_order_relaxed);
        } while (!m_head.compare_exchange_weak(old_head, new_node, 
            std::memory_order_release, std::memory_order_relaxed));
    }

    // Thread-safe pop
    std::optional<T> pop() {
        Node* old_head = m_head.load(std::memory_order_acquire);

        while (old_head) {
            Node* next = old_head->next.load(std::memory_order_relaxed);

            if (m_head.compare_exchange_weak(old_head, next,
                std::memory_order_release, std::memory_order_relaxed)) {
                    T value = std::move(old_head->data);
                    delete old_head;
                    return value;
            }
        }

        return std::nullopt;
    }

    // Check if empty
    bool empty() const {
        return m_head.load(std::memory_order_acquire) == nullptr;
    }

};

}