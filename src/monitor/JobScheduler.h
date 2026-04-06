#pragma once
#include <string>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <sstream>
#include <iostream>
#include <ctime>
#include "../common/CronUtils.h"
#include <trantor/utils/Logger.h>
#include "../services/DatabaseService.h"

// ── 内置任务注册表 ─────────────────────────────────────────────────────────────
// invokeTarget 格式: "ryTask.ryNoParams" / "ryTask.ryParams('hello')"
// 用户可通过 JobScheduler::instance().registerTask() 扩展

class JobScheduler {
public:
    struct JobEntry {
        long        jobId;
        std::string jobName;
        std::string jobGroup;
        std::string cronExpr;
        std::string invokeTarget;
        bool        concurrent;  // false = 串行（上次未完成则跳过）
        std::atomic<bool> running{false};
    };

    static JobScheduler& instance() {
        static JobScheduler inst;
        return inst;
    }

    // 注册自定义任务处理器（函数名 → 回调）
    using TaskHandler = std::function<std::string(const std::string& params)>;
    void registerTask(const std::string& name, TaskHandler handler) {
        std::lock_guard<std::mutex> lk(mu_);
        handlers_[name] = handler;
    }

    // 从数据库加载所有启用的任务并开始调度
    void init() {
        registerBuiltins();
        loadFromDb();
        if (!schedulerThread_.joinable()) {
            stopFlag_ = false;
            schedulerThread_ = std::thread([this]{ schedulerLoop(); });
        }
        LOG_INFO << "[JobScheduler] Started, " << jobs_.size() << " jobs loaded";
    }

    void stop() {
        stopFlag_ = true;
        if (schedulerThread_.joinable()) schedulerThread_.join();
    }

    // 添加/更新单个任务到调度器
    void scheduleJob(long jobId, const std::string& cron,
                     const std::string& invokeTarget,
                     const std::string& jobName, const std::string& jobGroup,
                     bool concurrent) {
        std::lock_guard<std::mutex> lk(mu_);
        auto& e = jobs_[jobId];
        e.jobId = jobId; e.cronExpr = cron;
        e.invokeTarget = invokeTarget;
        e.jobName = jobName; e.jobGroup = jobGroup;
        e.concurrent = concurrent;
    }

    // 停止并移除任务
    void unscheduleJob(long jobId) {
        std::lock_guard<std::mutex> lk(mu_);
        jobs_.erase(jobId);
    }

    // 立即执行一次（手动触发）
    void runOnce(long jobId, const std::string& invokeTarget,
                 const std::string& jobName, const std::string& jobGroup) {
        std::thread([this, jobId, invokeTarget, jobName, jobGroup]{
            executeJob(jobId, invokeTarget, jobName, jobGroup, "手动触发");
        }).detach();
    }

    // 重新从 DB 加载（任务编辑后调用）
    void reloadFromDb() {
        loadFromDb();
    }

private:
    std::map<long, JobEntry> jobs_;
    std::map<std::string, TaskHandler> handlers_;
    std::mutex mu_;
    std::thread schedulerThread_;
    std::atomic<bool> stopFlag_{false};

    JobScheduler() = default;
    ~JobScheduler() { stop(); }

    void registerBuiltins() {
        // 测试任务
        registerTask("ryTask.ryNoParams", [](const std::string&) {
            return std::string("无参任务执行成功");
        });
        registerTask("ryTask.ryParams", [](const std::string& p) {
            return "有参任务执行成功, 参数: " + p;
        });
        registerTask("ryTask.ryMultipleParams", [](const std::string& p) {
            return "多参任务执行成功, 参数: " + p;
        });
        // 日志清理任务
        registerTask("cleanLogTask.clean", [](const std::string& p) {
            int days = p.empty() ? 90 : std::stoi(p);
            auto& db = DatabaseService::instance();
            std::string d = std::to_string(days);
            db.execParams("DELETE FROM sys_oper_log   WHERE create_time < NOW() - ($1 || ' days')::INTERVAL", {d});
            db.execParams("DELETE FROM sys_logininfor WHERE login_time   < NOW() - ($1 || ' days')::INTERVAL", {d});
            db.execParams("DELETE FROM sys_job_log    WHERE create_time < NOW() - ($1 || ' days')::INTERVAL", {d});
            return "日志清理完成 (>" + d + "天)";
        });
        registerTask("sysJobLog.clean", [](const std::string& p) {
            int days = p.empty() ? 30 : std::stoi(p);
            std::string d = std::to_string(days);
            DatabaseService::instance().execParams(
                "DELETE FROM sys_job_log WHERE create_time < NOW() - ($1 || ' days')::INTERVAL", {d});
            return "任务日志清理完成";
        });
    }

