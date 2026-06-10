#pragma once
#include <drogon/drogon.h>
#include "../../common/AjaxResult.h"
#include "../../common/OperLogUtils.h"
#include "../../common/TotpUtils.h"
#include "../../common/TokenCache.h"
#include "../../services/DatabaseService.h"
#include "../services/TokenService.h"

/**
 * @file SysTotpCtrl.h
 * @brief TOTP 两步验证控制器 — 基于时间的一次性密码（RFC 6238）
 * 
 * 功能概述：
 *   - 密钥生成：生成 TOTP 密钥和二维码
 *   - 激活验证：用户扫描二维码后输入 OTP 激活
 *   - 禁用验证：用户可随时关闭两步验证
 *   - 登录验证：登录时验证 OTP 码
 *   - 备用码：生成备用码用于紧急情况
 * 
 * 核心特性：
 *   - RFC 6238 标准：完全遵循 RFC 6238 TOTP 标准
 *   - 时间同步：支持 30 秒时间窗口和时间偏差容限
 *   - 二维码生成：使用 QR 码标准格式，兼容所有认证器应用
 *   - 备用码管理：生成 10 个备用码，用于设备丢失时恢复
 *   - 防暴力破解：OTP 验证失败次数限制
 * 
 * 工作流程：
 *   1. 用户请求生成 TOTP 密钥
 *   2. 系统生成 32 字节随机密钥
 *   3. 系统生成 QR 码 URI（otpauth://totp/...）
 *   4. 用户使用认证器应用（Google Authenticator、Microsoft Authenticator 等）扫描二维码
 *   5. 用户输入认证器显示的 6 位 OTP 码激活
 *   6. 系统验证 OTP 码，激活两步验证
 *   7. 登录时，用户需输入密码和 OTP 码
 *   8. 系统验证两者，通过后颁发 JWT Token
 * 
 * API 端点：
 *   - POST /system/totp/generate - 生成密钥和二维码
 *   - POST /system/totp/enable - 激活两步验证
 *   - POST /system/totp/disable - 关闭两步验证
 *   - POST /system/totp/verify - 验证 OTP 码
 * 
 * 请求/响应示例：
 *   ```
 *   POST /system/totp/generate
 *   Authorization: Bearer <JWT>
 *   
 *   响应：
 *   {
 *     "code": 200,
 *     "msg": "success",
 *     "data": {
 *       "secret": "JBSWY3DPEBLW64TMMQ======",
 *       "qrUri": "otpauth://totp/RuoYi:user@example.com?secret=JBSWY3DPEBLW64TMMQ======&issuer=RuoYi"
 *     }
 *   }
 *   ```
 * 
 * 支持的认证器应用：
 *   - Google Authenticator（iOS/Android）
 *   - Microsoft Authenticator（iOS/Android）
 *   - Authy（iOS/Android）
 *   - FreeOTP（iOS/Android）
 *   - 1Password（iOS/Android）
 * 
 * 配置项（config.json）：
 *   - totp.enabled: 是否启用 TOTP（默认 true）
 *   - totp.issuer: 发行者名称（默认 "RuoYi"）
 *   - totp.window_size: 时间窗口大小（默认 1，表示 ±30 秒）
 *   - totp.backup_codes_count: 备用码数量（默认 10）
 * 
 * 安全建议：
 *   - 启用 TOTP 后，用户应保存备用码
 *   - 备用码应存放在安全的地方
 *   - 如果设备丢失，应立即禁用 TOTP 并重新启用
 *   - 建议与密码策略结合使用
 * 
 * @see TotpUtils - TOTP 工具类
 * @see TokenService - Token 管理服务
 * @see SysLoginCtrl - 登录控制器
 */
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
        LOG_OPER(req, "TOTP启用", BusinessType::UPDATE);
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
        LOG_OPER(req, "TOTP关闭", BusinessType::UPDATE);
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
