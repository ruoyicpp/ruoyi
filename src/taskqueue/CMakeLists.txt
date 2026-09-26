# Task Queue Module CMakeLists.txt
#
# Build wiring:
#   本模块的 TaskQueue.cc 由主 CMakeLists.txt 通过 glob 自动拉取：
#       src/taskqueue/*.cc
#   本文件只作为模块元数据展示和独立构建说明，不参与主项目构建。
#
# Standalone use:
#   如果要从外部项目独立编译本模块，把下面的注释块取消注释即可。
#
# Add to main CMakeLists.txt (only if standalone use is needed):
#   add_subdirectory(src/taskqueue)
#   target_link_libraries(ruoyi-cpp PRIVATE taskqueue_obj)

set(TASKQUEUE_SOURCES
    TaskQueue.cc
)

set(TASKQUEUE_HEADERS
    TaskQueue.h
    TaskQueueCtrl.h
    TaskQueueExample.h
)

# ----------------------------------------------------------------------
# 独立构建：取消下方整段注释可让本文件单独工作。
# 在主项目中请保持注释状态 —— 主项目通过 glob 拉取 TaskQueue.cc。
# ----------------------------------------------------------------------
# if(NOT TARGET taskqueue_obj)
#     find_library(HIREDIS_LIB
#         NAMES hiredis libhiredis
#         PATHS "${MSYS_PREFIX}/lib" "${VCPKG_LIB_DIR}" /usr/lib /usr/local/lib
#         NO_DEFAULT_PATH)
#     add_library(taskqueue_obj OBJECT ${TASKQUEUE_SOURCES})
#     target_include_directories(taskqueue_obj PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
#     if(HIREDIS_LIB)
#         target_include_directories(taskqueue_obj SYSTEM PUBLIC
#             "${MSYS_PREFIX}/include" "${VCPKG_INCLUDE_DIR}")
#         target_link_libraries(taskqueue_obj PUBLIC ${HIREDIS_LIB})
#     endif()
#     target_compile_features(taskqueue_obj PUBLIC cxx_std_20)
# endif()

message(STATUS "TaskQueue module configured (sources pulled by parent glob)")
message(STATUS "  Sources: ${TASKQUEUE_SOURCES}")
message(STATUS "  Headers: ${TASKQUEUE_HEADERS}")
message(STATUS "  Dependency: hiredis (linked globally by parent project)")
