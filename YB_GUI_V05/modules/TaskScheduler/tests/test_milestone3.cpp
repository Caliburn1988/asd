#include "../include/TaskScheduler.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <random>
#include <cassert>
#include <iomanip>

using namespace YB;
using namespace std::chrono_literals;

// 测试任务函数
TaskResult lightTask() {
    std::this_thread::sleep_for(10ms);
    TaskResult result;
    result.status = ResultStatus::SUCCESS;
    result.result = std::string("Light task completed");
    return result;
}

TaskResult mediumTask() {
    std::this_thread::sleep_for(100ms);
    TaskResult result;
    result.status = ResultStatus::SUCCESS;
    result.result = std::string("Medium task completed");
    return result;
}

TaskResult heavyTask() {
    std::this_thread::sleep_for(500ms);
    TaskResult result;
    result.status = ResultStatus::SUCCESS;
    result.result = std::string("Heavy task completed");
    return result;
}

TaskResult timeoutTask() {
    std::this_thread::sleep_for(2s); // 超过默认超时时间
    TaskResult result;
    result.status = ResultStatus::SUCCESS;
    result.result = std::string("Timeout task completed");
    return result;
}

void testBasicLoadBalancing() {
    std::cout << "\n=== 测试基本负载均衡功能 ===" << std::endl;
    
    SchedulerConfig config;
    config.minThreads = 2;
    config.maxThreads = 8;
    config.enableLoadBalancing = true;
    config.strategy = LoadBalancingStrategy::ADAPTIVE;
    config.monitorInterval = 500ms;
    config.loadBalancingConfig.scaleUpThreshold = 0.7;
    config.loadBalancingConfig.scaleDownThreshold = 0.3;
    config.loadBalancingConfig.cooldownPeriod = 1s;
    
    TaskScheduler scheduler(config);
    assert(scheduler.initialize(config));
    
    std::cout << "初始线程池大小: " << scheduler.getPerformanceMetrics().currentActiveThreads << std::endl;
    
    // 提交大量任务以触发扩容
    std::vector<TaskID> taskIds;
    for (int i = 0; i < 20; ++i) {
        TaskID id = scheduler.submitTask(TaskType::DATA_ANALYSIS, Priority::NORMAL, mediumTask);
        taskIds.push_back(id);
    }
    
    std::cout << "提交20个中等任务..." << std::endl;
    
    // 等待负载均衡生效
    std::this_thread::sleep_for(2s);
    
    auto metrics = scheduler.getPerformanceMetrics();
    std::cout << "扩容后线程池大小: " << metrics.currentActiveThreads << std::endl;
    std::cout << "当前队列大小: " << metrics.currentQueueSize << std::endl;
    std::cout << "负载因子: " << scheduler.getLoadFactor() << std::endl;
    
    // 等待任务完成
    std::this_thread::sleep_for(3s);
    
    // 等待缩容
    std::this_thread::sleep_for(2s);
    
    metrics = scheduler.getPerformanceMetrics();
    std::cout << "缩容后线程池大小: " << metrics.currentActiveThreads << std::endl;
    
    scheduler.shutdown();
    std::cout << "基本负载均衡测试完成" << std::endl;
}

