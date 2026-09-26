#include "AlertEngine.h"
// 引入告警引擎头文件，包含类声明和依赖

#include <trantor/utils/Logger.h>
// 引入 trantor 日志库，用于 LOG_INFO / LOG_WARN / LOG_ERROR 等宏

#include <chrono>
// 引入 chrono 时间库，用于 std::this_thread::sleep_for 等时间操作

#include <ctime>
// 引入 ctime，用于 std::time_t 和 std::time(nullptr) 获取当前时间戳

#include <sstream>
// 引入字符串流，用于 generateAlertId 中拼接字符串

#include <iomanip>
// 引入输入输出格式化（当前代码中未直接使用，保留以备扩展）

// ==================== 初始化 ====================

void AlertEngine::init(const Json::Value& config) {
    // 从配置中读取检查间隔（毫秒），默认 5000ms
    checkInterval_ = config.get("checkInterval", 5000).asInt();
    // 从配置中读取最大告警数量，默认 10000 条
    maxAlerts_ = config.get("maxAlerts", 10000).asInt();
    // 从配置中读取告警保留时间（秒），默认 86400 秒（1天）
    alertRetention_ = config.get("alertRetention", 86400).asInt();
    
    // 记录初始化日志，输出检查间隔和最大告警数
    LOG_INFO << "[AlertEngine] Initialized with checkInterval=" << checkInterval_ 
             << "ms, maxAlerts=" << maxAlerts_;
}

// ==================== 启动和停止 ====================

void AlertEngine::start() {
    // 原子地尝试将 running_ 设为 true，如果之前已经是 true 说明已在运行
    if (running_.exchange(true)) {
        // 已在运行，打警告日志并返回
        LOG_WARN << "[AlertEngine] Already running";
        return;
    }
    
    // 创建后台工作线程，线程执行 workerThread() 方法
    workerThread_ = std::thread([this]() { workerThread(); });
    // 记录启动日志
    LOG_INFO << "[AlertEngine] Started";
}

void AlertEngine::stop() {
    // 将 running_ 设为 false，通知工作线程退出循环
    running_ = false;
    // 等待工作线程结束（如果线程可 join）
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    // 记录停止日志
    LOG_INFO << "[AlertEngine] Stopped";
}

// ==================== 规则管理 ====================

bool AlertEngine::addRule(const AlertRule& rule) {
    // 加锁，保护 rules_ 的并发访问
    std::lock_guard<std::mutex> lock(rulesMutex_);
    
    // 检查规则 ID 是否已存在
    if (rules_.find(rule.id) != rules_.end()) {
        // 规则已存在，打警告日志并返回 false
        LOG_WARN << "[AlertEngine] Rule already exists: " << rule.id;
        return false;
    }
    
    // 拷贝规则对象
    AlertRule r = rule;
    // 设置创建时间为当前时间戳
    r.createTime = std::time(nullptr);
    // 设置更新时间为当前时间戳
    r.updateTime = r.createTime;
    
    // 将规则插入 rules_ 映射表
    rules_[rule.id] = r;
    // 记录添加成功日志
    LOG_INFO << "[AlertEngine] Rule added: " << rule.id << " (" << rule.name << ")";
    return true;
}

bool AlertEngine::updateRule(const AlertRule& rule) {
    // 加锁，保护 rules_ 的并发访问
    std::lock_guard<std::mutex> lock(rulesMutex_);
    
    // 查找要更新的规则
    auto it = rules_.find(rule.id);
    if (it == rules_.end()) {
        // 规则不存在，打警告日志并返回 false
        LOG_WARN << "[AlertEngine] Rule not found: " << rule.id;
        return false;
    }
    
    // 拷贝规则对象
    AlertRule r = rule;
    // 保留原有的创建时间
    r.createTime = it->second.createTime;
    // 设置更新时间为当前时间戳
    r.updateTime = std::time(nullptr);
    
    // 更新 rules_ 中的规则
    rules_[rule.id] = r;
    // 记录更新成功日志
    LOG_INFO << "[AlertEngine] Rule updated: " << rule.id;
    return true;
}

bool AlertEngine::deleteRule(const std::string& ruleId) {
    // 加锁，保护 rules_ 的并发访问
    std::lock_guard<std::mutex> lock(rulesMutex_);
    
    // 查找要删除的规则
    auto it = rules_.find(ruleId);
    if (it == rules_.end()) {
        // 规则不存在，打警告日志并返回 false
        LOG_WARN << "[AlertEngine] Rule not found: " << ruleId;
        return false;
    }
    
    // 从 rules_ 中擦除该规则
    rules_.erase(it);
    // 记录删除成功日志
    LOG_INFO << "[AlertEngine] Rule deleted: " << ruleId;
    return true;
}

AlertRule* AlertEngine::getRule(const std::string& ruleId) {
    // 加锁，保护 rules_ 的并发访问
    std::lock_guard<std::mutex> lock(rulesMutex_);
    
    // 查找规则
    auto it = rules_.find(ruleId);
    if (it != rules_.end()) {
        // 找到，返回指向规则对象的指针
        return &it->second;
    }
    // 未找到，返回 nullptr
    return nullptr;
}

