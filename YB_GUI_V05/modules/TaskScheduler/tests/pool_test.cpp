#include "../include/ThreadPool.h"
#include <iostream>
#include <chrono>
#include <atomic>

using namespace YB;
using namespace std::chrono_literals;

int main() {
    std::cout << "=== ThreadPool Test ===" << std::endl;
    
    ThreadPool pool(2);
    std::atomic<int> counter{0};
    
    std::cout << "Pool size: " << pool.getPoolSize() << std::endl;
    
    // 提交任务
    std::cout << "\nSubmitting 5 tasks..." << std::endl;
    
    for (int i = 0; i < 5; ++i) {
        auto future = pool.enqueue([&counter, i] {
            std::cout << "Task " << i << " executing in thread: " 
                      << std::this_thread::get_id() << std::endl;
            counter++;
            std::this_thread::sleep_for(100ms);
            return i * 2;
        });
        
        std::cout << "Task " << i << " submitted" << std::endl;
    }
    
    std::cout << "\nWaiting for completion..." << std::endl;
    std::this_thread::sleep_for(1s);
    
    std::cout << "\nTasks executed: " << counter.load() << std::endl;
    
    return 0;
}