#include "../include/TaskScheduler.h"
#include "../include/ThreadPool.h"
#include "../include/PriorityQueue.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace YB {

// 工具函数实现
std::string priorityToString(Priority priority) {
    switch (priority) {
        case Priority::CRITICAL: return "CRITICAL";
        case Priority::HIGH: return "HIGH";
        case Priority::NORMAL: return "NORMAL";
        case Priority::LOW: return "LOW";
        case Priority::BACKGROUND: return "BACKGROUND";
        default: return "UNKNOWN";
    }
}

std::string taskStatusToString(TaskStatus status) {
    switch (status) {
        case TaskStatus::PENDING: return "PENDING";
        case TaskStatus::RUNNING: return "RUNNING";
        case TaskStatus::COMPLETED: return "COMPLETED";
        case TaskStatus::FAILED: return "FAILED";
        case TaskStatus::CANCELLED: return "CANCELLED";
        case TaskStatus::TIMEOUT: return "TIMEOUT";
        default: return "UNKNOWN";
    }
}

std::string taskTypeToString(TaskType type) {
    switch (type) {
        case TaskType::AI_INFERENCE: return "AI_INFERENCE";
        case TaskType::IMAGE_PROCESSING: return "IMAGE_PROCESSING";
        case TaskType::DATA_ANALYSIS: return "DATA_ANALYSIS";
        case TaskType::SYSTEM_MAINTENANCE: return "SYSTEM_MAINTENANCE";
        case TaskType::USER_DEFINED: return "USER_DEFINED";
        default: return "UNKNOWN";
    }
}

Priority stringToPriority(const std::string& str) {
    if (str == "CRITICAL") return Priority::CRITICAL;
    if (str == "HIGH") return Priority::HIGH;
    if (str == "NORMAL") return Priority::NORMAL;
    if (str == "LOW") return Priority::LOW;
    if (str == "BACKGROUND") return Priority::BACKGROUND;
    return Priority::NORMAL; // 默认值
}

// TaskScheduler 构造函数和析构函数
TaskScheduler::TaskScheduler() 
    : running_(false), paused_(false), nextTaskId_(1) {
    config_ = SchedulerConfig();
    startTime_ = std::chrono::steady_clock::now();
    currentMetrics_ = PerformanceMetrics();
    currentMetrics_.lastUpdateTime = startTime_;
}

TaskScheduler::TaskScheduler(const SchedulerConfig& config) 
    : config_(config), running_(false), paused_(false), nextTaskId_(1) {
    startTime_ = std::chrono::steady_clock::now();
    currentMetrics_ = PerformanceMetrics();
    currentMetrics_.lastUpdateTime = startTime_;
}

TaskScheduler::~TaskScheduler() {
    if (running_) {
        shutdown();
    }
}

