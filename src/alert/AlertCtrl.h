#pragma once

#include <drogon/HttpController.h>
#include "AlertEngine.h"
#include "AlertAggregator.h"
#include "../common/AjaxResult.h"
#include "../filters/PermFilter.h"
#include <json/json.h>

/**
 * @file AlertCtrl.h
 * @brief 告警管理 API 控制器 — 告警规则、告警事件和统计管理
 * 
 * 功能概述：
 *   - 告警规则管理：创建、更新、删除告警规则
 *   - 告警事件查询：查询和过滤告警事件
 *   - 告警确认：确认和关闭告警
 *   - 告警聚合：查看聚合后的告警
 *   - 告警统计：统计告警数量和分布
 *   - 告警导出：导出告警数据
 * 
 * 核心特性：
 *   - 规则管理：灵活的告警规则配置
 *   - 实时告警：实时告警事件推送
 *   - 告警聚合：智能聚合相同告警
 *   - 多维统计：多维度告警统计
 *   - 权限控制：基于权限的访问控制
 *   - 数据导出：支持多格式导出
 * 
 * API 端点：
 *   - GET /monitor/alert/rules - 获取告警规则列表
 *   - POST /monitor/alert/rules - 创建告警规则
 *   - PUT /monitor/alert/rules/{id} - 更新告警规则
 *   - DELETE /monitor/alert/rules/{id} - 删除告警规则
 *   - GET /monitor/alert/alerts - 获取告警列表
 *   - GET /monitor/alert/alerts/{id} - 获取告警详情
 *   - POST /monitor/alert/alerts/{id}/ack - 确认告警
 *   - GET /monitor/alert/aggregated - 获取聚合告警
 *   - GET /monitor/alert/stats - 获取告警统计
 * 
 * 请求/响应示例：
 *   ```
 *   POST /monitor/alert/rules
 *   Authorization: Bearer <JWT>
 *   Content-Type: application/json
 *   
 *   {
 *     "name": "CPU 使用率告警",
 *     "description": "CPU 使用率超过 80%",
 *     "enabled": true,
 *     "condition": "cpu_usage > 80",
 *     "severity": "WARNING",
 *     "notificationChannels": ["email", "sms"]
 *   }
 *   
 *   响应：
 *   {
 *     "code": 200,
 *     "msg": "success",
 *     "data": {
 *       "id": "rule_123",
 *       "name": "CPU 使用率告警",
 *       "enabled": true
 *     }
 *   }
 *   ```
 * 
 * 权限要求：
 *   - monitor:alert:list - 查看告警列表
 *   - monitor:alert:add - 创建告警规则
 *   - monitor:alert:edit - 修改告警规则
 *   - monitor:alert:remove - 删除告警规则
 *   - monitor:alert:ack - 确认告警
 * 
 * 配置项（config.json）：
 *   - alert.enabled: 是否启用告警系统（默认 true）
 *   - alert.max_rules: 最大规则数（默认 1000）
 *   - alert.max_alerts: 最大告警数（默认 10000）
 *   - alert.retention_days: 告警保留天数（默认 30）
 * 
 * @see AlertEngine - 告警引擎
 * @see AlertAggregator - 告警聚合
 * @see AlertRule - 告警规则
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

