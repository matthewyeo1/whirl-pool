#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <optional>
#include <atomic>
#include <cstring>
#include "lockfree.h"

// ============ TEST FRAMEWORK ============
struct TestCase {
    std::string name;
    std::function<bool()> func;
};

struct TestObject {
    int value;
    TestObject() : value(67) {}
    explicit TestObject(int v) : value(v) {}
};

std::vector<TestCase>& get_tests() {
    static std::vector<TestCase> tests;
    return tests;
}

#define TEST_CASE(name) \
    bool name(); \
    static bool name##_registered = []() { \
        get_tests().push_back({#name, name}); \
        return true; \
    }(); \
    bool name()

#define ASSERT(expr) \
    if (!(expr)) { \
        std::cerr << "  FAIL: " << #expr << " line " << __LINE__ << std::endl; \
        return false; \
    }

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { \
        std::cerr << "  FAIL: " << #a << " (" << a << ") != " << #b << " (" << b << ") line " << __LINE__ << std::endl; \
        return false; \
    }

// After each MPMC test, reset the hazard pointers
#define MPMC_TEST_CASE(name) \
    TEST_CASE(name) { \
        bool result = false; \
        { \
            lockfree::MPMCQueue<int>::reset_for_testing(); \
            result = name(); \
        } \
        lockfree::MPMCQueue<int>::reset_for_testing(); \
        return result; \
    }

// After each stack test, reset the hazard pointers (test-only)
#define STACK_TEST_CASE(name) \
    TEST_CASE(name) { \
        bool result = false; \
        { \
            lockfree::TStack<int>::reset_for_testing(); \
            result = name(); \
        } \
        lockfree::TStack<int>::reset_for_testing(); \
        return result; \
    }

// ============ TESTS ============
TEST_CASE(test_pool_basic) {
    lockfree::ObjectPool<TestObject, 100> pool;

    auto obj = pool.acquire();
    ASSERT(obj);
    ASSERT_EQ(obj->value, 67);
    ASSERT_EQ(pool.used_count(), 1);
    ASSERT_EQ(pool.free_count(), 99);

    return true;
}

TEST_CASE(test_pool_multiple) {
    lockfree::ObjectPool<TestObject, 10> pool;
    
    std::vector<decltype(pool.acquire())> objects;
    for (int i = 0; i < 10; i++) {
        auto obj = pool.acquire();
        ASSERT(obj);
        objects.push_back(std::move(obj));
    }
    
    // Pool should be empty
    auto empty = pool.acquire();
    ASSERT(!empty);
    ASSERT_EQ(pool.used_count(), 10);
    ASSERT_EQ(pool.free_count(), 0);
    
    // Release one
    objects.pop_back();
    ASSERT_EQ(pool.used_count(), 9);
    ASSERT_EQ(pool.free_count(), 1);
    
    // Acquire should work again
    auto new_obj = pool.acquire();
    ASSERT(new_obj);
    
    return true;
}

TEST_CASE(test_pool_auto_release) {
    lockfree::ObjectPool<TestObject, 10> pool;
    
    {
        auto obj = pool.acquire();
        ASSERT(obj);
        ASSERT_EQ(pool.used_count(), 1);
        // obj goes out of scope here
    }
    
    ASSERT_EQ(pool.used_count(), 0);
    ASSERT_EQ(pool.free_count(), 10);
    
    return true;
}

TEST_CASE(test_pool_move_semantics) {
    lockfree::ObjectPool<TestObject, 10> pool;
    
    auto obj1 = pool.acquire();
    ASSERT(obj1);
    ASSERT_EQ(pool.used_count(), 1);
    
    auto obj2 = std::move(obj1);
    ASSERT(!obj1);  // obj1 is now empty
    ASSERT(obj2);   // obj2 has the object
    ASSERT_EQ(pool.used_count(), 1);  // Still only 1 object in use
    
    return true;
}

TEST_CASE(test_spsc_basic) {
    lockfree::SPSCQueue<int, 16> queue;
    
    ASSERT(queue.push(99));
    ASSERT(queue.push(100));
    
    auto val = queue.pop();
    ASSERT(val.has_value());
    ASSERT_EQ(*val, 99);
    
    val = queue.pop();
    ASSERT(val.has_value());
    ASSERT_EQ(*val, 100);
    
    ASSERT(!queue.pop().has_value());
    return true;
}

TEST_CASE(test_spsc_full) {
    lockfree::SPSCQueue<int, 8> queue; 
    
    for (int i = 0; i < 7; i++) {
        ASSERT(queue.push(i));
    }
    
    ASSERT(!queue.push(999));
    
    auto val = queue.pop();
    ASSERT(val.has_value());
    ASSERT_EQ(*val, 0); 
    
    ASSERT(queue.push(999));  
    
    return true;
}

TEST_CASE(test_spsc_size) {
    lockfree::SPSCQueue<int, 8> queue;
    
    ASSERT_EQ(queue.size(), 0);
    queue.push(1);
    ASSERT_EQ(queue.size(), 1);
    queue.push(2);
    ASSERT_EQ(queue.size(), 2);
    queue.pop();
    ASSERT_EQ(queue.size(), 1);
    queue.pop();
    ASSERT_EQ(queue.size(), 0);
    
    return true;
}

TEST_CASE(test_spsc_threaded) {
    lockfree::SPSCQueue<int, 1024> queue;
    const int NUM_ITEMS = 10000;
    std::atomic<int> received{0};
    
    std::thread producer([&]() {
        for (int i = 0; i < NUM_ITEMS; i++) {
            while (!queue.push(i)) {}
        }
    });
    
    std::thread consumer([&]() {
        while (received < NUM_ITEMS) {
            auto val = queue.pop();
            if (val.has_value()) {
                received++;
            }
        }
        return;
    });
    
    producer.join();
    consumer.join();
    
    ASSERT_EQ(received, NUM_ITEMS);
    return true;
}


TEST_CASE(test_mpmc_basic) {
    lockfree::MPMCQueue<int>::reset_for_testing();
    lockfree::MPMCQueue<int> queue;
    
    queue.push(67);
    queue.push(100);
    
    auto val = queue.pop();
    ASSERT(val.has_value());
    ASSERT_EQ(*val, 67);
    
    val = queue.pop();
    ASSERT(val.has_value());
    ASSERT_EQ(*val, 100);
    
    ASSERT(!queue.pop().has_value());
    
    return true;
}

TEST_CASE(test_mpmc_multi_producer_multi_consumer) {
    lockfree::MPMCQueue<int>::reset_for_testing();
    lockfree::MPMCQueue<int> queue;
    const int NUM_PRODUCERS = 4;
    const int NUM_CONSUMERS = 4;
    const int ITEMS_PER_PRODUCER = 5000;
    const int TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;
    
    std::atomic<int> consumed{0};
    std::atomic<int> producers_finished{0};
    
    // Producers
    std::vector<std::thread> producers;
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        producers.emplace_back([&]() {
            for (int j = 0; j < ITEMS_PER_PRODUCER; j++) {
                queue.push(j);
            }
            producers_finished++;
        });
    }
    
    // Consumers
    std::vector<std::thread> consumers;
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        consumers.emplace_back([&]() {
            while (consumed.load(std::memory_order_acquire) < TOTAL_ITEMS) {
                auto val = queue.pop();
                if (val.has_value()) {
                    consumed.fetch_add(1, std::memory_order_relaxed);
                } else if (producers_finished.load(std::memory_order_acquire) == NUM_PRODUCERS) {
                    // No items and all producers done - check one more time
                    if (consumed.load(std::memory_order_acquire) >= TOTAL_ITEMS) break;
                }
            }
        });
    }
    
    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();
    
    ASSERT_EQ(consumed.load(), TOTAL_ITEMS);
    return true;
}

