#include "AlertEngine.h"
#include <trantor/utils/Logger.h>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

void AlertEngine::init(const Json::Value& config) {
    checkInterval_ = config.get("checkInterval", 5000).asInt();
    maxAlerts_ = config.get("maxAlerts", 10000).asInt();
    alertRetention_ = config.get("alertRetention", 86400).asInt();
    
    LOG_INFO << "[AlertEngine] Initialized with checkInterval=" << checkInterval_ 
             << "ms, maxAlerts=" << maxAlerts_;
}

void AlertEngine::start() {
    if (running_.exchange(true)) {
        LOG_WARN << "[AlertEngine] Already running";
        return;
    }
    
    workerThread_ = std::thread([this]() { workerThread(); });
    LOG_INFO << "[AlertEngine] Started";
}

void AlertEngine::stop() {
    running_ = false;
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    LOG_INFO << "[AlertEngine] Stopped";
}

bool AlertEngine::addRule(const AlertRule& rule) {
    std::lock_guard<std::mutex> lock(rulesMutex_);
    
    if (rules_.find(rule.id) != rules_.end()) {
        LOG_WARN << "[AlertEngine] Rule already exists: " << rule.id;
        return false;
    }
    
    AlertRule r = rule;
    r.createTime = std::time(nullptr);
    r.updateTime = r.createTime;
    
    rules_[rule.id] = r;
    LOG_INFO << "[AlertEngine] Rule added: " << rule.id << " (" << rule.name << ")";
    return true;
}

bool AlertEngine::updateRule(const AlertRule& rule) {
    std::lock_guard<std::mutex> lock(rulesMutex_);
    
    auto it = rules_.find(rule.id);
    if (it == rules_.end()) {
        LOG_WARN << "[AlertEngine] Rule not found: " << rule.id;
        return false;
    }
    
    AlertRule r = rule;
    r.createTime = it->second.createTime;
    r.updateTime = std::time(nullptr);
    
    rules_[rule.id] = r;
    LOG_INFO << "[AlertEngine] Rule updated: " << rule.id;
    return true;
}

bool AlertEngine::deleteRule(const std::string& ruleId) {
    std::lock_guard<std::mutex> lock(rulesMutex_);
    
    auto it = rules_.find(ruleId);
    if (it == rules_.end()) {
        LOG_WARN << "[AlertEngine] Rule not found: " << ruleId;
        return false;
    }
    
    rules_.erase(it);
    LOG_INFO << "[AlertEngine] Rule deleted: " << ruleId;
    return true;
}

