/**
 * @file JobScheduler.h
 * @brief 定时任务调度器 — 支持 Cron 表达式的任务调度系统
 * 
 * 功能概述：
 *   - 任务调度：支持 Cron 表达式的定时任务调度
 *   - 任务执行：异步执行任务，支持并发和串行两种模式
 *   - 任务管理：动态添加、删除、更新任务
 *   - 任务日志：记录任务执行结果、耗时、异常信息
 *   - 内置任务：提供日志清理、OA 超时转交等内置任务
 *   - 自定义任务：支持用户注册自定义任务处理器
 * 
 * 核心特性：
 *   - Cron 表达式支持：标准 Cron 格式（秒 分 时 日 月 周）
 *   - 去重机制：同一分钟内只触发一次，防止重复执行
 *   - 并发控制：支持并发执行和串行执行两种模式
 *   - 异步执行：后台线程执行任务，不阻塞调度器
 *   - 数据库持久化：任务配置和执行日志存储在数据库
 *   - 热重载：支持动态加载任务配置，无需重启应用
 * 
 * 任务配置格式：
 *   - invokeTarget: "funcName" 或 "funcName('param1','param2')"
 *   - cronExpr: "0 0 * * * *" (秒 分 时 日 月 周)
 *   - concurrent: true (并发) 或 false (串行)
 * 
 * 内置任务列表：
 *   - ryTask.ryNoParams - 无参测试任务
 *   - ryTask.ryParams - 有参测试任务
 *   - cleanLogTask.clean - 清理操作日志（默认 90 天）
 *   - sysJobLog.clean - 清理任务日志（默认 30 天）
 *   - oaTask.approvalTimeoutTransfer - OA 审批超时转交
 *   - oaTask.assetOverdueReminder - OA 资产逾期提醒
 * 
 * 使用示例：
 *   // 初始化调度器
 *   JobScheduler::instance().init();
 *   
 *   // 注册自定义任务
 *   JobScheduler::instance().registerTask("myTask.hello", [](const std::string& p) {
 *       return "Hello " + p;
 *   });
 *   
 *   // 手动触发任务
 *   JobScheduler::instance().runOnce(1, "myTask.hello('world')", "测试", "自定义");
 *   
 *   // 停止调度器
 *   JobScheduler::instance().stop();
 * 
 * @see CronUtils - Cron 表达式工具
 * @see NotifyService - 通知服务
 * @see DatabaseService - 数据库服务
 */

#pragma once
#include <string>        ///< 字符串处理
#include <map>           ///< 任务映射表
#include <mutex>         ///< 互斥锁
#include <thread>        ///< 线程管理
#include <atomic>        ///< 原子操作
#include <functional>    ///< 函数对象
#include <sstream>       ///< 字符串流
#include <iostream>      ///< 输入输出
#include <ctime>         ///< 时间函数
#include "../common/CronUtils.h"           ///< Cron 表达式工具
#include "../common/NotifyService.h"       ///< 通知服务
#include <trantor/utils/Logger.h>         ///< 日志库
#include "../services/DatabaseService.h"  ///< 数据库服务

/**
 * @class JobScheduler
 * @brief 定时任务调度器单例类
 * 
 * 提供 Cron 表达式的任务调度功能。
 * 支持任务的动态注册、执行、日志记录等。
 * 
 * invokeTarget 格式说明：
 *   - "ryTask.ryNoParams" - 无参任务
 *   - "ryTask.ryParams('hello')" - 有参任务（参数用单引号括起）
 *   - "ryTask.ryMultipleParams('p1','p2')" - 多参任务
 * 
 * 用户可通过 JobScheduler::instance().registerTask() 扩展自定义任务
 */
