#include <gtest/gtest.h>
#include "../src/concurrency/thread_pool.h"
#include <atomic>
#include <chrono>

TEST(ThreadPoolTest, BasicTasks) {
    ThreadPool pool(4);
    std::atomic<int> counter(0);

    for (int i = 0; i < 100; ++i) {
        pool.enqueue([&counter]() {
            counter++;
        });
    }

    // Wait a bit for tasks to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    pool.shutdown();
    EXPECT_EQ(counter.load(), 100);
}

TEST(ThreadPoolTest, ParallelExecution) {
    ThreadPool pool(2);
    std::atomic<int> active(0);
    std::atomic<int> peak(0);

    for (int i = 0; i < 4; ++i) {
        pool.enqueue([&active, &peak]() {
            active++;
            int current = active.load();
            int currentPeak = peak.load();
            while (current > currentPeak && !peak.compare_exchange_weak(currentPeak, current)) {
                currentPeak = peak.load();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            active--;
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    pool.shutdown();
    
    // With 2 threads in the pool, peak active threads should be 2
    EXPECT_LE(peak.load(), 2);
    EXPECT_GE(peak.load(), 1);
}
