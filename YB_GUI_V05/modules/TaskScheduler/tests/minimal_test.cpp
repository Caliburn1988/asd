#include "../include/TaskScheduler.h"
#include "../include/ThreadPool.h"
#include "../include/PriorityQueue.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace YB;
using namespace std::chrono_literals;

// 简化版的TaskScheduler初始化，不启动worker线程
class SimpleScheduler : public TaskScheduler {
public:
    bool simpleInit(const SchedulerConfig& config) {
        config_ = config;
        
        // 创建线程池和队列
        threadPool_ = std::make_unique<ThreadPool>(config_.minThreads);
        taskQueue_ = std::make_unique<PriorityQueue>();
        
        running_ = true;
        paused_ = false;
        
        currentMetrics_ = PerformanceMetrics();
        currentMetrics_.currentActiveThreads = config_.minThreads;
        
        return true;
    }
    
    void testProcessing() {
        // 手动处理一个任务
        auto task = taskQueue_->tryPop();
        if (task) {
            std::cout << "Processing task manually..." << std::endl;
            processTask(task);
        }
    }
};

int main() {
    std::cout << "=== Minimal Test ===" << std::endl;
    
    SchedulerConfig config;
    config.minThreads = 2;
    
    SimpleScheduler scheduler;
    scheduler.simpleInit(config);
    
    std::cout << "Submitting task..." << std::endl;
    
    bool executed = false;
    TaskID id = scheduler.submitTask(
        TaskType::USER_DEFINED,
        Priority::NORMAL,
        [&executed]() {
            std::cout << ">>> Task executed!" << std::endl;
            executed = true;
            TaskResult result;
            result.status = ResultStatus::SUCCESS;
            return result;
        }
    );
    
    std::cout << "Task ID: " << id << std::endl;
    
    // 手动处理任务
    scheduler.testProcessing();
    
    std::this_thread::sleep_for(100ms);
    
    auto status = scheduler.getTaskStatus(id);
    std::cout << "Task status: " << taskStatusToString(status) << std::endl;
    std::cout << "Task executed: " << (executed ? "yes" : "no") << std::endl;
    
    return executed ? 0 : 1;
}