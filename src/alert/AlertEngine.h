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

