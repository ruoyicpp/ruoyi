#include "AlertNotifier.h"
#include "../common/SmtpUtils.h"
#include "../common/HttpCaller.h"

#include <trantor/utils/Logger.h>
#include <sstream>
#include <iomanip>

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

bool AlertNotifier::notifyByEmail(const Alert& alert,
                                   const std::vector<std::string>& recipients) {
    if (recipients.empty()) {
        LOG_WARN << "[AlertNotifier] No email recipients for alert: " << alert.id;
        return false;
    }

    if (!SmtpUtils::instance().isConfigured()) {
        LOG_ERROR << "[AlertNotifier] SMTP not configured, cannot send email for alert: "
                  << alert.id;
        return false;
    }

    const auto title = formatAlertTitle(alert, AlertRule());
    const auto message = formatAlertMessage(alert, AlertRule());

    bool allOk = true;
    for (const auto& recipient : recipients) {
        bool sent = SmtpUtils::instance().send(recipient, title, message);
        if (!sent) {
            LOG_ERROR << "[AlertNotifier] Failed to send email to " << recipient
                      << " for alert " << alert.id;
            allOk = false;
        } else {
            LOG_INFO << "[AlertNotifier] Email sent to " << recipient
                     << " for alert " << alert.id;
        }
    }
    return allOk;
}

bool AlertNotifier::notifyByDingTalk(const Alert& alert,
                                      const std::string& webhookUrl) {
    if (webhookUrl.empty() || webhookUrl.find("xxx") != std::string::npos) {
        LOG_WARN << "[AlertNotifier] DingTalk webhook URL is empty or placeholder, "
                    "skipping alert: " << alert.id;
        return false;
    }

    Json::Value payload;
    payload["msgtype"] = "markdown";

    Json::Value markdown;
    markdown["title"] = formatAlertTitle(alert, AlertRule());

    std::ostringstream body;
    body << formatAlertMessage(alert, AlertRule());
    body << "\n\n> 告警ID: `" << alert.id << "` | 规则: " << alert.ruleName
         << " | 时间: " << std::put_time(std::localtime(&alert.triggerTime), "%Y-%m-%d %H:%M:%S");
    markdown["text"] = body.str();

    payload["markdown"] = markdown;

    Json::StreamWriterBuilder writer;
    const auto bodyStr = Json::writeString(writer, payload);

    bool done = false;
    HttpCaller::asyncPost(webhookUrl, bodyStr, "application/json",
        [&done, &alert](bool ok, int status, const std::string& respBody) {
            if (ok && status == 200) {
                LOG_INFO << "[AlertNotifier] DingTalk notification sent for alert: " << alert.id;
                done = true;
            } else {
                LOG_ERROR << "[AlertNotifier] DingTalk notification failed for alert "
                          << alert.id << ": HTTP " << status << " body=" << respBody;
            }
        });

    return done;
}

bool AlertNotifier::notifyByWeChat(const Alert& alert,
                                     const std::string& webhookUrl) {
    if (webhookUrl.empty() || webhookUrl.find("xxx") != std::string::npos) {
        LOG_WARN << "[AlertNotifier] WeCom webhook URL is empty or placeholder, "
                    "skipping alert: " << alert.id;
        return false;
    }

    Json::Value payload;
    payload["msgtype"] = "markdown";

    Json::Value markdown;
    std::ostringstream content;
    content << formatAlertMessage(alert, AlertRule());
    content << "\n\n> 告警ID: `" << alert.id << "` | 时间: "
            << std::put_time(std::localtime(&alert.triggerTime), "%Y-%m-%d %H:%M:%S");
    markdown["content"] = content.str();

    payload["markdown"] = markdown;

    Json::StreamWriterBuilder writer;
    const auto bodyStr = Json::writeString(writer, payload);

    bool done = false;
    HttpCaller::asyncPost(webhookUrl, bodyStr, "application/json",
        [&done, &alert](bool ok, int status, const std::string& respBody) {
            if (ok && status == 200) {
                LOG_INFO << "[AlertNotifier] WeCom notification sent for alert: " << alert.id;
                done = true;
            } else {
                LOG_ERROR << "[AlertNotifier] WeCom notification failed for alert "
                          << alert.id << ": HTTP " << status << " body=" << respBody;
            }
        });

    return done;
}

