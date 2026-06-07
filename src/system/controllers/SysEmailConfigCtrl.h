/**
 * @file SysEmailConfigCtrl.h
 * @brief 邮件配置管理控制器
 * 
 * 功能概述：
 *   - 邮件配置查询：读取当前邮件发送配置
 *   - 邮件配置保存：保存和更新邮件配置
 *   - 测试邮件发送：发送测试邮件验证配置
 *   - 发件人管理：管理多个邮箱发件人
 * 
 * API 端点：
 *   - GET  /system/emailConfig       - 读取邮件配置
 *   - POST /system/emailConfig       - 保存邮件配置
 *   - POST /system/emailConfig/test  - 发送测试邮件
 * 
 * 权限要求：
 *   - system:emailConfig:query - 查看邮件配置
 *   - system:emailConfig:edit  - 修改邮件配置
 * 
 * 配置项（sys_config）：
 *   - sys.email.host: SMTP 服务器地址（如 "smtp.qq.com"）
 *   - sys.email.port: SMTP 端口（通常 465 或 587）
 *   - sys.email.fromName: 发件人显示名称（如 "系统通知"）
 *   - sys.email.senders: 发件人列表（JSON 数组）
 *     格式：[{"email":"user@qq.com","authCode":"xxxx"}, ...]
 * 
 * 核心特性：
 *   - 多发件人：支持配置多个邮箱发件人
 *   - 轮询发送：多个发件人时自动轮询
 *   - 安全脱敏：authCode 脱敏，只返回前 2 位 + ***
 *   - 测试邮件：支持发送测试邮件验证配置
 *   - 动态加载：配置修改后自动生效
 * 
 * 使用场景：
 *   - 系统通知：发送系统通知邮件
 *   - 用户邮件：发送用户相关邮件（注册、重置密码等）
 *   - 告警邮件：发送系统告警邮件
 *   - 定时报告：发送定时报告邮件
 * 
 * 邮件发件人配置示例：
 *   {
 *     "host": "smtp.qq.com",
 *     "port": "465",
 *     "fromName": "系统通知",
 *     "senders": [
 *       {
 *         "email": "noreply@qq.com",
 *         "authCode": "xxxxxxxxxxxx"
 *       },
 *       {
 *         "email": "support@qq.com",
 *         "authCode": "xxxxxxxxxxxx"
 *       }
 *     ]
 *   }
 */

#pragma once
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/OperLogUtils.h"
#include "../../common/SecurityUtils.h"
#include "../../common/SmtpUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"

/**
 * @class SysEmailConfigCtrl
 * @brief 邮件配置管理控制器
 * 
 * 对应 RuoYi-Vue 中的 SysEmailConfigController，提供邮件配置管理的所有 API 端点。
 * 所有操作都需要 JWT 认证，并根据权限字符串进行权限检查。
 */