TEST_CASE(test_mpmc_single_producer_single_consumer) {
    lockfree::MPMCQueue<int>::reset_for_testing();
    lockfree::MPMCQueue<int> queue;
    const int NUM_ITEMS = 50000;
    
    std::thread producer([&]() {
        for (int i = 0; i < NUM_ITEMS; i++) {
            queue.push(i);
        }
    });
    
    std::thread consumer([&]() {
        int received = 0;
        while (received < NUM_ITEMS) {
            auto val = queue.pop();
            if (val.has_value()) {
                received++;
            }
        }
        ASSERT_EQ(received, NUM_ITEMS);
    });
    
    producer.join();
    consumer.join();
    
    return true;
}

TEST_CASE(test_mpmc_multi_producer_single_consumer) {
    lockfree::MPMCQueue<int>::reset_for_testing();
    lockfree::MPMCQueue<int> queue;
    const int NUM_PRODUCERS = 4;
    const int ITEMS_PER_PRODUCER = 10000;
    const int TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;
    std::atomic<int> consumed{0};
    std::atomic<bool> producers_done{false};  // ← Add this
    
    std::vector<std::thread> producers;
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        producers.emplace_back([&]() {
            for (int j = 0; j < ITEMS_PER_PRODUCER; j++) {
                queue.push(j);
            }
        });
    }
    
    std::thread consumer([&]() {
        while (true) {
            auto val = queue.pop();
            if (val.has_value()) {
                consumed++;
            }
            
            // Exit condition: all producers done AND consumed all items
            if (producers_done.load() && consumed.load() >= TOTAL_ITEMS) {
                break;
            }
            
            // Small backoff to prevent CPU spinning when empty
            if (!val.has_value()) {
                std::this_thread::yield();
            }
        }
    });
    
    for (auto& t : producers) t.join();
    producers_done = true;  // ← Signal consumers
    
    consumer.join();
    
    ASSERT_EQ(consumed.load(), TOTAL_ITEMS);
    return true;
}

