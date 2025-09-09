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
    : running_(false), paused_(false), nextTaskId_(1),
      autoScalingEnabled_(true), currentCpuUsage_(0.0), currentMemoryUsage_(0),
      currentLoadFactor_(0.0), resourceLimitExceeded_(false) {
    config_ = SchedulerConfig();
    startTime_ = std::chrono::steady_clock::now();
    lastScalingAction_ = startTime_;
    currentMetrics_ = PerformanceMetrics();
    currentMetrics_.lastUpdateTime = startTime_;
}

TaskScheduler::TaskScheduler(const SchedulerConfig& config) 
    : config_(config), running_(false), paused_(false), nextTaskId_(1),
      autoScalingEnabled_(true), currentCpuUsage_(0.0), currentMemoryUsage_(0),
      currentLoadFactor_(0.0), resourceLimitExceeded_(false) {
    startTime_ = std::chrono::steady_clock::now();
    lastScalingAction_ = startTime_;
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
        // 定期更新性能指标和资源使用情况
        updateMetrics();
        updateResourceUsage();
        
        // 检查资源限制
        checkResourceLimits();
        
        // 执行负载均衡
        if (config_.enableLoadBalancing && autoScalingEnabled_) {
            performLoadBalancing();
        }
        
        // 休眠一段时间
        std::this_thread::sleep_for(config_.monitorInterval);
    }
}

void TaskScheduler::timeoutCheckThread() {
    while (running_) {
        std::vector<TaskID> timedOutTasks;
        
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
                        
                        // 使用任务特定的超时时间，如果没有设置则使用默认值
                        auto timeoutDuration = (task->timeout.count() > 0) ? 
                            task->timeout : config_.defaultTimeout;
                        
                        // 检查资源限制中的最大任务持续时间
                        if (timeoutDuration > config_.resourceLimits.maxTaskDuration) {
                            timeoutDuration = config_.resourceLimits.maxTaskDuration;
                        }
                        
                        // 检查是否超时
                        if (elapsed > timeoutDuration) {
                            timedOutTasks.push_back(taskId);
                        }
                    }
                }
                // 检查等待中的任务是否等待时间过长
                else if (status == TaskStatus::PENDING) {
                    auto taskIt = activeTasks_.find(taskId);
                    if (taskIt != activeTasks_.end()) {
                        auto& task = taskIt->second;
                        auto waitTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - task->submitTime
                        );
                        
                        // 如果等待时间超过超时时间的两倍，标记为超时
                        auto maxWaitTime = config_.defaultTimeout * 2;
                        if (waitTime > maxWaitTime) {
                            timedOutTasks.push_back(taskId);
                        }
                    }
                }
            }
        }
        
        // 处理超时任务
        for (TaskID taskId : timedOutTasks) {
            std::lock_guard<std::mutex> lock(statusMutex_);
            auto statusIt = taskStatuses_.find(taskId);
            if (statusIt != taskStatuses_.end()) {
                if (statusIt->second == TaskStatus::RUNNING) {
                    handleTaskFailure(taskId, "Task execution timeout");
                } else if (statusIt->second == TaskStatus::PENDING) {
                    handleTaskFailure(taskId, "Task waiting timeout");
                }
                statusIt->second = TaskStatus::TIMEOUT;
                
                // 从队列中移除（如果还在队列中）
                if (taskQueue_) {
                    taskQueue_->removeTask(taskId);
                }
            }
        }
        
        // 休眠100ms
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// 负载均衡和资源管理方法实现
void TaskScheduler::enableAutoScaling(bool enable) {
    autoScalingEnabled_ = enable;
}

bool TaskScheduler::isAutoScalingEnabled() const {
    return autoScalingEnabled_;
}

void TaskScheduler::setResourceLimits(const ResourceLimits& limits) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.resourceLimits = limits;
}

ResourceLimits TaskScheduler::getResourceLimits() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.resourceLimits;
}

void TaskScheduler::setLoadBalancingConfig(const LoadBalancingConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.loadBalancingConfig = config;
}

LoadBalancingConfig TaskScheduler::getLoadBalancingConfig() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.loadBalancingConfig;
}

double TaskScheduler::getCurrentCpuUsage() const {
    return currentCpuUsage_.load();
}

size_t TaskScheduler::getCurrentMemoryUsage() const {
    return currentMemoryUsage_.load();
}

double TaskScheduler::getLoadFactor() const {
    return currentLoadFactor_.load();
}

bool TaskScheduler::isResourceLimitExceeded() const {
    return resourceLimitExceeded_.load();
}

void TaskScheduler::performLoadBalancing() {
    if (!threadPool_ || !taskQueue_) {
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastAction = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - lastScalingAction_
    );
    
    // 检查冷却期
    if (timeSinceLastAction < config_.loadBalancingConfig.cooldownPeriod) {
        return;
    }
    
    // 根据策略执行负载均衡
    switch (config_.strategy) {
        case LoadBalancingStrategy::ADAPTIVE:
            if (shouldScaleUp()) {
                scaleUp();
                lastScalingAction_ = now;
            } else if (shouldScaleDown()) {
                scaleDown();
                lastScalingAction_ = now;
            }
            break;
            
        case LoadBalancingStrategy::LEAST_LOADED:
            // 基于最少负载的策略
            if (currentLoadFactor_ > config_.loadBalancingConfig.scaleUpThreshold) {
                scaleUp();
                lastScalingAction_ = now;
            } else if (currentLoadFactor_ < config_.loadBalancingConfig.scaleDownThreshold) {
                scaleDown();
                lastScalingAction_ = now;
            }
            break;
            
        case LoadBalancingStrategy::PRIORITY_BASED:
            // 基于优先级的策略
            {
                auto queueStatus = getQueueStatus();
                size_t highPriorityTasks = 0;
                for (const auto& [priority, count] : queueStatus.priorityDistribution) {
                    if (priority == Priority::CRITICAL || priority == Priority::HIGH) {
                        highPriorityTasks += count;
                    }
                }
                
                if (highPriorityTasks > threadPool_->getPoolSize()) {
                    scaleUp();
                    lastScalingAction_ = now;
                } else if (highPriorityTasks == 0 && currentLoadFactor_ < 0.2) {
                    scaleDown();
                    lastScalingAction_ = now;
                }
            }
            break;
            
        case LoadBalancingStrategy::ROUND_ROBIN:
            // 轮询策略（较少用于自动扩缩容）
            break;
    }
}

