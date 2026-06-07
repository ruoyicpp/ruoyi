#pragma once

#include "AlertRule.h"
#include <string>
#include <vector>
#include <functional>

/**
 * @file AlertNotifier.h
 * @brief 告警通知器
 * 
 * 支持多渠道通知：
 *   - 邮件通知
 *   - 钉钉通知
 *   - 企业微信通知
 *   - Webhook 通知
 *   - 短信通知（可选）
 */

class AlertNotifier {
public:
    static AlertNotifier& instance() {
        static AlertNotifier inst;
        return inst;
    }
    
    // 初始化
    void init(const Json::Value& config);
    
    // 发送通知
    bool notifyAlert(const Alert& alert, const AlertRule& rule);
    
    // 按渠道发送
    bool notifyByEmail(const Alert& alert, const std::vector<std::string>& recipients);
    bool notifyByDingTalk(const Alert& alert, const std::string& webhookUrl);
    bool notifyByWeChat(const Alert& alert, const std::string& webhookUrl);
    bool notifyByWebhook(const Alert& alert, const std::string& webhookUrl);
    bool notifyBySMS(const Alert& alert, const std::vector<std::string>& phoneNumbers);
    
    // 注册自定义通知处理器
    using NotificationHandler = std::function<bool(const Alert&, const std::string&)>;
    void registerHandler(const std::string& channel, NotificationHandler handler);

private:
    AlertNotifier() = default;
    ~AlertNotifier() = default;
    AlertNotifier(const AlertNotifier&) = delete;
    AlertNotifier& operator=(const AlertNotifier&) = delete;
    
    // 格式化告警消息
    std::string formatAlertMessage(const Alert& alert, const AlertRule& rule);
    std::string formatAlertTitle(const Alert& alert, const AlertRule& rule);
    
    // 数据成员
    std::unordered_map<std::string, NotificationHandler> handlers_;
    
    // 配置
    std::string smtpHost_;
    int smtpPort_;
    std::string smtpUser_;
    std::string smtpPassword_;
    
    std::unordered_map<std::string, std::string> webhookUrls_;  // 渠道 -> Webhook URL
};

