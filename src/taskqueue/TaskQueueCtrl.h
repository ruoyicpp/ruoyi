/**
 * @file TaskQueueCtrl.h
 * @brief Task queue management and monitoring controller
 * 
 * API Endpoints:
 *   GET  /monitor/taskqueue/stats - Get queue statistics
 *   GET  /monitor/taskqueue/task/{id} - Get task details
 *   GET  /monitor/taskqueue/task/{id}/log - Get task execution log
 *   POST /monitor/taskqueue/requeue/{id} - Requeue dead letter task
 *   DELETE /monitor/taskqueue/deadletter/{type} - Clear dead letter queue
 * 
 * Permissions:
 *   - monitor:taskqueue:list - View task queue statistics
 *   - monitor:taskqueue:detail - View task details
 *   - monitor:taskqueue:requeue - Requeue tasks
 *   - monitor:taskqueue:clear - Clear dead letter queue
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