class JobScheduler {
public:
    /**
     * @struct JobEntry
     * @brief 任务条目结构体
     * 
     * 存储单个任务的配置和运行状态。
     */
    struct JobEntry {
        long        jobId;              ///< 任务 ID（数据库主键）
        std::string jobName;            ///< 任务名称（用于日志和显示）
        std::string jobGroup;           ///< 任务分组（用于分类管理）
        std::string cronExpr;           ///< Cron 表达式（秒 分 时 日 月 周）
        std::string invokeTarget;       ///< 调用目标（函数名和参数）
        bool        concurrent;         ///< 并发模式：true=并发，false=串行（上次未完成则跳过）
        std::atomic<bool> running{false};  ///< 任务运行状态（原子操作，线程安全）
        // 记录上次触发的"年月日时分"标识 (YYYYMMDDHHmm)，去重整分钟内多次触发
        long long   lastFireMin = 0;    ///< 上次触发时间（分钟级去重）
    };

    /**
     * @brief 获取调度器单例实例
     * 
     * 使用静态变量实现单例模式，确保全局只有一个调度器实例。
     * 
     * @return 调度器单例引用
     */
    static JobScheduler& instance() {
        static JobScheduler inst;  ///< 静态实例，首次调用时创建
        return inst;
    }

    /**
     * @brief 任务处理器函数类型定义
     * 
     * 接收参数字符串，返回执行结果字符串。
     * 用于注册自定义任务的处理函数。
     */
    using TaskHandler = std::function<std::string(const std::string& params)>;
    
    /**
     * @brief 注册自定义任务处理器
     * 
     * 将任务名称与处理函数绑定，支持后续调用。
     * 线程安全，使用互斥锁保护共享数据。
     * 
     * @param name 任务名称（例如："myTask.hello"）
     * @param handler 任务处理函数（接收参数，返回结果）
     * 
     * @note 
     *   - 同名任务会被覆盖
     *   - 处理函数应该是线程安全的
     *   - 处理函数应该快速返回，避免阻塞调度器
     */
    void registerTask(const std::string& name, TaskHandler handler) {
        std::lock_guard<std::mutex> lk(mu_);  ///< 加锁保护 handlers_ 访问
        handlers_[name] = handler;             ///< 存储处理函数
    }

    /**
     * @brief 初始化调度器
     * 
     * 执行以下步骤：
     *   1. 注册内置任务处理器
     *   2. 从数据库加载任务配置
     *   3. 启动调度线程
     *   4. 记录启动日志
     * 
     * @note 
     *   - 应在应用启动时调用
     *   - 可以多次调用，但只会启动一个调度线程
     *   - 需要数据库连接可用
     */
    void init() {
        registerBuiltins();  ///< 注册内置任务（日志清理、OA 任务等）
        loadFromDb();        ///< 从数据库加载任务配置
        
        // 启动调度线程（如果还未启动）
        if (!schedulerThread_.joinable()) {
            stopFlag_ = false;  ///< 清除停止标志
            // 创建调度线程，执行 schedulerLoop()
            schedulerThread_ = std::thread([this]{ schedulerLoop(); });
        }
        
        // 记录启动日志
        LOG_INFO << "[JobScheduler] Started, " << jobs_.size() << " jobs loaded";
    }

    /**
     * @brief 停止调度器
     * 
     * 设置停止标志，等待调度线程退出。
     * 线程安全，可以从任何线程调用。
     * 
     * @note 
     *   - 应在应用关闭时调用
     *   - 会阻塞直到调度线程完全退出
     *   - 已执行的任务不会被中断
     */
    void stop() {
        stopFlag_ = true;  ///< 设置停止标志
        // 等待调度线程退出
        if (schedulerThread_.joinable()) {
            schedulerThread_.join();
        }
    }

