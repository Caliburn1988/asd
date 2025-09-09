# TaskScheduler Module

## 概述
TaskScheduler是YB_GUI_V05系统的核心任务调度组件，负责管理和协调系统中的各种计算任务。

## 当前状态
- **里程碑1**: ✅ 已完成 (基础框架实现)
- **里程碑2**: ✅ 已完成 (线程池和优先级调度)
- **里程碑3**: ⏳ 进行中 (负载均衡和资源管理)
- **里程碑4**: 📋 计划中 (监控系统和异常处理)
- **里程碑5**: 📋 计划中 (系统集成和优化)

## 项目结构
```
TaskScheduler/
├── include/          # 头文件
│   └── TaskScheduler.h
├── src/             # 源代码
│   └── TaskScheduler.cpp
├── tests/           # 测试代码
│   ├── test_task_scheduler.cpp
│   └── simple_test.cpp
├── docs/            # 文档
│   └── milestone1_summary.md
├── build/           # 构建目录
├── logs/            # 日志目录
├── CMakeLists.txt   # CMake配置
└── README.md        # 本文件
```

## 编译和测试

### 编译步骤
```bash
cd TaskScheduler
mkdir -p build && cd build
cmake ..
make -j4
```

### 运行测试
```bash
./test_task_scheduler
```

## 主要功能
- 任务提交与管理
- 优先级调度（5级优先级系统）
- 任务状态查询和取消
- 性能监控和统计
- 配置管理和动态更新

## API使用示例
```cpp
// 创建调度器
SchedulerConfig config;
TaskScheduler scheduler(config);
scheduler.initialize(config);

// 提交任务
auto task = []() -> TaskResult {
    TaskResult result;
    result.status = ResultStatus::SUCCESS;
    // 执行任务逻辑
    return result;
};

TaskID taskId = scheduler.submitTask(
    TaskType::USER_DEFINED, 
    Priority::NORMAL, 
    task
);

// 查询任务状态
TaskStatus status = scheduler.getTaskStatus(taskId);

// 获取性能指标
PerformanceMetrics metrics = scheduler.getPerformanceMetrics();

// 关闭调度器
scheduler.shutdown();
```

## 依赖
- C++17标准
- pthread库
- 标准C++库

## 开发者
YB_GUI_V05开发团队

## 许可证
专有软件