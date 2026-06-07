# Task Queue Module CMakeLists.txt

# 任务队列源文件
set(TASKQUEUE_SOURCES
    TaskQueue.cc
)

# 任务队列头文件
set(TASKQUEUE_HEADERS
    TaskQueue.h
    TaskQueueCtrl.h
    TaskQueueExample.h
)

# 添加任务队列源文件到项目
target_sources(ruoyi-cpp PRIVATE ${TASKQUEUE_SOURCES})

# 设置包含目录
target_include_directories(ruoyi-cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 打印编译信息
message(STATUS "TaskQueue module configured")
message(STATUS "  Sources: ${TASKQUEUE_SOURCES}")
message(STATUS "  Headers: ${TASKQUEUE_HEADERS}")