    /**
     * @brief 添加或更新单个任务到调度器
     * 
     * 如果任务 ID 已存在，则更新任务配置。
     * 否则创建新任务。
     * 线程安全。
     * 
     * @param jobId 任务 ID（数据库主键）
     * @param cron Cron 表达式（秒 分 时 日 月 周）
     * @param invokeTarget 调用目标（函数名和参数）
     * @param jobName 任务名称（用于日志）
     * @param jobGroup 任务分组（用于分类）
     * @param concurrent 是否并发执行（true=并发，false=串行）
     * 
     * @note 
     *   - 新任务会在下一个匹配的时刻执行
     *   - 串行模式下，如果上次执行未完成，会跳过本次执行
     */
    void scheduleJob(long jobId, const std::string& cron,
                     const std::string& invokeTarget,
                     const std::string& jobName, const std::string& jobGroup,
                     bool concurrent) {
        std::lock_guard<std::mutex> lk(mu_);  ///< 加锁保护 jobs_ 访问
        auto& e = jobs_[jobId];               ///< 获取或创建任务条目
        
        // 更新任务配置
        e.jobId = jobId;
        e.cronExpr = cron;
        e.invokeTarget = invokeTarget;
        e.jobName = jobName;
        e.jobGroup = jobGroup;
        e.concurrent = concurrent;
    }

    /**
     * @brief 停止并移除任务
     * 
     * 从调度器中删除指定任务。
     * 如果任务正在执行，执行会继续，但不会再次触发。
     * 线程安全。
     * 
     * @param jobId 任务 ID
     * 
     * @note 
     *   - 如果任务不存在，不会产生错误
     *   - 已执行的任务日志不会被删除
     */
    void unscheduleJob(long jobId) {
        std::lock_guard<std::mutex> lk(mu_);  ///< 加锁保护 jobs_ 访问
        jobs_.erase(jobId);                   ///< 删除任务
    }

    /**
     * @brief 立即执行一次任务（手动触发）
     * 
     * 在后台线程中立即执行指定任务，不受 Cron 表达式限制。
     * 异步执行，立即返回。
     * 
     * @param jobId 任务 ID
     * @param invokeTarget 调用目标（函数名和参数）
     * @param jobName 任务名称（用于日志）
     * @param jobGroup 任务分组（用于日志）
     * 
     * @note 
     *   - 异步执行，不会阻塞调用者
     *   - 执行结果会记录到数据库
     *   - 触发类型标记为"手动触发"
     */
    void runOnce(long jobId, const std::string& invokeTarget,
                 const std::string& jobName, const std::string& jobGroup) {
        // 在后台线程中执行任务
        std::thread([this, jobId, invokeTarget, jobName, jobGroup]{
            executeJob(jobId, invokeTarget, jobName, jobGroup, "手动触发");
        }).detach();  ///< 分离线程，不等待执行完成
    }

    /**
     * @brief 重新从数据库加载任务配置
     * 
     * 刷新任务列表，加载数据库中的最新配置。
     * 用于任务编辑后的配置更新。
     * 线程安全。
     * 
     * @note 
     *   - 会清空现有任务列表
     *   - 正在执行的任务不会被中断
     *   - 新任务会在下一个匹配的时刻执行
     */
    void reloadFromDb() {
        loadFromDb();  ///< 重新加载数据库配置
    }

private:
    // ── 私有成员变量 ────────────────────────────────────────────────────
    std::map<long, JobEntry> jobs_;                    ///< 任务映射表（ID → 任务条目）
    std::map<std::string, TaskHandler> handlers_;      ///< 处理器映射表（函数名 → 处理函数）
    std::mutex mu_;                                    ///< 互斥锁（保护 jobs_ 和 handlers_）
    std::thread schedulerThread_;                      ///< 调度线程
    std::atomic<bool> stopFlag_{false};                ///< 停止标志（原子操作，线程安全）

    /**
     * @brief 私有构造函数
     * 
     * 防止外部创建实例，确保单例模式。
     */
    JobScheduler() = default;
    
    /**
     * @brief 析构函数
     * 
     * 停止调度器，清理资源。
     */
    ~JobScheduler() { stop(); }