class SysEmailConfigCtrl : public drogon::HttpController<SysEmailConfigCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysEmailConfigCtrl::get,  "/system/emailConfig",       drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(SysEmailConfigCtrl::save, "/system/emailConfig",       drogon::Post, "JwtAuthFilter");
        ADD_METHOD_TO(SysEmailConfigCtrl::test, "/system/emailConfig/test",  drogon::Post, "JwtAuthFilter");
    METHOD_LIST_END

    // 读取配置（authCode 脱敏，只返回前2位+***）
    void get(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:emailConfig:query");
        auto& db = DatabaseService::instance();

        auto cfgRow = [&](const std::string& key, const std::string& def) -> std::string {
            auto r = db.queryParams(
                "SELECT config_value FROM sys_config WHERE config_key=$1 LIMIT 1", {key});
            return (r.ok() && r.rows() > 0) ? r.str(0,0) : def;
        };

        Json::Value data;
        data["host"]     = cfgRow("sys.email.host",     "smtp.qq.com");
        data["port"]     = cfgRow("sys.email.port",     "465");
        data["fromName"] = cfgRow("sys.email.fromName", "系统通知");

        // 解析发件人列表，脱敏 authCode
        std::string sendersJson = cfgRow("sys.email.senders", "[]");
        Json::Value raw; Json::Reader r;
        if (r.parse(sendersJson, raw) && raw.isArray()) {
            Json::Value arr(Json::arrayValue);
            for (auto& s : raw) {
                Json::Value item;
                item["email"] = s.get("email","").asString();
                std::string code = s.get("authCode","").asString();
                item["authCode"] = code.size() > 2
                    ? code.substr(0,2) + std::string(code.size()-2, '*') : "****";
                item["masked"] = true;
                arr.append(item);
            }
            data["senders"] = arr;
        } else {
            data["senders"] = Json::Value(Json::arrayValue);
        }
        RESP_OK(cb, data);
    }

    // 保存全量配置（senders 中 authCode 为 "***" 样式时保留旧值）
    void save(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:emailConfig:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }

        auto& db = DatabaseService::instance();

        std::string host     = (*body).get("host",     "smtp.qq.com").asString();
        std::string port     = (*body).get("port",     "465").asString();
        std::string fromName = (*body).get("fromName", "系统通知").asString();

        // 读取旧的 senders，用于保留被脱敏的 authCode
        auto oldRow = db.queryParams(
            "SELECT config_value FROM sys_config WHERE config_key='sys.email.senders' LIMIT 1", {});
        Json::Value oldSenders(Json::arrayValue);
        if (oldRow.ok() && oldRow.rows() > 0) {
            Json::Reader rd; rd.parse(oldRow.str(0,0), oldSenders);
        }
        auto findOldCode = [&](const std::string& email) -> std::string {
            for (auto& s : oldSenders)
                if (s.get("email","").asString() == email)
                    return s.get("authCode","").asString();
            return "";
        };

        // 合并新 senders
        Json::Value newSenders(Json::arrayValue);
        if ((*body).isMember("senders") && (*body)["senders"].isArray()) {
            for (auto& s : (*body)["senders"]) {
                std::string email = s.get("email","").asString();
                if (email.empty()) continue;
                std::string code = s.get("authCode","").asString();
                // 全是 * 说明是脱敏值，保留旧的
                bool isRedacted = !code.empty() &&
                    code.find_first_not_of('*') == std::string::npos;
                if (isRedacted) code = findOldCode(email);
                Json::Value item;
                item["email"]    = email;
                item["authCode"] = code;
                newSenders.append(item);
            }
        }

        Json::FastWriter fw;
        auto upsert = [&](const std::string& key, const std::string& val) {
            auto ex = db.queryParams(
                "SELECT config_id FROM sys_config WHERE config_key=$1 LIMIT 1", {key});
            if (ex.ok() && ex.rows() > 0) {
                db.execParams(
                    "UPDATE sys_config SET config_value=$1,update_time=NOW() WHERE config_key=$2",
                    {val, key});
            } else {
                db.execParams(
                    "INSERT INTO sys_config(config_name,config_key,config_value,config_type,create_by,create_time) "
                    "VALUES($1,$2,$3,'Y','admin',NOW())",
                    {key, key, val});
            }
        };
        upsert("sys.email.host",     host);
        upsert("sys.email.port",     port);
        upsert("sys.email.fromName", fromName);
        upsert("sys.email.senders",  fw.write(newSenders));

        // 同步更新内存中的 SmtpUtils 配置
        reloadSmtp();
        LOG_OPER(req, "邮件配置", BusinessType::UPDATE);
        RESP_MSG(cb, "保存成功");
    }

    // 发送测试邮件
    void test(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:emailConfig:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
        std::string to = (*body).get("to","").asString();
        if (to.empty()) { RESP_ERR(cb, "收件人邮箱不能为空"); return; }

        reloadSmtp();
        if (!SmtpUtils::instance().isConfigured()) {
            RESP_ERR(cb, "邮件未配置，请先添加发件箱"); return;
        }
        bool ok = SmtpUtils::instance().send(to, "测试邮件 - RuoYi系统",
            "这是一封来自 RuoYi-Cpp 系统的测试邮件，如收到请忽略。");
        OperLogUtils::write(req, "邮件配置", BusinessType::OTHER, "to=" + to,
                            ok ? 0 : 1, ok ? "send ok" : "send failed",
                            ok ? "" : "smtp send failed");
        if (ok) RESP_MSG(cb, "测试邮件发送成功，请查收");
        else    RESP_ERR(cb, "发送失败，请检查发件箱配置或查看系统日志");
    }

    // 供其他模块调用：从 DB 重新加载 SMTP 配置到内存
    static void reloadSmtp() {
        auto& db = DatabaseService::instance();
        auto cfgVal = [&](const std::string& key, const std::string& def) {
            auto r = db.queryParams(
                "SELECT config_value FROM sys_config WHERE config_key=$1 LIMIT 1", {key});
            return (r.ok() && r.rows() > 0) ? r.str(0,0) : def;
        };
        std::string host     = cfgVal("sys.email.host",     "smtp.qq.com");
        int         port     = SecurityUtils::parseInt(cfgVal("sys.email.port", "465"), 465);
        std::string fromName = cfgVal("sys.email.fromName", "系统通知");
        std::string sendersJ = cfgVal("sys.email.senders",  "[]");

        std::vector<SmtpUtils::Sender> senders;
        Json::Value arr; Json::Reader rd;
        if (rd.parse(sendersJ, arr) && arr.isArray()) {
            for (auto& s : arr) {
                std::string email = s.get("email","").asString();
                std::string code  = s.get("authCode","").asString();
                if (!email.empty() && !code.empty())
                    senders.push_back({email, code});
            }
        }
        SmtpUtils::instance().loadConfig(host, port, fromName, senders);
    }
};
