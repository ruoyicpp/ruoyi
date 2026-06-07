/**
 * @file UnlockScreenCtrl.h
 * @brief 锁屏解锁控制器
 * 
 * 功能概述：
 *   - 锁屏验证：验证用户密码以解锁锁屏状态
 *   - 会话保持：解锁时不重新签发 token，保持现有会话
 *   - 安全验证：使用 bcrypt 验证密码，防止暴力破解
 * 
 * API 端点：
 *   - POST /unlockscreen - 解锁锁屏（需要提供密码）
 * 
 * 请求格式：
 *   {
 *     "password": "用户密码"
 *   }
 * 
 * 响应格式：
 *   成功：{"code": 200, "msg": "解锁成功"}
 *   失败：{"code": 401, "msg": "密码错误"}
 * 
 * 核心特性：
 *   - JWT 认证：必须提供有效的 JWT token
 *   - 密码验证：使用 bcrypt 验证用户密码
 *   - 会话保持：解锁时不刷新 token，保持现有会话
 *   - 本地状态：前端仅切换本地锁屏状态，后端不维护锁屏状态
 * 
 * 使用场景：
 *   - 屏幕保护：用户离开时锁定屏幕
 *   - 安全保护：防止他人未经授权使用用户账号
 *   - 会话保持：解锁时保持现有会话，无需重新登录
 * 
 * 工作流程：
 *   1. 用户在前端点击"锁屏"，前端记录锁屏状态
 *   2. 用户返回时输入密码，前端调用 /unlockscreen
 *   3. 后端验证 JWT token 有效性
 *   4. 后端验证用户密码
 *   5. 验证成功则返回 200，前端解除锁屏状态
 *   6. 验证失败则返回 401，前端提示"密码错误"
 */
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/SecurityUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"
#include <json/json.h>

class UnlockScreenCtrl : public drogon::HttpController<UnlockScreenCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(UnlockScreenCtrl::unlock, "/unlockscreen", drogon::Post, "JwtAuthFilter");
    METHOD_LIST_END

    void unlock(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        long userId = GET_USER_ID(req);
        if (userId <= 0) { RESP_401(cb); return; }
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
        std::string password = body->get("password", "").asString();
        if (password.empty()) { RESP_ERR(cb, "密码不能为空"); return; }

        auto& db = DatabaseService::instance();
        auto res = db.queryParams(
            "SELECT password FROM sys_user WHERE user_id=$1 AND del_flag='0'",
            {std::to_string(userId)});
        if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "用户不存在"); return; }
        std::string encoded = res.str(0, 0);
        if (!SecurityUtils::matchesPassword(password, encoded)) {
            // 401 让前端 catch 分支展示 "密码错误，请重试"
            (cb)(drogon::HttpResponse::newHttpJsonResponse(AjaxResult::error(401, "密码错误")));
            return;
        }
        RESP_MSG(cb, "解锁成功");
    }
};
