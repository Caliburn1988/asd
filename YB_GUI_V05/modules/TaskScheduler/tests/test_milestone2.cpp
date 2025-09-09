#include "../include/TaskScheduler.h"
#include "../include/ThreadPool.h"
#include "../include/PriorityQueue.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <random>
#include <cassert>

using namespace YB;
using namespace std::chrono_literals;

// 测试结果统计
struct TestStats {
    std::atomic<int> tasksExecuted{0};
    std::atomic<int> criticalTasks{0};
    std::atomic<int> highTasks{0};
    std::atomic<int> normalTasks{0};
    std::atomic<int> lowTasks{0};
    std::atomic<int> backgroundTasks{0};
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;
};

// 测试任务函数
TaskResult createTestTask(TaskID id, Priority priority, TestStats& stats, int sleepMs = 10) {
    // 记录任务执行
    stats.tasksExecuted++;
    
    // 根据优先级更新统计
    switch (priority) {
        case Priority::CRITICAL:
            stats.criticalTasks++;
            break;
        case Priority::HIGH:
            stats.highTasks++;
            break;
        case Priority::NORMAL:
            stats.normalTasks++;
            break;
        case Priority::LOW:
            stats.lowTasks++;
            break;
        case Priority::BACKGROUND:
            stats.backgroundTasks++;
            break;
    }
    
    // 模拟任务执行时间
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    
    TaskResult result;
    result.taskId = id;
    result.status = ResultStatus::SUCCESS;
    result.result = std::string("Task " + std::to_string(id) + " completed");
    
    return result;
}

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

// 测试3：TaskScheduler集成测试
bool testTaskSchedulerIntegration() {
    std::cout << "\n=== Test 3: TaskScheduler Integration Test ===" << std::endl;
    
    try {
        SchedulerConfig config;
        config.minThreads = 4;
        config.maxThreads = 8;
        
        TaskScheduler scheduler;
        assert(scheduler.initialize(config));
        
        TestStats stats;
        stats.startTime = std::chrono::steady_clock::now();
        
        // 提交不同优先级的任务
        std::vector<TaskID> taskIds;
        
        // 10个CRITICAL任务
        for (int i = 0; i < 10; ++i) {
            TaskID id = scheduler.submitTask(
                TaskType::USER_DEFINED,
                Priority::CRITICAL,
                [i, &stats] { return createTestTask(i, Priority::CRITICAL, stats, 5); }
            );
            taskIds.push_back(id);
        }
        
        // 20个NORMAL任务
        for (int i = 10; i < 30; ++i) {
            TaskID id = scheduler.submitTask(
                TaskType::USER_DEFINED,
                Priority::NORMAL,
                [i, &stats] { return createTestTask(i, Priority::NORMAL, stats, 10); }
            );
            taskIds.push_back(id);
        }
        
        // 10个BACKGROUND任务
        for (int i = 30; i < 40; ++i) {
            TaskID id = scheduler.submitTask(
                TaskType::USER_DEFINED,
                Priority::BACKGROUND,
                [i, &stats] { return createTestTask(i, Priority::BACKGROUND, stats, 15); }
            );
            taskIds.push_back(id);
        }
        
        // 等待所有任务完成
        std::this_thread::sleep_for(2s);
        
        stats.endTime = std::chrono::steady_clock::now();
        
        // 获取性能指标
        auto metrics = scheduler.getPerformanceMetrics();
        auto queueStatus = scheduler.getQueueStatus();
        
        std::cout << "\nExecution Statistics:" << std::endl;
        std::cout << "Total tasks executed: " << stats.tasksExecuted.load() << std::endl;
        std::cout << "Critical tasks: " << stats.criticalTasks.load() << std::endl;
        std::cout << "Normal tasks: " << stats.normalTasks.load() << std::endl;
        std::cout << "Background tasks: " << stats.backgroundTasks.load() << std::endl;
        
        std::cout << "\nPerformance Metrics:" << std::endl;
        std::cout << "Tasks submitted: " << metrics.totalTasksSubmitted << std::endl;
        std::cout << "Tasks completed: " << metrics.totalTasksCompleted << std::endl;
        std::cout << "Tasks failed: " << metrics.totalTasksFailed << std::endl;
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            stats.endTime - stats.startTime
        );
        std::cout << "Total execution time: " << duration.count() << " ms" << std::endl;
        
        // 验证所有任务都已执行
        assert(stats.tasksExecuted == 40);
        assert(metrics.totalTasksSubmitted == 40);
        
        scheduler.shutdown();
        
        std::cout << "Integration test PASSED ✓" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Integration test FAILED: " << e.what() << std::endl;
        return false;
    }
}

