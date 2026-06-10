/**
 * @file TaskQueueCtrl.h
 * @brief 任务队列管理控制器 — 异步任务队列的监控和管理
 * 
 * 功能概述：
 *   - 队列统计：获取队列深度、成功率、失败率等统计信息
 *   - 任务查询：查询任务详情、执行日志、执行状态
 *   - 任务管理：重新入队失败任务、清空死信队列
 *   - 性能监控：监控队列处理速度、延迟、吞吐量
 *   - 告警管理：配置队列告警规则
 * 
 * 核心特性：
 *   - 实时统计：实时获取队列统计信息
 *   - 详细日志：完整的任务执行日志
 *   - 故障恢复：支持重新入队失败任务
 *   - 死信管理：管理无法处理的任务
 *   - 权限控制：基于权限的访问控制
 * 
 * API 端点：
 *   - GET /monitor/taskqueue/stats - 获取队列统计
 *   - GET /monitor/taskqueue/task/{id} - 获取任务详情
 *   - GET /monitor/taskqueue/task/{id}/log - 获取任务执行日志
 *   - POST /monitor/taskqueue/requeue/{id} - 重新入队任务
 *   - DELETE /monitor/taskqueue/deadletter/{type} - 清空死信队列
 * 
 * 请求/响应示例：
 *   ```
 *   GET /monitor/taskqueue/stats
 *   Authorization: Bearer <JWT>
 *   
 *   响应：
 *   {
 *     "code": 200,
 *     "msg": "success",
 *     "data": {
 *       "total_tasks": 10000,
 *       "pending_tasks": 50,
 *       "processing_tasks": 10,
 *       "success_tasks": 9800,
 *       "failed_tasks": 140,
 *       "success_rate": 98.5,
 *       "avg_processing_time": 125,
 *       "throughput": 100
 *     }
 *   }
 *   ```
 * 
 * 权限要求：
 *   - monitor:taskqueue:list - 查看队列统计
 *   - monitor:taskqueue:detail - 查看任务详情
 *   - monitor:taskqueue:requeue - 重新入队任务
 *   - monitor:taskqueue:clear - 清空死信队列
 * 
 * 配置项（config.json）：
 *   - taskQueue.enabled: 是否启用任务队列（默认 true）
 *   - taskQueue.workers: Worker 线程数（默认 4）
 *   - taskQueue.maxRetries: 最大重试次数（默认 3）
 *   - taskQueue.retryBackoff: 重试退避时间（秒，默认 60）
 * 
 * 队列统计字段：
 *   - total_tasks: 总任务数
 *   - pending_tasks: 待处理任务数
 *   - processing_tasks: 处理中任务数
 *   - success_tasks: 成功任务数
 *   - failed_tasks: 失败任务数
 *   - success_rate: 成功率（百分比）
 *   - avg_processing_time: 平均处理时间（毫秒）
 *   - throughput: 吞吐量（任务/秒）
 * 
 * 任务状态：
 *   - PENDING: 待处理
 *   - PROCESSING: 处理中
 *   - SUCCESS: 成功
 *   - FAILED: 失败
 *   - DEAD: 死信（无法处理）
 * 
 * 最佳实践：
 *   - 定期检查队列统计
 *   - 监控失败任务数量
 *   - 及时处理死信队列
 *   - 根据吞吐量调整 Worker 数量
 *   - 配置告警规则监控队列健康
 * 
 * @see TaskQueue - 任务队列核心
 * @see TaskQueueExample - 任务处理器示例
 */

#pragma once

#include <drogon/HttpController.h>
#include "../common/AjaxResult.h"
#include "TaskQueue.h"
#include "../filters/PermFilter.h"
#include <json/json.h>

class TaskQueueCtrl : public drogon::HttpController<TaskQueueCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(TaskQueueCtrl::stats,           "/monitor/taskqueue/stats",              drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(TaskQueueCtrl::getTask,         "/monitor/taskqueue/task/{id}",          drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(TaskQueueCtrl::getTaskLog,      "/monitor/taskqueue/task/{id}/log",      drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(TaskQueueCtrl::requeueTask,     "/monitor/taskqueue/requeue/{id}",       drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(TaskQueueCtrl::clearDeadLetter, "/monitor/taskqueue/deadletter/{type}",  drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    /**
     * Get queue statistics for all queue types
     * GET /monitor/taskqueue/stats
     */
    void stats(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "monitor:taskqueue:list");

        Json::Value result(Json::objectValue);
        
        // Get stats for each queue type
        std::vector<std::string> types = {"email", "export", "process", "notification"};
        for (const auto& type : types) {
            auto stat = TaskQueue::instance().getStats(type);
            Json::Value typeStats;
            typeStats["pending"] = (Json::Int64)stat.pendingCount;
            typeStats["processing"] = (Json::Int64)stat.processingCount;
            typeStats["success"] = (Json::Int64)stat.successCount;
            typeStats["failed"] = (Json::Int64)stat.failedCount;
            typeStats["deadLetter"] = (Json::Int64)stat.deadLetterCount;
            typeStats["total"] = (Json::Int64)(stat.pendingCount + stat.processingCount + 
                                               stat.successCount + stat.failedCount + stat.deadLetterCount);
            result[type] = typeStats;
        }

        RESP_OK(cb, result);
    }

    /**
     * Get task details
     * GET /monitor/taskqueue/task/{id}
     */
    void getTask(const drogon::HttpRequestPtr &req,
                 std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                 const std::string &id) {
        CHECK_PERM(req, cb, "monitor:taskqueue:detail");

        auto task = TaskQueue::instance().getTask(id);
        if (task.id.empty()) {
            RESP_ERR(cb, "Task not found");
            return;
        }

        RESP_OK(cb, task.toJson());
    }

    /**
     * Get task execution log
     * GET /monitor/taskqueue/task/{id}/log
     */
    void getTaskLog(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                    const std::string &id) {
        CHECK_PERM(req, cb, "monitor:taskqueue:detail");

        auto logs = TaskQueue::instance().getTaskLog(id);
        
        Json::Value result(Json::arrayValue);
        for (const auto& log : logs) {
            result.append(log);
        }

        RESP_OK(cb, result);
    }

    /**
     * Requeue dead letter task
     * POST /monitor/taskqueue/requeue/{id}
     */
    void requeueTask(const drogon::HttpRequestPtr &req,
                     std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                     const std::string &id) {
        CHECK_PERM(req, cb, "monitor:taskqueue:requeue");

        bool success = TaskQueue::instance().requeueDeadLetter(id);
        if (!success) {
            RESP_ERR(cb, "Failed to requeue task");
            return;
        }

        RESP_MSG(cb, "Task requeued successfully");
    }

    /**
     * Clear dead letter queue
     * DELETE /monitor/taskqueue/deadletter/{type}
     */
    void clearDeadLetter(const drogon::HttpRequestPtr &req,
                         std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                         const std::string &type) {
        CHECK_PERM(req, cb, "monitor:taskqueue:clear");

        TaskQueue::instance().clearDeadLetter(type);
        RESP_MSG(cb, "Dead letter queue cleared");
    }
};
