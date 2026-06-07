#pragma once

#include <drogon/HttpController.h>
#include "AlertEngine.h"
#include "AlertAggregator.h"
#include "../common/AjaxResult.h"
#include "../filters/PermFilter.h"
#include <json/json.h>

/**
 * @file AlertCtrl.h
 * @brief 告警管理 API 控制器
 * 
 * API 端点：
 *   GET  /monitor/alert/rules              - 获取告警规则列表
 *   POST /monitor/alert/rules              - 创建告警规则
 *   PUT  /monitor/alert/rules/{id}         - 更新告警规则
 *   DELETE /monitor/alert/rules/{id}       - 删除告警规则
 *   
 *   GET  /monitor/alert/alerts             - 获取告警列表
 *   GET  /monitor/alert/alerts/{id}        - 获取告警详情
 *   POST /monitor/alert/alerts/{id}/ack    - 确认告警
 *   
 *   GET  /monitor/alert/aggregated         - 获取聚合告警
 *   GET  /monitor/alert/stats              - 获取告警统计
 */

class AlertCtrl : public drogon::HttpController<AlertCtrl> {
public:
    METHOD_LIST_BEGIN
        // 规则管理
        ADD_METHOD_TO(AlertCtrl::listRules,    "/monitor/alert/rules",           drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(AlertCtrl::createRule,   "/monitor/alert/rules",           drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(AlertCtrl::updateRule,   "/monitor/alert/rules/{id}",      drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(AlertCtrl::deleteRule,   "/monitor/alert/rules/{id}",      drogon::Delete, "JwtAuthFilter");
        
        // 告警管理
        ADD_METHOD_TO(AlertCtrl::listAlerts,   "/monitor/alert/alerts",          drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(AlertCtrl::getAlert,     "/monitor/alert/alerts/{id}",     drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(AlertCtrl::ackAlert,     "/monitor/alert/alerts/{id}/ack", drogon::Post,   "JwtAuthFilter");
        
        // 聚合告警
        ADD_METHOD_TO(AlertCtrl::listAggregated, "/monitor/alert/aggregated",    drogon::Get,    "JwtAuthFilter");
        
        // 统计
        ADD_METHOD_TO(AlertCtrl::getStats,     "/monitor/alert/stats",           drogon::Get,    "JwtAuthFilter");
    METHOD_LIST_END
    
    // 规则管理
    void listRules(const drogon::HttpRequestPtr &req,
                   std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    
    void createRule(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    
    void updateRule(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                    const std::string &id);
    
    void deleteRule(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                    const std::string &id);
    
    // 告警管理
    void listAlerts(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    
    void getAlert(const drogon::HttpRequestPtr &req,
                  std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                  const std::string &id);
    
    void ackAlert(const drogon::HttpRequestPtr &req,
                  std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                  const std::string &id);
    
    // 聚合告警
    void listAggregated(const drogon::HttpRequestPtr &req,
                        std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    
    // 统计
    void getStats(const drogon::HttpRequestPtr &req,
                  std::function<void(const drogon::HttpResponsePtr &)> &&cb);
};

