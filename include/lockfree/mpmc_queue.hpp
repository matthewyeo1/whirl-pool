#pragma once
#include <atomic>
#include <optional>
#include <memory>
#include <vector>
#include <thread>
#include <stdexcept>
#include "utils.hpp"

namespace lockfree {

/**
 * Michael-Scott Lock-Free MPMC Queue with Hazard Pointers
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
        Node(T&& value) : data(std::move(value)), next(nullptr) {}
    };

    struct HazardPointer {
        std::atomic<std::thread::id> owner;
        std::atomic<Node*> pointer;
        HazardPointer() : owner(std::thread::id()), pointer(nullptr) {}
    };

    static constexpr int MAX_HAZARD_POINTERS = 128;
    static std::array<HazardPointer, MAX_HAZARD_POINTERS> h_ptrs;
    static thread_local std::vector<Node*> retired_nodes;
    static thread_local HazardPointer* local_hp;

    alignas(CACHE_LINE_SIZE) std::atomic<Node*> m_head;
    alignas(CACHE_LINE_SIZE) std::atomic<Node*> m_tail;

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

    static void release_hazard_pointer(HazardPointer* hp) {
        if (hp) {
            hp->pointer.store(nullptr, std::memory_order_release);
            hp->owner.store(std::thread::id(), std::memory_order_release);
            if (hp == local_hp) local_hp = nullptr;
        }
    }

    bool is_hazard(Node* node) {
        for (auto& hp : h_ptrs) {
            if (hp.pointer.load(std::memory_order_acquire) == node) return true;
        }
        return false;
    }

    void retire_node(Node* node) {
        if (!node) return;
        retired_nodes.push_back(node);
        if (retired_nodes.size() >= 100) scan_retired_nodes();
    }

    void scan_retired_nodes() {
        auto it = retired_nodes.begin();
        while (it != retired_nodes.end()) {
            if (!is_hazard(*it)) { 
                delete *it; it = retired_nodes.erase(it); 
            }
            else ++it;
        }
    }

public:
    MPMCQueue() {
        Node* node = new Node();
        m_head.store(node, std::memory_order_relaxed);
        m_tail.store(node, std::memory_order_relaxed);
    }

    ~MPMCQueue() {
        Node* node = m_head.load(std::memory_order_relaxed);
        while (node) {
            Node* next = node->next.load(std::memory_order_relaxed);
            delete node;
            node = next;
        }
        for (Node* n : retired_nodes) delete n;
        retired_nodes.clear();
    }

    MPMCQueue(const MPMCQueue&) = delete;
    MPMCQueue& operator=(const MPMCQueue&) = delete;

    void push(const T& value) {
        Node* node = new Node(value);
        while (true) {
            Node* tail = m_tail.load(std::memory_order_acquire);
            Node* next = tail->next.load(std::memory_order_acquire);
            if (tail != m_tail.load(std::memory_order_acquire)) continue;
            if (next != nullptr) {
                m_tail.compare_exchange_weak(tail, next,
                    std::memory_order_release, std::memory_order_relaxed);
                continue;
            }
            if (tail->next.compare_exchange_weak(next, node,
                std::memory_order_release, std::memory_order_relaxed)) {
                m_tail.compare_exchange_strong(tail, node,
                    std::memory_order_release, std::memory_order_relaxed);
                return;
            }
        }
    }

    std::optional<T> pop() {
        // Acquire two distinct hazard pointer slots
        HazardPointer* hp1 = get_hazard_pointer();

        HazardPointer* hp2 = nullptr;
        auto id = std::this_thread::get_id();
        for (auto& hp : h_ptrs) {
            if (&hp == hp1) continue;
            std::thread::id expected;
            if (hp.owner.compare_exchange_strong(expected, id)) {
                hp2 = &hp;
                break;
            }
        }
        if (!hp2) throw std::runtime_error("No second hazard pointer slot");

        auto cleanup = [&]() {
            hp1->pointer.store(nullptr, std::memory_order_release);
            hp2->pointer.store(nullptr, std::memory_order_release);
            // release hp2 (not cached), leave hp1 as thread's cached slot
            hp2->owner.store(std::thread::id(), std::memory_order_release);
        };

        while (true) {
            Node* head = m_head.load(std::memory_order_acquire);

            hp1->pointer.store(head, std::memory_order_release);

            if (head != m_head.load(std::memory_order_acquire)) continue;

            Node* next = head->next.load(std::memory_order_acquire);
            if (next == nullptr) {
                cleanup();
                return std::nullopt;
            }

            hp2->pointer.store(next, std::memory_order_release);
            // Validate both head and next are still consistent
            if (head != m_head.load(std::memory_order_acquire) ||
                next != head->next.load(std::memory_order_acquire)) continue;

            if (m_head.compare_exchange_weak(head, next,
                std::memory_order_release, std::memory_order_relaxed)) {
                T value = std::move(next->data);
                retire_node(head);
                cleanup();
                return value;
            }
        }
    }

    // Check if empty
    bool empty() const {
        Node* head = m_head.load(std::memory_order_acquire);
        Node* next = head->next.load(std::memory_order_acquire);
        return next == nullptr;
    }
};

template<typename T>
std::array<typename MPMCQueue<T>::HazardPointer, MPMCQueue<T>::MAX_HAZARD_POINTERS>
    MPMCQueue<T>::h_ptrs;

template<typename T>
thread_local std::vector<typename MPMCQueue<T>::Node*> MPMCQueue<T>::retired_nodes;

template<typename T>
thread_local typename MPMCQueue<T>::HazardPointer* MPMCQueue<T>::local_hp = nullptr;
} 