TEST_CASE(test_mpmc_empty) {
    lockfree::MPMCQueue<int> queue;
    ASSERT(queue.empty());
    
    queue.push(1);
    ASSERT(!queue.empty());
    
    queue.pop();
    ASSERT(queue.empty());
    
    return true;
}

TEST_CASE(test_stack_basic) {
    // Reset hazard pointers for test isolation
    lockfree::TStack<int>::reset_for_testing();
    lockfree::TStack<int> stack;
    
    stack.push(42);
    stack.push(100);
    
    auto val = stack.pop();
    ASSERT(val.has_value());
    ASSERT_EQ(*val, 100);  
    
    val = stack.pop();
    ASSERT(val.has_value());
    ASSERT_EQ(*val, 42);
    
    ASSERT(!stack.pop().has_value());
    // Cleanup
    lockfree::TStack<int>::reset_for_testing();
    return true;
}

TEST_CASE(test_stack_single_producer_single_consumer) {
    lockfree::TStack<int>::reset_for_testing();
    lockfree::TStack<int> stack;
    const int NUM_ITEMS = 50000;
    
    std::thread producer([&]() {
        for (int i = 0; i < NUM_ITEMS; i++) {
            stack.push(i);
        }
    });
    
    std::thread consumer([&]() {
        int received = 0;
        while (received < NUM_ITEMS) {
            auto val = stack.pop();
            if (val.has_value()) {
                received++;
            }
        }
        ASSERT_EQ(received, NUM_ITEMS);
    });
    
    producer.join();
    consumer.join();
    lockfree::TStack<int>::reset_for_testing();
    return true;
}

TEST_CASE(test_stack_multi_producer_multi_consumer) {
    lockfree::TStack<int>::reset_for_testing();
    lockfree::TStack<int> stack;
    const int NUM_PRODUCERS = 4;
    const int NUM_CONSUMERS = 4;
    const int ITEMS_PER_PRODUCER = 5000;
    const int TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;
    
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    
    std::vector<std::thread> producers;
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        producers.emplace_back([&]() {
            for (int j = 0; j < ITEMS_PER_PRODUCER; j++) {
                stack.push(j);
                produced++;
            }
        });
    }
    
    std::vector<std::thread> consumers;
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        consumers.emplace_back([&]() {
            while (consumed < TOTAL_ITEMS) {
                auto val = stack.pop();
                if (val.has_value()) {
                    consumed++;
                } else {
                    if (produced.load() < NUM_PRODUCERS) {
                        std::this_thread::yield();
                    }
                }
            }
        });
    }
    
    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();
    ASSERT_EQ(consumed, TOTAL_ITEMS);
    lockfree::TStack<int>::reset_for_testing();
    return true;
}

