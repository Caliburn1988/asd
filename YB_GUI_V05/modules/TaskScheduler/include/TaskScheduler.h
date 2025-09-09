#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <vector>
#include <unordered_map>
#include <map>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <algorithm>
#include <exception>
#include <string>
#include <any>

namespace YB {

// 前向声明
class ThreadPool;
class PriorityQueue;
class PerformanceMonitor;
class TaskTimeoutManager;
class Logger;

// 类型定义
using TaskID = uint64_t;

// 枚举定义
enum class Priority {
    CRITICAL = 0,
    HIGH = 1,
    NORMAL = 2,
    LOW = 3,
    BACKGROUND = 4
};

enum class TaskStatus {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED,
    CANCELLED,
    TIMEOUT
};

enum class TaskType {
    AI_INFERENCE,
    IMAGE_PROCESSING,
    DATA_ANALYSIS,
    SYSTEM_MAINTENANCE,
    USER_DEFINED
};

enum class ResultStatus {
    SUCCESS,
    FAILURE,
    TIMEOUT,
    CANCELLED
};

enum class LoadBalancingStrategy {
    ROUND_ROBIN,
    LEAST_LOADED,
    ADAPTIVE,
    PRIORITY_BASED
};

// 结构体定义
struct TaskResult {
    TaskID taskId;
    ResultStatus status;
    std::any result;
    std::string errorMessage;
    std::chrono::milliseconds executionTime;
    std::chrono::steady_clock::time_point completionTime;
    
    TaskResult() = default;
    TaskResult(TaskID id, ResultStatus s) : taskId(id), status(s) {}
};

struct Task {
    TaskID id;
    TaskType type;
    Priority priority;
    std::function<TaskResult()> function;
    std::chrono::milliseconds timeout;
    std::chrono::steady_clock::time_point submitTime;
    std::unordered_map<std::string, std::any> parameters;
    std::vector<TaskID> dependencies;
    
    Task() = default;
    Task(TaskID taskId, TaskType taskType, Priority prio, std::function<TaskResult()> func)
        : id(taskId), type(taskType), priority(prio), function(std::move(func)),
          timeout(std::chrono::milliseconds(30000)),
          submitTime(std::chrono::steady_clock::now()) {}
};

struct PerformanceMetrics {
    size_t totalTasksSubmitted = 0;
    size_t totalTasksCompleted = 0;
    size_t totalTasksFailed = 0;
    double averageExecutionTime = 0.0;
    double averageWaitTime = 0.0;
    size_t currentActiveThreads = 0;
    size_t currentQueueSize = 0;
    double cpuUsage = 0.0;
    double memoryUsage = 0.0;
    std::chrono::steady_clock::time_point lastUpdateTime;
};

struct QueueStatus {
    size_t pendingTasks = 0;
    size_t runningTasks = 0;
    size_t completedTasks = 0;
    std::map<Priority, size_t> priorityDistribution;
};

struct SchedulerConfig {
    size_t minThreads = 2;
    size_t maxThreads = 16;
    size_t maxQueueSize = 1000;
    std::chrono::milliseconds defaultTimeout = std::chrono::milliseconds(30000);
    bool enableLoadBalancing = true;
    LoadBalancingStrategy strategy = LoadBalancingStrategy::ADAPTIVE;
    std::chrono::milliseconds monitorInterval = std::chrono::milliseconds(1000);
    std::string logLevel = "INFO";
    std::string logFilePath = "./logs/scheduler.log";
};

// 主要类声明
class TaskScheduler {
public:
    // 构造函数和析构函数
    TaskScheduler();
    explicit TaskScheduler(const SchedulerConfig& config);
    ~TaskScheduler();
    
    // 禁用拷贝构造和赋值
    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;
    
    // 初始化和生命周期管理
    bool initialize(const SchedulerConfig& config);
    void shutdown();
    bool isRunning() const;
    
    // 任务管理
    TaskID submitTask(std::shared_ptr<Task> task);
    TaskID submitTask(TaskType type, Priority priority, std::function<TaskResult()> function);
    TaskID submitTask(TaskType type, Priority priority, std::function<TaskResult()> function,
                     std::chrono::milliseconds timeout);
    TaskID submitTask(TaskType type, Priority priority, std::function<TaskResult()> function,
                     const std::vector<TaskID>& dependencies);
    
    bool cancelTask(TaskID taskId);
    TaskStatus getTaskStatus(TaskID taskId);
    std::vector<TaskResult> getCompletedTasks();
    void clearCompletedTasks();
    
    // 配置和控制
    void updateConfig(const SchedulerConfig& config);
    SchedulerConfig getConfig() const;
    void pauseScheduling();
    void resumeScheduling();
    bool isPaused() const;
    
    // 监控和统计
    PerformanceMetrics getPerformanceMetrics();
    QueueStatus getQueueStatus();
    std::vector<std::string> getSystemLogs();
    void exportMetrics(const std::string& filePath);
    
    // 高级功能
    void setLoadBalancingStrategy(LoadBalancingStrategy strategy);
    void adjustThreadPoolSize(size_t newSize);
    void flushLogs();
    
private:
    // 内部方法
    TaskID generateTaskId();
    void workerThread();
    void monitorThread();
    void timeoutCheckThread();
    void processTask(std::shared_ptr<Task> task);
    void updateMetrics();
    void handleTaskCompletion(const TaskResult& result);
    void handleTaskFailure(TaskID taskId, const std::string& error);
    bool checkDependencies(const std::vector<TaskID>& dependencies);
    
    // 成员变量
    std::unique_ptr<ThreadPool> threadPool_;
    std::unique_ptr<PriorityQueue> taskQueue_;
    // 以下组件将在后续里程碑中实现
    // std::unique_ptr<PerformanceMonitor> performanceMonitor_;
    // std::unique_ptr<TaskTimeoutManager> timeoutManager_;
    // std::unique_ptr<Logger> logger_;
    
    SchedulerConfig config_;
    std::atomic<bool> running_;
    std::atomic<bool> paused_;
    std::atomic<TaskID> nextTaskId_;
    
    std::unordered_map<TaskID, TaskStatus> taskStatuses_;
    std::unordered_map<TaskID, std::shared_ptr<Task>> activeTasks_;
    std::vector<TaskResult> completedTasks_;
    
    mutable std::mutex statusMutex_;
    mutable std::mutex resultsMutex_;
    mutable std::mutex configMutex_;
    
    std::thread monitorThread_;
    std::thread timeoutThread_;
    
    PerformanceMetrics currentMetrics_;
    std::chrono::steady_clock::time_point startTime_;
};

// 辅助类和函数
class TaskComparator {
public:
    bool operator()(const std::shared_ptr<Task>& a, const std::shared_ptr<Task>& b) const {
        if (a->priority != b->priority) {
            return static_cast<int>(a->priority) > static_cast<int>(b->priority);
        }
        return a->submitTime > b->submitTime;
    }
};

// 工具函数
std::string priorityToString(Priority priority);
std::string taskStatusToString(TaskStatus status);
std::string taskTypeToString(TaskType type);
Priority stringToPriority(const std::string& str);

} // namespace YB

#endif // TASK_SCHEDULER_H