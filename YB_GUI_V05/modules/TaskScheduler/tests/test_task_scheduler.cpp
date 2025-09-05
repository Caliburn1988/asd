#include "../include/TaskScheduler.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <iomanip>

using namespace YB;
using namespace std::chrono_literals;

// 测试辅助函数
void printTestResult(const std::string& testName, bool passed) {
    std::cout << std::left << std::setw(50) << testName 
              << (passed ? "[ PASSED ]" : "[ FAILED ]") << std::endl;
}

// 测试用例1: 测试TaskScheduler的构造和初始化
bool testConstructorAndInitialization() {
    try {
        // 测试默认构造函数
        TaskScheduler scheduler1;
        assert(!scheduler1.isRunning());
        
        // 测试带配置的构造函数
        SchedulerConfig config;
        config.minThreads = 4;
        config.maxThreads = 8;
        TaskScheduler scheduler2(config);
        assert(!scheduler2.isRunning());
        
        // 测试初始化
        bool initResult = scheduler1.initialize(config);
        assert(initResult == true);
        assert(scheduler1.isRunning());
        
        // 测试重复初始化
        bool reinitResult = scheduler1.initialize(config);
        assert(reinitResult == false); // 应该失败
        
        // 测试关闭
        scheduler1.shutdown();
        assert(!scheduler1.isRunning());
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in testConstructorAndInitialization: " << e.what() << std::endl;
        return false;
    }
}

// 测试用例2: 测试任务提交功能
bool testTaskSubmission() {
    try {
        SchedulerConfig config;
        TaskScheduler scheduler(config);
        scheduler.initialize(config);
        
        // 创建一个简单的任务
        auto simpleTask = []() -> TaskResult {
            TaskResult result;
            result.status = ResultStatus::SUCCESS;
            result.executionTime = std::chrono::milliseconds(100);
            return result;
        };
        
        // 测试基本任务提交
        TaskID taskId1 = scheduler.submitTask(TaskType::USER_DEFINED, Priority::NORMAL, simpleTask);
        assert(taskId1 > 0);
        
        // 测试带超时的任务提交
        TaskID taskId2 = scheduler.submitTask(TaskType::AI_INFERENCE, Priority::HIGH, simpleTask, 5000ms);
        assert(taskId2 > taskId1);
        
        // 测试带依赖的任务提交
        std::vector<TaskID> dependencies = {taskId1, taskId2};
        TaskID taskId3 = scheduler.submitTask(TaskType::DATA_ANALYSIS, Priority::LOW, simpleTask, dependencies);
        assert(taskId3 > taskId2);
        
        // 测试使用shared_ptr提交任务
        auto task = std::make_shared<Task>(0, TaskType::IMAGE_PROCESSING, Priority::CRITICAL, simpleTask);
        TaskID taskId4 = scheduler.submitTask(task);
        assert(taskId4 > taskId3);
        
        scheduler.shutdown();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in testTaskSubmission: " << e.what() << std::endl;
        return false;
    }
}

// 测试用例3: 测试任务状态查询
bool testTaskStatusQuery() {
    try {
        SchedulerConfig config;
        TaskScheduler scheduler(config);
        scheduler.initialize(config);
        
        auto simpleTask = []() -> TaskResult {
            TaskResult result;
            result.status = ResultStatus::SUCCESS;
            return result;
        };
        
        // 提交任务
        TaskID taskId = scheduler.submitTask(TaskType::USER_DEFINED, Priority::NORMAL, simpleTask);
        
        // 查询任务状态
        TaskStatus status = scheduler.getTaskStatus(taskId);
        assert(status == TaskStatus::PENDING);
        
        // 查询不存在的任务
        TaskStatus invalidStatus = scheduler.getTaskStatus(999999);
        assert(invalidStatus == TaskStatus::CANCELLED); // 不存在的任务返回CANCELLED
        
        scheduler.shutdown();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in testTaskStatusQuery: " << e.what() << std::endl;
        return false;
    }
}

// 测试用例4: 测试任务取消功能
bool testTaskCancellation() {
    try {
        SchedulerConfig config;
        TaskScheduler scheduler(config);
        scheduler.initialize(config);
        
        auto simpleTask = []() -> TaskResult {
            TaskResult result;
            result.status = ResultStatus::SUCCESS;
            return result;
        };
        
        // 提交任务
        TaskID taskId = scheduler.submitTask(TaskType::USER_DEFINED, Priority::NORMAL, simpleTask);
        
        // 取消任务
        bool cancelResult = scheduler.cancelTask(taskId);
        assert(cancelResult == true);
        
        // 验证状态
        TaskStatus status = scheduler.getTaskStatus(taskId);
        assert(status == TaskStatus::CANCELLED);
        
        // 尝试取消不存在的任务
        bool invalidCancel = scheduler.cancelTask(999999);
        assert(invalidCancel == false);
        
        scheduler.shutdown();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in testTaskCancellation: " << e.what() << std::endl;
        return false;
    }
}

