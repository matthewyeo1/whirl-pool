#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <optional>
#include <atomic>

// ============ UTILITIES (Cache alignment) ============
namespace lockfree {
    constexpr size_t CACHE_LINE_SIZE = 64;
}

// ============ SPSC QUEUE IMPLEMENTATION ============
namespace lockfree {

template<typename T, size_t Capacity>
class SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    
public:
    SPSCQueue() : m_head(0), m_tail(0) {}
    
    bool push(const T& value) {
        size_t head = m_head.load(std::memory_order_relaxed);
        size_t next_head = (head + 1) & (Capacity - 1);
        
        if (next_head == m_tail.load(std::memory_order_acquire)) {
            return false;
        }
        
        m_buffer[head] = value;
        m_head.store(next_head, std::memory_order_release);
        return true;
    }
    
    std::optional<T> pop() {
        size_t tail = m_tail.load(std::memory_order_relaxed);
        
        if (tail == m_head.load(std::memory_order_acquire)) {
            return std::nullopt;
        }
        
        T value = std::move(m_buffer[tail]);
        m_tail.store((tail + 1) & (Capacity - 1), std::memory_order_release);
        return value;
    }
    
    bool empty() const {
        return m_head.load(std::memory_order_acquire) == 
               m_tail.load(std::memory_order_acquire);
    }
    
    size_t size() const {
        size_t head = m_head.load(std::memory_order_acquire);
        size_t tail = m_tail.load(std::memory_order_acquire);
        return (head - tail) & (Capacity - 1);
    }
    
private:
    alignas(lockfree::CACHE_LINE_SIZE) std::atomic<size_t> m_head;
    alignas(lockfree::CACHE_LINE_SIZE) std::atomic<size_t> m_tail;
    T m_buffer[Capacity];
};

}

// ============ TEST FRAMEWORK ============
struct TestCase {
    std::string name;
    std::function<bool()> func;
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

// ============ TESTS ============

TEST_CASE(test_spsc_basic) {
    lockfree::SPSCQueue<int, 16> queue;
    
    ASSERT(queue.push(42));
    ASSERT(queue.push(100));
    
    auto val = queue.pop();
    ASSERT(val.has_value());
    ASSERT_EQ(*val, 42);
    
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
    });
    
    producer.join();
    consumer.join();
    
    ASSERT_EQ(received, NUM_ITEMS);
    return true;
}

// ============ MAIN ============
int main() {
    auto& tests = get_tests();
    std::cout << "=== whirl-pool Test Suite ===" << std::endl;
    std::cout << "Running " << tests.size() << " tests..." << std::endl << std::endl;
    
    int passed = 0;
    for (auto& test : tests) {
        std::cout << "TEST: " << test.name << "... ";
        if (test.func()) {
            std::cout << "PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "FAILED" << std::endl;
        }
    }
    
    std::cout << std::endl << "Results: " << passed << "/" << tests.size() << " passed" << std::endl;
    return passed == tests.size() ? 0 : 1;
}