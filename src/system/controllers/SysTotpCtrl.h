#pragma once
#include <drogon/drogon.h>
#include "../../common/AjaxResult.h"
#include "../../common/TotpUtils.h"
#include "../../common/TokenCache.h"
#include "../../services/DatabaseService.h"
#include "../services/TokenService.h"

// TOTP 两步验证接口
// POST /system/totp/generate  — 生成密钥+二维码URI（未激活）
// POST /system/totp/enable    — 输入6位OTP激活
// POST /system/totp/disable   — 关闭两步验证
// POST /system/totp/verify    — 登录时校验OTP
class SysTotpCtrl : public drogon::HttpController<SysTotpCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysTotpCtrl::generate, "/system/totp/generate", drogon::Post);
        ADD_METHOD_TO(SysTotpCtrl::enable,   "/system/totp/enable",   drogon::Post);
        ADD_METHOD_TO(SysTotpCtrl::disable,  "/system/totp/disable",  drogon::Post);
        ADD_METHOD_TO(SysTotpCtrl::verify,   "/system/totp/verify",   drogon::Post);
    METHOD_LIST_END

    // 生成密钥，返回 qrUri 供前端渲染二维码
    void generate(const drogon::HttpRequestPtr &req,
                  std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }

        std::string secret = TotpUtils::generateSecret();
        std::string uri    = TotpUtils::otpauthUri(secret, user->userName);

        // 临时存储密钥（未激活）
        auto &db = DatabaseService::instance();
        db.execParams("UPDATE sys_user SET totp_secret_tmp=$1 WHERE user_id=$2",
                      {secret, std::to_string(user->userId)});

        Json::Value data;
        data["secret"] = secret;
        data["qrUri"]  = uri;
        Json::Value r  = AjaxResult::success();
        r["data"]      = data;
        RESP_JSON(cb, r);
    }

    // 激活：验证OTP后将 totp_secret_tmp 写入 totp_secret，enabled=1
    void enable(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "参数错误"); return; }
        std::string otp = (*body).get("otp", "").asString();

        auto &db = DatabaseService::instance();
        auto rs  = db.queryParams("SELECT totp_secret_tmp FROM sys_user WHERE user_id=$1",
                                  {std::to_string(user->userId)});
        if (rs.rows() == 0 || rs.str(0,0).empty()) { RESP_ERR(cb, "请先调用generate"); return; }
        std::string secret = rs.str(0,0);

        if (!TotpUtils::verify(secret, otp)) { RESP_ERR(cb, "OTP验证失败"); return; }

        db.execParams("UPDATE sys_user SET totp_secret=$1, totp_enabled=1, totp_secret_tmp='' WHERE user_id=$2",
                      std::vector<std::string>{secret, std::to_string(user->userId)});
        RESP_MSG(cb, "操作成功");
    }

    // 关闭两步验证
    void disable(const drogon::HttpRequestPtr &req,
                 std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        DatabaseService::instance().execParams(
            "UPDATE sys_user SET totp_secret='', totp_enabled=0 WHERE user_id=$1",
            std::vector<std::string>{std::to_string(user->userId)});
        RESP_MSG(cb, "操作成功");
    }

    // 登录时校验 OTP（由 SysLoginCtrl 内部调用或独立接口）
    void verify(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto user = TokenService::instance().getLoginUser(req);
        if (!user) { RESP_401(cb); return; }
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "参数错误"); return; }
        std::string otp = (*body).get("otp", "").asString();

        auto rs = DatabaseService::instance().queryParams(
            "SELECT totp_secret FROM sys_user WHERE user_id=$1",
            {std::to_string(user->userId)});
        if (rs.rows() == 0) { RESP_ERR(cb, "用户不存在"); return; }
        std::string secret = rs.str(0,0);
        if (secret.empty()) { RESP_ERR(cb, "未启用TOTP"); return; }

        if (!TotpUtils::verify(secret, otp)) { RESP_ERR(cb, "OTP验证失败"); return; }
        RESP_MSG(cb, "验证成功");
    }
};
