#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include "utils.hpp"

namespace lockfree {

/**
 * Fixed-capacity concurrent hash map.
 *
 * Entries are immutable and are published with the C++ atomic shared_ptr
 * operations.  A null slot is empty; m_tombstone is a deleted slot.  Keeping
 * tombstones is required by open addressing: a lookup may only stop at a null
 * slot, otherwise deletion could hide a colliding key farther down the probe
 * sequence.
 *
 * This class is thread-safe for insert, find, erase, and contains.  It is not
 * lock-free: atomic shared_ptr operations may use an internal lock.  In return
 * it safely supports non-trivial Key and Value types, including std::string.
 * The map must outlive all operations on it.
 */
template<typename Key, typename Value, std::size_t Capacity = 1048576>
class HashMap {
    static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a non-zero power of two");

private:
    struct Entry {
        Entry() = default;

        template<typename K, typename V>
        Entry(K&& entry_key, V&& entry_value)
            : key(std::forward<K>(entry_key)),
              value(std::forward<V>(entry_value)) {}

        std::optional<Key> key;
        std::optional<Value> value;
    };

    using entry_ptr = std::shared_ptr<const Entry>;

    // Avoid inflating every slot to a full cache line.
    static constexpr std::size_t slots_per_group = 8;
    static constexpr std::size_t group_count =
        (Capacity + slots_per_group - 1) / slots_per_group;

    struct alignas(CACHE_LINE_SIZE) SlotGroup {
        entry_ptr slots[slots_per_group];
    };

public:
    HashMap()
        : m_slots(std::make_unique<SlotGroup[]>(group_count)),
          m_tombstone(std::make_shared<Entry>()),
          m_size(0) {}

    HashMap(const HashMap&) = delete;
    HashMap& operator=(const HashMap&) = delete;

    bool insert(const Key& key, const Value& value) {
        const auto desired = std::make_shared<Entry>(key, value);

        // Restarting after a failed CAS ensures an update never overwrites a newer value and that concurrent same-key inserts cannot duplicate it.
        for (;;) {
            std::optional<std::size_t> first_tombstone;
            std::size_t index = hash(key);
            std::size_t step = 1;
            bool retry = false;

            for (std::size_t probes = 0; probes < Capacity; ++probes) {
                entry_ptr observed = load_slot(index);

                if (!observed) {
                    const std::size_t target = first_tombstone.value_or(index);
                    entry_ptr expected = first_tombstone ? m_tombstone : entry_ptr{};
                    if (compare_exchange_slot(target, expected, desired)) {
                        m_size.fetch_add(1, std::memory_order_relaxed);
                        return true;
                    }
                    retry = true;
                    break;
                }

                if (observed == m_tombstone) {
                    if (!first_tombstone) {
                        first_tombstone = index;
                    }
                } else if (*observed->key == key) {
                    entry_ptr expected = std::move(observed);
                    if (compare_exchange_slot(index, expected, desired)) {
                        return true;
                    }
                    retry = true;
                    break;
                }

                index = next_index(index, step++);
            }

            if (retry) {
                continue;
            }
            if (!first_tombstone) {
                return false;
            }

            entry_ptr expected = m_tombstone;
            if (compare_exchange_slot(*first_tombstone, expected, desired)) {
                m_size.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
        }
    }

    std::optional<Value> find(const Key& key) const {
        std::size_t index = hash(key);
        std::size_t step = 1;

        for (std::size_t probes = 0; probes < Capacity; ++probes) {
            const entry_ptr observed = load_slot(index);
            if (!observed) {
                return std::nullopt;
            }
            if (observed != m_tombstone && *observed->key == key) {
                return *observed->value;
            }
            index = next_index(index, step++);
        }
        return std::nullopt;
    }

    bool erase(const Key& key) {
        for (;;) {
            std::size_t index = hash(key);
            std::size_t step = 1;

            for (std::size_t probes = 0; probes < Capacity; ++probes) {
                entry_ptr observed = load_slot(index);
                if (!observed) {
                    return false;
                }
                if (observed != m_tombstone && *observed->key == key) {
                    entry_ptr expected = std::move(observed);
                    if (compare_exchange_slot(index, expected, m_tombstone)) {
                        m_size.fetch_sub(1, std::memory_order_relaxed);
                        return true;
                    }
                    break;
                }
                index = next_index(index, step++);
            }

            // We reached a real empty slot or lost a race.  Retrying handles
            // the latter; a complete probe without a matching key is absent.
            if (step > Capacity) {
                return false;
            }
        }
    }

    bool contains(const Key& key) const {
        return find(key).has_value();
    }

    // Concurrent operations may make this a transient snapshot.
    std::size_t size() const noexcept {
        return m_size.load(std::memory_order_relaxed);
    }

    bool empty() const noexcept {
        return size() == 0;
    }

    constexpr std::size_t capacity() const noexcept {
        return Capacity;
    }

private:
    std::size_t hash(const Key& key) const {
        return std::hash<Key>{}(key) & (Capacity - 1);
    }

    static std::size_t next_index(std::size_t index, std::size_t step) noexcept {
        return (index + step) & (Capacity - 1);
    }

    entry_ptr& slot(std::size_t index) noexcept {
        return m_slots[index / slots_per_group].slots[index % slots_per_group];
    }

    const entry_ptr& slot(std::size_t index) const noexcept {
        return m_slots[index / slots_per_group].slots[index % slots_per_group];
    }

    entry_ptr load_slot(std::size_t index) const {
        return std::atomic_load_explicit(&slot(index), std::memory_order_acquire);
    }

    bool compare_exchange_slot(std::size_t index, entry_ptr& expected,
                               const entry_ptr& desired) {
        return std::atomic_compare_exchange_strong_explicit(
            &slot(index), &expected, desired,
            std::memory_order_release, std::memory_order_acquire);
    }

    std::unique_ptr<SlotGroup[]> m_slots;
    const entry_ptr m_tombstone;
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> m_size;
};

} // namespace lockfree
