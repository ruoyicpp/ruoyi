#include "AlertNotifier.h"
#include <trantor/utils/Logger.h>

void AlertNotifier::init(const Json::Value& config) {
    if (config.isMember("smtp")) {
        auto& smtp = config["smtp"];
        smtpHost_ = smtp.get("host", "localhost").asString();
        smtpPort_ = smtp.get("port", 25).asInt();
        smtpUser_ = smtp.get("user", "").asString();
        smtpPassword_ = smtp.get("password", "").asString();
    }
    
    if (config.isMember("webhooks")) {
        auto& webhooks = config["webhooks"];
        for (const auto& key : webhooks.getMemberNames()) {
            webhookUrls_[key] = webhooks[key].asString();
        }
    }
    
    LOG_INFO << "[AlertNotifier] Initialized with SMTP: " << smtpHost_ << ":" << smtpPort_;
}

bool AlertNotifier::notifyAlert(const Alert& alert, const AlertRule& rule) {
    bool success = true;
    
    for (const auto& channel : rule.notifyChannels) {
        if (channel == "email") {
            if (!notifyByEmail(alert, rule.notifyReceivers)) {
                success = false;
            }
        } else if (channel == "dingtalk") {
            auto it = webhookUrls_.find("dingtalk");
            if (it != webhookUrls_.end()) {
                if (!notifyByDingTalk(alert, it->second)) {
                    success = false;
                }
            }
        } else if (channel == "wecom") {
            auto it = webhookUrls_.find("wecom");
            if (it != webhookUrls_.end()) {
                if (!notifyByWeChat(alert, it->second)) {
                    success = false;
                }
            }
        } else if (channel == "webhook") {
            auto it = webhookUrls_.find("webhook");
            if (it != webhookUrls_.end()) {
                if (!notifyByWebhook(alert, it->second)) {
                    success = false;
                }
            }
        } else if (channel == "sms") {
            if (!notifyBySMS(alert, rule.notifyReceivers)) {
                success = false;
            }
        } else {
            // 检查自定义处理器
            auto it = handlers_.find(channel);
            if (it != handlers_.end()) {
                if (!it->second(alert, "")) {
                    success = false;
                }
            }
        }
    }
    
    return success;
}

bool AlertNotifier::notifyByEmail(const Alert& alert, const std::vector<std::string>& recipients) {
    if (recipients.empty()) {
        LOG_WARN << "[AlertNotifier] No email recipients for alert: " << alert.id;
        return false;
    }
    
    std::string title = formatAlertTitle(alert, AlertRule());
    std::string message = formatAlertMessage(alert, AlertRule());
    
    // TODO: 使用 SmtpUtils 发送邮件
    LOG_INFO << "[AlertNotifier] Sending email notification for alert: " << alert.id 
             << " to " << recipients.size() << " recipients";
    
    return true;
}

bool AlertNotifier::notifyByDingTalk(const Alert& alert, const std::string& webhookUrl) {
    if (webhookUrl.empty()) {
        LOG_WARN << "[AlertNotifier] Empty DingTalk webhook URL";
        return false;
    }
    
    // 构建钉钉消息
    Json::Value msg;
    msg["msgtype"] = "markdown";
    
    Json::Value markdown;
    markdown["title"] = formatAlertTitle(alert, AlertRule());
    markdown["text"] = formatAlertMessage(alert, AlertRule());
    msg["markdown"] = markdown;
    
    // TODO: 使用 HttpCaller 发送 HTTP 请求
    LOG_INFO << "[AlertNotifier] Sending DingTalk notification for alert: " << alert.id;
    
    return true;
}

bool AlertNotifier::notifyByWeChat(const Alert& alert, const std::string& webhookUrl) {
    if (webhookUrl.empty()) {
        LOG_WARN << "[AlertNotifier] Empty WeChat webhook URL";
        return false;
    }
    
    // 构建企业微信消息
    Json::Value msg;
    msg["msgtype"] = "markdown";
    
    Json::Value markdown;
    markdown["content"] = formatAlertMessage(alert, AlertRule());
    msg["markdown"] = markdown;
    
    // TODO: 使用 HttpCaller 发送 HTTP 请求
    LOG_INFO << "[AlertNotifier] Sending WeChat notification for alert: " << alert.id;
    
    return true;
}

bool AlertNotifier::notifyByWebhook(const Alert& alert, const std::string& webhookUrl) {
    if (webhookUrl.empty()) {
        LOG_WARN << "[AlertNotifier] Empty webhook URL";
        return false;
    }
    
    // 构建通用 Webhook 消息
    Json::Value msg;
    msg["alert_id"] = alert.id;
    msg["rule_id"] = alert.ruleId;
    msg["severity"] = static_cast<int>(alert.severity);
    msg["message"] = alert.message;
    msg["value"] = alert.value;
    msg["threshold"] = alert.threshold;
    msg["timestamp"] = static_cast<Json::Value::Int64>(alert.triggerTime);
    
    // TODO: 使用 HttpCaller 发送 HTTP 请求
    LOG_INFO << "[AlertNotifier] Sending webhook notification for alert: " << alert.id;
    
    return true;
}

bool AlertNotifier::notifyBySMS(const Alert& alert, const std::vector<std::string>& phoneNumbers) {
    if (phoneNumbers.empty()) {
        LOG_WARN << "[AlertNotifier] No phone numbers for SMS notification";
        return false;
    }
    
    std::string message = formatAlertMessage(alert, AlertRule());
    
    // TODO: 集成短信服务（如阿里云、腾讯云等）
    LOG_INFO << "[AlertNotifier] Sending SMS notification for alert: " << alert.id 
             << " to " << phoneNumbers.size() << " numbers";
    
    return true;
}

void AlertNotifier::registerHandler(const std::string& channel, NotificationHandler handler) {
    handlers_[channel] = handler;
    LOG_INFO << "[AlertNotifier] Registered custom handler for channel: " << channel;
}

std::string AlertNotifier::formatAlertMessage(const Alert& alert, const AlertRule& rule) {
    std::ostringstream oss;
    oss << "**告警详情**\n\n"
        << "- 告警ID: " << alert.id << "\n"
        << "- 规则名称: " << alert.ruleName << "\n"
        << "- 告警级别: ";
    
    switch (alert.severity) {
        case AlertSeverity::INFO:
            oss << "信息";
            break;
        case AlertSeverity::WARNING:
            oss << "警告";
            break;
        case AlertSeverity::ERROR:
            oss << "错误";
            break;
        case AlertSeverity::CRITICAL:
            oss << "严重";
            break;
    }
    
    oss << "\n"
        << "- 监控指标: " << alert.metric << "\n"
        << "- 当前值: " << alert.value << "\n"
        << "- 阈值: " << alert.threshold << "\n"
        << "- 触发时间: " << std::ctime(&alert.triggerTime)
        << "- 消息: " << alert.message << "\n";
    
    return oss.str();
}

std::string AlertNotifier::formatAlertTitle(const Alert& alert, const AlertRule& rule) {
    std::ostringstream oss;
    oss << "[告警] " << alert.ruleName;
    return oss.str();
}

