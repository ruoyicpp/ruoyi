#pragma once
#include <drogon/HttpMiddleware.h>
#include "../common/JwtUtils.h"
#include "../common/TokenCache.h"
#include "../common/SecurityUtils.h"
#include "../common/AjaxResult.h"

// JWT ��֤�м��
class JwtAuthFilter : public drogon::HttpMiddleware<JwtAuthFilter> {
public:
    // ��ֹ Drogon �Զ���������Ϊ�ֶ�ע��
    static constexpr bool isAutoCreation = false;

    void invoke(const drogon::HttpRequestPtr &req,
                drogon::MiddlewareNextCallback &&nextCb,
                drogon::MiddlewareCallback &&mcb) override {
        auto token = SecurityUtils::getToken(req);
        if (token.empty()) {
            mcb(drogon::HttpResponse::newHttpJsonResponse(
                AjaxResult::error(401, "����δЯ��token���޷�����ϵͳ��Դ")));
            return;
        }

        try {
            auto uuid    = JwtUtils::parseUuid(token);
            auto userKey = SecurityUtils::getTokenKey(uuid);
            auto userOpt = TokenCache::instance().get(userKey);

            if (!userOpt) {
                mcb(drogon::HttpResponse::newHttpJsonResponse(
                    AjaxResult::error(401, "��¼״̬�ѹ��ڣ������µ�¼")));
                return;
            }

            // �Զ�ˢ�£�ʣ�಻��20����ʱ����
            auto &user = *userOpt;
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            long remaining = user.expireTime - now;
            if (remaining > 0 && remaining < 20LL * 60 * 1000) {
                auto &cfg = JwtUtils::config();
                user.expireTime = now + (long long)cfg.expireMinutes * 60 * 1000;
                TokenCache::instance().update(userKey, user);
            }

            // �� userId ��ע���������ԣ��� Controller ʹ��
            req->getAttributes()->insert("userId",   user.userId);
            req->getAttributes()->insert("deptId",   user.deptId);
            req->getAttributes()->insert("userName", user.userName);
            req->getAttributes()->insert("uuid",     uuid);
            req->getAttributes()->insert("loginUser", *userOpt);

            nextCb(std::move(mcb));
        } catch (const std::exception &e) {
            mcb(drogon::HttpResponse::newHttpJsonResponse(
                AjaxResult::error(401, std::string("token�Ƿ���") + e.what())));
        }
    }
};
