/**
 * @file FlowEngine.h
 * @brief 轻量工作流引擎 — 顺序审批流 + 或签/会签 + 流程变量
 *
 * 功能概述：
 *   - 流程定义：wf_definition 表，nodes JSON 数组描述顺序审批节点
 *   - 流程实例：wf_instance 表，记录当前节点/状态/变量
 *   - 审批任务：wf_task 表，每个节点按 assignees 生成任务
 *   - 或签(any)：任一审批人通过即进入下一节点
 *   - 会签(all)：全部审批人通过才进入下一节点，任一拒绝即驳回
 *   - 流程变量：variables JSON 随实例流转，审批时可追加
 *
 * nodes JSON 格式：
 *   [{"key":"n1","name":"主管审批","assignees":[2,3],"mode":"any"},
 *    {"key":"n2","name":"经理审批","assignees":[5],"mode":"all"}]
 *
 * 实例状态：running / approved / rejected / canceled
 * 任务状态：pending / approved / rejected / skipped（或签通过后其余自动跳过）
 *
 * 说明：轻量实现，覆盖常见审批场景；不含分支网关/条件路由（可后续扩展）。
 */

#pragma once
#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include <ctime>
#include <json/json.h>
#include <trantor/utils/Logger.h>
#include "../services/DatabaseService.h"
#include "../common/NotifyService.h"   // sendInbox 审批通知

/**
 * @class FlowEngine
 * @brief 工作流引擎（静态方法集，状态全在 DB）
 */
class FlowEngine {
public:
    // ── 流程定义 ────────────────────────────────────────────────────

    /// 校验 nodes JSON 合法性
    static bool validateNodes(const Json::Value& nodes, std::string& err) {
        if (!nodes.isArray() || nodes.empty()) { err = "nodes 须为非空数组"; return false; }
        for (auto& n : nodes) {
            if (!n.isMember("key") || !n.isMember("name") || !n.isMember("assignees")) {
                err = "节点缺少 key/name/assignees"; return false;
            }
            if (!n["assignees"].isArray() || n["assignees"].empty()) {
                err = "节点 " + n["key"].asString() + " 缺少审批人"; return false;
            }
            std::string mode = n.get("mode", "any").asString();
            if (mode != "any" && mode != "all") {
                err = "mode 仅支持 any(或签)/all(会签)"; return false;
            }
        }
        return true;
    }

    // ── 流程实例 ────────────────────────────────────────────────────

    /**
     * @brief 发起流程实例
     * @return 实例 ID，失败返回 0
     */
    static long start(const std::string& defKey, const std::string& businessKey,
                      const std::string& title, long starterId,
                      const std::string& starterName, const Json::Value& vars) {
        auto& db = DatabaseService::instance();
        // 取最新启用版本
        auto def = db.queryParams(
            "SELECT id,nodes FROM wf_definition WHERE def_key=$1 AND status='0' "
            "ORDER BY version DESC LIMIT 1", {defKey});
        if (!def.ok() || def.rows() == 0) {
            LOG_WARN << "[Flow] definition not found: " << defKey;
            return 0;
        }
        long defId = def.longVal(0, 0);
        Json::Value nodes; std::string err;
        Json::CharReaderBuilder rb;
        std::istringstream ss(def.str(0, 1));
        if (!Json::parseFromStream(rb, ss, &nodes, &err)) return 0;

        std::string varsJson = Json::writeString(Json::StreamWriterBuilder(), vars);
        std::string firstNode = nodes[0]["key"].asString();
        db.execParams(
            "INSERT INTO wf_instance(def_id,business_key,title,starter_id,starter_name,"
            "current_node,status,variables) VALUES($1,$2,$3,$4,$5,$6,'running',$7)",
            {std::to_string(defId), businessKey, title,
             std::to_string(starterId), starterName, firstNode, varsJson});

        auto idRes = db.query("SELECT MAX(id) FROM wf_instance");
        if (!idRes.ok() || idRes.rows() == 0) return 0;
        long instId = idRes.longVal(0, 0);

        createNodeTasks(instId, nodes[0]);
        LOG_INFO << "[Flow] instance " << instId << " started: " << defKey
                 << " node=" << firstNode;
        return instId;
    }

    /**
     * @brief 审批通过
     * @return 0=成功 1=任务不存在/已处理 2=无权限 3=实例已结束
     */
    static int approve(long taskId, long userId, const std::string& comment,
                       const Json::Value& appendVars = Json::Value()) {
        return doTask(taskId, userId, comment, true, appendVars);
    }

    /// 审批驳回
    static int reject(long taskId, long userId, const std::string& comment) {
        return doTask(taskId, userId, comment, false, Json::Value());
    }

    /// 撤销实例（发起人）
    static bool cancel(long instId, long userId) {
        auto& db = DatabaseService::instance();
        auto res = db.queryParams(
            "SELECT starter_id,status FROM wf_instance WHERE id=$1",
            {std::to_string(instId)});
        if (!res.ok() || res.rows() == 0) return false;
        if (res.longVal(0, 0) != userId || res.str(0, 1) != "running") return false;
        db.execParams("UPDATE wf_instance SET status='canceled',end_time=CURRENT_TIMESTAMP "
                      "WHERE id=$1", {std::to_string(instId)});
        db.execParams("UPDATE wf_task SET status='skipped',finish_time=CURRENT_TIMESTAMP "
                      "WHERE instance_id=$1 AND status='pending'", {std::to_string(instId)});
        return true;
    }

private:
    /// 为节点生成审批任务 + 站内通知
    static void createNodeTasks(long instId, const Json::Value& node) {
        auto& db = DatabaseService::instance();
        std::string nodeKey  = node["key"].asString();
        std::string nodeName = node["name"].asString();
        for (auto& a : node["assignees"]) {
            long uid = a.asInt64();
            db.execParams(
                "INSERT INTO wf_task(instance_id,node_key,node_name,assignee_id,status) "
                "VALUES($1,$2,$3,$4,'pending')",
                {std::to_string(instId), nodeKey, nodeName, std::to_string(uid)});
            // 站内通知审批人
            auto inst = db.queryParams(
                "SELECT title,starter_name FROM wf_instance WHERE id=$1",
                {std::to_string(instId)});
            if (inst.ok() && inst.rows() > 0) {
                NotifyService::sendInbox(uid,
                    "待办审批: " + inst.str(0, 0),
                    inst.str(0, 1) + " 提交的「" + inst.str(0, 0) + "」待您审批（" + nodeName + "）",
                    "warning");
            }
        }
    }