    /**
     * @brief 注册内置任务处理器
     * 
     * 在初始化时调用，注册系统提供的内置任务：
     *   - ryTask.ryNoParams - 无参测试任务
     *   - ryTask.ryParams - 有参测试任务
     *   - ryTask.ryMultipleParams - 多参测试任务
     *   - cleanLogTask.clean - 清理操作日志（默认 90 天）
     *   - sysJobLog.clean - 清理任务日志（默认 30 天）
     *   - oaTask.approvalTimeoutTransfer - OA 审批超时转交
     *   - oaTask.assetOverdueReminder - OA 资产逾期提醒
     * 
     * @note 
     *   - 用户可通过 registerTask() 添加自定义任务
     *   - 内置任务可以被用户任务覆盖
     */
    void registerBuiltins() {
        // ── 测试任务 ────────────────────────────────────────────────────────
        /**
         * @brief 无参测试任务
         * 
         * 简单的测试任务，不需要参数。
         * 用于验证任务调度器是否正常工作。
         */
        registerTask("ryTask.ryNoParams", [](const std::string&) {
            return std::string("无参任务执行成功");
        });
        
        /**
         * @brief 有参测试任务
         * 
         * 接收一个参数的测试任务。
         * 用于验证参数传递是否正常。
         */
        registerTask("ryTask.ryParams", [](const std::string& p) {
            return "有参任务执行成功, 参数: " + p;
        });
        
        /**
         * @brief 多参测试任务
         * 
         * 接收多个参数的测试任务。
         * 参数格式：'p1','p2','p3'
         */
        registerTask("ryTask.ryMultipleParams", [](const std::string& p) {
            return "多参任务执行成功, 参数: " + p;
        });
        
        // ── 日志清理任务 ────────────────────────────────────────────────────
        /**
         * @brief 清理操作日志和登录日志
         * 
         * 删除指定天数之前的日志记录。
         * 清理表：
         *   - sys_oper_log（操作日志）
         *   - sys_logininfor（登录日志）
         *   - sys_job_log（任务日志）
         * 
         * @param p 保留天数（默认 90 天）
         * @return 清理完成提示信息
         * 
         * @note 
         *   - 默认保留 90 天的日志
         *   - 参数为空时使用默认值
         *   - 参数无效时使用默认值
         */
        registerTask("cleanLogTask.clean", [](const std::string& p) {
            // 解析参数，获取保留天数
            int days = 90;  ///< 默认保留 90 天
            if (!p.empty()) {
                try {
                    days = std::stoi(p);  ///< 尝试解析参数
                } catch (...) {
                    days = 90;  ///< 解析失败使用默认值
                }
            }
            
            // 获取数据库实例
            auto& db = DatabaseService::instance();
            std::string d = std::to_string(days);
            
            // 删除操作日志
            db.execParams(
                "DELETE FROM sys_oper_log WHERE create_time < NOW() - ($1 || ' days')::INTERVAL", 
                {d});
            
            // 删除登录日志
            db.execParams(
                "DELETE FROM sys_logininfor WHERE login_time < NOW() - ($1 || ' days')::INTERVAL", 
                {d});
            
            // 删除任务日志
            db.execParams(
                "DELETE FROM sys_job_log WHERE create_time < NOW() - ($1 || ' days')::INTERVAL", 
                {d});
            
            // 返回执行结果
            return "日志清理完成 (>" + d + "天)";
        });
        
        /**
         * @brief 清理任务日志
         * 
         * 删除指定天数之前的任务执行日志。
         * 
         * @param p 保留天数（默认 30 天）
         * @return 清理完成提示信息
         * 
         * @note 
         *   - 默认保留 30 天的日志
         *   - 参数为空时使用默认值
         */
        registerTask("sysJobLog.clean", [](const std::string& p) {
            // 解析参数，获取保留天数
            int days = 30;  ///< 默认保留 30 天
            if (!p.empty()) {
                try {
                    days = std::stoi(p);  ///< 尝试解析参数
                } catch (...) {
                    days = 30;  ///< 解析失败使用默认值
                }
            }
            
            // 转换为字符串
            std::string d = std::to_string(days);
            
            // 删除任务日志
            DatabaseService::instance().execParams(
                "DELETE FROM sys_job_log WHERE create_time < NOW() - ($1 || ' days')::INTERVAL", 
                {d});
            
            // 返回执行结果
            return "任务日志清理完成";
        });
        // ── OA 审批超时转交任务 ────────────────────────────────────────────
        /**
         * @brief OA 审批超时自动转交
         * 
         * 查找超时的待审批项目，自动转交给指定的转交人。
         * 
         * 流程：
         *   1. 查询所有待审批且已超时的项目
         *   2. 对每个超时项目：
         *      - 获取新审批人信息
         *      - 更新审批记录（转交给新审批人）
         *      - 向新审批人发送通知（警告级别）
         *      - 向原审批人发送通知（信息级别）
         *   3. 返回转交数量
         * 
         * @return 转交完成提示信息（包含转交数量）
         * 
         * @note 
         *   - 只处理待审批状态的项目
         *   - 只处理配置了转交人的项目
         *   - 使用 timeout_hours 字段判断是否超时
         */
        registerTask("oaTask.approvalTimeoutTransfer", [](const std::string&) {
            // 获取数据库实例
            auto& db = DatabaseService::instance();
            
            // 查询所有超时的待审批项目
            auto res = db.query(
                "SELECT id,title,reviewer_id,transfer_target_id FROM oa_approval "
                "WHERE status='pending' AND transfer_target_id>0 AND submitted_at IS NOT NULL "
                "AND NOW() > submitted_at + (timeout_hours || ' hours')::INTERVAL");
            
            int changed = 0;  ///< 转交计数
            
            // 处理每个超时项目
            if (res.ok()) {
                for (int i = 0; i < res.rows(); ++i) {
                    // 提取项目信息
                    long long approvalId = res.longVal(i, 0);      ///< 审批 ID
                    std::string title = res.str(i, 1);             ///< 审批标题
                    long long oldReviewerId = res.longVal(i, 2);   ///< 原审批人 ID
                    long long newReviewerId = res.longVal(i, 3);   ///< 新审批人 ID
                    
                    // 获取新审批人名称
                    auto target = db.queryParams(
                        "SELECT user_name FROM sys_user WHERE user_id=$1", 
                        {std::to_string(newReviewerId)});
                    std::string newReviewerName = 
                        target.ok() && target.rows() > 0 ? 
                        target.str(0, 0) : 
                        std::string("系统转交");
                    
                    // 更新审批记录
                    db.execParams(
                        "UPDATE oa_approval SET reviewer_id=$1,reviewer_name=$2,next_reviewer_id=$1,"
                        "submitted_at=NOW(),update_by='job',update_time=NOW() WHERE id=$3",
                        {std::to_string(newReviewerId), newReviewerName, std::to_string(approvalId)});
                    
                    // 向新审批人发送通知（警告级别）
                    NotifyService::sendInbox(
                        newReviewerId, 
                        "超时转交待审批", 
                        "审批[" + title + "]已因超时转交给您", 
                        "warn");
                    
                    // 向原审批人发送通知（信息级别）
                    if (oldReviewerId > 0) {
                        NotifyService::sendInbox(
                            oldReviewerId, 
                            "审批已转交", 
                            "审批[" + title + "]因超时已被系统转交", 
                            "info");
                    }
                    
                    ++changed;  ///< 增加转交计数
                }
            }
            
            // 返回执行结果
            return "审批超时转交完成: " + std::to_string(changed) + " 条";
        });
        
        // ── OA 资产逾期提醒任务 ────────────────────────────────────────────
        /**
         * @brief OA 资产逾期提醒
         * 
         * 查找已逾期的借用资产，向借用人发送提醒通知。
         * 
         * 流程：
         *   1. 查询所有已逾期的借用资产
         *   2. 对每个逾期资产：
         *      - 获取借用人信息
         *      - 向借用人发送提醒通知（警告级别）
         *   3. 返回提醒数量
         * 
         * @return 提醒完成提示信息（包含提醒数量）
         * 
         * @note 
         *   - 只处理已借出状态的资产
         *   - 比较应还日期与当前日期
         *   - 通知级别为警告（warn）
         */
        registerTask("oaTask.assetOverdueReminder", [](const std::string&) {
            // 获取数据库实例
            auto& db = DatabaseService::instance();
            
            // 查询所有已逾期的借用资产
            auto res = db.query(
                "SELECT id,asset_name,applicant_id,expect_return_date FROM oa_asset_borrow "
                "WHERE status='borrowed' AND expect_return_date < CURRENT_DATE");
            
            int reminded = 0;  ///< 提醒计数
            
            // 处理每个逾期资产
            if (res.ok()) {
                for (int i = 0; i < res.rows(); ++i) {
                    // 提取资产信息
                    long long applicantId = res.longVal(i, 2);     ///< 借用人 ID
                    std::string assetName = res.str(i, 1);         ///< 资产名称
                    std::string dueDate = res.str(i, 3);           ///< 应还日期
                    
                    // 向借用人发送提醒通知（警告级别）
                    NotifyService::sendInbox(
                        applicantId, 
                        "资产归还提醒", 
                        "您借用的资产[" + assetName + "]已逾期，请尽快归还（应还日期：" + dueDate + "）", 
                        "warn");
                    
                    ++reminded;  ///< 增加提醒计数
                }
            }
            
            // 返回执行结果
            return "资产逾期提醒完成: " + std::to_string(reminded) + " 条";
        });
    }

