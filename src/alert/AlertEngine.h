/**
 * @file AlertEngine.h
 * @brief 告警引擎 — 实时监控和告警规则执行
 * 
 * 功能概述：
 *   - 规则引擎：支持复杂的告警规则定义和执行
 *   - 实时监控：持续监控系统指标和业务数据
 *   - 告警触发：当条件满足时自动触发告警
 *   - 告警聚合：支持告警去重、聚合、升级
 *   - 告警通知：支持多渠道通知（邮件、短信、钉钉等）
 *   - 告警恢复：自动检测告警恢复并发送恢复通知
 * 
 * 核心特性：
 *   - 灵活的规则语言：支持阈值、变化率、异常检测等多种规则类型
 *   - 动态规则加载：支持热更新告警规则，无需重启应用
 *   - 告警去重：相同告警在指定时间内只通知一次
 *   - 告警升级：支持告警级别升级（INFO → WARNING → CRITICAL）
 *   - 告警抑制：支持在特定时间段或条件下抑制告警
 *   - 告警历史：完整的告警历史记录和统计分析
 * 
 * 告警规则类型：
 *   - 阈值告警：指标 > 阈值时触发
 *   - 变化率告警：指标变化率超过阈值时触发
 *   - 异常检测：使用统计方法检测异常
 *   - 组合告警：多个条件的逻辑组合
 *   - 时间序列告警：基于历史数据的预测告警
 * 
 * 配置项（config.json）：
 *   - alert.enabled: 是否启用告警引擎（默认 true）
 *   - alert.check_interval_seconds: 检查间隔（秒，默认 60）
 *   - alert.dedup_window_minutes: 去重时间窗口（分钟，默认 5）
 *   - alert.max_history_size: 最大历史记录数（默认 10000）
 */

#pragma once

#include "AlertRule.h"
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <queue>
#include <functional>

/**
 * @file AlertEngine.h
 * @brief 告警规则引擎
 * 
 * 核心功能：
 *   - 告警规则管理（增删改查）
 *   - 指标监控和告警触发
 *   - 告警聚合和去重
 *   - 告警通知
 */

class AlertEngine {
public:
    static AlertEngine& instance() {
        static AlertEngine inst;
        return inst;
    }

    // 初始化
    void init(const Json::Value& config);
    
    // 启动和停止
    void start();
    void stop();
    
    // 规则管理
    bool addRule(const AlertRule& rule);
    bool updateRule(const AlertRule& rule);
    bool deleteRule(const std::string& ruleId);
    AlertRule* getRule(const std::string& ruleId);
    std::vector<AlertRule> getAllRules();
    
    // 指标报告
    void reportMetric(const std::string& metric, double value);
    void reportEvent(const std::string& eventType, const std::string& message);
    
    // 告警查询
    std::vector<Alert> getAlerts(const std::string& status = "");
    std::vector<Alert> getAlertsByRule(const std::string& ruleId);
    Alert* getAlert(const std::string& alertId);
    
    // 告警操作
    bool acknowledgeAlert(const std::string& alertId, const std::string& acknowledgedBy, const std::string& reason);
    bool resolveAlert(const std::string& alertId);
    
    // 统计信息
    struct Stats {
        int totalRules;
        int enabledRules;
        int totalAlerts;
        int triggeredAlerts;
        int acknowledgedAlerts;
        int resolvedAlerts;
    };
    Stats getStats();

private:
    AlertEngine() = default;
    ~AlertEngine();
    AlertEngine(const AlertEngine&) = delete;
    AlertEngine& operator=(const AlertEngine&) = delete;
    
    // 工作线程
    void workerThread();
    void checkMetrics();
    void processAlerts();
    
    // 辅助函数
    bool checkThreshold(const AlertRule& rule, double value);
    std::string generateAlertId();
    void notifyAlert(const Alert& alert);
    
    // 数据成员
    std::unordered_map<std::string, AlertRule> rules_;        // 规则列表
    std::unordered_map<std::string, Alert> alerts_;           // 告警列表
    std::unordered_map<std::string, double> metrics_;         // 指标值
    
    std::mutex rulesMutex_;
    std::mutex alertsMutex_;
    std::mutex metricsMutex_;
    
    std::thread workerThread_;
    std::atomic<bool> running_{false};
    
    // 配置
    int checkInterval_;                 // 检查间隔（毫秒）
    int maxAlerts_;                     // 最大告警数
    int alertRetention_;                // 告警保留时间（秒）
};

