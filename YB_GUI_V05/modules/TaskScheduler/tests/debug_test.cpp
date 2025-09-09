#include "../include/PriorityQueue.h"
#include "../include/TaskScheduler.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace YB;
using namespace std::chrono_literals;

int main() {
    std::cout << "=== Debug Test ===" << std::endl;
    
    // Test 1: Basic PriorityQueue
    {
        std::cout << "\nTest 1: Basic PriorityQueue" << std::endl;
        PriorityQueue queue;
        
        auto task = std::make_shared<Task>(1, TaskType::USER_DEFINED, Priority::NORMAL, nullptr);
        std::cout << "Pushing task..." << std::endl;
        queue.push(task);
        
        std::cout << "Queue size: " << queue.size() << std::endl;
        
        std::cout << "Trying to pop..." << std::endl;
        auto popped = queue.tryPop();
        if (popped) {
            std::cout << "Popped task ID: " << popped->id << std::endl;
        } else {
            std::cout << "Failed to pop task!" << std::endl;
        }
    }
    
    // Test 2: Pop with timeout
    {
        std::cout << "\nTest 2: Pop with timeout on empty queue" << std::endl;
        PriorityQueue queue;
        
        auto start = std::chrono::steady_clock::now();
        auto task = queue.popWithTimeout(500ms);
        auto end = std::chrono::steady_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Waited for: " << duration.count() << " ms" << std::endl;
        
        if (!task) {
            std::cout << "Correctly returned nullptr after timeout" << std::endl;
        }
    }
    
    // Test 3: Blocking pop with producer
    {
        std::cout << "\nTest 3: Blocking pop with producer" << std::endl;
        PriorityQueue queue;
        
        std::thread producer([&queue] {
            std::this_thread::sleep_for(100ms);
            auto task = std::make_shared<Task>(2, TaskType::USER_DEFINED, Priority::HIGH, nullptr);
            std::cout << "Producer: pushing task" << std::endl;
            queue.push(task);
        });
        
        std::cout << "Consumer: waiting for task..." << std::endl;
        auto task = queue.pop();
        if (task) {
            std::cout << "Consumer: got task ID: " << task->id << std::endl;
        }
        
        producer.join();
    }
    
    std::cout << "\nAll tests completed!" << std::endl;
    return 0;
}