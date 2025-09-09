#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>
#include <functional>
#include "TaskScheduler.h"

namespace YB {

class PriorityQueue {
public:
    PriorityQueue();
    ~PriorityQueue();
    
    // 禁用拷贝构造和拷贝赋值
    PriorityQueue(const PriorityQueue&) = delete;
    PriorityQueue& operator=(const PriorityQueue&) = delete;
    
    // 添加任务到队列
    void push(std::shared_ptr<Task> task);
    
    // 从队列取出最高优先级任务（阻塞直到有任务）
    std::shared_ptr<Task> pop();
    
    // 尝试从队列取出任务（非阻塞）
    std::shared_ptr<Task> tryPop();
    
    // 带超时的pop
    std::shared_ptr<Task> popWithTimeout(std::chrono::milliseconds timeout);
    
    // 检查队列是否为空
    bool empty() const;
    
    // 获取队列大小
    size_t size() const;
    
    // 清空队列
    void clear();
    
    // 停止队列（唤醒所有等待的线程）
    void stop();
    
    // 恢复队列
    void resume();
    
    // 检查队列是否停止
    bool isStopped() const;
    
    // 获取各优先级任务的分布
    std::map<Priority, size_t> getPriorityDistribution() const;
    
    // 移除指定ID的任务
    bool removeTask(TaskID taskId);
    
    // 获取队列中所有任务ID
    std::vector<TaskID> getAllTaskIds() const;
    
private:
    // 内部优先级队列
    std::priority_queue<std::shared_ptr<Task>, 
                       std::vector<std::shared_ptr<Task>>, 
                       TaskComparator> queue_;
    
    // 同步相关
    mutable std::mutex mutex_;
    std::condition_variable notEmpty_;
    
    // 状态控制
    std::atomic<bool> stopped_;
    
    // 任务计数（按优先级）
    mutable std::map<Priority, size_t> priorityCount_;
    
    // 辅助函数
    void updatePriorityCount(Priority priority, int delta);
};

} // namespace YB

#endif // PRIORITY_QUEUE_H