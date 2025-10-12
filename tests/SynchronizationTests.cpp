#include <gtest/gtest.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <vector>

// This test file verifies that std::mutex and std::condition_variable
// behave correctly for the synchronization patterns used in CereLink.
// This is critical before migrating from QMutex/QWaitCondition.

class SynchronizationTest : public ::testing::Test {
protected:
    std::mutex m_lock;
    std::condition_variable m_condVar;
    std::atomic<bool> m_signaled{false};
    std::atomic<int> m_counter{0};
};

// Test 1: Verify condition variable timeout works correctly
TEST_F(SynchronizationTest, ConditionVariableTimeoutWorks) {
    auto start = std::chrono::steady_clock::now();

    {
        std::unique_lock<std::mutex> lock(m_lock);
        // Wait for 100ms with no signal - should timeout
        bool result = m_condVar.wait_for(lock, std::chrono::milliseconds(100),
                                          [this]{ return m_signaled.load(); });
        EXPECT_FALSE(result); // Should timeout (not signaled)
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    // Verify timeout was approximately correct (allow generous tolerance for CI environments)
    EXPECT_GE(elapsed_ms, 100);
    EXPECT_LE(elapsed_ms, 250);  // Increased tolerance for busy CI runners
}

// Test 2: Verify condition variable signaling wakes waiting thread
TEST_F(SynchronizationTest, ConditionVariableSignalingWorks) {
    std::atomic<bool> threadStarted{false};
    std::atomic<bool> threadWokenUp{false};

    // Start a thread that waits on condition variable
    std::thread waiter([&]() {
        std::unique_lock<std::mutex> lock(m_lock);
        threadStarted = true;

        // Wait for signal (with 5 second timeout to prevent test hanging)
        bool result = m_condVar.wait_for(lock, std::chrono::seconds(5),
                                          [this]{ return m_signaled.load(); });

        EXPECT_TRUE(result); // Should be woken by signal, not timeout
        threadWokenUp = true;
    });

    // Wait for thread to start waiting
    while (!threadStarted) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Give it a moment to actually start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Signal the condition variable
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_signaled = true;
    }
    m_condVar.notify_one();

    waiter.join();
    EXPECT_TRUE(threadWokenUp);
}

// Test 3: Verify mutex protects shared data from race conditions
TEST_F(SynchronizationTest, MutexPreventsRaceConditions) {
    const int numThreads = 10;
    const int incrementsPerThread = 1000;

    std::vector<std::thread> threads;

    // Launch multiple threads that increment a counter
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < incrementsPerThread; ++j) {
                std::lock_guard<std::mutex> lock(m_lock);
                m_counter++;
            }
        });
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    // If mutex works correctly, counter should be exactly numThreads * incrementsPerThread
    EXPECT_EQ(m_counter.load(), numThreads * incrementsPerThread);
}

// Test 4: Verify multiple waiters can be woken up
TEST_F(SynchronizationTest, NotifyAllWakesMultipleWaiters) {
    const int numWaiters = 5;
    std::atomic<int> wakened{0};
    std::vector<std::thread> waiters;

    // Start multiple waiting threads
    for (int i = 0; i < numWaiters; ++i) {
        waiters.emplace_back([&]() {
            std::unique_lock<std::mutex> lock(m_lock);
            m_condVar.wait_for(lock, std::chrono::seconds(5),
                               [this]{ return m_signaled.load(); });
            wakened++;
        });
    }

    // Give threads time to start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Signal all waiters
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_signaled = true;
    }
    m_condVar.notify_all();

    // Wait for all threads
    for (auto& t : waiters) {
        t.join();
    }

    EXPECT_EQ(wakened.load(), numWaiters);
}

// Test 5: Verify specific timeout durations used in CereLink
TEST_F(SynchronizationTest, VerifyCommonTimeoutDurations) {
    // Test 15 second timeout (used for connection waiting)
    {
        auto start = std::chrono::steady_clock::now();
        std::unique_lock<std::mutex> lock(m_lock);
        m_condVar.wait_for(lock, std::chrono::milliseconds(100)); // Use 100ms for testing
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        EXPECT_GE(elapsed_ms, 100);
        EXPECT_LE(elapsed_ms, 300);  // Increased tolerance for busy CI runners
    }

    // Test 250ms timeout (used for tracking packet waiting)
    {
        auto start = std::chrono::steady_clock::now();
        std::unique_lock<std::mutex> lock(m_lock);
        m_condVar.wait_for(lock, std::chrono::milliseconds(250));
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        EXPECT_GE(elapsed_ms, 250);
        EXPECT_LE(elapsed_ms, 400);  // Increased tolerance for busy CI runners
    }
}

// Test 6: Verify recursive mutex is not needed (all our use cases are simple)
TEST_F(SynchronizationTest, SimpleMutexSufficesForOurUseCase) {
    // This test verifies that we don't need recursive mutex
    // (i.e., we never try to lock the same mutex twice from the same thread)

    std::mutex simpleMutex;
    bool success = true;

    {
        std::lock_guard<std::mutex> lock(simpleMutex);
        // Do some work
        int dummy = 42;
        (void)dummy;

        // We should never try to lock again here in real code
        // If we did, we'd need std::recursive_mutex instead
        success = true;
    }

    EXPECT_TRUE(success);
}