std::vector<AlertRule> AlertEngine::getAllRules() {
    // 加锁，保护 rules_ 的并发访问
    std::lock_guard<std::mutex> lock(rulesMutex_);
    
    // 创建结果向量
    std::vector<AlertRule> result;
    // 遍历所有规则，逐个拷贝到结果向量中
    for (const auto& kv : rules_) {
        result.push_back(kv.second);
    }
    // 返回结果（拷贝）
    return result;
}

// ==================== 指标报告 ====================

void AlertEngine::reportMetric(const std::string& metric, double value) {
    // 加锁，保护 metrics_ 的并发访问
    std::lock_guard<std::mutex> lock(metricsMutex_);
    // 将指标值存入 metrics_ 映射表（如果已存在则覆盖）
    metrics_[metric] = value;
}

void AlertEngine::reportEvent(const std::string& eventType, const std::string& message) {
    // 事件型告警的扩展入口，当前仅记录日志
    LOG_INFO << "[AlertEngine] Event: " << eventType << " - " << message;
}

// ==================== 告警查询 ====================

std::vector<Alert> AlertEngine::getAlerts(const std::string& status) {
    // 加锁，保护 alerts_ 的并发访问
    std::lock_guard<std::mutex> lock(alertsMutex_);
    
    // 创建结果向量
    std::vector<Alert> result;
    // 遍历所有告警
    for (const auto& kv : alerts_) {
        // 如果 status 为空（不过滤）或告警状态匹配，则加入结果
        if (status.empty() || kv.second.status == status) {
            result.push_back(kv.second);
        }
    }
    // 返回结果（拷贝）
    return result;
}

std::vector<Alert> AlertEngine::getAlertsByRule(const std::string& ruleId) {
    // 加锁，保护 alerts_ 的并发访问
    std::lock_guard<std::mutex> lock(alertsMutex_);
    
    // 创建结果向量
    std::vector<Alert> result;
    // 遍历所有告警
    for (const auto& kv : alerts_) {
        // 如果告警的 ruleId 匹配，则加入结果
        if (kv.second.ruleId == ruleId) {
            result.push_back(kv.second);
        }
    }
    // 返回结果（拷贝）
    return result;
}

Alert* AlertEngine::getAlert(const std::string& alertId) {
    // 加锁，保护 alerts_ 的并发访问
    std::lock_guard<std::mutex> lock(alertsMutex_);
    
    // 查找告警
    auto it = alerts_.find(alertId);
    if (it != alerts_.end()) {
        // 找到，返回指向告警对象的指针
        return &it->second;
    }
    // 未找到，返回 nullptr
    return nullptr;
}

// ==================== 告警操作 ====================

bool AlertEngine::acknowledgeAlert(const std::string& alertId, const std::string& acknowledgedBy, const std::string& reason) {
    // 加锁，保护 alerts_ 的并发访问
    std::lock_guard<std::mutex> lock(alertsMutex_);
    
    // 查找告警
    auto it = alerts_.find(alertId);
    if (it == alerts_.end()) {
        // 告警不存在，返回 false
        return false;
    }
    
    // 更新告警状态为"已确认"
    it->second.status = "acknowledged";
    // 记录确认时间
    it->second.acknowledgeTime = std::time(nullptr);
    // 记录确认人
    it->second.acknowledgedBy = acknowledgedBy;
    // 记录确认原因
    it->second.acknowledgeReason = reason;
    
    // 记录确认日志
    LOG_INFO << "[AlertEngine] Alert acknowledged: " << alertId << " by " << acknowledgedBy;
    return true;
}

bool AlertEngine::resolveAlert(const std::string& alertId) {
    // 加锁，保护 alerts_ 的并发访问
    std::lock_guard<std::mutex> lock(alertsMutex_);
    
    // 查找告警
    auto it = alerts_.find(alertId);
    if (it == alerts_.end()) {
        // 告警不存在，返回 false
        return false;
    }
    
    // 更新告警状态为"已解决"
    it->second.status = "resolved";
    // 记录解决时间
    it->second.resolveTime = std::time(nullptr);
    
    // 记录解决日志
    LOG_INFO << "[AlertEngine] Alert resolved: " << alertId;
    return true;
}

// ==================== 统计信息 ====================

AlertEngine::Stats AlertEngine::getStats() {
    // 加锁，保护 rules_ 的并发访问
    std::lock_guard<std::mutex> ruleLock(rulesMutex_);
    // 加锁，保护 alerts_ 的并发访问
    std::lock_guard<std::mutex> alertLock(alertsMutex_);
    
    // 创建统计结构体并清零
    Stats stats;
    stats.totalRules = rules_.size();
    stats.enabledRules = 0;
    stats.totalAlerts = alerts_.size();
    stats.triggeredAlerts = 0;
    stats.acknowledgedAlerts = 0;
    stats.resolvedAlerts = 0;
    
    // 统计启用的规则数
    for (const auto& kv : rules_) {
        if (kv.second.enabled) stats.enabledRules++;
    }
    
    // 统计各状态的告警数
    for (const auto& kv : alerts_) {
        if (kv.second.status == "triggered") stats.triggeredAlerts++;
        else if (kv.second.status == "acknowledged") stats.acknowledgedAlerts++;
        else if (kv.second.status == "resolved") stats.resolvedAlerts++;
    }
    
    // 返回统计结果（拷贝）
    return stats;
}