// 测试用例5: 测试性能指标获取
bool testPerformanceMetrics() {
    try {
        SchedulerConfig config;
        TaskScheduler scheduler(config);
        scheduler.initialize(config);
        
        auto simpleTask = []() -> TaskResult {
            TaskResult result;
            result.status = ResultStatus::SUCCESS;
            return result;
        };
        
        // 提交多个任务
        for (int i = 0; i < 5; ++i) {
            scheduler.submitTask(TaskType::USER_DEFINED, Priority::NORMAL, simpleTask);
        }
        
        // 获取性能指标
        PerformanceMetrics metrics = scheduler.getPerformanceMetrics();
        assert(metrics.totalTasksSubmitted == 5);
        assert(metrics.totalTasksCompleted == 0); // 在里程碑1中，任务不会实际执行
        assert(metrics.totalTasksFailed == 0);
        
        scheduler.shutdown();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in testPerformanceMetrics: " << e.what() << std::endl;
        return false;
    }
}

// 测试用例6: 测试队列状态查询
bool testQueueStatus() {
    try {
        SchedulerConfig config;
        TaskScheduler scheduler(config);
        scheduler.initialize(config);
        
        auto simpleTask = []() -> TaskResult {
            TaskResult result;
            result.status = ResultStatus::SUCCESS;
            return result;
        };
        
        // 提交不同优先级的任务
        scheduler.submitTask(TaskType::USER_DEFINED, Priority::CRITICAL, simpleTask);
        scheduler.submitTask(TaskType::USER_DEFINED, Priority::HIGH, simpleTask);
        scheduler.submitTask(TaskType::USER_DEFINED, Priority::NORMAL, simpleTask);
        scheduler.submitTask(TaskType::USER_DEFINED, Priority::LOW, simpleTask);
        scheduler.submitTask(TaskType::USER_DEFINED, Priority::BACKGROUND, simpleTask);
        
        // 获取队列状态
        QueueStatus status = scheduler.getQueueStatus();
        assert(status.pendingTasks == 5);
        assert(status.runningTasks == 0); // 在里程碑1中，没有运行的任务
        assert(status.priorityDistribution.size() == 5);
        
        scheduler.shutdown();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in testQueueStatus: " << e.what() << std::endl;
        return false;
    }
}

// 测试用例7: 测试配置管理
bool testConfigManagement() {
    try {
        SchedulerConfig config;
        config.minThreads = 2;
        config.maxThreads = 8;
        config.maxQueueSize = 500;
        
        TaskScheduler scheduler(config);
        scheduler.initialize(config);
        
        // 获取配置
        SchedulerConfig retrievedConfig = scheduler.getConfig();
        assert(retrievedConfig.minThreads == 2);
        assert(retrievedConfig.maxThreads == 8);
        assert(retrievedConfig.maxQueueSize == 500);
        
        // 更新配置
        SchedulerConfig newConfig;
        newConfig.minThreads = 4;
        newConfig.maxThreads = 16;
        scheduler.updateConfig(newConfig);
        
        // 验证更新
        retrievedConfig = scheduler.getConfig();
        assert(retrievedConfig.minThreads == 4);
        assert(retrievedConfig.maxThreads == 16);
        
        scheduler.shutdown();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in testConfigManagement: " << e.what() << std::endl;
        return false;
    }
}

// 测试用例8: 测试暂停和恢复功能
bool testPauseAndResume() {
    try {
        SchedulerConfig config;
        TaskScheduler scheduler(config);
        scheduler.initialize(config);
        
        // 初始状态应该是未暂停
        assert(!scheduler.isPaused());
        
        // 暂停调度
        scheduler.pauseScheduling();
        assert(scheduler.isPaused());
        
        // 恢复调度
        scheduler.resumeScheduling();
        assert(!scheduler.isPaused());
        
        scheduler.shutdown();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in testPauseAndResume: " << e.what() << std::endl;
        return false;
    }
}

