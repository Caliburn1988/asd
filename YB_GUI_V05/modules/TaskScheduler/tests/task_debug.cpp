#include "../include/TaskScheduler.h"
#include "../include/ThreadPool.h"
#include "../include/PriorityQueue.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

using namespace YB;
using namespace std::chrono_literals;

int main() {
    std::cout << "=== Task Submission Debug Test ===" << std::endl;
    
    SchedulerConfig config;
    config.minThreads = 2;
    config.maxThreads = 4;
    
    TaskScheduler scheduler;
    scheduler.initialize(config);
    
    std::atomic<bool> taskExecuted{false};
    
    std::cout << "\nSubmitting task..." << std::endl;
    
    TaskID taskId = scheduler.submitTask(
        TaskType::USER_DEFINED,
        Priority::NORMAL,
        [&taskExecuted]() {
            std::cout << ">>> Task is executing! Thread: " << std::this_thread::get_id() << std::endl;
            taskExecuted = true;
            std::this_thread::sleep_for(100ms);
            
            TaskResult result;
            result.status = ResultStatus::SUCCESS;
            return result;
        }
    );
    
    std::cout << "Task submitted with ID: " << taskId << std::endl;
    
    // 等待任务执行
    for (int i = 0; i < 20; ++i) {
        std::this_thread::sleep_for(100ms);
        auto status = scheduler.getTaskStatus(taskId);
        std::cout << "Checking status (" << i << "): " << taskStatusToString(status) 
                  << ", Executed: " << (taskExecuted ? "yes" : "no") << std::endl;
        
        if (status == TaskStatus::COMPLETED) {
            break;
        }
    }
    
    auto metrics = scheduler.getPerformanceMetrics();
    std::cout << "\nFinal metrics:" << std::endl;
    std::cout << "- Tasks submitted: " << metrics.totalTasksSubmitted << std::endl;
    std::cout << "- Tasks completed: " << metrics.totalTasksCompleted << std::endl;
    std::cout << "- Task executed: " << (taskExecuted ? "yes" : "no") << std::endl;
    
    scheduler.shutdown();
    
    return taskExecuted ? 0 : 1;
}