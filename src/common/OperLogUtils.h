/**
 * @file OperLogUtils.h
 * @brief 操作日志工具 — 记录用户操作审计日志
 * 
 * 功能概述：
 *   - 操作记录：记录用户的增删改查等操作
 *   - 审计日志：记录操作前后的数据快照，用于审计和追溯
 *   - 异步写入：后台单线程异步队列，不阻塞主线程
 *   - 自动脱敏：敏感信息自动脱敏，保护隐私
 * 
 * 日志字段：
 *   - title: 操作标题（如 "新增用户"）
 *   - businessType: 业务类型（INSERT、UPDATE、DELETE 等）
 *   - operName: 操作人员名称
 *   - url: 请求 URL
 *   - ip: 操作人 IP 地址
 *   - location: IP 地理位置
 *   - param: 请求参数（JSON 格式）
 *   - result: 返回结果（JSON 格式）
 *   - beforeData: 操作前数据快照（审计用）
 *   - afterData: 操作后数据快照（审计用）
 *   - costTime: 操作耗时（毫秒）
 *   - status: 操作状态（0 成功，1 失败）
 * 
 * 使用示例：
 *   OperLogUtils::LogEntry log;
 *   log.title = "新增用户";
 *   log.businessType = (int)BusinessType::INSERT;
 *   log.operName = user.userName;
 *   log.url = req->getPath();
 *   log.ip = IpUtils::getIpAddr(req);
 *   log.param = jsonParam;
 *   log.result = jsonResult;
 *   log.costTime = duration;
 *   OperLogUtils::write(log);
 * 
 * 配置项（config.json）：
 *   - operlog.enabled: 是否启用操作日志（默认 true）
 *   - operlog.maskSensitive: 是否脱敏敏感信息（默认 true）
 */

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

/**
 * @enum BusinessType
 * @brief 业务类型枚举
 * 
 * 用于分类操作日志的业务类型。
 */
enum class BusinessType : int {
    OTHER   = 0,  ///< 其他
    INSERT  = 1,  ///< 新增
    UPDATE  = 2,  ///< 修改
    REMOVE  = 3,  ///< 删除
    GRANT   = 4,  ///< 授权
    EXPORT  = 5,  ///< 导出
    IMPORT  = 6,  ///< 导入
    FORCE   = 7,  ///< 强制
    GENCODE = 8,  ///< 代码生成
    CLEAN   = 9,  ///< 清理
};

/**
 * @namespace OperLogUtils
 * @brief 操作日志工具命名空间
 * 
 * 提供操作日志的记录和管理功能。
 * 采用异步队列模式，write() 立即返回，DB 写入在后台单线程中执行。
 */
namespace OperLogUtils {

    // ── 日志条目（纯数据，不持有 HttpRequest 引用）──────────────────────
    struct LogEntry {
        std::string title, requestMethod, operName, url, ip, location, param, result, errorMsg;
        std::string beforeData, afterData;       // f17 审计增强：变更前/后快照
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
                        " oper_url,oper_ip,oper_location,oper_param,json_result,status,error_msg,oper_time,cost_time,"
                        " before_data,after_data) "
                        "VALUES($1,$2,$3,$4,0,$5,'',$6,$7,$8,$9,$10,$11,$12,NOW(),$13,$14,$15)",
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
                         std::to_string(e.costTime),
                         e.beforeData,
                         e.afterData});
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

    // ── write()：提取数据后立即返回，DB 写由后台线程完成 ───────────
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
        e.param         = operParam.empty() ? getOperParam(req) : truncate(operParam);
        e.result        = truncate(jsonResult);
        e.errorMsg      = errorMsg;
        e.status        = status;
        e.costTime      = costTime;
        IpUtils::getIpLocationAsync(e.ip, [e = std::move(e)](std::string loc) mutable {
            e.location = std::move(loc);
            AsyncQueue::instance().push(std::move(e));
        });
    }

    //  审计增强：依靠 diffJson() 计算的 before/after 只记变更字段 ────
    inline void writeAudit(const drogon::HttpRequestPtr &req,
                           const std::string &title,
                           BusinessType businessType,
                           const std::string &beforeData,
                           const std::string &afterData,
                           long long costTime = 0) {
        LogEntry e;
        e.title         = title;
        e.businessType  = static_cast<int>(businessType);
        e.requestMethod = req->getMethodString();
        e.operName      = PermissionChecker::getUserName(req);
        e.url           = std::string(req->getPath());
        e.ip            = IpUtils::getIpAddr(req);
        e.param         = getOperParam(req);
        e.beforeData    = truncate(beforeData, 4000);
        e.afterData     = truncate(afterData,  4000);
        e.costTime      = costTime;
        IpUtils::getIpLocationAsync(e.ip, [e = std::move(e)](std::string loc) mutable {
            e.location = std::move(loc);
            AsyncQueue::instance().push(std::move(e));
        });
    }

    // ── diffJson()：计算两个 JSON 对象的差异，返回 {field: [old, new]} 格式 ──
    // - 只记录 "发生变化" 的字段（减少价值低的全量复制）
    // - 新增/删除字段也会被记录。顺序不稳定，仅供审计。
    inline std::string diffJson(const Json::Value &before, const Json::Value &after) {
        Json::Value diff(Json::objectValue);
        // 扫 before 中有但 after 改动/删除的字段
        if (before.isObject()) {
            for (const auto &k : before.getMemberNames()) {
                if (!after.isMember(k)) {
                    Json::Value pair(Json::arrayValue);
                    pair.append(before[k]); pair.append(Json::nullValue);
                    diff[k] = pair;
                } else if (before[k] != after[k]) {
                    Json::Value pair(Json::arrayValue);
                    pair.append(before[k]); pair.append(after[k]);
                    diff[k] = pair;
                }
            }
        }
        // 扫 after 中新增的字段
        if (after.isObject()) {
            for (const auto &k : after.getMemberNames()) {
                if (!before.isMember(k)) {
                    Json::Value pair(Json::arrayValue);
                    pair.append(Json::nullValue); pair.append(after[k]);
                    diff[k] = pair;
                }
            }
        }
        if (diff.empty()) return "";
        Json::StreamWriterBuilder wb;
        wb["indentation"] = "";
        return Json::writeString(wb, diff);
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

// ── f17 审计宏：传入 before/after 两个 Json::Value，自动 diff 后入库 ─────
#define LOG_AUDIT(req, title, btype, before, after) \
    OperLogUtils::writeAudit((req), (title), (btype), \
        OperLogUtils::diffJson((before), (after)), "")

#define LOG_AUDIT_TIMED(req, title, btype, before, after, startMs) \
    OperLogUtils::writeAudit((req), (title), (btype), \
        OperLogUtils::diffJson((before), (after)), "", \
        OperLogUtils::nowMs() - (startMs))
