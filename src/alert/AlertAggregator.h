#pragma once

#include "AlertRule.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <ctime>

/**
 * @file AlertAggregator.h
 * @brief 告警聚合和去重 — 合并相同告警，避免告警风暴
 * 
 * 功能概述：
 *   - 告警聚合：合并相同或相似的告警
 *   - 告警去重：避免重复告警
 *   - 告警分组：按类型、源、级别分组
 *   - 告警统计：统计告警数量和分布
 *   - 告警窗口：时间窗口内的告警聚合
 *   - 告警关联：关联相关的告警
 * 
 * 核心特性：
 *   - 智能聚合：根据规则聚合相同告警
 *   - 时间窗口：支持时间窗口聚合
 *   - 阈值控制：支持聚合阈值配置
 *   - 去重机制：避免重复告警
 *   - 告警分组：多维度分组统计
 *   - 实时更新：实时更新聚合状态
 * 
 * 聚合策略：
 *   - 完全匹配：相同规则、源、级别的告警合并
 *   - 模糊匹配：相似的告警合并
 *   - 时间窗口：时间窗口内的告警合并
 *   - 阈值聚合：达到阈值后聚合
 * 
 * 使用示例：
 *   ```cpp
 *   AlertAggregator& aggregator = AlertAggregator::instance();
 *   
 *   // 添加告警
 *   Alert alert;
 *   alert.ruleId = "rule_1";
 *   alert.severity = AlertSeverity::ERROR;
 *   aggregator.addAlert(alert);
 *   
 *   // 获取聚合告警
 *   auto aggregated = aggregator.getAggregatedAlerts();
 *   for (const auto& alert : aggregated) {
 *       std::cout << "Alert: " << alert.ruleName << std::endl;
 *   }
 *   ```
 * 
 * 配置项（config.json）：
 *   - alert.aggregation.enabled: 是否启用聚合（默认 true）
 *   - alert.aggregation.window: 聚合时间窗口（秒，默认 60）
 *   - alert.aggregation.threshold: 聚合阈值（默认 5）
 *   - alert.aggregation.dedup_enabled: 是否启用去重（默认 true）
 * 
 * @see AlertEngine - 告警引擎
 * @see AlertRule - 告警规则
 * @see AlertNotifier - 告警通知器
 */

/**
 * @struct AggregatedAlert
 * @brief 聚合后的告警
 */
struct AggregatedAlert {
    std::string id;                     // 聚合告警 ID
    std::string ruleId;                 // 规则 ID
    std::string ruleName;               // 规则名称
    AlertSeverity severity;             // 告警级别
    
    int count;                          // 告警次数
    std::string lastMessage;            // 最后一条消息
    
    time_t firstTriggerTime;            // 首次触发时间
    time_t lastTriggerTime;             // 最后触发时间
    
    std::vector<std::string> alertIds;  // 包含的告警 ID 列表
    
    // JSON 序列化
    Json::Value toJson() const {
        Json::Value j;
        j["id"] = id;
        j["ruleId"] = ruleId;
        j["ruleName"] = ruleName;
        j["severity"] = static_cast<int>(severity);
        j["count"] = count;
        j["lastMessage"] = lastMessage;
        j["firstTriggerTime"] = static_cast<Json::Value::Int64>(firstTriggerTime);
        j["lastTriggerTime"] = static_cast<Json::Value::Int64>(lastTriggerTime);
        
        Json::Value ids(Json::arrayValue);
        for (const auto& aid : alertIds) ids.append(aid);
        j["alertIds"] = ids;
        
        return j;
    }
};

class AlertAggregator {
public:
    static AlertAggregator& instance() {
        static AlertAggregator inst;
        return inst;
    }
    
    // 初始化
    void init(int windowSize = 300);  // 聚合窗口大小（秒）
    
    // 添加告警
    void addAlert(const Alert& alert);
    
    // 获取聚合告警
    std::vector<AggregatedAlert> getAggregatedAlerts();
    AggregatedAlert* getAggregatedAlert(const std::string& aggregatedId);
    
    // 去重检查
    bool isDuplicate(const Alert& alert);
    
    // 清理过期告警
    void cleanup();
    
    // 统计信息
    struct Stats {
        int totalAlerts;
        int aggregatedAlerts;
        int duplicateAlerts;
        int averageAlertsPerGroup;
    };
    Stats getStats();

private:
    AlertAggregator() = default;
    ~AlertAggregator() = default;
    AlertAggregator(const AlertAggregator&) = delete;
    AlertAggregator& operator=(const AlertAggregator&) = delete;
    
    // 生成聚合键
    std::string generateAggregateKey(const Alert& alert);
    
    // 数据成员
    std::unordered_map<std::string, AggregatedAlert> aggregatedAlerts_;
    std::unordered_map<std::string, time_t> lastAlertTime_;  // 用于去重
    
    std::mutex mutex_;
    
    int windowSize_;                    // 聚合窗口大小（秒）
    int deduplicationWindow_;           // 去重窗口大小（秒）
};