TEST_CASE(test_stack_empty) {
    lockfree::TStack<int>::reset_for_testing();
    lockfree::TStack<int> stack;
    ASSERT(stack.empty());
    
    stack.push(1);
    ASSERT(!stack.empty());
    
    stack.pop();
    ASSERT(stack.empty());
    lockfree::TStack<int>::reset_for_testing();
    return true;
}

TEST_CASE(test_ring_buffer_basic) {
    lockfree::RingBuffer<int, 8> buffer;
    
    ASSERT(buffer.empty());
    ASSERT(!buffer.full());
    
    ASSERT(buffer.push(42));
    ASSERT(!buffer.empty());
    ASSERT_EQ(buffer.size(), 1);
    
    auto val = buffer.pop();
    ASSERT(val.has_value());
    ASSERT_EQ(*val, 42);
    ASSERT(buffer.empty());
    
    return true;
}

TEST_CASE(test_ring_buffer_full) {
    lockfree::RingBuffer<int, 4> buffer;
    
    // Fill the buffer
    for (int i = 0; i < 4; i++) {
        ASSERT(buffer.push(i));
    }
    
    ASSERT(buffer.full());
    ASSERT(!buffer.push(999));  // Should fail
    
    // Empty it
    for (int i = 0; i < 4; i++) {
        auto val = buffer.pop();
        ASSERT(val.has_value());
        ASSERT_EQ(*val, i);
    }
    
    ASSERT(buffer.empty());
    
    return true;
}

TEST_CASE(test_ring_buffer_multi_producer_multi_consumer) {
    lockfree::RingBuffer<int, 65536> buffer;
    const int NUM_PRODUCERS = 4;
    const int NUM_CONSUMERS = 4;
    const int ITEMS_PER_PRODUCER = 10000;
    const int TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;
    
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    
    std::vector<std::thread> producers;
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        producers.emplace_back([&]() {
            for (int j = 0; j < ITEMS_PER_PRODUCER; j++) {
                while (!buffer.push(j)) {
                    std::this_thread::yield();
                }
                produced++;
            }
        });
    }
    
    std::vector<std::thread> consumers;
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        consumers.emplace_back([&]() {
            while (consumed < TOTAL_ITEMS) {
                auto val = buffer.pop();
                if (val.has_value()) {
                    consumed++;
                }
            }
        });
    }
    
    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();
    
    ASSERT_EQ(consumed, TOTAL_ITEMS);
    return true;
}

TEST_CASE(test_ring_buffer_wraparound) {
    lockfree::RingBuffer<int, 4> buffer;
    
    // Fill and empty multiple times to test wraparound
    for (int round = 0; round < 10; round++) {
        for (int i = 0; i < 4; i++) {
            ASSERT(buffer.push(i));
        }
        for (int i = 0; i < 4; i++) {
            auto val = buffer.pop();
            ASSERT(val.has_value());
            ASSERT_EQ(*val, i);
        }
    }
    
    return true;
}

TEST_CASE(test_hashmap_large_capacity) {
    lockfree::HashMap<uint64_t, uint64_t, 16384> map;  
    
    // Insert 2K items (50% load factor)
    for (uint64_t i = 0; i < 2000; i++) {
        ASSERT(map.insert(i, i * 100));
    }
    
    ASSERT_EQ(map.size(), 2000);
    
    // Verify all inserted
    for (uint64_t i = 0; i < 2000; i++) {
        auto val = map.find(i);
        ASSERT(val.has_value());
        ASSERT_EQ(*val, i * 100);
    }
    
    return true;
}

TEST_CASE(test_hashmap_collision_handling) {
    lockfree::HashMap<int, int, 16> map;  // Very small capacity to force collisions
    
    // Insert keys that will collide
    for (int i = 0; i < 14; i++) {
        ASSERT(map.insert(i, i * 10));
    }
    
    ASSERT_EQ(map.size(), 14);
    
    // Verify all found
    for (int i = 0; i < 14; i++) {
        auto val = map.find(i);
        ASSERT(val.has_value());
        ASSERT_EQ(*val, i * 10);
    }
    
    // Update existing keys
    for (int i = 0; i < 14; i++) {
        ASSERT(map.insert(i, i * 20));
    }
    
    // Verify updates
    for (int i = 0; i < 14; i++) {
        auto val = map.find(i);
        ASSERT(val.has_value());
        ASSERT_EQ(*val, i * 20);
    }
    
    return true;
}

