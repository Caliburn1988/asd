#include "../include/TaskScheduler.h"
#include <iostream>
#include <cassert>

using namespace YB;

int main() {
    std::cout << "TaskScheduler Simple Test" << std::endl;
    
    // 测试1: 基本功能
    {
        std::cout << "Test 1: Basic functionality... ";
        SchedulerConfig config;
        TaskScheduler scheduler(config);
        assert(scheduler.initialize(config));
        assert(scheduler.isRunning());
        
        auto task = []() -> TaskResult {
            TaskResult result;
            result.status = ResultStatus::SUCCESS;
            return result;
        };
        
        TaskID id = scheduler.submitTask(TaskType::USER_DEFINED, Priority::NORMAL, task);
        assert(id > 0);
        assert(scheduler.getTaskStatus(id) == TaskStatus::PENDING);
        
        scheduler.shutdown();
        assert(!scheduler.isRunning());
        std::cout << "PASSED" << std::endl;
    }
    
    // 测试2: 数据结构
    {
        std::cout << "Test 2: Data structures... ";
        Task task;
        assert(task.id == 0);
        
        TaskResult result(123, ResultStatus::SUCCESS);
        assert(result.taskId == 123);
        assert(result.status == ResultStatus::SUCCESS);
        
        PerformanceMetrics metrics;
        assert(metrics.totalTasksSubmitted == 0);
        
        std::cout << "PASSED" << std::endl;
    }
    
    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}