/**
 * @file FlowCtrl.h
 * @brief 工作流控制器 — 流程定义管理 + 实例发起 + 待办/已办 + 审批操作
 *
 * 功能概述：
 *   - 定义管理：wf_definition CRUD（管理员），nodes JSON 校验
 *   - 实例发起：按 def_key 启动流程，生成首节点审批任务
 *   - 待办/已办：按当前用户过滤 wf_task
 *   - 审批操作：approve/reject/cancel，或签/会签自动推进
 *
 * API 端点：
 *   定义管理（管理员）：
 *   - GET    /flow/definition/list        定义列表（flow:def:list）
 *   - POST   /flow/definition             新增定义（flow:def:add）
 *   - PUT    /flow/definition             修改定义（flow:def:edit）
 *   - DELETE /flow/definition/{id}        删除定义（flow:def:remove）
 *   实例操作（登录用户）：
 *   - POST   /flow/instance/start         发起流程 {def_key, business_key, title, variables}
 *   - GET    /flow/instance/list          我发起的实例
 *   - GET    /flow/instance/{id}          实例详情（含任务列表）
 *   - POST   /flow/instance/{id}/cancel   撤销（仅发起人）
 *   审批操作（登录用户）：
 *   - GET    /flow/task/todo              我的待办
 *   - GET    /flow/task/done              我的已办
 *   - POST   /flow/task/{id}/approve      通过 {comment?, variables?}
 *   - POST   /flow/task/{id}/reject       驳回 {comment}
 */

#pragma once
#include <drogon/HttpController.h>
#include <json/json.h>
#include <sstream>
#include "../common/AjaxResult.h"
#include "../common/OperLogUtils.h"
#include "../common/PageUtils.h"
#include "../filters/PermFilter.h"
#include "../services/DatabaseService.h"
#include "../system/services/TokenService.h"
#include "FlowEngine.h"

/**
 * @class FlowCtrl
 * @brief 工作流控制器
 */
