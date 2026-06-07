#include "AlertAggregator.h"
#include <trantor/utils/Logger.h>
#include <sstream>
#include <iomanip>

void AlertAggregator::init(int windowSize) {
    windowSize_ = windowSize;
    deduplicationWindow_ = windowSize / 2;  // 去重窗口为聚合窗口的一半
    LOG_INFO << "[AlertAggregator] Initialized with windowSize=" << windowSize << "s";
}

void AlertAggregator::addAlert(const Alert& alert) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查是否重复
    std::string dedupeKey = alert.ruleId + "_" + alert.metric;
    auto it = lastAlertTime_.find(dedupeKey);
    
    time_t now = std::time(nullptr);
    if (it != lastAlertTime_.end() && (now - it->second) < deduplicationWindow_) {
        LOG_DEBUG << "[AlertAggregator] Duplicate alert ignored: " << dedupeKey;
        return;
    }
    
    lastAlertTime_[dedupeKey] = now;
    
    // 生成聚合键
    std::string aggKey = generateAggregateKey(alert);
    
    // 查找或创建聚合告警
    auto aggIt = aggregatedAlerts_.find(aggKey);
    if (aggIt != aggregatedAlerts_.end()) {
        // 更新现有聚合告警
        aggIt->second.count++;
        aggIt->second.lastMessage = alert.message;
        aggIt->second.lastTriggerTime = alert.triggerTime;
        aggIt->second.alertIds.push_back(alert.id);
        
        LOG_DEBUG << "[AlertAggregator] Alert aggregated: " << alert.id << " -> " << aggKey;
    } else {
        // 创建新的聚合告警
        AggregatedAlert agg;
        agg.id = aggKey;
        agg.ruleId = alert.ruleId;
        agg.ruleName = alert.ruleName;
        agg.severity = alert.severity;
        agg.count = 1;
        agg.lastMessage = alert.message;
        agg.firstTriggerTime = alert.triggerTime;
        agg.lastTriggerTime = alert.triggerTime;
        agg.alertIds.push_back(alert.id);
        
        aggregatedAlerts_[aggKey] = agg;
        
        LOG_INFO << "[AlertAggregator] New aggregated alert: " << aggKey;
    }
}

std::vector<AggregatedAlert> AlertAggregator::getAggregatedAlerts() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<AggregatedAlert> result;
    for (const auto& kv : aggregatedAlerts_) {
        result.push_back(kv.second);
    }
    return result;
}

AggregatedAlert* AlertAggregator::getAggregatedAlert(const std::string& aggregatedId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = aggregatedAlerts_.find(aggregatedId);
    if (it != aggregatedAlerts_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool AlertAggregator::isDuplicate(const Alert& alert) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string dedupeKey = alert.ruleId + "_" + alert.metric;
    auto it = lastAlertTime_.find(dedupeKey);
    
    if (it != lastAlertTime_.end()) {
        time_t now = std::time(nullptr);
        if ((now - it->second) < deduplicationWindow_) {
            return true;
        }
    }
    
    return false;
}

void AlertAggregator::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    time_t now = std::time(nullptr);
    
    // 清理过期的聚合告警
    for (auto it = aggregatedAlerts_.begin(); it != aggregatedAlerts_.end(); ) {
        if ((now - it->second.lastTriggerTime) > windowSize_) {
            LOG_DEBUG << "[AlertAggregator] Cleaning up aggregated alert: " << it->first;
            it = aggregatedAlerts_.erase(it);
        } else {
            ++it;
        }
    }
    
    // 清理过期的去重记录
    for (auto it = lastAlertTime_.begin(); it != lastAlertTime_.end(); ) {
        if ((now - it->second) > deduplicationWindow_) {
            it = lastAlertTime_.erase(it);
        } else {
            ++it;
        }
    }
}

AlertAggregator::Stats AlertAggregator::getStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Stats stats;
    stats.totalAlerts = 0;
    stats.aggregatedAlerts = aggregatedAlerts_.size();
    stats.duplicateAlerts = 0;
    stats.averageAlertsPerGroup = 0;
    
    for (const auto& kv : aggregatedAlerts_) {
        stats.totalAlerts += kv.second.count;
        stats.duplicateAlerts += (kv.second.count - 1);
    }
    
    if (!aggregatedAlerts_.empty()) {
        stats.averageAlertsPerGroup = stats.totalAlerts / aggregatedAlerts_.size();
    }
    
    return stats;
}

std::string AlertAggregator::generateAggregateKey(const Alert& alert) {
    std::ostringstream oss;
    oss << alert.ruleId << "_" << alert.metric << "_" << static_cast<int>(alert.severity);
    return oss.str();
}

