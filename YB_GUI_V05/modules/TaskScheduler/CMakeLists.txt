cmake_minimum_required(VERSION 3.10)
project(TaskScheduler)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加编译选项
add_compile_options(-Wall -Wextra -Wpedantic -O2)

# 包含目录
include_directories(include)

# 源文件
set(SOURCES
    src/TaskScheduler.cpp
)

# 创建静态库
add_library(taskscheduler STATIC ${SOURCES})

# 测试可执行文件
add_executable(test_task_scheduler tests/test_task_scheduler.cpp)

# 链接库
target_link_libraries(test_task_scheduler taskscheduler pthread)

# 添加测试
enable_testing()
add_test(NAME TaskSchedulerBasicTests COMMAND test_task_scheduler)