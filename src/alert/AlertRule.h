#pragma once

#include <string>
#include <vector>
#include <map>
#include <json/json.h>
#include <ctime>

/**
 * @file AlertRule.h
 * @brief 告警规则定义
 * 
 * 定义告警规则的数据结构和枚举类型
 */

// 告警规则类型
enum class AlertRuleType {
    THRESHOLD,      // 阈值告警
    ANOMALY,        // 异常告警
    TREND,          // 趋势告警
    COMPOSITE       // 组合告警
};

// 告警级别
enum class AlertSeverity {
    INFO,           // 信息
    WARNING,        // 警告
    ERROR,          // 错误
    CRITICAL        // 严重
};

// 比较操作符
enum class CompareOperator {
    EQUAL,          // ==
    NOT_EQUAL,      // !=
    GREATER,        // >
    GREATER_EQUAL,  // >=
    LESS,           // <
    LESS_EQUAL,     // <=
    CONTAINS,       // 包含
    NOT_CONTAINS    // 不包含
};

/**
 * @struct AlertRule
 * @brief 告警规则
 */
struct AlertRule {
    std::string id;                     // 规则 ID
    std::string name;                   // 规则名称
    std::string description;            // 规则描述
    AlertRuleType type;                 // 规则类型
    AlertSeverity severity;             // 告警级别
    
    // 阈值告警相关
    std::string metric;                 // 监控指标
    CompareOperator operator_;          // 比较操作符
    double threshold;                   // 阈值
    int duration;                       // 持续时间（秒）
    
    // 规则配置
    bool enabled;                       // 是否启用
    int maxAlerts;                      // 最大告警数
    int silenceDuration;                // 沉默时间（秒）
    
    // 通知配置
    std::vector<std::string> notifyChannels;  // 通知渠道（email, dingtalk, wecom）
    std::vector<std::string> notifyReceivers; // 通知接收人
    
    // 时间戳
    time_t createTime;                  // 创建时间
    time_t updateTime;                  // 更新时间
    
    // JSON 序列化
    Json::Value toJson() const {
        Json::Value j;
        j["id"] = id;
        j["name"] = name;
        j["description"] = description;
        j["type"] = static_cast<int>(type);
        j["severity"] = static_cast<int>(severity);
        j["metric"] = metric;
        j["operator"] = static_cast<int>(operator_);
        j["threshold"] = threshold;
        j["duration"] = duration;
        j["enabled"] = enabled;
        j["maxAlerts"] = maxAlerts;
        j["silenceDuration"] = silenceDuration;
        
        Json::Value channels(Json::arrayValue);
        for (const auto& ch : notifyChannels) channels.append(ch);
        j["notifyChannels"] = channels;
        
        Json::Value receivers(Json::arrayValue);
        for (const auto& r : notifyReceivers) receivers.append(r);
        j["notifyReceivers"] = receivers;
        
        j["createTime"] = static_cast<Json::Value::Int64>(createTime);
        j["updateTime"] = static_cast<Json::Value::Int64>(updateTime);
        return j;
    }
    
    static AlertRule fromJson(const Json::Value& j) {
        AlertRule rule;
        rule.id = j.get("id", "").asString();
        rule.name = j.get("name", "").asString();
        rule.description = j.get("description", "").asString();
        rule.type = static_cast<AlertRuleType>(j.get("type", 0).asInt());
        rule.severity = static_cast<AlertSeverity>(j.get("severity", 0).asInt());
        rule.metric = j.get("metric", "").asString();
        rule.operator_ = static_cast<CompareOperator>(j.get("operator", 0).asInt());
        rule.threshold = j.get("threshold", 0.0).asDouble();
        rule.duration = j.get("duration", 0).asInt();
        rule.enabled = j.get("enabled", true).asBool();
        rule.maxAlerts = j.get("maxAlerts", 10).asInt();
        rule.silenceDuration = j.get("silenceDuration", 300).asInt();
        
        if (j.isMember("notifyChannels") && j["notifyChannels"].isArray()) {
            for (const auto& ch : j["notifyChannels"]) {
                rule.notifyChannels.push_back(ch.asString());
            }
        }
        
        if (j.isMember("notifyReceivers") && j["notifyReceivers"].isArray()) {
            for (const auto& r : j["notifyReceivers"]) {
                rule.notifyReceivers.push_back(r.asString());
            }
        }
        
        rule.createTime = j.get("createTime", 0).asInt64();
        rule.updateTime = j.get("updateTime", 0).asInt64();
        return rule;
    }
};

/**
 * @struct Alert
 * @brief 告警事件
 */
struct Alert {
    std::string id;                     // 告警 ID
    std::string ruleId;                 // 规则 ID
    std::string ruleName;               // 规则名称
    AlertSeverity severity;             // 告警级别
    
    std::string metric;                 // 监控指标
    double value;                       // 当前值
    double threshold;                   // 阈值
    
    std::string message;                // 告警消息
    std::string status;                 // 状态（triggered, acknowledged, resolved）
    
    time_t triggerTime;                 // 触发时间
    time_t acknowledgeTime;             // 确认时间
    time_t resolveTime;                 // 解决时间
    
    std::string acknowledgedBy;         // 确认人
    std::string acknowledgeReason;      // 确认原因
    
    // JSON 序列化
    Json::Value toJson() const {
        Json::Value j;
        j["id"] = id;
        j["ruleId"] = ruleId;
        j["ruleName"] = ruleName;
        j["severity"] = static_cast<int>(severity);
        j["metric"] = metric;
        j["value"] = value;
        j["threshold"] = threshold;
        j["message"] = message;
        j["status"] = status;
        j["triggerTime"] = static_cast<Json::Value::Int64>(triggerTime);
        j["acknowledgeTime"] = static_cast<Json::Value::Int64>(acknowledgeTime);
        j["resolveTime"] = static_cast<Json::Value::Int64>(resolveTime);
        j["acknowledgedBy"] = acknowledgedBy;
        j["acknowledgeReason"] = acknowledgeReason;
        return j;
    }
};