    /**
     * @brief 从数据库加载任务配置
     * 
     * 从 sys_job 表读取所有启用的任务配置。
     * 线程安全，使用互斥锁保护。
     * 
     * 流程：
     *   1. 查询数据库中所有状态为 '0'（启用）的任务
     *   2. 清空现有任务列表
     *   3. 遍历查询结果，构建任务条目
     *   4. 存储到 jobs_ 映射表
     * 
     * @note 
     *   - 会清空现有任务列表
     *   - 只加载状态为 '0' 的任务
     *   - 正在执行的任务不会被中断
     */
    void loadFromDb() {
        // 从数据库查询所有启用的任务
        auto res = DatabaseService::instance().query(
            "SELECT job_id,job_name,job_group,invoke_target,cron_expression,concurrent "
            "FROM sys_job WHERE status='0'");  // '0' = 启用状态
        
        // 加锁保护 jobs_ 访问
        std::lock_guard<std::mutex> lk(mu_);
        
        // 清空现有任务列表
        jobs_.clear();
        
        // 如果查询失败，直接返回
        if (!res.ok()) return;
        
        // 遍历查询结果，构建任务条目
        for (int i = 0; i < res.rows(); ++i) {
            // 获取任务 ID
            long id = res.longVal(i, 0);
            
            // 获取或创建任务条目
            auto& e = jobs_[id];
            
            // 填充任务信息
            e.jobId        = id;                          ///< 任务 ID
            e.jobName      = res.str(i, 1);               ///< 任务名称
            e.jobGroup     = res.str(i, 2);               ///< 任务分组
            e.invokeTarget = res.str(i, 3);               ///< 调用目标
            e.cronExpr     = res.str(i, 4);               ///< Cron 表达式
            e.concurrent   = (res.str(i, 5) == "1");      ///< 并发模式（"1"=并发，其他=串行）
        }
    }