TEST_CASE(test_hashmap_erase_reinsert) {
    lockfree::HashMap<int, int, 1024> map;
    
    // Insert
    for (int i = 0; i < 100; i++) {
        ASSERT(map.insert(i, i));
    }
    
    // Erase half
    for (int i = 0; i < 50; i++) {
        ASSERT(map.erase(i));
    }
    
    ASSERT_EQ(map.size(), 50);
    
    // Re-insert erased keys
    for (int i = 0; i < 50; i++) {
        ASSERT(map.insert(i, i * 100));
    }
    
    ASSERT_EQ(map.size(), 100);
    
    // Verify all
    for (int i = 0; i < 100; i++) {
        auto val = map.find(i);
        ASSERT(val.has_value());
        if (i < 50) {
            ASSERT_EQ(*val, i * 100);  // Re-inserted
        } else {
            ASSERT_EQ(*val, i);         // Original
        }
    }
    
    return true;
}

TEST_CASE(test_hashmap_update_value) {
    lockfree::HashMap<int, int, 1024> map;
    
    map.insert(42, 100);
    ASSERT_EQ(map.find(42).value(), 100);
    
    map.insert(42, 200);
    ASSERT_EQ(map.find(42).value(), 200);
    
    map.insert(42, 300);
    ASSERT_EQ(map.find(42).value(), 300);
    ASSERT_EQ(map.size(), 1);  // Size should not increase
    
    return true;
}

TEST_CASE(test_hashmap_contains) {
    lockfree::HashMap<int, int, 1024> map;
    
    map.insert(10, 100);
    map.insert(20, 200);
    
    ASSERT(map.contains(10));
    ASSERT(map.contains(20));
    ASSERT(!map.contains(30));
    ASSERT(!map.contains(999));
    
    map.erase(10);
    ASSERT(!map.contains(10));
    ASSERT(map.contains(20));
    
    return true;
}

TEST_CASE(test_hashmap_empty_and_clear) {
    lockfree::HashMap<int, int, 1024> map;
    
    ASSERT(map.empty());
    ASSERT_EQ(map.size(), 0);
    
    map.insert(1, 100);
    map.insert(2, 200);
    ASSERT(!map.empty());
    ASSERT_EQ(map.size(), 2);
    
    map.erase(1);
    map.erase(2);
    ASSERT(map.empty());
    ASSERT_EQ(map.size(), 0);
    
    return true;
}

TEST_CASE(test_hashmap_mixed_types) {
    lockfree::HashMap<std::string, double, 1024> map;
    
    map.insert("apple", 1.23);
    map.insert("banana", 4.56);
    map.insert("cherry", 7.89);
    
    auto val = map.find("banana");
    ASSERT(val.has_value());
    ASSERT_EQ(*val, 4.56);
    
    ASSERT(!map.find("grape").has_value());
    ASSERT_EQ(map.size(), 3);
    
    map.insert("apple", 9.99);  // Update
    val = map.find("apple");
    ASSERT(val.has_value());
    ASSERT_EQ(*val, 9.99);
    ASSERT_EQ(map.size(), 3);  // Size unchanged
    
    return true;
}

TEST_CASE(test_hashmap_edge_keys) {
    lockfree::HashMap<int, int, 1024> map;
    
    // Test with 0
    map.insert(0, 999);
    auto val = map.find(0);
    ASSERT(val.has_value());
    ASSERT_EQ(*val, 999);
    
    // Test with negative keys (if int is signed)
    map.insert(-1, 777);
    val = map.find(-1);
    ASSERT(val.has_value());
    ASSERT_EQ(*val, 777);
    
    // Test with max int
    map.insert(2147483647, 555);
    val = map.find(2147483647);
    ASSERT(val.has_value());
    ASSERT_EQ(*val, 555);
    
    ASSERT_EQ(map.size(), 3);
    
    return true;
}

