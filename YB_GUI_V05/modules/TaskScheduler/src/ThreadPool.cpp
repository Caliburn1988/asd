#include "../include/ThreadPool.h"
#include <iostream>
#include <algorithm>

namespace YB {

ThreadPool::ThreadPool(size_t numThreads) 
    : stop_(false), activeThreads_(0), threadsToRemove_(0), poolSize_(numThreads) {
    
    if (numThreads == 0) {
        throw std::invalid_argument("ThreadPool size must be greater than 0");
    }
    
    addThreads(numThreads);
}

ThreadPool::~ThreadPool() {
    stop();
    
    // 等待所有线程结束
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::workerThread() {
    while (true) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            
            // 等待任务或停止信号
            condition_.wait(lock, [this] {
                return stop_ || !tasks_.empty() || threadsToRemove_ > 0;
            });
            
            // 检查是否需要减少线程
            if (threadsToRemove_ > 0 && tasks_.empty()) {
                threadsToRemove_--;
                poolSize_--;
                resizeCondition_.notify_all();
                return;
            }
            
            // 检查是否停止
            if (stop_ && tasks_.empty()) {
                return;
            }
            
            // 获取任务
            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
                activeThreads_++;
            }
        }
        
        // 执行任务
        if (task) {
            try {
                task();
            } catch (const std::exception& e) {
                std::cerr << "Exception in thread pool task: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown exception in thread pool task" << std::endl;
            }
            
            activeThreads_--;
        }
    }
}

void ThreadPool::addThreads(size_t count) {
    for (size_t i = 0; i < count; ++i) {
        workers_.emplace_back([this] { workerThread(); });
    }
}

void ThreadPool::removeThreads(size_t count) {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        threadsToRemove_ += count;
    }
    
    // 唤醒线程以便它们可以检查是否需要退出
    condition_.notify_all();
    
    // 等待线程退出
    std::unique_lock<std::mutex> lock(queueMutex_);
    resizeCondition_.wait(lock, [this] {
        return threadsToRemove_ == 0;
    });
    
    // 清理已结束的线程
    workers_.erase(
        std::remove_if(workers_.begin(), workers_.end(),
            [](std::thread& t) { return !t.joinable(); }),
        workers_.end()
    );
}

void ThreadPool::resize(size_t newSize) {
    if (newSize == 0) {
        throw std::invalid_argument("ThreadPool size must be greater than 0");
    }
    
    size_t currentSize = poolSize_.load();
    
    if (newSize > currentSize) {
        // 增加线程
        addThreads(newSize - currentSize);
        poolSize_ = newSize;
    } else if (newSize < currentSize) {
        // 减少线程
        removeThreads(currentSize - newSize);
    }
}

size_t ThreadPool::getActiveThreads() const {
    return activeThreads_.load();
}

size_t ThreadPool::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return tasks_.size();
}

size_t ThreadPool::getPoolSize() const {
    return poolSize_.load();
}

void ThreadPool::stop() {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        stop_ = true;
    }
    condition_.notify_all();
}

bool ThreadPool::isStopped() const {
    return stop_.load();
}

} // namespace YB