// 测试4：并发性能测试
bool testConcurrentPerformance() {
    std::cout << "\n=== Test 4: Concurrent Performance Test ===" << std::endl;
    
    try {
        SchedulerConfig config;
        config.minThreads = 8;
        config.maxThreads = 16;
        config.enableLoadBalancing = true;
        
        TaskScheduler scheduler;
        assert(scheduler.initialize(config));
        
        const int NUM_TASKS = 100;
        std::atomic<int> completed{0};
        auto startTime = std::chrono::steady_clock::now();
        
        // 提交100个混合优先级的任务
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> priorityDist(0, 4);
        
        for (int i = 0; i < NUM_TASKS; ++i) {
            Priority priority = static_cast<Priority>(priorityDist(gen));
            
            scheduler.submitTask(
                TaskType::USER_DEFINED,
                priority,
                [i, &completed] {
                    // 模拟不同的任务负载
                    std::this_thread::sleep_for(std::chrono::milliseconds(5 + (i % 10)));
                    completed++;
                    
                    TaskResult result;
                    result.status = ResultStatus::SUCCESS;
                    return result;
                }
            );
        }
        
        // 等待所有任务完成
        while (completed < NUM_TASKS) {
            std::this_thread::sleep_for(10ms);
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        auto metrics = scheduler.getPerformanceMetrics();
        
        std::cout << "\nPerformance Results:" << std::endl;
        std::cout << "Tasks completed: " << completed.load() << "/" << NUM_TASKS << std::endl;
        std::cout << "Total time: " << duration.count() << " ms" << std::endl;
        std::cout << "Throughput: " << (NUM_TASKS * 1000.0 / duration.count()) << " tasks/second" << std::endl;
        std::cout << "Average execution time: " << metrics.averageExecutionTime << " ms" << std::endl;
        
        // 验证性能目标
        assert(completed == NUM_TASKS);
        assert(duration.count() < 5000); // 应该在5秒内完成
        
        scheduler.shutdown();
        
        std::cout << "Performance test PASSED ✓" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Performance test FAILED: " << e.what() << std::endl;
        return false;
    }
}

// 测试5：线程安全测试
bool testThreadSafety() {
    std::cout << "\n=== Test 5: Thread Safety Test ===" << std::endl;
    
    try {
        SchedulerConfig config;
        config.minThreads = 4;
        
        TaskScheduler scheduler;
        assert(scheduler.initialize(config));
        
        const int NUM_THREADS = 10;
        const int TASKS_PER_THREAD = 50;
        std::atomic<int> totalSubmitted{0};
        std::atomic<int> totalCompleted{0};
        
        // 多个线程同时提交任务
        std::vector<std::thread> submitters;
        
        for (int t = 0; t < NUM_THREADS; ++t) {
            submitters.emplace_back([&scheduler, &totalSubmitted, &totalCompleted] {
                for (int i = 0; i < TASKS_PER_THREAD; ++i) {
                    TaskID id = scheduler.submitTask(
                        TaskType::USER_DEFINED,
                        Priority::NORMAL,
                        [&totalCompleted] {
                            std::this_thread::sleep_for(1ms);
                            totalCompleted++;
                            
                            TaskResult result;
                            result.status = ResultStatus::SUCCESS;
                            return result;
                        }
                    );
                    
                    if (id > 0) {
                        totalSubmitted++;
                    }
                }
            });
        }
        
        // 等待所有提交线程完成
        for (auto& t : submitters) {
            t.join();
        }
        
        // 等待所有任务执行完成
        std::this_thread::sleep_for(2s);
        
        auto metrics = scheduler.getPerformanceMetrics();
        
        std::cout << "Total submitted: " << totalSubmitted.load() << std::endl;
        std::cout << "Total completed: " << totalCompleted.load() << std::endl;
        std::cout << "Metrics - submitted: " << metrics.totalTasksSubmitted << std::endl;
        std::cout << "Metrics - completed: " << metrics.totalTasksCompleted << std::endl;
        
        // 验证线程安全
        assert(totalSubmitted == NUM_THREADS * TASKS_PER_THREAD);
        assert(totalCompleted == totalSubmitted);
        
        scheduler.shutdown();
        
        std::cout << "Thread safety test PASSED ✓" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Thread safety test FAILED: " << e.what() << std::endl;
        return false;
    }
}

// 主测试函数
int main() {
    std::cout << "=== TaskScheduler Milestone 2 Tests ===" << std::endl;
    std::cout << "Testing ThreadPool and Priority Scheduling Implementation\n" << std::endl;
    
    int passed = 0;
    int total = 5;
    
    if (testThreadPool()) passed++;
    if (testPriorityQueue()) passed++;
    if (testTaskSchedulerIntegration()) passed++;
    if (testConcurrentPerformance()) passed++;
    if (testThreadSafety()) passed++;
    
    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Passed: " << passed << "/" << total << std::endl;
    
    if (passed == total) {
        std::cout << "\n✅ All Milestone 2 tests PASSED!" << std::endl;
        std::cout << "✅ ThreadPool implementation verified" << std::endl;
        std::cout << "✅ PriorityQueue implementation verified" << std::endl;
        std::cout << "✅ Concurrent execution verified" << std::endl;
        std::cout << "✅ Thread safety verified" << std::endl;
        std::cout << "✅ Performance targets met (100+ concurrent tasks)" << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ Some tests FAILED!" << std::endl;
        return 1;
    }
}