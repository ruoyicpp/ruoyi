/**
 * @file CaptchaCtrl.h
 * @brief 行为验证码控制器 — /captcha/get + /captcha/check（anji-plus 惯例）
 *
 * 功能概述：
 *   - POST /captcha/get   获取验证码（slide/rotate，由 config.type 决定）
 *   - POST /captcha/check 校验验证码（一次性，通过即销毁 key）
 *   - 仅 captcha.behavioral.enabled=true 时生效，否则返回 501
 *   - 与旧 /captchaImage（GIF 字符验证码）并存，前端按 captchaType 切换
 *
 * 请求/响应格式（兼容 anji-plus AJ-Captcha 前端组件）：
 *   POST /captcha/get
 *     → { code:200, data:{ captchaType, key, masterImage, tileImage|thumbImage,
 *                          tileX, tileY, tileWidth, tileHeight,
 *                          masterWidth, masterHeight } }
 *   POST /captcha/check  { key, x, y } 或 { key, angle }
 *     → { code:200, data:{ result:true|false } }
 *
 * 安全：
 *   - 限流：同 IP 获取验证码 ≤30 次/分钟（复用 RateLimiter::allowKey）
 *   - 答案存 captcha-server 内存，一次性校验，天然防重放
 *   - 校验接口不返回正确答案，只回 true/false
 */

#pragma once
#include <drogon/drogon.h>
#include <drogon/HttpController.h>
#include <json/json.h>
#include "../common/AjaxResult.h"
#include "../common/RateLimiter.h"
#include "../common/IpUtils.h"
#include "../common/TokenCache.h"   // MemCache 定义在此
#include "CaptchaClient.h"

class CaptchaCtrl : public drogon::HttpController<CaptchaCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(CaptchaCtrl::get,   "/captcha/get",   drogon::Post);
        ADD_METHOD_TO(CaptchaCtrl::check, "/captcha/check", drogon::Post);
    METHOD_LIST_END

    /// POST /captcha/get — 生成行为验证码
    void get(const drogon::HttpRequestPtr& req,
             std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto& cli = CaptchaClient::instance();
        cli.syncFromDb();   // sys_config 参数设置可改 enabled/type（MemCache 缓存，开销可忽略）
        if (!cli.isEnabled()) {
            auto r = drogon::HttpResponse::newHttpResponse();
            r->setStatusCode(drogon::k501NotImplemented);
            r->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            r->setBody(R"({"code":501,"msg":"behavioral captcha not enabled"})");
            cb(r);
            return;
        }
        // 限流：同 IP 30 次/分钟
        std::string ip = IpUtils::getIpAddr(req);
        if (!::RateLimiter::instance().allowKey("captcha:ip:" + ip, 30, 60)) {
            RESP_ERR(cb, "验证码获取过于频繁，请稍后再试");
            return;
        }

        Json::Value data;
        data["captchaType"] = cli.type();

        if (cli.type() == "rotate") {
            CaptchaClient::RotateData rd;
            if (!cli.getRotate(rd)) {
                RESP_ERR(cb, "captcha-server unavailable");
                return;
            }
            data["key"]         = rd.key;
            data["masterImage"] = rd.masterImage;
            data["thumbImage"]  = rd.thumbImage;
        } else {
            CaptchaClient::SlideData sd;
            if (!cli.getSlide(sd)) {
                RESP_ERR(cb, "captcha-server unavailable");
                return;
            }
            data["key"]          = sd.key;
            data["masterImage"]  = sd.masterImage;
            data["tileImage"]    = sd.tileImage;
            data["tileX"]        = sd.tileX;
            data["tileY"]        = sd.tileY;
            data["tileWidth"]    = sd.tileWidth;
            data["tileHeight"]   = sd.tileHeight;
            data["masterWidth"]  = sd.masterWidth;
            data["masterHeight"] = sd.masterHeight;
        }

        Json::Value r = AjaxResult::successMap();
        r["data"] = data;
        RESP_JSON(cb, r);
    }

    /// POST /captcha/check — 校验行为验证码（一次性）
    void check(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto& cli = CaptchaClient::instance();
        cli.syncFromDb();
        if (!cli.isEnabled()) {
            auto r = drogon::HttpResponse::newHttpResponse();
            r->setStatusCode(drogon::k501NotImplemented);
            r->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            r->setBody(R"({"code":501,"msg":"behavioral captcha not enabled"})");
            cb(r);
            return;
        }
        auto body = req->getJsonObject();
        if (!body || !body->isMember("key")) {
            RESP_ERR(cb, "missing key");
            return;
        }
        std::string key = (*body)["key"].asString();
        bool ok = false;

        // 安全取 int：前端传字符串/对象时 asInt() 会抛 LogicError → 500
        auto getInt = [](const Json::Value& b, const char* k) -> int {
            const auto& v = b[k];
            if (v.isInt())    return v.asInt();
            if (v.isDouble()) return (int)v.asDouble();
            return 0;
        };

        if (cli.type() == "rotate") {
            ok = cli.verifyRotate(key, getInt(*body, "angle"));
        } else {
            ok = cli.verifySlide(key, getInt(*body, "x"), getInt(*body, "y"));
        }

        Json::Value r = AjaxResult::successMap();
        r["data"]["result"] = ok;
        if (ok) {
            // 校验通过 → 发一次性登录票据（2分钟），前端以 code=ticket,uuid=bhc 提交登录
            std::string ticket = drogon::utils::getUuid();
            MemCache::instance().setString("bhc:" + ticket, "1", 120);
            r["data"]["ticket"] = ticket;
        }
        RESP_JSON(cb, r);
    }
};
