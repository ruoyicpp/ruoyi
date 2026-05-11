#pragma once
/**
 * UnlockScreenCtrl —— 锁屏解锁
 *
 * 端点：POST /unlockscreen   body: {"password": "xxx"}
 *
 * 行为：
 *   1) 当前 JWT 必须有效（JwtAuthFilter 拦截非法 token）
 *   2) 取当前用户的 sys_user.password 哈希
 *   3) 用 SecurityUtils::matchesPassword 校验
 *   4) 通过即返 success；不通过返 401（前端会显示 "密码错误"）
 *
 * 不做：不重新签发 token、不刷新会话——前端 store/lock 仅切本地状态。
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
