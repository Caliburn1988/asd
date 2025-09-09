#include "../include/TaskScheduler.h"
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
    
    // 初始化各个组件（在里程碑1中，我们只初始化基础组件）
    try {
        // 创建日志目录
        std::string logDir = config_.logFilePath.substr(0, config_.logFilePath.find_last_of('/'));
        std::string mkdirCmd = "mkdir -p " + logDir;
        system(mkdirCmd.c_str());
        
        // 标记为运行状态
        running_ = true;
        paused_ = false;
        
        // 初始化性能指标
        currentMetrics_ = PerformanceMetrics();
        currentMetrics_.lastUpdateTime = std::chrono::steady_clock::now();
        
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
    
    running_ = false;
    
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
    if (!running_) {
        return 0; // 无效的任务ID
    }
    
    // 生成任务ID
    task->id = generateTaskId();
    
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
    }
    
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
        statusIt->second = TaskStatus::CANCELLED;
        activeTasks_.erase(taskId);
        return true;
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
}

SchedulerConfig TaskScheduler::getConfig() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_;
}

void TaskScheduler::pauseScheduling() {
    paused_ = true;
}

void TaskScheduler::resumeScheduling() {
    paused_ = false;
}

bool TaskScheduler::isPaused() const {
    return paused_;
}

// 监控和统计
PerformanceMetrics TaskScheduler::getPerformanceMetrics() {
    std::lock_guard<std::mutex> lock(resultsMutex_);
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
    
    // 统计优先级分布
    for (const auto& [taskId, task] : activeTasks_) {
        if (taskStatuses_[taskId] == TaskStatus::PENDING || 
            taskStatuses_[taskId] == TaskStatus::RUNNING) {
            status.priorityDistribution[task->priority]++;
        }
    }
    
    return status;
}

std::vector<std::string> TaskScheduler::getSystemLogs() {
    // 在里程碑1中，返回简单的日志信息
    std::vector<std::string> logs;
    logs.push_back("TaskScheduler initialized");
    logs.push_back("System running: " + std::string(running_ ? "true" : "false"));
    logs.push_back("System paused: " + std::string(paused_ ? "true" : "false"));
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
        file.close();
    }
}

// 高级功能（在里程碑1中只提供空实现）
void TaskScheduler::setLoadBalancingStrategy(LoadBalancingStrategy strategy) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.strategy = strategy;
}

void TaskScheduler::adjustThreadPoolSize(size_t newSize) {
    // 在后续里程碑中实现
}

void TaskScheduler::flushLogs() {
    // 在后续里程碑中实现
}

// 内部方法的基础实现
void TaskScheduler::processTask(std::shared_ptr<Task> task) {
    // 在里程碑1中，这个方法暂时为空
    // 实际的任务处理将在后续里程碑中实现
}

void TaskScheduler::updateMetrics() {
    // 更新性能指标
    currentMetrics_.lastUpdateTime = std::chrono::steady_clock::now();
}

void TaskScheduler::handleTaskCompletion(const TaskResult& result) {
    std::lock_guard<std::mutex> statusLock(statusMutex_);
    std::lock_guard<std::mutex> resultsLock(resultsMutex_);
    
    // 更新任务状态
    taskStatuses_[result.taskId] = TaskStatus::COMPLETED;
    
    // 保存结果
    completedTasks_.push_back(result);
    
    // 更新指标
    currentMetrics_.totalTasksCompleted++;
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
    // 在后续里程碑中实现
}

void TaskScheduler::monitorThread() {
    // 在后续里程碑中实现
}

void TaskScheduler::timeoutCheckThread() {
    // 在后续里程碑中实现
}

} // namespace YB