// ==================== 析构函数 ====================

AlertEngine::~AlertEngine() {
    // 析构时调用 stop() 确保线程正确退出
    stop();
}

// ==================== 工作线程 ====================

void AlertEngine::workerThread() {
    // 循环，直到 running_ 被设为 false
    while (running_) {
        try {
            // 检查所有指标是否触发告警
            checkMetrics();
            // 处理告警（聚合、去重、通知等）
            processAlerts();
            
            // 休眠指定间隔（毫秒）
            std::this_thread::sleep_for(std::chrono::milliseconds(checkInterval_));
        } catch (const std::exception& e) {
            // 捕获异常，记录错误日志，防止线程崩溃
            LOG_ERROR << "[AlertEngine] Worker thread error: " << e.what();
        }
    }
}

// ==================== 指标检查 ====================

void AlertEngine::checkMetrics() {
    // 加锁，保护 rules_ 的并发访问
    std::lock_guard<std::mutex> ruleLock(rulesMutex_);
    // 加锁，保护 metrics_ 的并发访问
    std::lock_guard<std::mutex> metricLock(metricsMutex_);
    
    // 遍历所有规则
    for (const auto& rule : rules_) {
        // 跳过禁用的规则
        if (!rule.second.enabled) continue;
        
        // 在 metrics_ 中查找该规则对应的指标
        auto it = metrics_.find(rule.second.metric);
        if (it == metrics_.end()) continue;
        
        // 获取当前指标值
        double value = it->second;
        
        // 检查是否超过阈值
        if (checkThreshold(rule.second, value)) {
            // 超过阈值，创建告警对象
            Alert alert;
            // 生成唯一告警 ID
            alert.id = generateAlertId();
            // 设置关联的规则 ID
            alert.ruleId = rule.second.id;
            // 设置规则名称
            alert.ruleName = rule.second.name;
            // 设置告警级别
            alert.severity = rule.second.severity;
            // 设置指标名称
            alert.metric = rule.second.metric;
            // 设置当前指标值
            alert.value = value;
            // 设置阈值
            alert.threshold = rule.second.threshold;
            // 拼接告警消息
            alert.message = rule.second.name + ": " + std::to_string(value);
            // 设置状态为"已触发"
            alert.status = "triggered";
            // 记录触发时间
            alert.triggerTime = std::time(nullptr);
            
            {
                // 加锁，保护 alerts_ 的并发访问
                std::lock_guard<std::mutex> alertLock(alertsMutex_);
                // 将告警存入 alerts_ 映射表
                alerts_[alert.id] = alert;
                
                // 如果告警数量超过上限，清理过期告警
                if (alerts_.size() > maxAlerts_) {
                    time_t now = std::time(nullptr);
                    for (auto it = alerts_.begin(); it != alerts_.end(); ) {
                        // 如果告警超过保留时间，删除
                        if (now - it->second.triggerTime > alertRetention_) {
                            it = alerts_.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            }
            
            // 记录告警触发日志
            LOG_WARN << "[AlertEngine] Alert triggered: " << alert.id << " (" << rule.second.name << ")";
        }
    }
}

// ==================== 告警处理 ====================

void AlertEngine::processAlerts() {
    // 可以在这里处理告警，如聚合、去重、通知等
    // 当前为空实现，预留扩展
}

// ==================== 阈值检查 ====================

bool AlertEngine::checkThreshold(const AlertRule& rule, double value) {
    // 根据比较运算符进行阈值判断
    switch (rule.operator_) {
        case CompareOperator::GREATER:
            // 大于
            return value > rule.threshold;
        case CompareOperator::GREATER_EQUAL:
            // 大于等于
            return value >= rule.threshold;
        case CompareOperator::LESS:
            // 小于
            return value < rule.threshold;
        case CompareOperator::LESS_EQUAL:
            // 小于等于
            return value <= rule.threshold;
        case CompareOperator::EQUAL:
            // 等于
            return value == rule.threshold;
        case CompareOperator::NOT_EQUAL:
            // 不等于
            return value != rule.threshold;
        default:
            // 未知运算符，返回 false
            return false;
    }
}

// ==================== 辅助函数 ====================

std::string AlertEngine::generateAlertId() {
    // 静态计数器，用于生成唯一 ID
    static int counter = 0;
    // 创建字符串流
    std::ostringstream oss;
    // 拼接 "alert_" + 当前时间戳 + "_" + 递增计数器
    oss << "alert_" << std::time(nullptr) << "_" << (counter++);
    // 返回生成的字符串
    return oss.str();
}

void AlertEngine::notifyAlert(const Alert& alert) {
    // 通知逻辑，可以集成 AlertNotifier
    // 当前仅记录日志
    LOG_INFO << "[AlertEngine] Notifying alert: " << alert.id;
}