#include "AlertCtrl.h"
#include <trantor/utils/Logger.h>

void AlertCtrl::listRules(const drogon::HttpRequestPtr &req,
                          std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto rules = AlertEngine::instance().getAllRules();
    
    Json::Value j(Json::arrayValue);
    for (const auto& rule : rules) {
        j.append(rule.toJson());
    }
    
    RESP_OK(cb, j);
}

void AlertCtrl::createRule(const drogon::HttpRequestPtr &req,
                           std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto body = req->getJsonObject();
    if (!body) {
        RESP_ERR(cb, "请求体格式错误");
        return;
    }
    
    AlertRule rule = AlertRule::fromJson(*body);
    
    if (rule.id.empty()) {
        RESP_ERR(cb, "规则ID不能为空");
        return;
    }
    
    if (!AlertEngine::instance().addRule(rule)) {
        RESP_ERR(cb, "规则已存在");
        return;
    }
    
    LOG_OPER(req, "创建告警规则: " + rule.id, BusinessType::INSERT);
    RESP_MSG(cb, "规则创建成功");
}

void AlertCtrl::updateRule(const drogon::HttpRequestPtr &req,
                           std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                           const std::string &id) {
    auto body = req->getJsonObject();
    if (!body) {
        RESP_ERR(cb, "请求体格式错误");
        return;
    }
    
    AlertRule rule = AlertRule::fromJson(*body);
    rule.id = id;
    
    if (!AlertEngine::instance().updateRule(rule)) {
        RESP_ERR(cb, "规则不存在");
        return;
    }
    
    LOG_OPER(req, "更新告警规则: " + id, BusinessType::UPDATE);
    RESP_MSG(cb, "规则更新成功");
}

void AlertCtrl::deleteRule(const drogon::HttpRequestPtr &req,
                           std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                           const std::string &id) {
    if (!AlertEngine::instance().deleteRule(id)) {
        RESP_ERR(cb, "规则不存在");
        return;
    }
    
    LOG_OPER(req, "删除告警规则: " + id, BusinessType::DELETE);
    RESP_MSG(cb, "规则删除成功");
}

void AlertCtrl::listAlerts(const drogon::HttpRequestPtr &req,
                           std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    std::string status = req->getParameter("status");
    auto alerts = AlertEngine::instance().getAlerts(status);
    
    Json::Value j(Json::arrayValue);
    for (const auto& alert : alerts) {
        j.append(alert.toJson());
    }
    
    RESP_OK(cb, j);
}

void AlertCtrl::getAlert(const drogon::HttpRequestPtr &req,
                         std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                         const std::string &id) {
    auto alert = AlertEngine::instance().getAlert(id);
    if (!alert) {
        RESP_ERR(cb, "告警不存在");
        return;
    }
    
    RESP_OK(cb, alert->toJson());
}

void AlertCtrl::ackAlert(const drogon::HttpRequestPtr &req,
                         std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                         const std::string &id) {
    auto body = req->getJsonObject();
    std::string acknowledgedBy = GET_USER_NAME(req);
    std::string reason = body ? body->get("reason", "").asString() : "";
    
    if (!AlertEngine::instance().acknowledgeAlert(id, acknowledgedBy, reason)) {
        RESP_ERR(cb, "告警不存在");
        return;
    }
    
    LOG_OPER(req, "确认告警: " + id, BusinessType::UPDATE);
    RESP_MSG(cb, "告警已确认");
}

void AlertCtrl::listAggregated(const drogon::HttpRequestPtr &req,
                               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto aggregated = AlertAggregator::instance().getAggregatedAlerts();
    
    Json::Value j(Json::arrayValue);
    for (const auto& agg : aggregated) {
        j.append(agg.toJson());
    }
    
    RESP_OK(cb, j);
}

void AlertCtrl::getStats(const drogon::HttpRequestPtr &req,
                         std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto engineStats = AlertEngine::instance().getStats();
    auto aggStats = AlertAggregator::instance().getStats();
    
    Json::Value j;
    j["engine"]["totalRules"] = engineStats.totalRules;
    j["engine"]["enabledRules"] = engineStats.enabledRules;
    j["engine"]["totalAlerts"] = engineStats.totalAlerts;
    j["engine"]["triggeredAlerts"] = engineStats.triggeredAlerts;
    j["engine"]["acknowledgedAlerts"] = engineStats.acknowledgedAlerts;
    j["engine"]["resolvedAlerts"] = engineStats.resolvedAlerts;
    
    j["aggregator"]["totalAlerts"] = aggStats.totalAlerts;
    j["aggregator"]["aggregatedAlerts"] = aggStats.aggregatedAlerts;
    j["aggregator"]["duplicateAlerts"] = aggStats.duplicateAlerts;
    j["aggregator"]["averageAlertsPerGroup"] = aggStats.averageAlertsPerGroup;
    
    RESP_OK(cb, j);
}

