/**
 * @file SmsCtrl.h
 * @brief 短信控制器 — 验证码收发 + 模板管理 + 发送日志
 *
 * 功能概述：
 *   - 验证码：发送（限流 60s/手机号）+ 校验（一次性）
 *   - 模板管理：sys_sms_template CRUD（管理员）
 *   - 发送日志：sys_sms_log 分页查询（管理员）
 *
 * API 端点：
 *   - POST /sms/code          发送验证码 {phone, template_code}（公开，限流）
 *   - POST /sms/verify       校验验证码 {phone, code}（公开）
 *   - POST /sms/send         指定模板发送 {phone, template_code, params}（system:sms:send）
 *   - GET  /sms/template/list    模板列表（system:sms:list）
 *   - POST /sms/template         新增模板（system:sms:add）
 *   - PUT  /sms/template         修改模板（system:sms:edit）
 *   - DELETE /sms/template/{id}  删除模板（system:sms:remove）
 *   - GET  /sms/log/list         发送日志（monitor:sms:list）
 */

#pragma once
#include <drogon/HttpController.h>
#include <json/json.h>
#include <unordered_map>
#include <deque>
#include <mutex>
#include <chrono>
#include "../common/AjaxResult.h"
#include "../common/OperLogUtils.h"
#include "../common/PageUtils.h"
#include "../filters/PermFilter.h"
#include "../services/DatabaseService.h"
#include "SmsService.h"

/**
 * @class SmsCtrl
 * @brief 短信控制器
 */
