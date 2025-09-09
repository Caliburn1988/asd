#include "../include/TaskScheduler.h"
#include "../include/ThreadPool.h"
#include "../include/PriorityQueue.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <cassert>

using namespace YB;
using namespace std::chrono_literals;

// 测试1：基础线程池功能
bool testThreadPool() {
    std::cout << "\n=== Test 1: ThreadPool Basic Functionality ===" << std::endl;
    
    try {
        ThreadPool pool(4);
        std::atomic<int> counter{0};
        std::vector<std::future<int>> futures;
        
        // 提交20个任务
        for (int i = 0; i < 20; ++i) {
            futures.emplace_back(
                pool.enqueue([&counter, i] {
                    counter++;
                    std::this_thread::sleep_for(10ms);
                    return i * i;
                })
            );
        }
        
        // 等待所有任务完成
        int sum = 0;
        for (auto& future : futures) {
            sum += future.get();
        }
        
        std::cout << "Tasks executed: " << counter.load() << std::endl;
        std::cout << "Sum of squares: " << sum << std::endl;
        
        // 验证结果
        assert(counter == 20);
        assert(sum == 2470); // 0^2 + 1^2 + ... + 19^2
        
        std::cout << "ThreadPool test PASSED ✓" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "ThreadPool test FAILED: " << e.what() << std::endl;
        return false;
    }
}

// 测试2：优先级队列功能
bool testPriorityQueue() {
    std::cout << "\n=== Test 2: PriorityQueue Functionality ===" << std::endl;
    
    try {
        PriorityQueue queue;
        
        // 创建不同优先级的任务
        auto task1 = std::make_shared<Task>(1, TaskType::USER_DEFINED, Priority::LOW, nullptr);
        auto task2 = std::make_shared<Task>(2, TaskType::USER_DEFINED, Priority::CRITICAL, nullptr);
        auto task3 = std::make_shared<Task>(3, TaskType::USER_DEFINED, Priority::NORMAL, nullptr);
        auto task4 = std::make_shared<Task>(4, TaskType::USER_DEFINED, Priority::HIGH, nullptr);
        auto task5 = std::make_shared<Task>(5, TaskType::USER_DEFINED, Priority::BACKGROUND, nullptr);
        
        // 添加到队列
        queue.push(task1);
        queue.push(task2);
        queue.push(task3);
        queue.push(task4);
        queue.push(task5);
        
        // 按优先级取出
        std::vector<Priority> expectedOrder = {
            Priority::CRITICAL,
            Priority::HIGH,
            Priority::NORMAL,
            Priority::LOW,
            Priority::BACKGROUND
        };
        
        for (auto expectedPriority : expectedOrder) {
            auto task = queue.pop();
            assert(task != nullptr);
            assert(task->priority == expectedPriority);
            std::cout << "Retrieved task with priority: " << priorityToString(task->priority) << std::endl;
        }
        
        std::cout << "PriorityQueue test PASSED ✓" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "PriorityQueue test FAILED: " << e.what() << std::endl;
        return false;
    }
}

// 测试3：简单的TaskScheduler集成测试
bool testSimpleIntegration() {
    std::cout << "\n=== Test 3: Simple TaskScheduler Integration ===" << std::endl;
    
    try {
        SchedulerConfig config;
        config.minThreads = 2;
        config.maxThreads = 4;
        
        TaskScheduler scheduler;
        if (!scheduler.initialize(config)) {
            std::cerr << "Failed to initialize scheduler" << std::endl;
            return false;
        }
        
        std::atomic<int> executed{0};
        
        // 提交5个简单任务
        std::cout << "Submitting 5 tasks..." << std::endl;
        std::vector<TaskID> taskIds;
        
        for (int i = 0; i < 5; ++i) {
            TaskID id = scheduler.submitTask(
                TaskType::USER_DEFINED,
                Priority::NORMAL,
                [&executed, i]() {
                    std::cout << "Task " << i << " executing" << std::endl;
                    executed++;
                    std::this_thread::sleep_for(50ms);
                    
                    TaskResult result;
                    result.status = ResultStatus::SUCCESS;
                    return result;
                }
            );
            taskIds.push_back(id);
            std::cout << "Submitted task " << i << " with ID: " << id << std::endl;
        }
        
        // 等待执行
        std::cout << "Waiting for tasks to complete..." << std::endl;
        std::this_thread::sleep_for(1s);
        
        std::cout << "Tasks executed: " << executed.load() << std::endl;
        
        // 检查指标
        auto metrics = scheduler.getPerformanceMetrics();
        std::cout << "Metrics - submitted: " << metrics.totalTasksSubmitted 
                  << ", completed: " << metrics.totalTasksCompleted << std::endl;
        
        // 关闭调度器
        std::cout << "Shutting down..." << std::endl;
        scheduler.shutdown();
        
        bool success = (executed == 5);
        std::cout << "Simple integration test " << (success ? "PASSED ✓" : "FAILED ✗") << std::endl;
        
        return success;
        
    } catch (const std::exception& e) {
        std::cerr << "Integration test FAILED with exception: " << e.what() << std::endl;
        return false;
    }
}

// 主测试函数
int main() {
    std::cout << "=== TaskScheduler Milestone 2 Tests (Simplified) ===" << std::endl;
    
    int passed = 0;
    int total = 3;
    
    if (testThreadPool()) passed++;
    if (testPriorityQueue()) passed++;
    if (testSimpleIntegration()) passed++;
    
    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Passed: " << passed << "/" << total << std::endl;
    
    if (passed == total) {
        std::cout << "\n✅ All tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ Some tests FAILED!" << std::endl;
        return 1;
    }
}