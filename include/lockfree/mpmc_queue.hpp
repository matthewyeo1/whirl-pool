#pragma once

#include <array>
#include <atomic>
#include <memory_resource>
#include <new>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

#include "utils.hpp"

namespace lockfree {

/**
 * Michael-Scott lock-free MPMC queue.
 *
 * Nodes removed from the queue are reclaimed with hazard pointers.  Every
 * operation that dereferences a shared node first publishes that node in a
 * hazard pointer and validates the source pointer afterwards.
 *
 * The queue must not be destroyed while another thread is using it.
 */
template<typename T>
class MPMCQueue {
private:
    struct Node {
        std::optional<T> data;
        std::atomic<Node*> next{nullptr};
        std::atomic<Node*> retired_next{nullptr};

        Node() = default;

        template<typename U>
        explicit Node(U&& value) : data(std::forward<U>(value)) {}
    };

    struct HazardPointer {
        std::atomic<std::thread::id> owner{};
        std::atomic<Node*> pointer{nullptr};
    };

    static constexpr size_t MAX_HAZARD_POINTERS = 256;
    static constexpr size_t RETIRE_SCAN_THRESHOLD = 100;

    class HazardPointerGuard {
    public:
        explicit HazardPointerGuard(MPMCQueue& queue) : m_queue(queue) {
            const std::thread::id id = std::this_thread::get_id();
            for (auto& hp : m_queue.m_h_ptrs) {
                std::thread::id expected;
                if (hp.owner.compare_exchange_strong(expected, id,
                        std::memory_order_seq_cst, std::memory_order_relaxed)) {
                    m_hp = &hp;
                    return;
                }
            }
            throw std::runtime_error("No hazard pointer slot available");
        }

        HazardPointerGuard(const HazardPointerGuard&) = delete;
        HazardPointerGuard& operator=(const HazardPointerGuard&) = delete;

        ~HazardPointerGuard() {
            if (m_hp) {
                m_hp->pointer.store(nullptr, std::memory_order_seq_cst);
                m_hp->owner.store(std::thread::id(), std::memory_order_seq_cst);
            }
        }

        Node* protect(const std::atomic<Node*>& source) {
            Node* node;
            do {
                node = source.load(std::memory_order_seq_cst);
                m_hp->pointer.store(node, std::memory_order_seq_cst);
            } while (node != source.load(std::memory_order_seq_cst));
            return node;
        }

    private:
        MPMCQueue& m_queue;
        HazardPointer* m_hp{nullptr};
    };

    alignas(CACHE_LINE_SIZE) std::atomic<Node*> m_head;
    alignas(CACHE_LINE_SIZE) std::atomic<Node*> m_tail;
    std::pmr::memory_resource* const m_memory_resource;
    mutable std::array<HazardPointer, MAX_HAZARD_POINTERS> m_h_ptrs{};
    std::atomic<Node*> m_retired{nullptr};
    std::atomic<size_t> m_retired_count{0};

    bool is_hazard(Node* node) const {
        // m_tail is allowed to lag after an enqueue.  Do not reclaim its node
        // until it advances, even when no thread has published a hazard yet.
        if (m_tail.load(std::memory_order_seq_cst) == node) {
            return true;
        }
        for (const auto& hp : m_h_ptrs) {
            if (hp.pointer.load(std::memory_order_seq_cst) == node) {
                return true;
            }
        }
        return false;
    }

