#include "../include/TaskScheduler.h"
#include "../include/ThreadPool.h"
#include "../include/PriorityQueue.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace YB;
using namespace std::chrono_literals;

int main() {
    std::cout << "=== TaskScheduler Debug Test ===" << std::endl;
    
    SchedulerConfig config;
    config.minThreads = 2;
    config.maxThreads = 4;
    
    TaskScheduler scheduler;
    
    std::cout << "Step 1: Initializing scheduler..." << std::endl;
    if (!scheduler.initialize(config)) {
        std::cerr << "Failed to initialize!" << std::endl;
        return 1;
    }
    std::cout << "Initialization successful!" << std::endl;
    
    std::cout << "\nStep 2: Checking running status..." << std::endl;
    std::cout << "Is running: " << (scheduler.isRunning() ? "yes" : "no") << std::endl;
    
    std::cout << "\nStep 3: Waiting 1 second..." << std::endl;
    std::this_thread::sleep_for(1s);
    
    std::cout << "\nStep 4: Getting performance metrics..." << std::endl;
    auto metrics = scheduler.getPerformanceMetrics();
    std::cout << "Active threads: " << metrics.currentActiveThreads << std::endl;
    std::cout << "Queue size: " << metrics.currentQueueSize << std::endl;
    
    std::cout << "\nStep 5: Shutting down..." << std::endl;
    scheduler.shutdown();
    
    std::cout << "\nStep 6: Checking running status after shutdown..." << std::endl;
    std::cout << "Is running: " << (scheduler.isRunning() ? "yes" : "no") << std::endl;
    
    std::cout << "\nDebug test completed!" << std::endl;
    return 0;
}