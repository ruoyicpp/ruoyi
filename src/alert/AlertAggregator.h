#pragma once

#include "AlertRule.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <ctime>

/**
 * @file AlertAggregator.h
 * @brief 告警聚合和去重
 * 
 * 功能：
 *   - 告警聚合（相同告警合并）
 *   - 告警去重（避免重复告警）
 *   - 告警分组（按类型、源、级别分组）
 *   - 告警统计
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

