#include "../include/TaskScheduler.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

using namespace YB;

TaskResult lightTask() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    TaskResult result;
    result.status = ResultStatus::SUCCESS;
    result.result = std::string("Light task completed");
    return result;
}

int main() {
    std::cout << "TaskScheduler 负载均衡演示" << std::endl;
    std::cout << "========================" << std::endl;
    
    // 配置调度器
    SchedulerConfig config;
    config.minThreads = 2;
    config.maxThreads = 6;
    config.enableLoadBalancing = true;
    config.strategy = LoadBalancingStrategy::ADAPTIVE;
    config.monitorInterval = std::chrono::milliseconds(500);
    config.loadBalancingConfig.scaleUpThreshold = 0.6;
    config.loadBalancingConfig.scaleDownThreshold = 0.3;
    config.loadBalancingConfig.cooldownPeriod = std::chrono::milliseconds(1000);
    
    TaskScheduler scheduler(config);
    if (!scheduler.initialize(config)) {
        std::cerr << "调度器初始化失败" << std::endl;
        return 1;
    }
    
    std::cout << "初始状态:" << std::endl;
    std::cout << "  线程数: " << scheduler.getPerformanceMetrics().currentActiveThreads << std::endl;
    std::cout << "  负载因子: " << scheduler.getLoadFactor() << std::endl;
    
    // 阶段1：提交少量任务
    std::cout << "\n阶段1：提交3个任务..." << std::endl;
    for (int i = 0; i < 3; ++i) {
        scheduler.submitTask(TaskType::DATA_ANALYSIS, Priority::NORMAL, lightTask);
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto metrics = scheduler.getPerformanceMetrics();
    std::cout << "  线程数: " << metrics.currentActiveThreads << std::endl;
    std::cout << "  队列大小: " << metrics.currentQueueSize << std::endl;
    std::cout << "  负载因子: " << scheduler.getLoadFactor() << std::endl;
    
    // 阶段2：提交大量任务触发扩容
    std::cout << "\n阶段2：提交15个任务触发扩容..." << std::endl;
    for (int i = 0; i < 15; ++i) {
        scheduler.submitTask(TaskType::DATA_ANALYSIS, Priority::NORMAL, lightTask);
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    metrics = scheduler.getPerformanceMetrics();
    std::cout << "  线程数: " << metrics.currentActiveThreads << std::endl;
    std::cout << "  队列大小: " << metrics.currentQueueSize << std::endl;
    std::cout << "  负载因子: " << scheduler.getLoadFactor() << std::endl;
    
    // 阶段3：等待任务完成，观察缩容
    std::cout << "\n阶段3：等待任务完成，观察自动缩容..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    metrics = scheduler.getPerformanceMetrics();
    std::cout << "  线程数: " << metrics.currentActiveThreads << std::endl;
    std::cout << "  队列大小: " << metrics.currentQueueSize << std::endl;
    std::cout << "  负载因子: " << scheduler.getLoadFactor() << std::endl;
    std::cout << "  完成任务数: " << metrics.totalTasksCompleted << std::endl;
    
    std::cout << "\n演示完成！负载均衡功能正常工作。" << std::endl;
    
    scheduler.shutdown();
    return 0;
}