void TaskScheduler::checkResourceLimits() {
    bool limitExceeded = false;
    
    // 检查CPU使用率
    if (currentCpuUsage_ > config_.resourceLimits.maxCpuUsage) {
        limitExceeded = true;
    }
    
    // 检查内存使用量
    if (currentMemoryUsage_ > config_.resourceLimits.maxMemoryUsage) {
        limitExceeded = true;
    }
    
    // 检查队列长度
    if (taskQueue_ && taskQueue_->size() > config_.resourceLimits.maxQueueLength) {
        limitExceeded = true;
    }
    
    resourceLimitExceeded_ = limitExceeded;
    
    // 如果资源超限，暂停任务提交
    if (limitExceeded && !paused_) {
        std::cout << "Resource limits exceeded, pausing task submission" << std::endl;
        pauseScheduling();
    } else if (!limitExceeded && paused_) {
        std::cout << "Resource usage back to normal, resuming task submission" << std::endl;
        resumeScheduling();
    }
}

void TaskScheduler::updateResourceUsage() {
    // 更新负载因子
    currentLoadFactor_ = calculateLoadFactor();
    
    // 简化的CPU和内存使用率计算
    // 在实际实现中，应该使用系统API获取真实的资源使用情况
    if (threadPool_) {
        double cpuUsage = (static_cast<double>(threadPool_->getActiveThreads()) / 
                          threadPool_->getPoolSize()) * 100.0;
        currentCpuUsage_ = std::min(cpuUsage, 100.0);
    }
    
    // 简化的内存使用量计算
    size_t estimatedMemory = 0;
    if (taskQueue_) {
        estimatedMemory = taskQueue_->size() * 1024; // 每个任务估算1KB
    }
    if (threadPool_) {
        estimatedMemory += threadPool_->getPoolSize() * 1024 * 1024; // 每个线程估算1MB
    }
    currentMemoryUsage_ = estimatedMemory;
}

bool TaskScheduler::shouldScaleUp() const {
    if (!threadPool_ || threadPool_->getPoolSize() >= config_.maxThreads) {
        return false;
    }
    
    // 检查负载因子
    if (currentLoadFactor_ > config_.loadBalancingConfig.scaleUpThreshold) {
        return true;
    }
    
    // 检查队列积压
    if (taskQueue_) {
        size_t queueSize = taskQueue_->size();
        size_t poolSize = threadPool_->getPoolSize();
        if (queueSize > poolSize * 3) { // 队列长度超过线程数的3倍
            return true;
        }
    }
    
    return false;
}

bool TaskScheduler::shouldScaleDown() const {
    if (!threadPool_ || threadPool_->getPoolSize() <= config_.minThreads) {
        return false;
    }
    
    // 检查负载因子
    if (currentLoadFactor_ < config_.loadBalancingConfig.scaleDownThreshold) {
        return true;
    }
    
    // 检查空闲线程
    if (threadPool_) {
        size_t activeThreads = threadPool_->getActiveThreads();
        size_t poolSize = threadPool_->getPoolSize();
        if (activeThreads < poolSize / 3) { // 活跃线程少于总数的1/3
            return true;
        }
    }
    
    return false;
}

void TaskScheduler::scaleUp() {
    if (!threadPool_ || threadPool_->getPoolSize() >= config_.maxThreads) {
        return;
    }
    
    size_t currentSize = threadPool_->getPoolSize();
    size_t newSize = std::min(
        currentSize + config_.loadBalancingConfig.scaleUpStep,
        config_.maxThreads
    );
    
    std::cout << "Scaling up from " << currentSize << " to " << newSize << " threads" << std::endl;
    adjustThreadPoolSize(newSize);
}

void TaskScheduler::scaleDown() {
    if (!threadPool_ || threadPool_->getPoolSize() <= config_.minThreads) {
        return;
    }
    
    size_t currentSize = threadPool_->getPoolSize();
    size_t newSize = std::max(
        currentSize - config_.loadBalancingConfig.scaleDownStep,
        config_.minThreads
    );
    
    std::cout << "Scaling down from " << currentSize << " to " << newSize << " threads" << std::endl;
    adjustThreadPoolSize(newSize);
}

double TaskScheduler::calculateLoadFactor() const {
    if (!threadPool_ || !taskQueue_) {
        return 0.0;
    }
    
    size_t activeThreads = threadPool_->getActiveThreads();
    size_t totalThreads = threadPool_->getPoolSize();
    size_t queueSize = taskQueue_->size();
    
    if (totalThreads == 0) {
        return 0.0;
    }
    
    // 负载因子 = (活跃线程数 + 队列长度权重) / 总线程数
    double queueWeight = std::min(static_cast<double>(queueSize) / totalThreads, 1.0);
    double threadUtilization = static_cast<double>(activeThreads) / totalThreads;
    
    return std::min(threadUtilization + queueWeight * 0.5, 1.0);
}

} // namespace YB