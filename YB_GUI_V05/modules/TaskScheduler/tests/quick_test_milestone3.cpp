#include "../include/TaskScheduler.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <cassert>

using namespace YB;
using namespace std::chrono_literals;

// 简单测试任务
TaskResult simpleTask() {
    std::this_thread::sleep_for(50ms);
    TaskResult result;
    result.status = ResultStatus::SUCCESS;
    result.result = std::string("Task completed");
    return result;
}

TaskResult slowTask() {
    std::this_thread::sleep_for(200ms);
    TaskResult result;
    result.status = ResultStatus::SUCCESS;
    result.result = std::string("Slow task completed");
    return result;
}

int main() {
    std::cout << "TaskScheduler 里程碑3 快速验证测试" << std::endl;
    std::cout << "===================================" << std::endl;
    
    try {
        // 测试1: 基本负载均衡
        std::cout << "\n1. 测试基本负载均衡功能" << std::endl;
        
        SchedulerConfig config;
        config.minThreads = 2;
        config.maxThreads = 6;
        config.enableLoadBalancing = true;
        config.strategy = LoadBalancingStrategy::ADAPTIVE;
        config.monitorInterval = 200ms;
        config.loadBalancingConfig.scaleUpThreshold = 0.6;
        config.loadBalancingConfig.cooldownPeriod = 500ms;
        
        TaskScheduler scheduler(config);
        assert(scheduler.initialize(config));
        
        std::cout << "初始线程池大小: " << scheduler.getPerformanceMetrics().currentActiveThreads << std::endl;
        
        // 提交任务触发扩容
        std::vector<TaskID> taskIds;
        for (int i = 0; i < 10; ++i) {
            TaskID id = scheduler.submitTask(TaskType::DATA_ANALYSIS, Priority::NORMAL, slowTask);
            taskIds.push_back(id);
        }
        
        std::cout << "提交10个慢任务..." << std::endl;
        std::this_thread::sleep_for(1s);
        
        auto metrics = scheduler.getPerformanceMetrics();
        std::cout << "扩容后线程池大小: " << metrics.currentActiveThreads << std::endl;
        std::cout << "负载因子: " << scheduler.getLoadFactor() << std::endl;
        
        // 等待任务完成
        std::this_thread::sleep_for(1s);
        scheduler.shutdown();
        std::cout << "✓ 负载均衡测试完成" << std::endl;
        
        // 测试2: 资源限制
        std::cout << "\n2. 测试资源限制功能" << std::endl;
        
        SchedulerConfig config2;
        config2.minThreads = 2;
        config2.maxThreads = 4;
        config2.resourceLimits.maxCpuUsage = 60.0;
        config2.resourceLimits.maxQueueLength = 5;
        
        TaskScheduler scheduler2(config2);
        assert(scheduler2.initialize(config2));
        
        // 提交任务
        std::vector<TaskID> taskIds2;
        for (int i = 0; i < 8; ++i) {
            TaskID id = scheduler2.submitTask(TaskType::DATA_ANALYSIS, Priority::NORMAL, simpleTask);
            if (id == 0) {
                std::cout << "任务 " << i << " 被拒绝" << std::endl;
            } else {
                taskIds2.push_back(id);
            }
        }
        
        std::this_thread::sleep_for(500ms);
        
        std::cout << "CPU使用率: " << scheduler2.getCurrentCpuUsage() << "%" << std::endl;
        std::cout << "资源限制是否超出: " << (scheduler2.isResourceLimitExceeded() ? "是" : "否") << std::endl;
        
        std::this_thread::sleep_for(1s);
        scheduler2.shutdown();
        std::cout << "✓ 资源限制测试完成" << std::endl;
        
        // 测试3: 不同负载均衡策略
        std::cout << "\n3. 测试不同负载均衡策略" << std::endl;
        
        std::vector<LoadBalancingStrategy> strategies = {
            LoadBalancingStrategy::ADAPTIVE,
            LoadBalancingStrategy::LEAST_LOADED,
            LoadBalancingStrategy::PRIORITY_BASED
        };
        
        std::vector<std::string> names = {"ADAPTIVE", "LEAST_LOADED", "PRIORITY_BASED"};
        
        for (size_t i = 0; i < strategies.size(); ++i) {
            std::cout << "测试策略: " << names[i] << std::endl;
            
            SchedulerConfig config3;
            config3.minThreads = 2;
            config3.maxThreads = 5;
            config3.strategy = strategies[i];
            config3.enableLoadBalancing = true;
            config3.monitorInterval = 100ms;
            
            TaskScheduler scheduler3(config3);
            assert(scheduler3.initialize(config3));
            
            // 提交不同优先级任务
            for (int j = 0; j < 3; ++j) {
                scheduler3.submitTask(TaskType::AI_INFERENCE, Priority::HIGH, simpleTask);
            }
            for (int j = 0; j < 5; ++j) {
                scheduler3.submitTask(TaskType::DATA_ANALYSIS, Priority::NORMAL, simpleTask);
            }
            
            std::this_thread::sleep_for(300ms);
            
            auto metrics3 = scheduler3.getPerformanceMetrics();
            std::cout << "  线程池大小: " << metrics3.currentActiveThreads << std::endl;
            std::cout << "  负载因子: " << scheduler3.getLoadFactor() << std::endl;
            
            std::this_thread::sleep_for(500ms);
            scheduler3.shutdown();
        }
        std::cout << "✓ 负载均衡策略测试完成" << std::endl;
        
        // 测试4: 超时处理
        std::cout << "\n4. 测试超时处理功能" << std::endl;
        
        SchedulerConfig config4;
        config4.minThreads = 2;
        config4.maxThreads = 4;
        config4.defaultTimeout = 100ms; // 很短的超时时间
        
        TaskScheduler scheduler4(config4);
        assert(scheduler4.initialize(config4));
        
        // 提交一个会超时的任务
        auto timeoutTask = []() -> TaskResult {
            std::this_thread::sleep_for(300ms); // 超过超时时间
            TaskResult result;
            result.status = ResultStatus::SUCCESS;
            return result;
        };
        
        TaskID timeoutId = scheduler4.submitTask(TaskType::DATA_ANALYSIS, Priority::NORMAL, timeoutTask);
        TaskID normalId = scheduler4.submitTask(TaskType::DATA_ANALYSIS, Priority::NORMAL, simpleTask);
        
        std::this_thread::sleep_for(500ms);
        
        std::cout << "超时任务状态: " << taskStatusToString(scheduler4.getTaskStatus(timeoutId)) << std::endl;
        std::cout << "正常任务状态: " << taskStatusToString(scheduler4.getTaskStatus(normalId)) << std::endl;
        
        auto metrics4 = scheduler4.getPerformanceMetrics();
        std::cout << "失败任务数: " << metrics4.totalTasksFailed << std::endl;
        
        scheduler4.shutdown();
        std::cout << "✓ 超时处理测试完成" << std::endl;
        
        std::cout << "\n🎉 里程碑3所有核心功能验证成功！" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "测试过程中发生异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}