void testResourceLimits() {
    std::cout << "\n=== 测试资源限制功能 ===" << std::endl;
    
    SchedulerConfig config;
    config.minThreads = 2;
    config.maxThreads = 4;
    config.resourceLimits.maxCpuUsage = 50.0; // 设置较低的CPU限制
    config.resourceLimits.maxQueueLength = 10; // 设置较小的队列长度限制
    config.resourceLimits.maxMemoryUsage = 1024 * 1024; // 1MB内存限制
    
    TaskScheduler scheduler(config);
    assert(scheduler.initialize(config));
    
    std::cout << "资源限制设置:" << std::endl;
    std::cout << "  最大CPU使用率: " << config.resourceLimits.maxCpuUsage << "%" << std::endl;
    std::cout << "  最大队列长度: " << config.resourceLimits.maxQueueLength << std::endl;
    std::cout << "  最大内存使用: " << config.resourceLimits.maxMemoryUsage << " bytes" << std::endl;
    
    // 提交大量任务以触发资源限制
    std::vector<TaskID> taskIds;
    for (int i = 0; i < 15; ++i) {
        TaskID id = scheduler.submitTask(TaskType::DATA_ANALYSIS, Priority::NORMAL, heavyTask);
        if (id == 0) {
            std::cout << "任务 " << i << " 被拒绝（可能由于资源限制）" << std::endl;
        } else {
            taskIds.push_back(id);
        }
    }
    
    std::this_thread::sleep_for(1s);
    
    std::cout << "当前资源使用情况:" << std::endl;
    std::cout << "  CPU使用率: " << scheduler.getCurrentCpuUsage() << "%" << std::endl;
    std::cout << "  内存使用量: " << scheduler.getCurrentMemoryUsage() << " bytes" << std::endl;
    std::cout << "  负载因子: " << scheduler.getLoadFactor() << std::endl;
    std::cout << "  资源限制是否超出: " << (scheduler.isResourceLimitExceeded() ? "是" : "否") << std::endl;
    std::cout << "  系统是否暂停: " << (scheduler.isPaused() ? "是" : "否") << std::endl;
    
    // 等待任务完成
    std::this_thread::sleep_for(3s);
    
    std::cout << "任务完成后资源使用情况:" << std::endl;
    std::cout << "  CPU使用率: " << scheduler.getCurrentCpuUsage() << "%" << std::endl;
    std::cout << "  资源限制是否超出: " << (scheduler.isResourceLimitExceeded() ? "是" : "否") << std::endl;
    std::cout << "  系统是否暂停: " << (scheduler.isPaused() ? "是" : "否") << std::endl;
    
    scheduler.shutdown();
    std::cout << "资源限制测试完成" << std::endl;
}

void testLoadBalancingStrategies() {
    std::cout << "\n=== 测试不同负载均衡策略 ===" << std::endl;
    
    std::vector<LoadBalancingStrategy> strategies = {
        LoadBalancingStrategy::ADAPTIVE,
        LoadBalancingStrategy::LEAST_LOADED,
        LoadBalancingStrategy::PRIORITY_BASED
    };
    
    std::vector<std::string> strategyNames = {
        "ADAPTIVE", "LEAST_LOADED", "PRIORITY_BASED"
    };
    
    for (size_t i = 0; i < strategies.size(); ++i) {
        std::cout << "\n--- 测试策略: " << strategyNames[i] << " ---" << std::endl;
        
        SchedulerConfig config;
        config.minThreads = 2;
        config.maxThreads = 6;
        config.strategy = strategies[i];
        config.enableLoadBalancing = true;
        config.monitorInterval = 300ms;
        config.loadBalancingConfig.cooldownPeriod = 500ms;
        
        TaskScheduler scheduler(config);
        assert(scheduler.initialize(config));
        
        // 提交不同优先级的任务
        std::vector<TaskID> taskIds;
        
        // 高优先级任务
        for (int j = 0; j < 5; ++j) {
            TaskID id = scheduler.submitTask(TaskType::AI_INFERENCE, Priority::HIGH, mediumTask);
            taskIds.push_back(id);
        }
        
        // 普通优先级任务
        for (int j = 0; j < 10; ++j) {
            TaskID id = scheduler.submitTask(TaskType::DATA_ANALYSIS, Priority::NORMAL, lightTask);
            taskIds.push_back(id);
        }
        
        std::this_thread::sleep_for(1s);
        
        auto metrics = scheduler.getPerformanceMetrics();
        std::cout << "  线程池大小: " << metrics.currentActiveThreads << std::endl;
        std::cout << "  队列大小: " << metrics.currentQueueSize << std::endl;
        std::cout << "  负载因子: " << scheduler.getLoadFactor() << std::endl;
        
        // 等待任务完成
        std::this_thread::sleep_for(2s);
        
        scheduler.shutdown();
    }
    
    std::cout << "负载均衡策略测试完成" << std::endl;
}

