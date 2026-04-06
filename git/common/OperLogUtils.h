#pragma once
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <drogon/drogon.h>
#include "../services/DatabaseService.h"
#include "IpUtils.h"
#include "../filters/PermFilter.h"

enum class BusinessType : int {
    OTHER   = 0,
    INSERT  = 1,
    UPDATE  = 2,
    REMOVE  = 3,
    GRANT   = 4,
    EXPORT  = 5,
    IMPORT  = 6,
    FORCE   = 7,
    GENCODE = 8,
    CLEAN   = 9,
};

// 操作日志工具（对应 C# [Log] AOP 属性）
// write() 立即返回，DB 写入在后台单线程队列中异步执行
namespace OperLogUtils {

    // ── 日志条目（纯数据，不持有 HttpRequest 引用）──────────────────────────
    struct LogEntry {
        std::string title, requestMethod, operName, url, ip, location, param, result, errorMsg;
        int businessType = 0;
        int status       = 0;
        long long costTime = 0;
    };

    // ── 后台单线程异步队列 ──────────────────────────────────────────────────
    class AsyncQueue {
    public:
        static AsyncQueue& instance() {
            static AsyncQueue q;
            return q;
        }
        void push(LogEntry e) {
            { std::lock_guard<std::mutex> lk(mu_); q_.push(std::move(e)); }
            cv_.notify_one();
        }
        ~AsyncQueue() {
            stop_.store(true);
            cv_.notify_all();
            if (worker_.joinable()) worker_.join();
        }
    private:
        AsyncQueue() { worker_ = std::thread([this]{ run(); }); }
        void run() {
            while (true) {
                LogEntry e;
                {
                    std::unique_lock<std::mutex> lk(mu_);
                    cv_.wait(lk, [this]{ return !q_.empty() || stop_.load(); });
                    if (q_.empty()) return;
                    e = std::move(q_.front()); q_.pop();
                }
                try {
                    DatabaseService::instance().execParams(
                        "INSERT INTO sys_oper_log"
                        "(title,business_type,method,request_method,operator_type,oper_name,dept_name,"
                        " oper_url,oper_ip,oper_location,oper_param,json_result,status,error_msg,oper_time,cost_time) "
                        "VALUES($1,$2,$3,$4,0,$5,'',$6,$7,$8,$9,$10,$11,$12,NOW(),$13)",
                        {e.title,
                         std::to_string(e.businessType),
                         e.title,
                         e.requestMethod,
                         e.operName,
                         e.url,
                         e.ip,
                         e.location,
                         e.param,
                         e.result,
                         std::to_string(e.status),
                         e.errorMsg,
                         std::to_string(e.costTime)});
                } catch (...) {}
            }
        }
        std::queue<LogEntry> q_;
        std::mutex           mu_;
        std::condition_variable cv_;
        std::thread          worker_;
        std::atomic<bool>    stop_{false};
    };

    // ── 辅助函数 ────────────────────────────────────────────────────────────
    inline std::string truncate(const std::string &s, size_t maxLen = 2000) {
        return s.size() <= maxLen ? s : s.substr(0, maxLen);
    }

    inline std::string getOperParam(const drogon::HttpRequestPtr &req) {
        auto body = req->getJsonObject();
        if (body) {
            Json::StreamWriterBuilder wb;
            wb["indentation"] = "";
            return truncate(Json::writeString(wb, *body));
        }
        std::string q;
        for (auto &[k, v] : req->getParameters()) {
            if (!q.empty()) q += "&";
            q += k + "=" + v;
        }
        return truncate(q);
    }

    inline long long nowMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    // ── write()：提取数据后立即返回，DB 写由后台线程完成 ───────────────────
    inline void write(const drogon::HttpRequestPtr &req,
                      const std::string &title,
                      BusinessType businessType,
                      const std::string &operParam = "",
                      int status = 0,
                      const std::string &jsonResult = "",
                      const std::string &errorMsg = "",
                      long long costTime = 0) {
        LogEntry e;
        e.title         = title;
        e.businessType  = static_cast<int>(businessType);
        e.requestMethod = req->getMethodString();
        e.operName      = PermissionChecker::getUserName(req);
        e.url           = std::string(req->getPath());
        e.ip            = IpUtils::getIpAddr(req);
        e.location      = IpUtils::getIpLocation(e.ip);
        e.param         = operParam.empty() ? getOperParam(req) : truncate(operParam);
        e.result        = truncate(jsonResult);
        e.errorMsg      = errorMsg;
        e.status        = status;
        e.costTime      = costTime;
        AsyncQueue::instance().push(std::move(e));
    }

}

// 快捷宏，在控制器函数体内使用
#define LOG_OPER(req, title, btype) \
    OperLogUtils::write((req), (title), (btype))

#define LOG_OPER_PARAM(req, title, btype, param) \
    OperLogUtils::write((req), (title), (btype), (param))

// 带耗时版本：startMs = OperLogUtils::nowMs() 在处理前调用
#define LOG_OPER_TIMED(req, title, btype, startMs) \
    OperLogUtils::write((req), (title), (btype), "", 0, "", "", \
        OperLogUtils::nowMs() - (startMs))

#define LOG_OPER_PARAM_TIMED(req, title, btype, param, startMs) \
    OperLogUtils::write((req), (title), (btype), (param), 0, "", "", \
        OperLogUtils::nowMs() - (startMs))