    void add_to_retired(Node* node, bool count_node) {
        Node* retired = m_retired.load(std::memory_order_relaxed);
        do {
            node->retired_next.store(retired, std::memory_order_relaxed);
        } while (!m_retired.compare_exchange_weak(retired, node,
            std::memory_order_release, std::memory_order_relaxed));

        if (count_node) {
            m_retired_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void scan_retired_nodes() {
        Node* node = m_retired.exchange(nullptr, std::memory_order_acq_rel);
        while (node) {
            Node* next = node->retired_next.load(std::memory_order_relaxed);
            if (is_hazard(node)) {
                add_to_retired(node, false);
            } else {
                destroy_node(node);
                m_retired_count.fetch_sub(1, std::memory_order_relaxed);
            }
            node = next;
        }
    }

    void retire_node(Node* node) {
        add_to_retired(node, true);
        if (m_retired_count.load(std::memory_order_relaxed) >= RETIRE_SCAN_THRESHOLD) {
            scan_retired_nodes();
        }
    }

    template<typename... Args>
    Node* make_node(Args&&... args) {
        void* memory = m_memory_resource->allocate(sizeof(Node), alignof(Node));
        try {
            return ::new (memory) Node(std::forward<Args>(args)...);
        } catch (...) {
            m_memory_resource->deallocate(memory, sizeof(Node), alignof(Node));
            throw;
        }
    }

    void destroy_node(Node* node) noexcept {
        node->~Node();
        m_memory_resource->deallocate(node, sizeof(Node), alignof(Node));
    }

public:
    explicit MPMCQueue(std::pmr::memory_resource* memory_resource =
            std::pmr::get_default_resource())
        : m_memory_resource(memory_resource ? memory_resource :
            std::pmr::get_default_resource()) {
        Node* dummy = make_node();
        m_head.store(dummy, std::memory_order_relaxed);
        m_tail.store(dummy, std::memory_order_relaxed);
    }

    ~MPMCQueue() {
        // The destructor requires all queue users to have stopped.
        Node* node = m_head.load(std::memory_order_relaxed);
        while (node) {
            Node* next = node->next.load(std::memory_order_relaxed);
            destroy_node(node);
            node = next;
        }

        node = m_retired.exchange(nullptr, std::memory_order_relaxed);
        while (node) {
            Node* next = node->retired_next.load(std::memory_order_relaxed);
            destroy_node(node);
            node = next;
        }
    }

    MPMCQueue(const MPMCQueue&) = delete;
    MPMCQueue& operator=(const MPMCQueue&) = delete;

    void push(const T& value) {
        push_impl(value);
    }

    void push(T&& value) {
        push_impl(std::move(value));
    }

    template<typename... Args>
    void emplace(Args&&... args) {
        Node* node = make_node(T(std::forward<Args>(args)...));
        link_node(node);
    }

    std::optional<T> pop() {
        HazardPointerGuard head_guard(*this);
        HazardPointerGuard next_guard(*this);

        while (true) {
            Node* head = head_guard.protect(m_head);
            Node* next = next_guard.protect(head->next);

            if (head != m_head.load(std::memory_order_seq_cst)) {
                continue;
            }
            if (next == nullptr) {
                return std::nullopt;
            }

            if (m_head.compare_exchange_weak(head, next,
                    std::memory_order_seq_cst, std::memory_order_seq_cst)) {
                T value = std::move(*next->data);
                retire_node(head);
                return value;
            }
        }
    }

    bool empty() const {
        HazardPointerGuard head_guard(const_cast<MPMCQueue&>(*this));
        Node* head = head_guard.protect(m_head);
        return head->next.load(std::memory_order_acquire) == nullptr;
    }

private:
    template<typename U>
    void push_impl(U&& value) {
        Node* node = make_node(std::forward<U>(value));
        link_node(node);
    }

    void link_node(Node* node) {
        HazardPointerGuard tail_guard(*this);

        while (true) {
            Node* tail = tail_guard.protect(m_tail);
            Node* next = tail->next.load(std::memory_order_acquire);

            if (tail != m_tail.load(std::memory_order_seq_cst)) {
                continue;
            }
            if (next != nullptr) {
                m_tail.compare_exchange_weak(tail, next,
                    std::memory_order_seq_cst, std::memory_order_seq_cst);
                continue;
            }
            if (tail->next.compare_exchange_weak(next, node,
                    std::memory_order_seq_cst, std::memory_order_seq_cst)) {
                m_tail.compare_exchange_strong(tail, node,
                    std::memory_order_seq_cst, std::memory_order_seq_cst);
                return;
            }
        }
    }
};

} // namespace lockfree
