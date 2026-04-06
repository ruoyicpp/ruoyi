#pragma once
#include <drogon/HttpController.h>
#include <sstream>
#include <algorithm>
#include "../../common/AjaxResult.h"
#include "../../common/RateLimiter.h"
#include "../../common/TokenCache.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"
#include "../../common/OperLogUtils.h"

// IP 封禁管理（速率限制自动封禁 + 系统配置静态黑名单）
class SysIpCtrl : public drogon::HttpController<SysIpCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysIpCtrl::bannedList,    "/monitor/ip/banned",           drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysIpCtrl::unban,         "/monitor/ip/banned/{ip}",      drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(SysIpCtrl::blacklist,     "/monitor/ip/blacklist",        drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysIpCtrl::addBlacklist,  "/monitor/ip/blacklist",        drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SysIpCtrl::delBlacklist,  "/monitor/ip/blacklist/{ip}",   drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    // 动态封禁列表（速率限制自动触发）
    void bannedList(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:ip:list");
        auto list = RateLimiter::instance().bannedList();
        Json::Value arr(Json::arrayValue);
        for (auto& e : list) {
            Json::Value j;
            j["ip"]          = e.ip;
            j["remainSecs"]  = (Json::Int64)e.remainSecs;
            j["banCount"]    = e.banCount;
            arr.append(j);
        }
        RESP_OK(cb, arr);
    }

    // 手动解封（速率限制层）
    void unban(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb,
               const std::string& ip) {
        CHECK_PERM(req, cb, "monitor:ip:unban");
        RateLimiter::instance().unban(ip);
        LOG_OPER_PARAM(req, "IP封禁管理", BusinessType::CLEAN, ip);
        RESP_MSG(cb, "解封成功");
    }

    // 静态黑名单（sys_config: sys.login.blackIPList，逗号分隔）
    void blacklist(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:ip:list");
        std::string raw = getBlacklistRaw();
        Json::Value arr(Json::arrayValue);
        if (!raw.empty()) {
            std::istringstream ss(raw);
            std::string token;
            while (std::getline(ss, token, ',')) {
                auto ip = trim(token);
                if (!ip.empty()) arr.append(ip);
            }
        }
        RESP_OK(cb, arr);
    }

    // 添加到静态黑名单
    void addBlacklist(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:ip:edit");
        auto body = req->getJsonObject();
        if (!body || !(*body).isMember("ip")) { RESP_ERR(cb, "缺少 ip 参数"); return; }
        std::string newIp = trim((*body)["ip"].asString());
        if (newIp.empty()) { RESP_ERR(cb, "ip 不能为空"); return; }

        std::string raw = getBlacklistRaw();
        // 检查是否已存在
        auto parts = split(raw);
        for (auto& p : parts) if (p == newIp) { RESP_ERR(cb, "该 IP 已在黑名单中"); return; }

        parts.push_back(newIp);
        std::string newVal = join(parts);
        saveBlacklist(newVal);
        LOG_OPER_PARAM(req, "IP黑名单", BusinessType::INSERT, newIp);
        RESP_MSG(cb, "添加成功");
    }

    // 从静态黑名单删除
    void delBlacklist(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                      const std::string& ip) {
        CHECK_PERM(req, cb, "monitor:ip:edit");
        std::string raw = getBlacklistRaw();
        auto parts = split(raw);
        parts.erase(std::remove(parts.begin(), parts.end(), ip), parts.end());
        saveBlacklist(join(parts));
        LOG_OPER_PARAM(req, "IP黑名单", BusinessType::REMOVE, ip);
        RESP_MSG(cb, "删除成功");
    }

private:
    std::string getBlacklistRaw() {
        auto res = DatabaseService::instance().queryParams(
            "SELECT config_value FROM sys_config WHERE config_key='sys.login.blackIPList' LIMIT 1", {});
        return (res.ok() && res.rows() > 0) ? res.str(0, 0) : "";
    }

    void saveBlacklist(const std::string& val) {
        DatabaseService::instance().execParams(
            "UPDATE sys_config SET config_value=$1 WHERE config_key='sys.login.blackIPList'",
            {val});
        MemCache::instance().setString("sys_config:sys.login.blackIPList", val);
    }

    std::vector<std::string> split(const std::string& s) {
        std::vector<std::string> v;
        std::istringstream ss(s);
        std::string t;
        while (std::getline(ss, t, ',')) { auto p = trim(t); if (!p.empty()) v.push_back(p); }
        return v;
    }

    std::string join(const std::vector<std::string>& v) {
        std::string r;
        for (size_t i = 0; i < v.size(); i++) { if (i) r += ','; r += v[i]; }
        return r;
    }

    static std::string trim(std::string s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            [](unsigned char c){ return !std::isspace(c); }));
        s.erase(std::find_if(s.rbegin(), s.rend(),
            [](unsigned char c){ return !std::isspace(c); }).base(), s.end());
        return s;
    }
};