bool AlertNotifier::notifyByWebhook(const Alert& alert,
                                      const std::string& webhookUrl) {
    if (webhookUrl.empty() || webhookUrl.find("xxx") != std::string::npos) {
        LOG_WARN << "[AlertNotifier] Webhook URL is empty or placeholder, "
                    "skipping alert: " << alert.id;
        return false;
    }

    Json::Value payload;
    payload["alert_id"]      = alert.id;
    payload["rule_id"]       = alert.ruleId;
    payload["rule_name"]     = alert.ruleName;
    payload["severity"]      = static_cast<int>(alert.severity);
    payload["severity_str"]  = severityLabel(alert.severity);
    payload["metric"]        = alert.metric;
    payload["value"]         = alert.value;
    payload["threshold"]     = alert.threshold;
    payload["message"]       = alert.message;
    payload["trigger_time"]  = static_cast<Json::Value::Int64>(alert.triggerTime);
    payload["status"]        = alert.status;

    Json::StreamWriterBuilder writer;
    const auto bodyStr = Json::writeString(writer, payload);

    bool done = false;
    HttpCaller::asyncPost(webhookUrl, bodyStr, "application/json",
        [&done, &alert](bool ok, int status, const std::string& respBody) {
            if (ok && status >= 200 && status < 300) {
                LOG_INFO << "[AlertNotifier] Webhook notification sent for alert: " << alert.id;
                done = true;
            } else {
                LOG_ERROR << "[AlertNotifier] Webhook notification failed for alert "
                          << alert.id << ": HTTP " << status << " body=" << respBody;
            }
        });

    return done;
}

bool AlertNotifier::notifyBySMS(const Alert& alert,
                                 const std::vector<std::string>& phoneNumbers) {
    if (phoneNumbers.empty()) {
        LOG_WARN << "[AlertNotifier] No phone numbers for SMS notification: " << alert.id;
        return false;
    }

    auto it = webhookUrls_.find("sms");
    if (it == webhookUrls_.end() || it->second.empty()
        || it->second.find("xxx") != std::string::npos) {
        // 没有配置短信网关时，只记录告警信息（可接入阿里云/腾讯云短信 API）
        LOG_WARN << "[AlertNotifier] SMS gateway not configured (alert."  // sic — log level
                 << "), would send to " << phoneNumbers.size() << " numbers for alert "
                 << alert.id << ": " << alert.message;
        return false;
    }

    // 通用的 HTTP SMS 网关（如阿里云/腾讯云企业短信 API）
    // payload 按标准短信网关格式：{ "phones": [...], "content": "..." }
    Json::Value payload;
    Json::Value phones(Json::arrayValue);
    for (const auto& p : phoneNumbers) {
        phones.append(p);
    }
    payload["phones"]  = phones;
    payload["content"] = formatAlertTitle(alert, AlertRule()) + " " + alert.message;

    Json::StreamWriterBuilder writer;
    const auto bodyStr = Json::writeString(writer, payload);

    bool done = false;
    HttpCaller::asyncPost(it->second, bodyStr, "application/json",
        [&done, &alert](bool ok, int status, const std::string& respBody) {
            if (ok && status >= 200 && status < 300) {
                LOG_INFO << "[AlertNotifier] SMS notification sent for alert: " << alert.id;
                done = true;
            } else {
                LOG_ERROR << "[AlertNotifier] SMS notification failed for alert "
                          << alert.id << ": HTTP " << status << " body=" << respBody;
            }
        });

    return done;
}

void AlertNotifier::registerHandler(const std::string& channel,
                                     NotificationHandler handler) {
    handlers_[channel] = handler;
    LOG_INFO << "[AlertNotifier] Registered custom handler for channel: " << channel;
}

std::string AlertNotifier::formatAlertMessage(const Alert& alert,
                                               const AlertRule& /*rule*/) {
    std::ostringstream oss;
    oss << "**告警详情**\n\n"
        << "- 告警ID: " << alert.id << "\n"
        << "- 规则名称: " << alert.ruleName << "\n"
        << "- 告警级别: **" << severityLabel(alert.severity) << "**\n"
        << "- 监控指标: " << alert.metric << "\n"
        << "- 当前值: **" << alert.value << "**\n"
        << "- 阈值: " << alert.threshold << "\n"
        << "- 触发时间: "
        << std::put_time(std::localtime(&alert.triggerTime), "%Y-%m-%d %H:%M:%S") << "\n"
        << "- 消息: " << alert.message << "\n";
    return oss.str();
}

std::string AlertNotifier::formatAlertTitle(const Alert& alert,
                                              const AlertRule& /*rule*/) {
    std::ostringstream oss;
    oss << "[" << severityLabel(alert.severity) << "] " << alert.ruleName;
    return oss.str();
}

std::string AlertNotifier::severityLabel(AlertSeverity s) {
    switch (s) {
        case AlertSeverity::INFO:     return "信息";
        case AlertSeverity::WARNING: return "警告";
        case AlertSeverity::ERROR:   return "错误";
        case AlertSeverity::CRITICAL: return "严重";
    }
    return "未知";
}
