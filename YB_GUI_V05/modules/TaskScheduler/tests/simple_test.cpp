#include "../include/TaskScheduler.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace YB;
using namespace std::chrono_literals;

int main() {
    std::cout << "=== Simple TaskScheduler Test ===" << std::endl;
    
    SchedulerConfig config;
    config.minThreads = 2;
    config.maxThreads = 4;
    
    TaskScheduler scheduler;
    
    std::cout << "Initializing scheduler..." << std::endl;
    if (!scheduler.initialize(config)) {
        std::cerr << "Failed to initialize scheduler!" << std::endl;
        return 1;
    }
    
    std::cout << "Scheduler initialized successfully." << std::endl;
    
    // 提交一个简单任务
    std::cout << "\nSubmitting a simple task..." << std::endl;
    
    TaskID taskId = scheduler.submitTask(
        TaskType::USER_DEFINED,
        Priority::NORMAL,
        []() {
            std::cout << "Task executing in thread: " << std::this_thread::get_id() << std::endl;
            std::this_thread::sleep_for(100ms);
            
            TaskResult result;
            result.status = ResultStatus::SUCCESS;
            result.result = std::string("Task completed successfully!");
            return result;
        }
    );
    
    std::cout << "Task submitted with ID: " << taskId << std::endl;
    
    // 等待任务完成
    std::cout << "Waiting for task completion..." << std::endl;
    std::this_thread::sleep_for(500ms);
    
    // 检查任务状态
    TaskStatus status = scheduler.getTaskStatus(taskId);
    std::cout << "Task status: " << taskStatusToString(status) << std::endl;
    
    // 获取性能指标
    auto metrics = scheduler.getPerformanceMetrics();
    std::cout << "\nPerformance Metrics:" << std::endl;
    std::cout << "- Tasks submitted: " << metrics.totalTasksSubmitted << std::endl;
    std::cout << "- Tasks completed: " << metrics.totalTasksCompleted << std::endl;
    std::cout << "- Active threads: " << metrics.currentActiveThreads << std::endl;
    
    // 关闭调度器
    std::cout << "\nShutting down scheduler..." << std::endl;
    scheduler.shutdown();
    
    std::cout << "Test completed successfully!" << std::endl;
    
    return 0;
}