#pragma once
#include <atomic>
#include <optional>
#include <array>
#include <thread>
#include <stdexcept>
#include <vector>
#include "utils.hpp"

namespace lockfree {

/**
 * Treiber Stack - Lock-free LIFO stack with Hazard Pointers
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
        Node(T&& value) : data(std::move(value)), next(nullptr) {}
    };

    struct HazardPointer {
        std::atomic<std::thread::id> owner;
        std::atomic<Node*> pointer;
        
        HazardPointer() : owner(std::thread::id()), pointer(nullptr) {}
    };

    static constexpr int MAX_HAZARD_POINTERS = 128;
    static std::array<HazardPointer, MAX_HAZARD_POINTERS> h_ptrs;
    
    // Retired nodes list for deferred deletion
    struct RetiredNode {
        Node* node;
        RetiredNode* next;
    };
    
    static thread_local std::vector<Node*> retired_nodes;
    static thread_local HazardPointer* local_hp;

    alignas(CACHE_LINE_SIZE) std::atomic<Node*> m_head;

    // Get this thread's hazard pointer slot
    static HazardPointer* get_hazard_pointer() {
        if (local_hp) return local_hp;
        
        auto id = std::this_thread::get_id();
        for (auto& hp : h_ptrs) {
            std::thread::id expected;
            if (hp.owner.compare_exchange_strong(expected, id)) {
                local_hp = &hp;
                return &hp;
            }
        }
        throw std::runtime_error("No hazard pointer slot available");
    }
    
    // Release hazard pointer slot
    static void release_hazard_pointer(HazardPointer* hp) {
        if (hp) {
            hp->pointer.store(nullptr, std::memory_order_release);
            hp->owner.store(std::thread::id(), std::memory_order_release);
            local_hp = nullptr;
        }
    }

    bool is_hazard(Node* node) {
        for (auto& hp : h_ptrs) {
            Node* protected_node = hp.pointer.load(std::memory_order_acquire);
            if (protected_node == node) {
                return true;
            }
        }
        return false;
    }
    
    void retire_node(Node* node) {
        // Add to thread-local retired list
        retired_nodes.push_back(node);
        
        // Batch deletion when list gets large
        if (retired_nodes.size() >= 100) {
            scan_retired_nodes();
        }
    }
    
    void scan_retired_nodes() {
        auto it = retired_nodes.begin();
        while (it != retired_nodes.end()) {
            if (!is_hazard(*it)) {
                delete *it;
                it = retired_nodes.erase(it);
            } else {
                ++it;
            }
        }
    }

public:
    TStack() : m_head(nullptr) {}

    ~TStack() {
        // Clean up all nodes in the stack
        Node* node = m_head.load(std::memory_order_relaxed);
        while (node) {
            Node* next = node->next.load(std::memory_order_relaxed);
            delete node;
            node = next;
        }
        
        // Clean up any retired nodes
        for (Node* node : retired_nodes) {
            delete node;
        }
        retired_nodes.clear();
        
        // Release hazard pointer slots
        for (auto& hp : h_ptrs) {
            std::thread::id expected = std::this_thread::get_id();
            std::thread::id current = hp.owner.load();
            if (current == expected) {
                hp.owner.store(std::thread::id(), std::memory_order_release);
            }
        }
    }

    // Non-copyable
    TStack(const TStack&) = delete;
    TStack& operator=(const TStack&) = delete;
    
    TStack(TStack&& other) noexcept : m_head(other.m_head.load()) {
        other.m_head.store(nullptr);
    }
    
    TStack& operator=(TStack&& other) noexcept {
        if (this != &other) {
            m_head.store(other.m_head.load());
            other.m_head.store(nullptr);
        }
        return *this;
    }

    // Thread-safe push
    void push(const T& value) {
        Node* new_node = new Node(value);
        Node* old_head = m_head.load(std::memory_order_relaxed);

        do {
            new_node->next.store(old_head, std::memory_order_relaxed);
        } while (!m_head.compare_exchange_weak(old_head, new_node, 
            std::memory_order_release, std::memory_order_relaxed));
    }
    
    // Thread-safe push with move semantics
    void push(T&& value) {
        Node* new_node = new Node(std::move(value));
        Node* old_head = m_head.load(std::memory_order_relaxed);

        do {
            new_node->next.store(old_head, std::memory_order_relaxed);
        } while (!m_head.compare_exchange_weak(old_head, new_node, 
            std::memory_order_release, std::memory_order_relaxed));
    }

    // Thread-safe pop
    std::optional<T> pop() {
        auto* h_ptr = get_hazard_pointer();

        while (true) {
            Node* old_head = m_head.load(std::memory_order_acquire);

            if (!old_head) {
                h_ptr->pointer.store(nullptr, std::memory_order_release);
                return std::nullopt;
            }

            // Set hazard pointer before reading next
            h_ptr->pointer.store(old_head, std::memory_order_release);

            // Double-check head hasn't changed
            if (m_head.load(std::memory_order_acquire) != old_head) {
                continue;  
            }

            Node* next = old_head->next.load(std::memory_order_relaxed);

            if (m_head.compare_exchange_weak(old_head, next,
                std::memory_order_release, std::memory_order_relaxed)) {
                    
                // Successfully popped the node
                T value = std::move(old_head->data);
                
                // Retire the node 
                retire_node(old_head);
                
                h_ptr->pointer.store(nullptr, std::memory_order_release);
                return value;
            }
        }
    }

    // Check if empty
    bool empty() const {
        return m_head.load(std::memory_order_acquire) == nullptr;
    }
    
    // Get approximate size (not accurate for concurrent use)
    size_t size() const {
        size_t count = 0;
        Node* node = m_head.load(std::memory_order_acquire);
        while (node) {
            count++;
            node = node->next.load(std::memory_order_relaxed);
        }
        return count;
    }
};

template<typename T>
std::array<typename TStack<T>::HazardPointer, TStack<T>::MAX_HAZARD_POINTERS> 
    TStack<T>::h_ptrs;

template<typename T>
thread_local std::vector<typename TStack<T>::Node*> TStack<T>::retired_nodes;

template<typename T>
thread_local typename TStack<T>::HazardPointer* TStack<T>::local_hp = nullptr;

} 