// 初始化和生命周期管理
bool TaskScheduler::initialize(const SchedulerConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    if (running_) {
        return false; // 已经在运行
    }
    
    config_ = config;
    
    try {
        // 创建日志目录
        std::string logDir = config_.logFilePath.substr(0, config_.logFilePath.find_last_of('/'));
        std::string mkdirCmd = "mkdir -p " + logDir;
        system(mkdirCmd.c_str());
        
        // 初始化线程池
        threadPool_ = std::make_unique<ThreadPool>(config_.minThreads);
        
        // 初始化优先级队列
        taskQueue_ = std::make_unique<PriorityQueue>();
        
        // 标记为运行状态
        running_ = true;
        paused_ = false;
        
        // 初始化性能指标
        currentMetrics_ = PerformanceMetrics();
        currentMetrics_.lastUpdateTime = std::chrono::steady_clock::now();
        currentMetrics_.currentActiveThreads = config_.minThreads;
        
        // 启动工作线程
        for (size_t i = 0; i < config_.minThreads; ++i) {
            threadPool_->enqueue([this] { workerThread(); });
        }
        
        // 启动监控线程
        monitorThread_ = std::thread([this] { monitorThread(); });
        
        // 启动超时检查线程
        timeoutThread_ = std::thread([this] { timeoutCheckThread(); });
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "TaskScheduler initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void TaskScheduler::shutdown() {
    if (!running_) {
        return;
    }
    
    // 先标记停止
    running_ = false;
    
    // 等待一小段时间让工作线程退出
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 停止队列（唤醒所有等待的线程）
    if (taskQueue_) {
        taskQueue_->stop();
    }
    
    // 停止线程池
    if (threadPool_) {
        threadPool_->stop();
    }
    
    // 等待监控线程结束
    if (monitorThread_.joinable()) {
        monitorThread_.join();
    }
    
    // 等待超时检查线程结束
    if (timeoutThread_.joinable()) {
        timeoutThread_.join();
    }
    
    // 清理资源
    {
        std::lock_guard<std::mutex> statusLock(statusMutex_);
        taskStatuses_.clear();
        activeTasks_.clear();
    }
    
    {
        std::lock_guard<std::mutex> resultsLock(resultsMutex_);
        completedTasks_.clear();
    }
}

bool TaskScheduler::isRunning() const {
    return running_;
}

// 任务管理
TaskID TaskScheduler::generateTaskId() {
    return nextTaskId_.fetch_add(1);
}

TaskID TaskScheduler::submitTask(std::shared_ptr<Task> task) {
    if (!running_ || !task) {
        return 0; // 无效的任务ID
    }
    
    if (paused_) {
        return 0; // 系统暂停中
    }
    
    // 生成任务ID
    task->id = generateTaskId();
    
    // 检查依赖（在里程碑2中，暂时忽略依赖检查）
    // if (!task->dependencies.empty() && !checkDependencies(task->dependencies)) {
    //     return 0; // 依赖未满足
    // }
    
    // 记录任务状态
    {
        std::lock_guard<std::mutex> lock(statusMutex_);
        taskStatuses_[task->id] = TaskStatus::PENDING;
        activeTasks_[task->id] = task;
    }
    
    // 更新性能指标
    {
        std::lock_guard<std::mutex> lock(resultsMutex_);
        currentMetrics_.totalTasksSubmitted++;
        currentMetrics_.currentQueueSize = taskQueue_->size() + 1;
    }
    
    // 将任务加入队列
    taskQueue_->push(task);
    
    return task->id;
}

TaskID TaskScheduler::submitTask(TaskType type, Priority priority, std::function<TaskResult()> function) {
    auto task = std::make_shared<Task>(0, type, priority, std::move(function));
    return submitTask(task);
}

TaskID TaskScheduler::submitTask(TaskType type, Priority priority, std::function<TaskResult()> function,
                               std::chrono::milliseconds timeout) {
    auto task = std::make_shared<Task>(0, type, priority, std::move(function));
    task->timeout = timeout;
    return submitTask(task);
}

TaskID TaskScheduler::submitTask(TaskType type, Priority priority, std::function<TaskResult()> function,
                               const std::vector<TaskID>& dependencies) {
    auto task = std::make_shared<Task>(0, type, priority, std::move(function));
    task->dependencies = dependencies;
    return submitTask(task);
}

bool TaskScheduler::cancelTask(TaskID taskId) {
    std::lock_guard<std::mutex> lock(statusMutex_);
    
    auto statusIt = taskStatuses_.find(taskId);
    if (statusIt == taskStatuses_.end()) {
        return false; // 任务不存在
    }
    
    // 只能取消处于PENDING状态的任务
    if (statusIt->second == TaskStatus::PENDING) {
        // 尝试从队列中移除任务
        if (taskQueue_->removeTask(taskId)) {
            statusIt->second = TaskStatus::CANCELLED;
            activeTasks_.erase(taskId);
            return true;
        }
    }
    
    return false;
}

TaskStatus TaskScheduler::getTaskStatus(TaskID taskId) {
    std::lock_guard<std::mutex> lock(statusMutex_);
    
    auto it = taskStatuses_.find(taskId);
    if (it != taskStatuses_.end()) {
        return it->second;
    }
    
    return TaskStatus::CANCELLED; // 任务不存在，返回CANCELLED
}

std::vector<TaskResult> TaskScheduler::getCompletedTasks() {
    std::lock_guard<std::mutex> lock(resultsMutex_);
    return completedTasks_;
}

void TaskScheduler::clearCompletedTasks() {
    std::lock_guard<std::mutex> lock(resultsMutex_);
    completedTasks_.clear();
}

// 配置和控制
void TaskScheduler::updateConfig(const SchedulerConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;
    
    // 调整线程池大小
    if (threadPool_ && config_.minThreads != threadPool_->getPoolSize()) {
        threadPool_->resize(config_.minThreads);
    }
}

SchedulerConfig TaskScheduler::getConfig() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_;
}

void TaskScheduler::pauseScheduling() {
    paused_ = true;
    if (taskQueue_) {
        taskQueue_->stop();
    }
}

void TaskScheduler::resumeScheduling() {
    paused_ = false;
    if (taskQueue_) {
        taskQueue_->resume();
    }
}

bool TaskScheduler::isPaused() const {
    return paused_;
}

// 监控和统计
PerformanceMetrics TaskScheduler::getPerformanceMetrics() {
    std::lock_guard<std::mutex> lock(resultsMutex_);
    
    // 更新实时指标
    if (threadPool_) {
        currentMetrics_.currentActiveThreads = threadPool_->getActiveThreads();
    }
    if (taskQueue_) {
        currentMetrics_.currentQueueSize = taskQueue_->size();
    }
    
    currentMetrics_.lastUpdateTime = std::chrono::steady_clock::now();
    return currentMetrics_;
}