class FlowCtrl : public drogon::HttpController<FlowCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(FlowCtrl::defList,   "/flow/definition/list",      drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(FlowCtrl::defAdd,    "/flow/definition",           drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(FlowCtrl::defEdit,   "/flow/definition",           drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(FlowCtrl::defDel,    "/flow/definition/{id}",      drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(FlowCtrl::start,     "/flow/instance/start",       drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(FlowCtrl::instList,  "/flow/instance/list",        drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(FlowCtrl::instDetail,"/flow/instance/{id}",        drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(FlowCtrl::cancel,    "/flow/instance/{id}/cancel", drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(FlowCtrl::todo,      "/flow/task/todo",            drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(FlowCtrl::done,      "/flow/task/done",            drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(FlowCtrl::approve,   "/flow/task/{id}/approve",    drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(FlowCtrl::reject,    "/flow/task/{id}/reject",     drogon::Post,   "JwtAuthFilter");
    METHOD_LIST_END

    // ── 定义管理 ────────────────────────────────────────────────────

    void defList(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "flow:def:list");
        auto res = DatabaseService::instance().query(
            "SELECT id,name,def_key,version,nodes,status,remark,create_time "
            "FROM wf_definition ORDER BY def_key, version DESC");
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["id"]       = (Json::Int64)res.longVal(i, 0);
            j["name"]     = res.str(i, 1);
            j["def_key"]  = res.str(i, 2);
            j["version"]  = res.intVal(i, 3);
            j["nodes"]    = res.str(i, 4);   // JSON 串原样返回，前端解析
            j["status"]   = res.str(i, 5);
            j["remark"]   = res.str(i, 6);
            j["create_time"] = res.str(i, 7);
            rows.append(j);
        }
        RESP_OK(cb, rows);
    }

    void defAdd(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "flow:def:add");
        auto b = req->getJsonObject();
        if (!b || !(*b).isMember("def_key") || !(*b).isMember("nodes")) {
            RESP_ERR(cb, "缺少 def_key/nodes"); return;
        }
        std::string err;
        if (!FlowEngine::validateNodes((*b)["nodes"], err)) { RESP_ERR(cb, err); return; }
        DatabaseService::instance().execParams(
            "INSERT INTO wf_definition(name,def_key,version,nodes,status,remark) "
            "VALUES($1,$2,$3,$4,$5,$6)",
            {b->get("name","").asString(), (*b)["def_key"].asString(),
             std::to_string(b->get("version",1).asInt()),
             Json::writeString(Json::StreamWriterBuilder(), (*b)["nodes"]),
             b->get("status","0").asString(), b->get("remark","").asString()});
        LOG_OPER(req, "流程定义", BusinessType::INSERT);
        RESP_MSG(cb, "添加成功");
    }

    void defEdit(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "flow:def:edit");
        auto b = req->getJsonObject();
        if (!b || !(*b).isMember("id")) { RESP_ERR(cb, "缺少 id"); return; }
        if ((*b).isMember("nodes")) {
            std::string err;
            if (!FlowEngine::validateNodes((*b)["nodes"], err)) { RESP_ERR(cb, err); return; }
        }
        DatabaseService::instance().execParams(
            "UPDATE wf_definition SET name=$1,nodes=$2,status=$3,remark=$4 WHERE id=$5",
            {b->get("name","").asString(),
             b->isMember("nodes") ? Json::writeString(Json::StreamWriterBuilder(), (*b)["nodes"]) : "[]",
             b->get("status","0").asString(), b->get("remark","").asString(),
             std::to_string((*b)["id"].asInt64())});
        LOG_OPER(req, "流程定义", BusinessType::UPDATE);
        RESP_MSG(cb, "修改成功");
    }

    void defDel(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                long id) {
        CHECK_PERM(req, cb, "flow:def:remove");
        DatabaseService::instance().execParams(
            "DELETE FROM wf_definition WHERE id=$1", {std::to_string(id)});
        LOG_OPER_PARAM(req, "流程定义", BusinessType::REMOVE, std::to_string(id));
        RESP_MSG(cb, "删除成功");
    }

    // ── 实例操作 ────────────────────────────────────────────────────

    /// POST /flow/instance/start {def_key, business_key?, title, variables?}
    void start(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        auto b = req->getJsonObject();
        if (!b || !(*b).isMember("def_key") || !(*b).isMember("title")) {
            RESP_ERR(cb, "缺少 def_key/title"); return;
        }
        Json::Value vars = b->isMember("variables") ? (*b)["variables"] : Json::Value(Json::objectValue);
        long instId = FlowEngine::start(
            (*b)["def_key"].asString(), b->get("business_key","").asString(),
            (*b)["title"].asString(), user->userId, user->userName, vars);
        if (instId <= 0) { RESP_ERR(cb, "流程定义不存在或已停用"); return; }
        LOG_OPER_PARAM(req, "流程实例", BusinessType::INSERT, std::to_string(instId));
        Json::Value r = AjaxResult::success();
        r["instance_id"] = (Json::Int64)instId;
        RESP_JSON(cb, r);
    }

    /// GET /flow/instance/list — 我发起的
    void instList(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        auto cnt = db.queryParams(
            "SELECT COUNT(*) FROM wf_instance WHERE starter_id=$1",
            {std::to_string(user->userId)});
        long total = (cnt.ok() && cnt.rows() > 0) ? cnt.longVal(0, 0) : 0;
        auto res = db.queryParams(
            "SELECT id,def_id,business_key,title,current_node,status,variables,"
            "create_time,end_time FROM wf_instance WHERE starter_id=$1 "
            "ORDER BY id DESC LIMIT $2 OFFSET $3",
            {std::to_string(user->userId),
             std::to_string(page.pageSize), std::to_string(page.offset())});
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) rows.append(instJson(res, i));
        Json::Value data; data["total"] = (Json::Int64)total; data["rows"] = rows;
        RESP_OK(cb, data);
    }

    /// GET /flow/instance/{id} — 详情（含任务列表）
    void instDetail(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                    long id) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        auto& db = DatabaseService::instance();
        auto res = db.queryParams(
            "SELECT id,def_id,business_key,title,current_node,status,variables,"
            "create_time,end_time FROM wf_instance WHERE id=$1",
            {std::to_string(id)});
        if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "实例不存在"); return; }
        Json::Value j = instJson(res, 0);
        // 任务列表
        auto tasks = db.queryParams(
            "SELECT id,node_key,node_name,assignee_id,status,comment,create_time,finish_time "
            "FROM wf_task WHERE instance_id=$1 ORDER BY id", {std::to_string(id)});
        if (tasks.ok()) for (int i = 0; i < tasks.rows(); ++i) {
            Json::Value t;
            t["id"]          = (Json::Int64)tasks.longVal(i, 0);
            t["node_key"]    = tasks.str(i, 1);
            t["node_name"]   = tasks.str(i, 2);
            t["assignee_id"] = (Json::Int64)tasks.longVal(i, 3);
            t["status"]      = tasks.str(i, 4);
            t["comment"]     = tasks.str(i, 5);
            t["create_time"] = tasks.str(i, 6);
            t["finish_time"] = tasks.str(i, 7);
            j["tasks"].append(t);
        }
        RESP_OK(cb, j);
    }

    /// POST /flow/instance/{id}/cancel — 撤销（仅发起人）
    void cancel(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                long id) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        if (!FlowEngine::cancel(id, user->userId)) {
            RESP_ERR(cb, "撤销失败：非发起人或实例已结束"); return;
        }
        LOG_OPER_PARAM(req, "流程实例", BusinessType::REMOVE, std::to_string(id));
        RESP_MSG(cb, "已撤销");
    }

    // ── 审批操作 ────────────────────────────────────────────────────

    /// GET /flow/task/todo — 我的待办
    void todo(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        taskList(req, std::move(cb), "pending");
    }

    /// GET /flow/task/done — 我的已办
    void done(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        taskList(req, std::move(cb), "");
    }

    /// POST /flow/task/{id}/approve {comment?, variables?}
    void approve(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                 long id) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        auto b = req->getJsonObject();
        std::string comment = b ? b->get("comment","").asString() : "";
        Json::Value vars = (b && b->isMember("variables")) ? (*b)["variables"] : Json::Value();
        int rc = FlowEngine::approve(id, user->userId, comment, vars);
        if (rc == 1) { RESP_ERR(cb, "任务不存在或已处理"); return; }
        if (rc == 2) { RESP_ERR(cb, "非本任务审批人"); return; }
        if (rc == 3) { RESP_ERR(cb, "流程实例已结束"); return; }
        LOG_OPER_PARAM(req, "流程审批", BusinessType::UPDATE, std::to_string(id));
        RESP_MSG(cb, "已通过");
    }

    /// POST /flow/task/{id}/reject {comment}
    void reject(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                long id) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        auto b = req->getJsonObject();
        std::string comment = b ? b->get("comment","").asString() : "";
        int rc = FlowEngine::reject(id, user->userId, comment);
        if (rc == 1) { RESP_ERR(cb, "任务不存在或已处理"); return; }
        if (rc == 2) { RESP_ERR(cb, "非本任务审批人"); return; }
        if (rc == 3) { RESP_ERR(cb, "流程实例已结束"); return; }
        LOG_OPER_PARAM(req, "流程审批", BusinessType::UPDATE, std::to_string(id));
        RESP_MSG(cb, "已驳回");
    }