    void loadFromDb() {
        auto res = DatabaseService::instance().query(
            "SELECT job_id,job_name,job_group,invoke_target,cron_expression,concurrent "
            "FROM sys_job WHERE status='0'"); // '0'=正常
        std::lock_guard<std::mutex> lk(mu_);
        jobs_.clear();
        if (!res.ok()) return;
        for (int i = 0; i < res.rows(); ++i) {
            long id = res.longVal(i, 0);
            auto& e = jobs_[id];
            e.jobId        = id;
            e.jobName      = res.str(i, 1);
            e.jobGroup     = res.str(i, 2);
            e.invokeTarget = res.str(i, 3);
            e.cronExpr     = res.str(i, 4);
            e.concurrent   = (res.str(i, 5) == "1");
        }
    }

    void schedulerLoop() {
        while (!stopFlag_) {
            auto now = std::time(nullptr);
            struct tm t{};
#ifdef _WIN32
            localtime_s(&t, &now);
#else
            localtime_r(&now, &t);
#endif
            {
                std::lock_guard<std::mutex> lk(mu_);
                for (auto& [id, job] : jobs_) {
                    if (!CronUtils::matches(job.cronExpr, t)) continue;
                    if (!job.concurrent && job.running) continue; // 串行模式跳过
                    // 异步执行
                    long jid = job.jobId;
                    std::string tgt = job.invokeTarget;
                    std::string nm  = job.jobName;
                    std::string grp = job.jobGroup;
                    job.running = true;
                    std::thread([this, jid, tgt, nm, grp]{
                        executeJob(jid, tgt, nm, grp, "定时触发");
                        std::lock_guard<std::mutex> lk2(mu_);
                        if (jobs_.count(jid)) jobs_[jid].running = false;
                    }).detach();
                }
            }
            // 等到下一秒整
            auto elapsed = (int)(std::time(nullptr) - now);
            if (elapsed < 1) std::this_thread::sleep_for(std::chrono::milliseconds(1000 - elapsed * 1000 + 50));
            else std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    void executeJob(long jobId, const std::string& invokeTarget,
                    const std::string& jobName, const std::string& jobGroup,
                    const std::string& triggerType) {
        auto start = std::chrono::steady_clock::now();
        std::string result, exInfo;
        std::string status = "0"; // 0=成功

        try {
            result = dispatch(invokeTarget);
        } catch (const std::exception& ex) {
            exInfo = ex.what();
            status = "1";
            LOG_ERROR << "[JobScheduler] Job " << jobName << " failed: " << ex.what();
        } catch (...) {
            exInfo = "未知异常";
            status = "1";
        }

        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        std::string msg = triggerType + " | 耗时 " + std::to_string(ms) + "ms";
        if (!result.empty()) msg += " | " + result;

        DatabaseService::instance().execParams(
            "INSERT INTO sys_job_log(job_name,job_group,invoke_target,job_message,status,exception_info,create_time) "
            "VALUES($1,$2,$3,$4,$5,$6,NOW())",
            {jobName, jobGroup, invokeTarget, msg, status, exInfo});

        LOG_INFO << "[JobScheduler] " << jobName << " [" << triggerType << "] "
                 << (status=="0"?"OK":"FAIL") << " " << ms << "ms";
    }

    // 解析 invokeTarget 并调用对应处理器
    // 格式: "funcName" 或 "funcName('param1', 'param2')"
    std::string dispatch(const std::string& target) {
        std::string funcName = target;
        std::string params;
        auto lp = target.find('(');
        if (lp != std::string::npos) {
            funcName = target.substr(0, lp);
            auto rp = target.rfind(')');
            if (rp != std::string::npos && rp > lp)
                params = target.substr(lp + 1, rp - lp - 1);
            // 去除参数两端引号
            if (!params.empty() && params.front() == '\'') params = params.substr(1);
            if (!params.empty() && params.back()  == '\'') params.pop_back();
        }
        // trim whitespace
        funcName.erase(0, funcName.find_first_not_of(" \t"));
        funcName.erase(funcName.find_last_not_of(" \t") + 1);

        std::lock_guard<std::mutex> lk(mu_);
        auto it = handlers_.find(funcName);
        if (it != handlers_.end()) return it->second(params);
        return "执行完成 [" + funcName + "] (未注册处理器)";
    }
};