QueueStatus TaskScheduler::getQueueStatus() {
    std::lock_guard<std::mutex> lock(statusMutex_);
    
    QueueStatus status;
    
    // 统计各种状态的任务数量
    for (const auto& [taskId, taskStatus] : taskStatuses_) {
        switch (taskStatus) {
            case TaskStatus::PENDING:
                status.pendingTasks++;
                break;
            case TaskStatus::RUNNING:
                status.runningTasks++;
                break;
            case TaskStatus::COMPLETED:
                status.completedTasks++;
                break;
            default:
                break;
        }
    }
    
    // 从优先级队列获取优先级分布
    if (taskQueue_) {
        status.priorityDistribution = taskQueue_->getPriorityDistribution();
    }
    
    return status;
}

std::vector<std::string> TaskScheduler::getSystemLogs() {
    std::vector<std::string> logs;
    
    logs.push_back("TaskScheduler Status Report");
    logs.push_back("==========================");
    logs.push_back("System running: " + std::string(running_ ? "true" : "false"));
    logs.push_back("System paused: " + std::string(paused_ ? "true" : "false"));
    
    if (threadPool_) {
        logs.push_back("Thread pool size: " + std::to_string(threadPool_->getPoolSize()));
        logs.push_back("Active threads: " + std::to_string(threadPool_->getActiveThreads()));
    }
    
    if (taskQueue_) {
        logs.push_back("Queue size: " + std::to_string(taskQueue_->size()));
    }
    
    auto metrics = getPerformanceMetrics();
    logs.push_back("Total tasks submitted: " + std::to_string(metrics.totalTasksSubmitted));
    logs.push_back("Total tasks completed: " + std::to_string(metrics.totalTasksCompleted));
    logs.push_back("Total tasks failed: " + std::to_string(metrics.totalTasksFailed));
    
    return logs;
}

void TaskScheduler::exportMetrics(const std::string& filePath) {
    auto metrics = getPerformanceMetrics();
    
    std::ofstream file(filePath);
    if (file.is_open()) {
        file << "TaskScheduler Performance Metrics\n";
        file << "================================\n";
        file << "Total Tasks Submitted: " << metrics.totalTasksSubmitted << "\n";
        file << "Total Tasks Completed: " << metrics.totalTasksCompleted << "\n";
        file << "Total Tasks Failed: " << metrics.totalTasksFailed << "\n";
        file << "Average Execution Time: " << metrics.averageExecutionTime << " ms\n";
        file << "Average Wait Time: " << metrics.averageWaitTime << " ms\n";
        file << "Current Active Threads: " << metrics.currentActiveThreads << "\n";
        file << "Current Queue Size: " << metrics.currentQueueSize << "\n";
        file.close();
    }
}

// 高级功能
void TaskScheduler::setLoadBalancingStrategy(LoadBalancingStrategy strategy) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.strategy = strategy;
}

void TaskScheduler::adjustThreadPoolSize(size_t newSize) {
    if (threadPool_ && newSize > 0 && newSize <= config_.maxThreads) {
        size_t currentSize = threadPool_->getPoolSize();
        
        if (newSize > currentSize) {
            // 增加工作线程
            for (size_t i = currentSize; i < newSize; ++i) {
                threadPool_->enqueue([this] { workerThread(); });
            }
        }
        
        threadPool_->resize(newSize);
        
        std::lock_guard<std::mutex> lock(resultsMutex_);
        currentMetrics_.currentActiveThreads = newSize;
    }
}

void TaskScheduler::flushLogs() {
    // 强制刷新日志（在后续里程碑中实现完整的日志系统）
}

// 内部方法的实现
void TaskScheduler::processTask(std::shared_ptr<Task> task) {
    if (!task) return;
    
    auto startTime = std::chrono::steady_clock::now();
    
    // 更新任务状态为运行中
    {
        std::lock_guard<std::mutex> lock(statusMutex_);
        taskStatuses_[task->id] = TaskStatus::RUNNING;
    }
    
    TaskResult result;
    result.taskId = task->id;
    
    try {
        // 执行任务
        if (task->function) {
            result = task->function();
            result.taskId = task->id; // 确保任务ID正确
        } else {
            throw std::runtime_error("Task function is null");
        }
        
        // 计算执行时间
        auto endTime = std::chrono::steady_clock::now();
        result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        result.completionTime = endTime;
        
        // 处理任务完成
        handleTaskCompletion(result);
        
    } catch (const std::exception& e) {
        // 处理任务失败
        handleTaskFailure(task->id, e.what());
    } catch (...) {
        // 处理未知异常
        handleTaskFailure(task->id, "Unknown exception occurred");
    }
    
    // 从活跃任务中移除
    {
        std::lock_guard<std::mutex> lock(statusMutex_);
        activeTasks_.erase(task->id);
    }
}