private:
    /// 实例行 → JSON
    static Json::Value instJson(const DatabaseService::QueryResult& res, int i) {
        Json::Value j;
        j["id"]           = (Json::Int64)res.longVal(i, 0);
        j["def_id"]       = (Json::Int64)res.longVal(i, 1);
        j["business_key"] = res.str(i, 2);
        j["title"]        = res.str(i, 3);
        j["current_node"] = res.str(i, 4);
        j["status"]       = res.str(i, 5);
        j["variables"]    = res.str(i, 6);
        j["create_time"]  = res.str(i, 7);
        j["end_time"]     = res.str(i, 8);
        return j;
    }

    /// 待办/已办共用查询（status 空=已办即非 pending）
    void taskList(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                  const std::string& status) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        std::string cond = status.empty() ? "t.status<>'pending'" : "t.status='pending'";
        auto cnt = db.queryParams(
            "SELECT COUNT(*) FROM wf_task t WHERE t.assignee_id=$1 AND " + cond,
            {std::to_string(user->userId)});
        long total = (cnt.ok() && cnt.rows() > 0) ? cnt.longVal(0, 0) : 0;
        auto res = db.queryParams(
            "SELECT t.id,t.instance_id,t.node_key,t.node_name,t.status,t.comment,"
            "t.create_time,t.finish_time,i.title,i.starter_name "
            "FROM wf_task t JOIN wf_instance i ON t.instance_id=i.id "
            "WHERE t.assignee_id=$1 AND " + cond + " ORDER BY t.id DESC LIMIT $2 OFFSET $3",
            {std::to_string(user->userId),
             std::to_string(page.pageSize), std::to_string(page.offset())});
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["id"]           = (Json::Int64)res.longVal(i, 0);
            j["instance_id"]  = (Json::Int64)res.longVal(i, 1);
            j["node_key"]     = res.str(i, 2);
            j["node_name"]    = res.str(i, 3);
            j["status"]       = res.str(i, 4);
            j["comment"]      = res.str(i, 5);
            j["create_time"]  = res.str(i, 6);
            j["finish_time"]  = res.str(i, 7);
            j["title"]        = res.str(i, 8);
            j["starter_name"] = res.str(i, 9);
            rows.append(j);
        }
        Json::Value data; data["total"] = (Json::Int64)total; data["rows"] = rows;
        RESP_OK(cb, data);
    }
};