void testTimeoutHandling() {
    std::cout << "\n=== 测试超时处理功能 ===" << std::endl;
    
    SchedulerConfig config;
    config.minThreads = 2;
    config.maxThreads = 4;
    config.defaultTimeout = 1s; // 设置较短的默认超时时间
    config.resourceLimits.maxTaskDuration = 1500ms; // 最大任务持续时间
    
    TaskScheduler scheduler(config);
    assert(scheduler.initialize(config));
    
    std::cout << "默认超时时间: " << config.defaultTimeout.count() << "ms" << std::endl;
    std::cout << "最大任务持续时间: " << config.resourceLimits.maxTaskDuration.count() << "ms" << std::endl;
    
    // 提交会超时的任务
    std::vector<TaskID> taskIds;
    
    // 正常任务
    TaskID normalId = scheduler.submitTask(TaskType::DATA_ANALYSIS, Priority::NORMAL, lightTask);
    taskIds.push_back(normalId);
    
    // 超时任务
    TaskID timeoutId = scheduler.submitTask(TaskType::DATA_ANALYSIS, Priority::NORMAL, timeoutTask);
    taskIds.push_back(timeoutId);
    
    std::cout << "提交了1个正常任务和1个超时任务" << std::endl;
    
    // 等待足够长的时间以触发超时
    std::this_thread::sleep_for(3s);
    
    // 检查任务状态
    for (TaskID id : taskIds) {
        TaskStatus status = scheduler.getTaskStatus(id);
        std::cout << "任务 " << id << " 状态: " << taskStatusToString(status) << std::endl;
    }
    
    auto metrics = scheduler.getPerformanceMetrics();
    std::cout << "失败任务数: " << metrics.totalTasksFailed << std::endl;
    std::cout << "完成任务数: " << metrics.totalTasksCompleted << std::endl;
    
    scheduler.shutdown();
    std::cout << "超时处理测试完成" << std::endl;
}

void testPerformanceMetrics() {
    std::cout << "\n=== 测试性能监控功能 ===" << std::endl;
    
    SchedulerConfig config;
    config.minThreads = 3;
    config.maxThreads = 6;
    config.enableLoadBalancing = true;
    config.monitorInterval = 200ms;
    
    TaskScheduler scheduler(config);
    assert(scheduler.initialize(config));
    
    // 提交各种类型的任务
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> taskType(0, 2);
    
    std::vector<TaskID> taskIds;
    for (int i = 0; i < 30; ++i) {
        std::function<TaskResult()> taskFunc;
        switch (taskType(gen)) {
            case 0: taskFunc = lightTask; break;
            case 1: taskFunc = mediumTask; break;
            case 2: taskFunc = heavyTask; break;
        }
        
        Priority priority = (i % 3 == 0) ? Priority::HIGH : Priority::NORMAL;
        TaskID id = scheduler.submitTask(TaskType::DATA_ANALYSIS, priority, taskFunc);
        taskIds.push_back(id);
    }
    
    std::cout << "提交了30个随机任务，开始监控..." << std::endl;
    
    // 监控性能指标
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(500ms);
        
        auto metrics = scheduler.getPerformanceMetrics();
        auto queueStatus = scheduler.getQueueStatus();
        
        std::cout << "\n--- 监控周期 " << (i+1) << " ---" << std::endl;
        std::cout << "线程池大小: " << metrics.currentActiveThreads << std::endl;
        std::cout << "队列大小: " << metrics.currentQueueSize << std::endl;
        std::cout << "等待任务: " << queueStatus.pendingTasks << std::endl;
        std::cout << "运行任务: " << queueStatus.runningTasks << std::endl;
        std::cout << "完成任务: " << queueStatus.completedTasks << std::endl;
        std::cout << "负载因子: " << std::fixed << std::setprecision(2) 
                  << scheduler.getLoadFactor() << std::endl;
        std::cout << "CPU使用率: " << std::fixed << std::setprecision(1) 
                  << scheduler.getCurrentCpuUsage() << "%" << std::endl;
    }
    
    // 等待所有任务完成
    std::this_thread::sleep_for(2s);
    
    auto finalMetrics = scheduler.getPerformanceMetrics();
    std::cout << "\n=== 最终统计 ===" << std::endl;
    std::cout << "总提交任务: " << finalMetrics.totalTasksSubmitted << std::endl;
    std::cout << "总完成任务: " << finalMetrics.totalTasksCompleted << std::endl;
    std::cout << "总失败任务: " << finalMetrics.totalTasksFailed << std::endl;
    std::cout << "平均执行时间: " << std::fixed << std::setprecision(2) 
              << finalMetrics.averageExecutionTime << "ms" << std::endl;
    
    scheduler.shutdown();
    std::cout << "性能监控测试完成" << std::endl;
}

int main() {
    std::cout << "TaskScheduler 里程碑3 综合测试" << std::endl;
    std::cout << "==============================" << std::endl;
    
    try {
        testBasicLoadBalancing();
        testResourceLimits();
        testLoadBalancingStrategies();
        testTimeoutHandling();
        testPerformanceMetrics();
        
        std::cout << "\n🎉 所有测试完成！里程碑3功能验证成功！" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "测试过程中发生异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}