// 测试用例9: 测试工具函数
bool testUtilityFunctions() {
    try {
        // 测试Priority转换
        assert(priorityToString(Priority::CRITICAL) == "CRITICAL");
        assert(priorityToString(Priority::HIGH) == "HIGH");
        assert(priorityToString(Priority::NORMAL) == "NORMAL");
        assert(priorityToString(Priority::LOW) == "LOW");
        assert(priorityToString(Priority::BACKGROUND) == "BACKGROUND");
        
        // 测试TaskStatus转换
        assert(taskStatusToString(TaskStatus::PENDING) == "PENDING");
        assert(taskStatusToString(TaskStatus::RUNNING) == "RUNNING");
        assert(taskStatusToString(TaskStatus::COMPLETED) == "COMPLETED");
        assert(taskStatusToString(TaskStatus::FAILED) == "FAILED");
        assert(taskStatusToString(TaskStatus::CANCELLED) == "CANCELLED");
        assert(taskStatusToString(TaskStatus::TIMEOUT) == "TIMEOUT");
        
        // 测试TaskType转换
        assert(taskTypeToString(TaskType::AI_INFERENCE) == "AI_INFERENCE");
        assert(taskTypeToString(TaskType::IMAGE_PROCESSING) == "IMAGE_PROCESSING");
        assert(taskTypeToString(TaskType::DATA_ANALYSIS) == "DATA_ANALYSIS");
        assert(taskTypeToString(TaskType::SYSTEM_MAINTENANCE) == "SYSTEM_MAINTENANCE");
        assert(taskTypeToString(TaskType::USER_DEFINED) == "USER_DEFINED");
        
        // 测试字符串到Priority的转换
        assert(stringToPriority("CRITICAL") == Priority::CRITICAL);
        assert(stringToPriority("HIGH") == Priority::HIGH);
        assert(stringToPriority("NORMAL") == Priority::NORMAL);
        assert(stringToPriority("LOW") == Priority::LOW);
        assert(stringToPriority("BACKGROUND") == Priority::BACKGROUND);
        assert(stringToPriority("INVALID") == Priority::NORMAL); // 默认值
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in testUtilityFunctions: " << e.what() << std::endl;
        return false;
    }
}

// 测试用例10: 测试数据结构
bool testDataStructures() {
    try {
        // 测试Task结构体
        auto taskFunc = []() -> TaskResult {
            TaskResult result;
            result.status = ResultStatus::SUCCESS;
            return result;
        };
        
        Task task1;
        assert(task1.id == 0);
        
        Task task2(123, TaskType::AI_INFERENCE, Priority::HIGH, taskFunc);
        assert(task2.id == 123);
        assert(task2.type == TaskType::AI_INFERENCE);
        assert(task2.priority == Priority::HIGH);
        assert(task2.timeout == std::chrono::milliseconds(30000)); // 默认超时
        
        // 测试TaskResult结构体
        TaskResult result1;
        assert(result1.taskId == 0);
        
        TaskResult result2(456, ResultStatus::SUCCESS);
        assert(result2.taskId == 456);
        assert(result2.status == ResultStatus::SUCCESS);
        
        // 测试PerformanceMetrics结构体
        PerformanceMetrics metrics;
        assert(metrics.totalTasksSubmitted == 0);
        assert(metrics.totalTasksCompleted == 0);
        assert(metrics.totalTasksFailed == 0);
        
        // 测试QueueStatus结构体
        QueueStatus queueStatus;
        assert(queueStatus.pendingTasks == 0);
        assert(queueStatus.runningTasks == 0);
        assert(queueStatus.completedTasks == 0);
        
        // 测试SchedulerConfig结构体
        SchedulerConfig config;
        assert(config.minThreads == 2);
        assert(config.maxThreads == 16);
        assert(config.maxQueueSize == 1000);
        assert(config.enableLoadBalancing == true);
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in testDataStructures: " << e.what() << std::endl;
        return false;
    }
}

// 主测试函数
int main() {
    std::cout << "\n=== TaskScheduler Basic Unit Tests ===\n" << std::endl;
    
    int totalTests = 0;
    int passedTests = 0;
    
    // 运行所有测试
    struct TestCase {
        std::string name;
        std::function<bool()> test;
    };
    
    std::vector<TestCase> tests = {
        {"Constructor and Initialization", testConstructorAndInitialization},
        {"Task Submission", testTaskSubmission},
        {"Task Status Query", testTaskStatusQuery},
        {"Task Cancellation", testTaskCancellation},
        {"Performance Metrics", testPerformanceMetrics},
        {"Queue Status", testQueueStatus},
        {"Config Management", testConfigManagement},
        {"Pause and Resume", testPauseAndResume},
        {"Utility Functions", testUtilityFunctions},
        {"Data Structures", testDataStructures}
    };
    
    for (const auto& test : tests) {
        totalTests++;
        bool passed = test.test();
        if (passed) passedTests++;
        printTestResult(test.name, passed);
        std::cout.flush(); // 强制刷新输出
    }
    
    // 打印总结
    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Total Tests: " << totalTests << std::endl;
    std::cout << "Passed: " << passedTests << std::endl;
    std::cout << "Failed: " << (totalTests - passedTests) << std::endl;
    std::cout << "Coverage: " << std::fixed << std::setprecision(1) << (passedTests * 100.0 / totalTests) << "%" << std::endl;
    std::cout << std::endl;
    
    return (passedTests == totalTests) ? 0 : 1;
}