class SmsCtrl : public drogon::HttpController<SmsCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SmsCtrl::sendCode,    "/sms/code",           drogon::Post);
        ADD_METHOD_TO(SmsCtrl::verifyCode,  "/sms/verify",         drogon::Post);
        ADD_METHOD_TO(SmsCtrl::send,        "/sms/send",           drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SmsCtrl::tplList,     "/sms/template/list",  drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SmsCtrl::tplAdd,      "/sms/template",       drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SmsCtrl::tplEdit,     "/sms/template",       drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SmsCtrl::tplDel,      "/sms/template/{id}",  drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(SmsCtrl::logList,     "/sms/log/list",       drogon::Get,    "JwtAuthFilter");
    METHOD_LIST_END

    /// POST /sms/code — 发送验证码（60s/手机号限流）
    void sendCode(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto body = req->getJsonObject();
        if (!body || !(*body).isMember("phone")) { RESP_ERR(cb, "缺少 phone"); return; }
        std::string phone = (*body)["phone"].asString();
        std::string tpl   = body->get("template_code", "").asString();
        if (phone.size() < 7 || phone.size() > 15) { RESP_ERR(cb, "手机号格式错误"); return; }

        // 60s 重发限流（内存滑动窗口）
        {
            std::lock_guard<std::mutex> lk(rateMu_);
            auto now = std::chrono::steady_clock::now();
            auto& rec = rateMap_[phone];
            while (!rec.empty() && now - rec.front() > std::chrono::seconds(60))
                rec.pop_front();
            if (!rec.empty()) { RESP_ERR(cb, "发送过于频繁，请60秒后重试"); return; }
            rec.push_back(now);
        }

        if (tpl.empty()) tpl = defaultCodeTemplate();
        if (tpl.empty()) { RESP_ERR(cb, "未配置验证码模板"); return; }
        SmsService::instance().sendCode(phone, tpl);
        RESP_MSG(cb, "验证码已发送");
    }

    /// POST /sms/verify — 校验验证码
    void verifyCode(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto body = req->getJsonObject();
        if (!body || !(*body).isMember("phone") || !(*body).isMember("code")) {
            RESP_ERR(cb, "缺少 phone/code"); return;
        }
        bool ok = SmsService::instance().verifyCode(
            (*body)["phone"].asString(), (*body)["code"].asString());
        if (!ok) { RESP_ERR(cb, "验证码错误或已过期"); return; }
        RESP_MSG(cb, "验证通过");
    }

    /// POST /sms/send — 指定模板发送（管理员）
    void send(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:sms:send");
        auto body = req->getJsonObject();
        if (!body || !(*body).isMember("phone") || !(*body).isMember("template_code")) {
            RESP_ERR(cb, "缺少 phone/template_code"); return;
        }
        std::string params = body->isMember("params")
            ? Json::writeString(Json::StreamWriterBuilder(), (*body)["params"]) : "{}";
        SmsService::instance().send((*body)["phone"].asString(),
                                    (*body)["template_code"].asString(), params);
        LOG_OPER(req, "短信发送", BusinessType::OTHER);
        RESP_MSG(cb, "已提交发送");
    }

    // ── 模板管理 ──────────────────────────────────────────────────────

    void tplList(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:sms:list");
        auto res = DatabaseService::instance().query(
            "SELECT id,name,provider,template_code,sign_name,content,status,create_time "
            "FROM sys_sms_template ORDER BY id DESC");
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["id"]            = (Json::Int64)res.longVal(i, 0);
            j["name"]          = res.str(i, 1);
            j["provider"]      = res.str(i, 2);
            j["template_code"] = res.str(i, 3);
            j["sign_name"]     = res.str(i, 4);
            j["content"]       = res.str(i, 5);
            j["status"]        = res.str(i, 6);
            j["create_time"]   = res.str(i, 7);
            rows.append(j);
        }
        RESP_OK(cb, rows);
    }

    void tplAdd(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:sms:add");
        auto b = req->getJsonObject();
        if (!b || !(*b).isMember("template_code")) { RESP_ERR(cb, "缺少 template_code"); return; }
        DatabaseService::instance().execParams(
            "INSERT INTO sys_sms_template(name,provider,template_code,sign_name,content,status) "
            "VALUES($1,$2,$3,$4,$5,$6)",
            {b->get("name","").asString(), b->get("provider","aliyun").asString(),
             (*b)["template_code"].asString(), b->get("sign_name","").asString(),
             b->get("content","").asString(), b->get("status","0").asString()});
        LOG_OPER(req, "短信模板", BusinessType::INSERT);
        RESP_MSG(cb, "添加成功");
    }

    void tplEdit(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:sms:edit");
        auto b = req->getJsonObject();
        if (!b || !(*b).isMember("id")) { RESP_ERR(cb, "缺少 id"); return; }
        DatabaseService::instance().execParams(
            "UPDATE sys_sms_template SET name=$1,provider=$2,template_code=$3,"
            "sign_name=$4,content=$5,status=$6 WHERE id=$7",
            {b->get("name","").asString(), b->get("provider","aliyun").asString(),
             b->get("template_code","").asString(), b->get("sign_name","").asString(),
             b->get("content","").asString(), b->get("status","0").asString(),
             std::to_string((*b)["id"].asInt64())});
        LOG_OPER(req, "短信模板", BusinessType::UPDATE);
        RESP_MSG(cb, "修改成功");
    }

    void tplDel(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                long id) {
        CHECK_PERM(req, cb, "system:sms:remove");
        DatabaseService::instance().execParams(
            "DELETE FROM sys_sms_template WHERE id=$1", {std::to_string(id)});
        LOG_OPER_PARAM(req, "短信模板", BusinessType::REMOVE, std::to_string(id));
        RESP_MSG(cb, "删除成功");
    }

    /// GET /sms/log/list — 发送日志分页
    void logList(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:sms:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        auto cnt = db.query("SELECT COUNT(*) FROM sys_sms_log");
        long total = (cnt.ok() && cnt.rows() > 0) ? cnt.longVal(0, 0) : 0;
        auto res = db.queryParams(
            "SELECT id,phone,template_code,provider,status,response,create_time "
            "FROM sys_sms_log ORDER BY id DESC LIMIT $1 OFFSET $2",
            {std::to_string(page.pageSize), std::to_string(page.offset())});
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["id"]            = (Json::Int64)res.longVal(i, 0);
            j["phone"]         = res.str(i, 1);
            j["template_code"] = res.str(i, 2);
            j["provider"]      = res.str(i, 3);
            j["status"]        = res.str(i, 4);
            j["response"]      = res.str(i, 5);
            j["create_time"]   = res.str(i, 6);
            rows.append(j);
        }
        Json::Value data;
        data["total"] = (Json::Int64)total;
        data["rows"]  = rows;
        RESP_OK(cb, data);
    }

private:
    /// 取默认验证码模板（status=0 且 name 含"验证码"的第一条）
    static std::string defaultCodeTemplate() {
        auto res = DatabaseService::instance().query(
            "SELECT template_code FROM sys_sms_template "
            "WHERE status='0' AND name LIKE '%验证码%' ORDER BY id LIMIT 1");
        if (res.ok() && res.rows() > 0) return res.str(0, 0);
        // 兜底：第一条启用模板
        res = DatabaseService::instance().query(
            "SELECT template_code FROM sys_sms_template WHERE status='0' ORDER BY id LIMIT 1");
        return (res.ok() && res.rows() > 0) ? res.str(0, 0) : "";
    }

    std::unordered_map<std::string, std::deque<std::chrono::steady_clock::time_point>> rateMap_;
    std::mutex rateMu_;
};