void TaskScheduler::updateMetrics() {
    // 更新性能指标
    std::lock_guard<std::mutex> lock(resultsMutex_);
    
    // 计算平均执行时间
    if (currentMetrics_.totalTasksCompleted > 0) {
        double totalExecTime = 0;
        for (const auto& result : completedTasks_) {
            totalExecTime += result.executionTime.count();
        }
        currentMetrics_.averageExecutionTime = totalExecTime / currentMetrics_.totalTasksCompleted;
    }
    
    // 计算平均等待时间（简化版本）
    // 在完整实现中，需要记录任务从提交到开始执行的时间
    
    currentMetrics_.lastUpdateTime = std::chrono::steady_clock::now();
}

void TaskScheduler::handleTaskCompletion(const TaskResult& result) {
    std::lock_guard<std::mutex> statusLock(statusMutex_);
    std::lock_guard<std::mutex> resultsLock(resultsMutex_);
    
    // 更新任务状态
    taskStatuses_[result.taskId] = TaskStatus::COMPLETED;
    
    // 保存结果
    completedTasks_.push_back(result);
    
    // 限制完成任务列表的大小
    if (completedTasks_.size() > 1000) {
        completedTasks_.erase(completedTasks_.begin());
    }
    
    // 更新指标
    currentMetrics_.totalTasksCompleted++;
    updateMetrics();
}

void TaskScheduler::handleTaskFailure(TaskID taskId, const std::string& error) {
    std::lock_guard<std::mutex> statusLock(statusMutex_);
    std::lock_guard<std::mutex> resultsLock(resultsMutex_);
    
    // 更新任务状态
    taskStatuses_[taskId] = TaskStatus::FAILED;
    
    // 创建失败结果
    TaskResult result;
    result.taskId = taskId;
    result.status = ResultStatus::FAILURE;
    result.errorMessage = error;
    result.completionTime = std::chrono::steady_clock::now();
    
    completedTasks_.push_back(result);
    
    // 更新指标
    currentMetrics_.totalTasksFailed++;
}

bool TaskScheduler::checkDependencies(const std::vector<TaskID>& dependencies) {
    std::lock_guard<std::mutex> lock(statusMutex_);
    
    for (TaskID depId : dependencies) {
        auto it = taskStatuses_.find(depId);
        if (it == taskStatuses_.end() || it->second != TaskStatus::COMPLETED) {
            return false;
        }
    }
    
    return true;
}

void TaskScheduler::workerThread() {
    while (running_) {
        // 从队列中获取任务
        auto task = taskQueue_->popWithTimeout(std::chrono::milliseconds(100));
        
        if (task && !paused_) {
            processTask(task);
        }
    }
}

void TaskScheduler::monitorThread() {
    while (running_) {
        // 定期更新性能指标
        updateMetrics();
        
        // 检查是否需要调整线程池大小（简单的负载均衡）
        if (config_.enableLoadBalancing && threadPool_ && taskQueue_) {
            size_t queueSize = taskQueue_->size();
            // size_t activeThreads = threadPool_->getActiveThreads();
            size_t poolSize = threadPool_->getPoolSize();
            
            // 如果队列太长且有空闲容量，增加线程
            if (queueSize > poolSize * 2 && poolSize < config_.maxThreads) {
                size_t newSize = std::min(poolSize + 1, config_.maxThreads);
                adjustThreadPoolSize(newSize);
            }
            // 如果队列很短且线程过多，减少线程
            else if (queueSize < poolSize / 2 && poolSize > config_.minThreads) {
                size_t newSize = std::max(poolSize - 1, config_.minThreads);
                adjustThreadPoolSize(newSize);
            }
        }
        
        // 休眠一段时间
        std::this_thread::sleep_for(config_.monitorInterval);
    }
}

void TaskScheduler::timeoutCheckThread() {
    while (running_) {
        {
            std::lock_guard<std::mutex> lock(statusMutex_);
            
            auto now = std::chrono::steady_clock::now();
            
            // 检查所有运行中的任务
            for (auto& [taskId, status] : taskStatuses_) {
                if (status == TaskStatus::RUNNING) {
                    auto taskIt = activeTasks_.find(taskId);
                    if (taskIt != activeTasks_.end()) {
                        auto& task = taskIt->second;
                        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - task->submitTime
                        );
                        
                        // 检查是否超时
                        if (elapsed > task->timeout) {
                            handleTaskFailure(taskId, "Task timeout");
                            status = TaskStatus::TIMEOUT;
                        }
                    }
                }
            }
        }
        
        // 休眠100ms
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace YB