AlertRule* AlertEngine::getRule(const std::string& ruleId) {
    std::lock_guard<std::mutex> lock(rulesMutex_);
    
    auto it = rules_.find(ruleId);
    if (it != rules_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<AlertRule> AlertEngine::getAllRules() {
    std::lock_guard<std::mutex> lock(rulesMutex_);
    
    std::vector<AlertRule> result;
    for (const auto& kv : rules_) {
        result.push_back(kv.second);
    }
    return result;
}

void AlertEngine::reportMetric(const std::string& metric, double value) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    metrics_[metric] = value;
}

void AlertEngine::reportEvent(const std::string& eventType, const std::string& message) {
    // 可以扩展为处理事件型告警
    LOG_INFO << "[AlertEngine] Event: " << eventType << " - " << message;
}

std::vector<Alert> AlertEngine::getAlerts(const std::string& status) {
    std::lock_guard<std::mutex> lock(alertsMutex_);
    
    std::vector<Alert> result;
    for (const auto& kv : alerts_) {
        if (status.empty() || kv.second.status == status) {
            result.push_back(kv.second);
        }
    }
    return result;
}

std::vector<Alert> AlertEngine::getAlertsByRule(const std::string& ruleId) {
    std::lock_guard<std::mutex> lock(alertsMutex_);
    
    std::vector<Alert> result;
    for (const auto& kv : alerts_) {
        if (kv.second.ruleId == ruleId) {
            result.push_back(kv.second);
        }
    }
    return result;
}

Alert* AlertEngine::getAlert(const std::string& alertId) {
    std::lock_guard<std::mutex> lock(alertsMutex_);
    
    auto it = alerts_.find(alertId);
    if (it != alerts_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool AlertEngine::acknowledgeAlert(const std::string& alertId, const std::string& acknowledgedBy, const std::string& reason) {
    std::lock_guard<std::mutex> lock(alertsMutex_);
    
    auto it = alerts_.find(alertId);
    if (it == alerts_.end()) {
        return false;
    }
    
    it->second.status = "acknowledged";
    it->second.acknowledgeTime = std::time(nullptr);
    it->second.acknowledgedBy = acknowledgedBy;
    it->second.acknowledgeReason = reason;
    
    LOG_INFO << "[AlertEngine] Alert acknowledged: " << alertId << " by " << acknowledgedBy;
    return true;
}

bool AlertEngine::resolveAlert(const std::string& alertId) {
    std::lock_guard<std::mutex> lock(alertsMutex_);
    
    auto it = alerts_.find(alertId);
    if (it == alerts_.end()) {
        return false;
    }
    
    it->second.status = "resolved";
    it->second.resolveTime = std::time(nullptr);
    
    LOG_INFO << "[AlertEngine] Alert resolved: " << alertId;
    return true;
}

AlertEngine::Stats AlertEngine::getStats() {
    std::lock_guard<std::mutex> ruleLock(rulesMutex_);
    std::lock_guard<std::mutex> alertLock(alertsMutex_);
    
    Stats stats;
    stats.totalRules = rules_.size();
    stats.enabledRules = 0;
    stats.totalAlerts = alerts_.size();
    stats.triggeredAlerts = 0;
    stats.acknowledgedAlerts = 0;
    stats.resolvedAlerts = 0;
    
    for (const auto& kv : rules_) {
        if (kv.second.enabled) stats.enabledRules++;
    }
    
    for (const auto& kv : alerts_) {
        if (kv.second.status == "triggered") stats.triggeredAlerts++;
        else if (kv.second.status == "acknowledged") stats.acknowledgedAlerts++;
        else if (kv.second.status == "resolved") stats.resolvedAlerts++;
    }
    
    return stats;
}

AlertEngine::~AlertEngine() {
    stop();
}

void AlertEngine::workerThread() {
    while (running_) {
        try {
            checkMetrics();
            processAlerts();
            
            std::this_thread::sleep_for(std::chrono::milliseconds(checkInterval_));
        } catch (const std::exception& e) {
            LOG_ERROR << "[AlertEngine] Worker thread error: " << e.what();
        }
    }
}

void AlertEngine::checkMetrics() {
    std::lock_guard<std::mutex> ruleLock(rulesMutex_);
    std::lock_guard<std::mutex> metricLock(metricsMutex_);
    
    for (const auto& rule : rules_) {
        if (!rule.second.enabled) continue;
        
        auto it = metrics_.find(rule.second.metric);
        if (it == metrics_.end()) continue;
        
        double value = it->second;
        
        if (checkThreshold(rule.second, value)) {
            // 触发告警
            Alert alert;
            alert.id = generateAlertId();
            alert.ruleId = rule.second.id;
            alert.ruleName = rule.second.name;
            alert.severity = rule.second.severity;
            alert.metric = rule.second.metric;
            alert.value = value;
            alert.threshold = rule.second.threshold;
            alert.message = rule.second.name + ": " + std::to_string(value);
            alert.status = "triggered";
            alert.triggerTime = std::time(nullptr);
            
            {
                std::lock_guard<std::mutex> alertLock(alertsMutex_);
                alerts_[alert.id] = alert;
                
                // 清理过期告警
                if (alerts_.size() > maxAlerts_) {
                    time_t now = std::time(nullptr);
                    for (auto it = alerts_.begin(); it != alerts_.end(); ) {
                        if (now - it->second.triggerTime > alertRetention_) {
                            it = alerts_.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            }
            
            LOG_WARN << "[AlertEngine] Alert triggered: " << alert.id << " (" << rule.second.name << ")";
        }
    }
}

void AlertEngine::processAlerts() {
    // 可以在这里处理告警，如聚合、去重、通知等
}

bool AlertEngine::checkThreshold(const AlertRule& rule, double value) {
    switch (rule.operator_) {
        case CompareOperator::GREATER:
            return value > rule.threshold;
        case CompareOperator::GREATER_EQUAL:
            return value >= rule.threshold;
        case CompareOperator::LESS:
            return value < rule.threshold;
        case CompareOperator::LESS_EQUAL:
            return value <= rule.threshold;
        case CompareOperator::EQUAL:
            return value == rule.threshold;
        case CompareOperator::NOT_EQUAL:
            return value != rule.threshold;
        default:
            return false;
    }
}

std::string AlertEngine::generateAlertId() {
    static int counter = 0;
    std::ostringstream oss;
    oss << "alert_" << std::time(nullptr) << "_" << (counter++);
    return oss.str();
}

void AlertEngine::notifyAlert(const Alert& alert) {
    // 通知逻辑，可以集成 AlertNotifier
    LOG_INFO << "[AlertEngine] Notifying alert: " << alert.id;
}

