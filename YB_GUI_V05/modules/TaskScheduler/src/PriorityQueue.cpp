#include "../include/PriorityQueue.h"
#include <algorithm>

namespace YB {

PriorityQueue::PriorityQueue() : stopped_(false) {
    // 初始化优先级计数
    priorityCount_[Priority::CRITICAL] = 0;
    priorityCount_[Priority::HIGH] = 0;
    priorityCount_[Priority::NORMAL] = 0;
    priorityCount_[Priority::LOW] = 0;
    priorityCount_[Priority::BACKGROUND] = 0;
}

PriorityQueue::~PriorityQueue() {
    stop();
}

void PriorityQueue::push(std::shared_ptr<Task> task) {
    if (!task) {
        throw std::invalid_argument("Cannot push null task to queue");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (stopped_) {
        throw std::runtime_error("Cannot push to stopped queue");
    }
    
    queue_.push(task);
    updatePriorityCount(task->priority, 1);
    notEmpty_.notify_one();
}

std::shared_ptr<Task> PriorityQueue::pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    notEmpty_.wait(lock, [this] {
        return stopped_ || !queue_.empty();
    });
    
    if (stopped_ && queue_.empty()) {
        return nullptr;
    }
    
    auto task = queue_.top();
    queue_.pop();
    updatePriorityCount(task->priority, -1);
    
    return task;
}

std::shared_ptr<Task> PriorityQueue::tryPop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (queue_.empty()) {
        return nullptr;
    }
    
    auto task = queue_.top();
    queue_.pop();
    updatePriorityCount(task->priority, -1);
    
    return task;
}

std::shared_ptr<Task> PriorityQueue::popWithTimeout(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (!notEmpty_.wait_for(lock, timeout, [this] {
        return stopped_ || !queue_.empty();
    })) {
        return nullptr; // 超时
    }
    
    if (stopped_ && queue_.empty()) {
        return nullptr;
    }
    
    auto task = queue_.top();
    queue_.pop();
    updatePriorityCount(task->priority, -1);
    
    return task;
}

bool PriorityQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

size_t PriorityQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

void PriorityQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    while (!queue_.empty()) {
        queue_.pop();
    }
    
    // 重置优先级计数
    for (auto& pair : priorityCount_) {
        pair.second = 0;
    }
}

void PriorityQueue::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
    }
    notEmpty_.notify_all();
}

void PriorityQueue::resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    stopped_ = false;
}

bool PriorityQueue::isStopped() const {
    return stopped_.load();
}

std::map<Priority, size_t> PriorityQueue::getPriorityDistribution() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return priorityCount_;
}

bool PriorityQueue::removeTask(TaskID taskId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 由于std::priority_queue不支持直接删除元素，需要重建队列
    std::vector<std::shared_ptr<Task>> temp;
    bool found = false;
    
    while (!queue_.empty()) {
        auto task = queue_.top();
        queue_.pop();
        
        if (task->id == taskId) {
            found = true;
            updatePriorityCount(task->priority, -1);
        } else {
            temp.push_back(task);
        }
    }
    
    // 重建队列
    for (const auto& task : temp) {
        queue_.push(task);
    }
    
    return found;
}

std::vector<TaskID> PriorityQueue::getAllTaskIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TaskID> ids;
    
    // 创建队列的副本以遍历
    auto queueCopy = queue_;
    
    while (!queueCopy.empty()) {
        ids.push_back(queueCopy.top()->id);
        queueCopy.pop();
    }
    
    return ids;
}

void PriorityQueue::updatePriorityCount(Priority priority, int delta) {
    priorityCount_[priority] += delta;
    if (priorityCount_[priority] < 0) {
        priorityCount_[priority] = 0;
    }
}

} // namespace YB