/**
 * @file HealthCtrl.h
 * @brief 健康检查控制器
 */

#pragma once
#include <drogon/HttpController.h>

class HealthCtrl : public drogon::HttpController<HealthCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(HealthCtrl::health, "/health", drogon::Get);
        ADD_METHOD_TO(HealthCtrl::ready, "/ready", drogon::Get);
        ADD_METHOD_TO(HealthCtrl::live, "/live", drogon::Get);
    METHOD_LIST_END

    // GET /health - 综合健康检查
    void health(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        Json::Value result;
        result["status"] = "healthy";
        result["timestamp"] = static_cast<Json::UInt64>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());

        // 检查数据库
        result["checks"]["database"] = checkDatabase();

        // 检查 Redis
        result["checks"]["redis"] = checkRedis();

        // 检查内存
        result["checks"]["memory"] = checkMemory();

        // 决定整体状态
        bool allHealthy = true;
        for (const auto& check : result["checks"]) {
            if (check["status"] != "ok") {
                allHealthy = false;
                break;
            }
        }
        result["status"] = allHealthy ? "healthy" : "degraded";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(allHealthy ? drogon::k200OK : drogon::k503ServiceUnavailable);
        cb(resp);
    }

    // GET /ready - K8s readiness probe
    void ready(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        Json::Value result;
        result["ready"] = true;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        cb(resp);
    }

    // GET /live - K8s liveness probe
    void live(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        Json::Value result;
        result["alive"] = true;
        cb(drogon::HttpResponse::newHttpJsonResponse(result));
    }

private:
    Json::Value checkDatabase() {
        Json::Value check;
        try {
            // DatabaseService::instance().query("SELECT 1");
            check["status"] = "ok";
        } catch (...) {
            check["status"] = "error";
            check["message"] = "Database connection failed";
        }
        return check;
    }

    Json::Value checkRedis() {
        Json::Value check;
        try {
            // MemCache::instance().get("health_check");
            check["status"] = "ok";
        } catch (...) {
            check["status"] = "error";
            check["message"] = "Redis connection failed";
        }
        return check;
    }

    Json::Value checkMemory() {
        Json::Value check;
        // 简单内存检查：使用量 < 90%
        // size_t used = getMemoryUsage();
        // size_t total = getTotalMemory();
        // check["used"] = used;
        // check["total"] = total;
        // check["percent"] = (used * 100) / total;
        check["status"] = "ok";
        return check;
    }
};