    /// 处理审批任务（通过/驳回共用）
    static int doTask(long taskId, long userId, const std::string& comment,
                      bool pass, const Json::Value& appendVars) {
        auto& db = DatabaseService::instance();
        auto t = db.queryParams(
            "SELECT instance_id,node_key,assignee_id,status FROM wf_task WHERE id=$1",
            {std::to_string(taskId)});
        if (!t.ok() || t.rows() == 0) return 1;
        long instId = t.longVal(0, 0);
        std::string nodeKey = t.str(0, 1);
        if (t.longVal(0, 2) != userId) return 2;
        if (t.str(0, 3) != "pending") return 1;

        auto inst = db.queryParams(
            "SELECT def_id,status,variables,starter_id,title FROM wf_instance WHERE id=$1",
            {std::to_string(instId)});
        if (!inst.ok() || inst.rows() == 0 || inst.str(0, 1) != "running") return 3;

        // 更新任务状态
        db.execParams(
            "UPDATE wf_task SET status=$1,comment=$2,finish_time=CURRENT_TIMESTAMP WHERE id=$3",
            {pass ? "approved" : "rejected", comment, std::to_string(taskId)});

        // 追加流程变量
        if (!appendVars.empty()) {
            Json::Value vars; std::string err;
            Json::CharReaderBuilder rb;
            std::istringstream ss(inst.str(0, 2));
            if (Json::parseFromStream(rb, ss, &vars, &err))
                for (auto& k : appendVars.getMemberNames()) vars[k] = appendVars[k];
            db.execParams("UPDATE wf_instance SET variables=$1 WHERE id=$2",
                {Json::writeString(Json::StreamWriterBuilder(), vars),
                 std::to_string(instId)});
        }

        if (!pass) {
            // 任一拒绝 → 实例驳回，同节点其余任务跳过
            db.execParams("UPDATE wf_instance SET status='rejected',end_time=CURRENT_TIMESTAMP "
                          "WHERE id=$1", {std::to_string(instId)});
            db.execParams("UPDATE wf_task SET status='skipped' WHERE instance_id=$1 "
                          "AND status='pending'", {std::to_string(instId)});
            notifyStarter(instId, "rejected");
            return 0;
        }

        // 查节点 mode 和剩余 pending 任务
        auto def = db.queryParams("SELECT nodes FROM wf_definition WHERE id=$1",
                                  {std::to_string(inst.longVal(0, 0))});
        if (!def.ok() || def.rows() == 0) return 0;
        Json::Value nodes; std::string err;
        Json::CharReaderBuilder rb;
        std::istringstream nss(def.str(0, 0));
        if (!Json::parseFromStream(rb, nss, &nodes, &err)) return 0;

        int nodeIdx = -1; std::string mode = "any";
        for (int i = 0; i < (int)nodes.size(); ++i)
            if (nodes[i]["key"].asString() == nodeKey) {
                nodeIdx = i; mode = nodes[i].get("mode", "any").asString(); break;
            }
        if (nodeIdx < 0) return 0;

        auto pending = db.queryParams(
            "SELECT COUNT(*) FROM wf_task WHERE instance_id=$1 AND node_key=$2 AND status='pending'",
            {std::to_string(instId), nodeKey});
        long remain = (pending.ok() && pending.rows() > 0) ? pending.longVal(0, 0) : 0;

        if (mode == "any" || remain == 0) {
            // 或签一人通过 / 会签全部通过 → 同节点其余 pending 跳过，推进下一节点
            db.execParams("UPDATE wf_task SET status='skipped' WHERE instance_id=$1 "
                          "AND node_key=$2 AND status='pending'",
                          {std::to_string(instId), nodeKey});
            if (nodeIdx + 1 < (int)nodes.size()) {
                std::string nextKey = nodes[nodeIdx + 1]["key"].asString();
                db.execParams("UPDATE wf_instance SET current_node=$1 WHERE id=$2",
                              {nextKey, std::to_string(instId)});
                createNodeTasks(instId, nodes[nodeIdx + 1]);
            } else {
                db.execParams("UPDATE wf_instance SET status='approved',"
                              "end_time=CURRENT_TIMESTAMP WHERE id=$1",
                              {std::to_string(instId)});
                notifyStarter(instId, "approved");
            }
        }
        return 0;
    }

    /// 通知发起人审批结果
    static void notifyStarter(long instId, const std::string& result) {
        auto& db = DatabaseService::instance();
        auto inst = db.queryParams(
            "SELECT starter_id,title FROM wf_instance WHERE id=$1",
            {std::to_string(instId)});
        if (!inst.ok() || inst.rows() == 0) return;
        NotifyService::sendInbox(inst.longVal(0, 0),
            "审批" + std::string(result == "approved" ? "通过" : "驳回") + ": " + inst.str(0, 1),
            "您的流程「" + inst.str(0, 1) + "」已" +
                (result == "approved" ? "全部通过" : "被驳回"),
            result == "approved" ? "info" : "danger");
    }
};
