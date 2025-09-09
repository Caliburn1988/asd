#include "../include/TaskScheduler.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace YB;
using namespace std::chrono_literals;

int main() {
    std::cout << "=== Shutdown Test ===" << std::endl;
    
    SchedulerConfig config;
    config.minThreads = 1;  // 使用最少的线程
    config.maxThreads = 2;
    
    TaskScheduler scheduler;
    
    std::cout << "Step 1: Initialize" << std::endl;
    scheduler.initialize(config);
    
    std::cout << "Step 2: Wait 1 second" << std::endl;
    std::this_thread::sleep_for(1s);
    
    std::cout << "Step 3: Begin shutdown" << std::endl;
    std::cout.flush();
    
    // 在另一个线程中调用shutdown，这样我们可以看到是否卡住
    std::thread shutdownThread([&scheduler] {
        std::cout << "Shutdown thread: calling shutdown()" << std::endl;
        std::cout.flush();
        scheduler.shutdown();
        std::cout << "Shutdown thread: shutdown() returned" << std::endl;
        std::cout.flush();
    });
    
    // 等待最多3秒
    std::cout << "Main thread: waiting for shutdown..." << std::endl;
    std::cout.flush();
    
    for (int i = 0; i < 30; ++i) {
        std::this_thread::sleep_for(100ms);
        if (!scheduler.isRunning()) {
            std::cout << "Main thread: scheduler stopped!" << std::endl;
            break;
        }
        if (i % 10 == 0) {
            std::cout << "Main thread: still waiting... " << i/10 << "s" << std::endl;
            std::cout.flush();
        }
    }
    
    if (shutdownThread.joinable()) {
        shutdownThread.join();
    }
    
    std::cout << "Test completed!" << std::endl;
    return 0;
}