TEST_CASE(test_hashmap_concurrent_insert_find) {
    lockfree::HashMap<int, int, 16384> map;
    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 2500;
    std::atomic<bool> start{false};
    std::atomic<int> errors{0};
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&, t]() {
            while (!start) std::this_thread::yield();
            
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                int key = t * OPS_PER_THREAD + i;
                map.insert(key, key * 10);
                
                // Verify immediately
                auto val = map.find(key);
                if (!val.has_value() || *val != key * 10) {
                    errors++;
                }
            }
        });
    }
    
    start = true;
    for (auto& th : threads) th.join();
    
    ASSERT_EQ(errors, 0);
    ASSERT_EQ(map.size(), NUM_THREADS * OPS_PER_THREAD);
    
    return true;
}

TEST_CASE(test_hashmap_string_keys) {
    lockfree::HashMap<std::string, int, 1024> map;
    
    // Insert with string keys
    map.insert("order_12345", 100);
    map.insert("order_67890", 200);
    map.insert("trade_ABC", 300);
    
    auto val = map.find("order_67890");
    ASSERT(val.has_value());
    ASSERT_EQ(*val, 200);
    
    // Update string key
    map.insert("order_12345", 999);
    val = map.find("order_12345");
    ASSERT(val.has_value());
    ASSERT_EQ(*val, 999);
    
    // Erase
    map.erase("trade_ABC");
    ASSERT(!map.contains("trade_ABC"));
    ASSERT_EQ(map.size(), 2);
    
    return true;
}

TEST_CASE(test_hashmap_load_factor) {
    lockfree::HashMap<int, int, 1024> map;
    
    // Fill to ~90% capacity
    for (int i = 0; i < 900; i++) {
        ASSERT(map.insert(i, i));
    }
    
    ASSERT_EQ(map.size(), 900);
    
    // Should still work
    for (int i = 0; i < 900; i++) {
        auto val = map.find(i);
        ASSERT(val.has_value());
        ASSERT_EQ(*val, i);
    }
    
    // Should still allow inserts
    for (int i = 900; i < 950; i++) {
        ASSERT(map.insert(i, i));
    }
    ASSERT_EQ(map.size(), 950);
    
    // Should allow updates
    for (int i = 0; i < 950; i++) {
        ASSERT(map.insert(i, i * 2));
    }
    
    // Verify updates
    for (int i = 0; i < 950; i++) {
        auto val = map.find(i);
        ASSERT(val.has_value());
        ASSERT_EQ(*val, i * 2);
    }
    
    return true;
}

// ============ MAIN ============
int main(int argc, char** argv) {
    std::string filter;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        // Handle --gtest_filter=pattern
        if (arg.find("--gtest_filter=") == 0) {
            filter = arg.substr(15);  // Remove "--gtest_filter=" prefix
        }
        // Handle simple positional argument
        else if (i == 1 && arg[0] != '-') {
            filter = arg;
        }
    }
    
    auto& tests = get_tests();
    
    // Build list of tests to run
    std::vector<TestCase> to_run;
    for (auto& test : tests) {
        if (filter.empty()) {
            to_run.push_back(test);
        } else if (filter.back() == '*') {
            // Wildcard match (e.g., "test_spsc_*")
            std::string prefix = filter.substr(0, filter.length() - 1);
            if (test.name.find(prefix) == 0) {
                to_run.push_back(test);
            }
        } else if (test.name == filter) {
            to_run.push_back(test);
        }
    }
    
    std::cout << "=== whirl-pool Test Suite ===" << std::endl;
    if (!filter.empty()) {
        std::cout << "Filter: " << filter << std::endl;
    }
    std::cout << "Running " << to_run.size() << " tests..." << std::endl << std::endl;
    
    int passed = 0;
    for (auto& test : to_run) {
        std::cout << "TEST: " << test.name << "... ";
        if (test.func()) {
            std::cout << "PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "FAILED" << std::endl;
        }
    }
    
    std::cout << std::endl << "Results: " << passed << "/" << to_run.size() << " passed" << std::endl;
    return passed == to_run.size() ? 0 : 1;
}