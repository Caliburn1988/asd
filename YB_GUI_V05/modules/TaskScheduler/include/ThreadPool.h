#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <future>
#include <stdexcept>
#include <memory>
#include <type_traits>

namespace YB {

class ThreadPool {
public:
    // 构造函数
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    
    // 析构函数
    ~ThreadPool();
    
    // 禁用拷贝构造和拷贝赋值
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    
    // 提交任务到线程池
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>;
    
    // 调整线程池大小
    void resize(size_t newSize);
    
    // 获取当前活跃线程数
    size_t getActiveThreads() const;
    
    // 获取队列中等待任务数
    size_t getQueueSize() const;
    
    // 获取线程池大小
    size_t getPoolSize() const;
    
    // 停止线程池
    void stop();
    
    // 检查线程池是否停止
    bool isStopped() const;
    
private:
    // 工作线程函数
    void workerThread();
    
    // 添加新线程
    void addThreads(size_t count);
    
    // 移除线程
    void removeThreads(size_t count);
    
private:
    // 工作线程
    std::vector<std::thread> workers_;
    
    // 任务队列
    std::queue<std::function<void()>> tasks_;
    
    // 同步相关
    mutable std::mutex queueMutex_;
    std::condition_variable condition_;
    std::condition_variable resizeCondition_;
    
    // 状态控制
    std::atomic<bool> stop_;
    std::atomic<size_t> activeThreads_;
    std::atomic<size_t> threadsToRemove_;
    
    // 线程池大小
    std::atomic<size_t> poolSize_;
};

// 模板函数实现
template<typename F, typename... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
    using return_type = typename std::invoke_result<F, Args...>::type;
    
    if (stop_) {
        throw std::runtime_error("enqueue on stopped ThreadPool");
    }
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> res = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        tasks_.emplace([task](){ (*task)(); });
    }
    
    condition_.notify_one();
    return res;
}

} // namespace YB

#endif // THREAD_POOL_H