    /**
     * @brief 调度器主循环
     * 
     * 在后台线程中持续运行，每秒检查一次是否有任务需要执行。
     * 
     * 流程：
     *   1. 获取当前时间
     *   2. 遍历所有任务，检查 Cron 表达式是否匹配
     *   3. 对于匹配的任务：
     *      - 检查是否已在本分钟内执行过（去重）
     *      - 检查串行模式下是否已在执行（跳过）
     *      - 在后台线程中异步执行任务
     *   4. 等待到下一秒整，重复
     * 
     * @note 
     *   - 按秒级精度检查任务
     *   - 同一分钟内只触发一次（去重机制）
     *   - 串行模式下，上次执行未完成会跳过本次执行
     *   - 并发模式下，允许多个任务实例同时执行
     */
    void schedulerLoop() {
        // 主循环，直到收到停止信号
        while (!stopFlag_) {
            // 获取当前时间戳
            auto now = std::time(nullptr);
            
            // 转换为本地时间结构体
            struct tm t{};
#ifdef _WIN32
            localtime_s(&t, &now);  ///< Windows 平台
#else
            localtime_r(&now, &t);  ///< Unix/Linux 平台
#endif
            
            // 构建当前"年月日时分"指纹，用于按分钟去重
            // 格式：YYYYMMDDHHmm（例如：202606101230）
            long long curMin = (long long)(t.tm_year + 1900) * 100000000LL  ///< 年份
                             + (long long)(t.tm_mon  + 1) * 1000000LL       ///< 月份
                             + (long long)t.tm_mday * 10000LL               ///< 日期
                             + (long long)t.tm_hour * 100LL                 ///< 小时
                             + (long long)t.tm_min;                         ///< 分钟
            
            // 检查和执行任务
            {
                // 加锁保护 jobs_ 访问
                std::lock_guard<std::mutex> lk(mu_);
                
                // 遍历所有任务
                for (auto& [id, job] : jobs_) {
                    // 检查 Cron 表达式是否匹配当前时间
                    if (!CronUtils::matches(job.cronExpr, t)) {
                        continue;  ///< 不匹配，跳过此任务
                    }
                    
                    // 检查是否已在本分钟内执行过（去重机制）
                    if (job.lastFireMin == curMin) {
                        continue;  ///< 已执行过，跳过此任务
                    }
                    
                    // 检查串行模式下是否已在执行
                    if (!job.concurrent && job.running) {
                        continue;  ///< 串行模式且正在执行，跳过此任务
                    }
                    
                    // 更新上次触发时间
                    job.lastFireMin = curMin;
                    
                    // 保存任务信息（用于后台线程）
                    long jid = job.jobId;
                    std::string tgt = job.invokeTarget;
                    std::string nm  = job.jobName;
                    std::string grp = job.jobGroup;
                    
                    // 标记任务为运行中
                    job.running = true;
                    
                    // 在后台线程中异步执行任务
                    std::thread([this, jid, tgt, nm, grp]{
                        // 执行任务
                        executeJob(jid, tgt, nm, grp, "定时触发");
                        
                        // 执行完成后，标记任务为非运行中
                        std::lock_guard<std::mutex> lk2(mu_);
                        if (jobs_.count(jid)) {
                            jobs_[jid].running = false;
                        }
                    }).detach();  ///< 分离线程，不等待执行完成
                }
            }
            
            // 等待到下一秒整
            auto elapsed = (int)(std::time(nullptr) - now);
            if (elapsed < 1) {
                // 还没到下一秒，计算需要等待的毫秒数
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(1000 - elapsed * 1000 + 50));
            } else {
                // 已经过了一秒或多秒，短暂等待后重新检查
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    }

    /**
     * @brief 执行单个任务
     * 
     * 调用任务处理器，记录执行结果和异常信息。
     * 
     * 流程：
     *   1. 记录开始时间
     *   2. 调用 dispatch() 执行任务
     *   3. 捕获异常，记录错误信息
     *   4. 计算执行耗时
     *   5. 将执行结果写入数据库日志
     *   6. 记录日志信息
     * 
     * @param jobId 任务 ID
     * @param invokeTarget 调用目标（函数名和参数）
     * @param jobName 任务名称（用于日志）
     * @param jobGroup 任务分组（用于日志）
     * @param triggerType 触发类型（"定时触发" 或 "手动触发"）
     * 
     * @note 
     *   - 异常会被捕获，不会中断调度器
     *   - 执行结果会记录到 sys_job_log 表
     *   - 状态码：0=成功，1=失败
     */
    void executeJob(long jobId, const std::string& invokeTarget,
                    const std::string& jobName, const std::string& jobGroup,
                    const std::string& triggerType) {
        // 记录开始时间
        auto start = std::chrono::steady_clock::now();
        
        // 初始化结果和异常信息
        std::string result, exInfo;
        std::string status = "0";  ///< 0=成功，1=失败
        
        // 执行任务，捕获异常
        try {
            // 调用任务处理器
            result = dispatch(invokeTarget);
        } catch (const std::exception& ex) {
            // 捕获标准异常
            exInfo = ex.what();
            status = "1";
            LOG_ERROR << "[JobScheduler] Job " << jobName << " failed: " << ex.what();
        } catch (...) {
            // 捕获未知异常
            exInfo = "未知异常";
            status = "1";
        }
        
        // 计算执行耗时（毫秒）
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        
        // 构建日志消息
        std::string msg = triggerType + " | 耗时 " + std::to_string(ms) + "ms";
        if (!result.empty()) {
            msg += " | " + result;  ///< 添加执行结果
        }
        
        // 将执行结果写入数据库日志
        DatabaseService::instance().execParams(
            "INSERT INTO sys_job_log(job_name,job_group,invoke_target,job_message,status,exception_info,create_time) "
            "VALUES($1,$2,$3,$4,$5,$6,NOW())",
            {jobName, jobGroup, invokeTarget, msg, status, exInfo});
        
        // 记录日志信息
        LOG_INFO << "[JobScheduler] " << jobName << " [" << triggerType << "] "
                 << (status=="0"?"OK":"FAIL") << " " << ms << "ms";
    }

    /**
     * @brief 解析并执行任务处理器
     * 
     * 解析 invokeTarget 字符串，提取函数名和参数，调用对应的处理器。
     * 
     * invokeTarget 格式：
     *   - "funcName" - 无参任务
     *   - "funcName('param')" - 单参任务
     *   - "funcName('p1','p2')" - 多参任务
     * 
     * 流程：
     *   1. 查找左括号，分离函数名和参数部分
     *   2. 如果有参数，提取括号内的内容
     *   3. 去除参数两端的单引号
     *   4. 去除函数名两端的空白字符
     *   5. 从处理器映射表中查找并调用处理器
     *   6. 如果处理器不存在，返回提示信息
     * 
     * @param target 调用目标字符串
     * @return 处理器的返回结果，或提示信息
     * 
     * @note 
     *   - 参数必须用单引号括起
     *   - 多个参数用逗号分隔（作为单个字符串传递）
     *   - 函数名会自动去除两端空白
     *   - 如果处理器不存在，返回提示而不是异常
     */
    std::string dispatch(const std::string& target) {
        // 初始化函数名和参数
        std::string funcName = target;
        std::string params;
        
        // 查找左括号，分离函数名和参数
        auto lp = target.find('(');
        if (lp != std::string::npos) {
            // 提取函数名（括号前的部分）
            funcName = target.substr(0, lp);
            
            // 查找右括号，提取参数
            auto rp = target.rfind(')');
            if (rp != std::string::npos && rp > lp) {
                // 提取括号内的参数
                params = target.substr(lp + 1, rp - lp - 1);
            }
            
            // 去除参数两端的单引号
            if (!params.empty() && params.front() == '\'') {
                params = params.substr(1);  ///< 去除左引号
            }
            if (!params.empty() && params.back() == '\'') {
                params.pop_back();  ///< 去除右引号
            }
        }
        
        // 去除函数名两端的空白字符（trim）
        funcName.erase(0, funcName.find_first_not_of(" \t"));
        funcName.erase(funcName.find_last_not_of(" \t") + 1);
        
        // 从处理器映射表中查找并调用处理器
        std::lock_guard<std::mutex> lk(mu_);
        auto it = handlers_.find(funcName);
        if (it != handlers_.end()) {
            // 处理器存在，调用并返回结果
            return it->second(params);
        }
        
        // 处理器不存在，返回提示信息
        return "执行完成 [" + funcName + "] (未注册处理器)";
    }
};
