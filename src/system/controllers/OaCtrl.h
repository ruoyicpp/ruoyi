/**
 * @file OaCtrl.h
 * @brief 办公自动化（OA）控制器 — 提供审批、会议、知识库、车辆、资产等功能
 * 
 * 功能概述：
 *   - 审批流程：审批申请、审批模板、工作流管理
 *   - 会议管理：会议预订、会议室管理、会议记录
 *   - 知识库：知识文档、评论、点赞、收藏
 *   - 车辆管理：车辆列表、使用申请、使用记录
 *   - 资产管理：资产借用、归还、借用记录
 *   - 待办事项：个人待办、任务管理
 *   - 仪表板：OA 数据统计和展示
 * 
 * API 端点（审批）：
 *   - GET    /oa/approval/list              - 审批列表
 *   - GET    /oa/approval/{id}              - 审批详情
 *   - POST   /oa/approval                   - 创建审批
 *   - PUT    /oa/approval                   - 更新审批
 *   - POST   /oa/approval/{id}/review       - 审批审核
 *   - GET    /oa/approval/template/list     - 审批模板列表
 *   - GET    /oa/workflow/{workflowId}/nodes - 工作流节点
 * 
 * API 端点（会议）：
 *   - GET    /oa/meeting/list               - 会议列表
 *   - POST   /oa/meeting                    - 创建会议
 *   - PUT    /oa/meeting                    - 更新会议
 *   - GET    /oa/meeting/room/list          - 会议室列表
 *   - POST   /oa/meeting/room               - 创建会议室
 *   - PUT    /oa/meeting/room               - 更新会议室
 * 
 * API 端点（知识库）：
 *   - GET    /oa/knowledge/list             - 知识列表
 *   - GET    /oa/knowledge/{id}             - 知识详情
 *   - POST   /oa/knowledge                  - 创建知识
 *   - PUT    /oa/knowledge                  - 更新知识
 *   - GET    /oa/knowledge/{id}/comment/list - 评论列表
 *   - POST   /oa/knowledge/{id}/comment     - 创建评论
 *   - POST   /oa/knowledge/{id}/like        - 点赞
 *   - POST   /oa/knowledge/{id}/favorite    - 收藏
 * 
 * API 端点（车辆和资产）：
 *   - GET    /oa/vehicle/list               - 车辆列表
 *   - POST   /oa/vehicle/usage              - 车辆使用申请
 *   - GET    /oa/asset/borrow/list          - 资产借用列表
 *   - POST   /oa/asset/borrow               - 资产借用申请
 *   - POST   /oa/asset/borrow/{id}/return   - 资产归还
 * 
 * API 端点（其他）：
 *   - GET    /oa/todo/my                    - 我的待办
 *   - GET    /oa/dashboard                  - OA 仪表板
 * 
 * @see DatabaseService - 数据库服务
 * @see OperLogUtils - 操作日志工具
 */

#pragma once
#include "../../common/OperLogUtils.h"
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/PageUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"

/**
 * @class OaCtrl
 * @brief 办公自动化（OA）控制器
 * 
 * 提供企业办公自动化的各项功能，包括审批流程、会议管理、知识库、
 * 车辆管理、资产管理等，支持完整的工作流和权限控制。
 */
class OaCtrl : public drogon::HttpController<OaCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(OaCtrl::listApprovals,      "/oa/approval/list",                drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::getApproval,        "/oa/approval/{id}",               drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::createApproval,     "/oa/approval",                    drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::updateApproval,     "/oa/approval",                    drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::reviewApproval,     "/oa/approval/{id}/review",        drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::listApprovalTemplates, "/oa/approval/template/list",    drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::listWorkflowNodes,  "/oa/workflow/{workflowId}/nodes",  drogon::Get,    "JwtAuthFilter");

        ADD_METHOD_TO(OaCtrl::listMeetings,       "/oa/meeting/list",                drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::createMeeting,      "/oa/meeting",                     drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::updateMeeting,      "/oa/meeting",                     drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::listMeetingRooms,   "/oa/meeting/room/list",           drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::createMeetingRoom,  "/oa/meeting/room",                drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::updateMeetingRoom,  "/oa/meeting/room",                drogon::Put,    "JwtAuthFilter");

        ADD_METHOD_TO(OaCtrl::listKnowledge,      "/oa/knowledge/list",              drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::getKnowledge,       "/oa/knowledge/{id}",              drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::createKnowledge,    "/oa/knowledge",                   drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::updateKnowledge,    "/oa/knowledge",                   drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::listKnowledgeComments, "/oa/knowledge/{id}/comment/list", drogon::Get, "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::createKnowledgeComment, "/oa/knowledge/{id}/comment",     drogon::Post, "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::toggleKnowledgeLike, "/oa/knowledge/{id}/like",        drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::toggleKnowledgeFavorite, "/oa/knowledge/{id}/favorite", drogon::Post,  "JwtAuthFilter");

        ADD_METHOD_TO(OaCtrl::listVehicles,       "/oa/vehicle/list",                drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::createVehicleUsage, "/oa/vehicle/usage",               drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::listBorrowAssets,   "/oa/asset/borrow/list",           drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::createBorrowAsset,  "/oa/asset/borrow",                drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::returnBorrowAsset,  "/oa/asset/borrow/{id}/return",    drogon::Post,   "JwtAuthFilter");

        ADD_METHOD_TO(OaCtrl::myTodo,             "/oa/todo/my",                     drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(OaCtrl::dashboard,          "/oa/dashboard",                   drogon::Get,    "JwtAuthFilter");
    METHOD_LIST_END

    /**
     * @brief 获取审批列表
     * @param req HTTP 请求
     * @param cb 回调函数
     * @return 审批列表和分页信息
     */
    void listApprovals(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    
    /**
     * @brief 获取审批详情
     * @param req HTTP 请求
     * @param cb 回调函数
     * @param id 审批 ID
     * @return 审批详情
     */
    void getApproval(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long long id);
    
    /**
     * @brief 创建审批
     * @param req HTTP 请求
     * @param cb 回调函数
     * @return 创建结果
     */
    void createApproval(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    
    /**
     * @brief 更新审批
     * @param req HTTP 请求
     * @param cb 回调函数
     * @return 更新结果
     */
    void updateApproval(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    
    /**
     * @brief 审批审核
     * @param req HTTP 请求
     * @param cb 回调函数
     * @param id 审批 ID
     * @return 审核结果
     */
    void reviewApproval(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long long id);
    void listApprovalTemplates(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void listWorkflowNodes(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long long workflowId);

    void listMeetings(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void createMeeting(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void updateMeeting(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void listMeetingRooms(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void createMeetingRoom(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void updateMeetingRoom(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);

    void listKnowledge(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void getKnowledge(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long long id);
    void createKnowledge(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void updateKnowledge(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void listKnowledgeComments(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long long id);
    void createKnowledgeComment(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long long id);
    void toggleKnowledgeLike(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long long id);
    void toggleKnowledgeFavorite(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long long id);

    void listVehicles(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void createVehicleUsage(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void listBorrowAssets(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void createBorrowAsset(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void returnBorrowAsset(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long long id);

    void myTodo(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);
    void dashboard(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb);

private:
    std::vector<std::string> splitIds(const std::string &ids);
};
