#pragma once
#include <drogon/HttpController.h>
#include <thread>
#include "../../common/AjaxResult.h"
#include "../../common/OperLogUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/CertManagerDriver.h"

// ACME 证书管理（通过 certmanager 动态库）
//
// GET  /system/cert/list              列出所有证书
// GET  /system/cert/info?certId=xxx   获取证书详情
// GET  /system/cert/dns-providers     列出支持的 DNS 提供商
// GET  /system/cert/status            运行状态（是否已加载 DLL）
// POST /system/cert/obtain            申请新证书（DNS-01，异步）
// POST /system/cert/renew             续期证书（异步）
// POST /system/cert/revoke            吊销证书
// POST /system/cert/renew-check       立即触发一次续期检查（调度线程任务）

class SysCertManagerCtrl : public drogon::HttpController<SysCertManagerCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysCertManagerCtrl::list,        "/system/cert/list",         drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(SysCertManagerCtrl::info,        "/system/cert/info",         drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(SysCertManagerCtrl::dnsProviders,"/system/cert/dns-providers",drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(SysCertManagerCtrl::status,      "/system/cert/status",       drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(SysCertManagerCtrl::obtain,      "/system/cert/obtain",       drogon::Post, "JwtAuthFilter");
        ADD_METHOD_TO(SysCertManagerCtrl::renew,       "/system/cert/renew",        drogon::Post, "JwtAuthFilter");
        ADD_METHOD_TO(SysCertManagerCtrl::revoke,      "/system/cert/revoke",       drogon::Post, "JwtAuthFilter");
        ADD_METHOD_TO(SysCertManagerCtrl::renewCheck,  "/system/cert/renew-check",  drogon::Post, "JwtAuthFilter");
    METHOD_LIST_END

    // ── GET /system/cert/list ──────────────────────────────────────────────
    void list(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:cert:query");
        auto& acme = CertManagerAcme::instance();
        if (!acme.isRunning()) { RESP_ERR(cb, "certmanager 未运行（DLL 未加载）"); return; }
        Json::Value certs = acme.listCertsJson();
        RESP_OK(cb, certs);
    }

    // ── GET /system/cert/info?certId=xxx ──────────────────────────────────
    void info(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:cert:query");
        std::string certId = req->getParameter("certId");
        if (certId.empty()) { RESP_ERR(cb, "certId 参数不能为空"); return; }
        auto& acme = CertManagerAcme::instance();
        if (!acme.isRunning()) { RESP_ERR(cb, "certmanager 未运行"); return; }
        Json::Value v = acme.getCertInfoJson(certId);
        if (v.isNull()) { RESP_ERR(cb, "证书不存在: " + certId); return; }
        RESP_OK(cb, v);
    }

    // ── GET /system/cert/dns-providers ────────────────────────────────────
    void dnsProviders(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:cert:query");
        auto& acme = CertManagerAcme::instance();
        if (!acme.isRunning()) { RESP_ERR(cb, "certmanager 未运行"); return; }
        Json::Value provs = acme.listDNSProvidersJson();
        RESP_OK(cb, provs);
    }

    // ── GET /system/cert/status ───────────────────────────────────────────
    void status(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:cert:query");
        auto& acme = CertManagerAcme::instance();
        Json::Value d;
        d["running"] = acme.isRunning();
        d["version"] = acme.version();
        RESP_OK(cb, d);
    }

    // ── POST /system/cert/obtain ──────────────────────────────────────────
    // Body: { "email":"x@y", "domains":["a.com","*.a.com"],
    //         "dnsProvider":"alidns", "envVars":{"KEY":"val",...},
    //         "keyType":"EC256", "bundle":false }
    void obtain(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:cert:add");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体必须为 JSON"); return; }

        std::string email       = (*body).get("email",       "").asString();
        std::string dnsProvider = (*body).get("dnsProvider", "").asString();
        std::string keyType     = (*body).get("keyType",     "EC256").asString();
        bool        bundle      = (*body).get("bundle",      false).asBool();

        if (email.empty())       { RESP_ERR(cb, "email 不能为空"); return; }
        if (dnsProvider.empty()) { RESP_ERR(cb, "dnsProvider 不能为空"); return; }

        // domains 列表
        std::string domainList;
        if ((*body).isMember("domains") && (*body)["domains"].isArray()) {
            for (auto& d : (*body)["domains"]) {
                if (!domainList.empty()) domainList += ",";
                domainList += d.asString();
            }
        }
        if (domainList.empty()) { RESP_ERR(cb, "domains 不能为空"); return; }

        // envVars → JSON 字符串
        std::string envVarsJson;
        if ((*body).isMember("envVars") && (*body)["envVars"].isObject()) {
            Json::StreamWriterBuilder wb; wb["indentation"] = "";
            envVarsJson = Json::writeString(wb, (*body)["envVars"]);
        }

        auto& acme = CertManagerAcme::instance();
        if (!acme.isRunning()) { RESP_ERR(cb, "certmanager 未运行（DLL 未加载）"); return; }

        LOG_OPER(req, "ACME申请证书", BusinessType::OTHER);

        // 耗时操作：DNS-01 验证 + LE 交互，放到独立线程避免阻塞 drogon
        std::thread([
            cb = std::move(cb), &acme,
            email, domainList, dnsProvider, envVarsJson, keyType, bundle
        ]() mutable {
            Json::Value result = acme.obtainCertJson(
                email, domainList, dnsProvider, envVarsJson, keyType, bundle ? 1 : 0);
            if (result.isNull() || !result.get("success", false).asBool()) {
                std::string err = result.get("error", "申请失败").asString();
                RESP_ERR(cb, err);
            } else {
                RESP_OK(cb, result["data"]);
            }
        }).detach();
    }

    // ── POST /system/cert/renew ───────────────────────────────────────────
    // Body: { "certId": "example.com", "bundle": false }
    void renew(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:cert:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体必须为 JSON"); return; }
        std::string certId = (*body).get("certId", "").asString();
        bool bundle = (*body).get("bundle", false).asBool();
        if (certId.empty()) { RESP_ERR(cb, "certId 不能为空"); return; }

        auto& acme = CertManagerAcme::instance();
        if (!acme.isRunning()) { RESP_ERR(cb, "certmanager 未运行"); return; }

        LOG_OPER(req, "ACME续期证书", BusinessType::OTHER);

        std::thread([cb = std::move(cb), &acme, certId, bundle]() mutable {
            Json::Value result = acme.renewCertJson(certId, bundle ? 1 : 0);
            if (result.isNull() || !result.get("success", false).asBool()) {
                RESP_ERR(cb, result.get("error", "续期失败").asString());
            } else {
                RESP_OK(cb, result["data"]);
            }
        }).detach();
    }

    // ── POST /system/cert/revoke ──────────────────────────────────────────
    // Body: { "certId": "example.com" }
    void revoke(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:cert:remove");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体必须为 JSON"); return; }
        std::string certId = (*body).get("certId", "").asString();
        if (certId.empty()) { RESP_ERR(cb, "certId 不能为空"); return; }

        auto& acme = CertManagerAcme::instance();
        if (!acme.isRunning()) { RESP_ERR(cb, "certmanager 未运行"); return; }

        Json::Value result = acme.revokeCertJson(certId);
        if (result.isNull() || !result.get("success", false).asBool()) {
            RESP_ERR(cb, result.get("error", "吊销失败").asString());
        } else {
            LOG_OPER(req, "ACME吊销证书", BusinessType::REMOVE);
            RESP_MSG(cb, "证书已吊销: " + certId);
        }
    }

    // ── POST /system/cert/renew-check ─────────────────────────────────────
    // 立即触发一次续期检查（同 scheduler 的逻辑）
    void renewCheck(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:cert:edit");
        auto& acme = CertManagerAcme::instance();
        if (!acme.isRunning()) { RESP_ERR(cb, "certmanager 未运行"); return; }

        std::thread([cb = std::move(cb), &acme]() mutable {
            bool ok = acme.checkAndRenewOnce();
            if (ok) RESP_MSG(cb, "续期检查完成");
            else    RESP_ERR(cb, "续期检查失败，请查看日志");
        }).detach();
    }
};
