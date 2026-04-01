#pragma once
#include "../../common/UaUtils.h"
#include <string>
#include <sstream>
#include <stdexcept>
#include <drogon/drogon.h>
#include <json/json.h>
#include "../../common/LoginUser.h"
#include "../../common/SecurityUtils.h"
#include "../../common/TokenCache.h"
#include "../../common/Constants.h"
#include "../../common/IpUtils.h"
#include "../../services/DatabaseService.h"
#include "SysPasswordService.h"
#include "SysConfigService.h"
#include "TokenService.h"

// 对应 RuoYi.Net SysLoginService，使用直接 libpq 查询
class SysLoginService {
public:
    static SysLoginService &instance() {
        static SysLoginService inst;
        return inst;
    }

    // 登录成功后返回 JWT token 字符串
    std::string login(const std::string &username, const std::string &password,
                      const std::string &code, const std::string &uuid,
                      const drogon::HttpRequestPtr &req) {
        // 1. 验证码校验
        try {
            validateCaptcha(username, code, uuid);
        } catch (const std::exception &e) {
            insertLoginLog(username, "1", e.what(), req);
            throw;
        }

        // 2. 前置校验
        try {
            loginPreCheck(username, password, req);
        } catch (const std::exception &e) {
            insertLoginLog(username, "1", e.what(), req);
            throw;
        }

        // 3. 查询用户（直接 libpq）
        auto& db = DatabaseService::instance();
        auto res = db.queryParams(
            "SELECT user_id, dept_id, user_name, nick_name, password, "
            "status, del_flag, email, phonenumber, avatar "
            "FROM sys_user WHERE user_name=$1 AND del_flag='0' LIMIT 1",
            {username});

        if (!res.ok() || res.rows() == 0) {
            insertLoginLog(username, "1", "用户不存在/密码错误", req);
            throw std::runtime_error("用户不存在/密码错误");
        }

        std::string status  = res.str(0, 5);  // status
        std::string delFlag = res.str(0, 6);  // del_flag

        if (delFlag == "2") {
            insertLoginLog(username, "1", "该账号已被删除", req);
            throw std::runtime_error("用户不存在/密码错误");
        }
        if (status  == "1") {
            insertLoginLog(username, "1", "账号已停用，请联系管理员", req);
            throw std::runtime_error("用户不存在/密码错误");
        }

        std::string encodedPwd = res.str(0, 4);  // password

        // 4. 密码验证（含错误次数限制）
        // 传入 ip/ua，SysPasswordService 内部写登录已尝试N次（日志，对应 C# 同名日志行为）
        try {
            std::string ip = IpUtils::getIpAddr(req);
            std::string ua = req->getHeader("User-Agent");
            if (ua.size() > 50) ua = ua.substr(0, 50);
            SysPasswordService::instance().validate(username, password, encodedPwd, ip, ua);
        } catch (const std::exception &e) {
            insertLoginLog(username, "1", e.what(), req);
            throw;
        }

        // 5. 构建 LoginUser
        LoginUser user;
        user.userId   = res.longVal(0, 0);   // user_id
        user.deptId   = res.longVal(0, 1);   // dept_id
        user.userName = username;
        user.password = encodedPwd;

        // 查找部门名称
        auto deptRes = db.queryParams(
            "SELECT dept_name FROM sys_dept WHERE dept_id=$1",
            {std::to_string(user.deptId)});
        if (deptRes.ok() && deptRes.rows() > 0)
            user.deptName = deptRes.str(0, 0);

        // 加载权限
        loadPermissions(user);

        // 6. 记录登录信息
        recordLoginInfo(user.userId, IpUtils::getIpAddr(req));

        // 7. 记录登录日志
        insertLoginLog(username, "0", "登录成功", req);

        // 8. 生成 token
        return TokenService::instance().createToken(user, req);
    }

private:
    void validateCaptcha(const std::string &username,
                         const std::string &code, const std::string &uuid) {
        if (!SysConfigService::instance().isCaptchaEnabled()) return;
        if (code.empty() || uuid.empty())
            throw std::runtime_error("验证码不能为空");

        auto cacheKey = Constants::CAPTCHA_CODE_KEY + uuid;
        auto cached = MemCache::instance().getString(cacheKey);
        MemCache::instance().remove(cacheKey);

        if (!cached)
            throw std::runtime_error("验证码已失效，请重新获取");
        // 大小写不敏感比较（字母验证码）
        auto toLower = [](std::string s) { for (auto& c : s) c = std::tolower(c); return s; };
        if (toLower(*cached) != toLower(code))
            throw std::runtime_error("验证码错误");
    }

    void loginPreCheck(const std::string &username, const std::string &password,
                       const drogon::HttpRequestPtr &req) {
        if (username.empty() || password.empty())
            throw std::runtime_error("用户不存在/密码错误");

        auto userCfg = drogon::app().getCustomConfig()["user"];
        int minPwd = userCfg.get("password_min_length", 5).asInt();
        int maxPwd = userCfg.get("password_max_length", 20).asInt();
        int minUsr = userCfg.get("username_min_length", 2).asInt();
        int maxUsr = userCfg.get("username_max_length", 20).asInt();

        if ((int)password.size() < minPwd || (int)password.size() > maxPwd)
            throw std::runtime_error("用户不存在/密码错误");
        if ((int)username.size() < minUsr || (int)username.size() > maxUsr)
            throw std::runtime_error("用户不存在/密码错误");

        auto blackList = SysConfigService::instance().selectConfigByKey("sys.login.blackIPList");
        if (IpUtils::isMatchedIp(blackList, IpUtils::getIpAddr(req)))
            throw std::runtime_error("登录IP已被系统黑名单拦截，请联系管理员");
    }

    void loadPermissions(LoginUser &user) {
        if (SecurityUtils::isAdmin(user.userId)) {
            user.permissions = {"*:*:*"};
            user.roles       = {"admin"};
            return;
        }
        auto& db = DatabaseService::instance();
        // 查角色 key
        auto roleRes = db.queryParams(
            "SELECT r.role_key FROM sys_role r "
            "INNER JOIN sys_user_role ur ON r.role_id=ur.role_id "
            "WHERE ur.user_id=$1 AND r.status='0' AND r.del_flag='0'",
            {std::to_string(user.userId)});
        if (roleRes.ok()) {
            for (int i = 0; i < roleRes.rows(); ++i)
                user.roles.push_back(roleRes.str(i, 0));
        }
        // 查菜单权限
        auto permRes = db.queryParams(
            "SELECT DISTINCT m.perms FROM sys_menu m "
            "INNER JOIN sys_role_menu rm ON m.menu_id=rm.menu_id "
            "INNER JOIN sys_user_role ur ON rm.role_id=ur.role_id "
            "WHERE ur.user_id=$1 AND m.status='0' AND m.perms IS NOT NULL AND m.perms!=''",
            {std::to_string(user.userId)});
        if (permRes.ok()) {
            for (int i = 0; i < permRes.rows(); ++i) {
                std::string perms = permRes.str(i, 0);
                std::stringstream ss(perms);
                std::string p;
                while (std::getline(ss, p, ','))
                    if (!p.empty()) user.permissions.push_back(p);
            }
        }
    }

    void recordLoginInfo(long userId, const std::string &ip) {
        DatabaseService::instance().execParams(
            "UPDATE sys_user SET login_ip=$1, login_date=NOW() WHERE user_id=$2",
            {ip, std::to_string(userId)});
    }

    void insertLoginLog(const std::string &username, const std::string &status,
                        const std::string &msg, const drogon::HttpRequestPtr &req) {
        std::string ip       = IpUtils::getIpAddr(req);
        auto        ua       = UaUtils::parse(req->getHeader("User-Agent"));
        std::string location = IpUtils::getIpLocation(ip);  // 内网IP/XX XX 兜底
        auto& db = DatabaseService::instance();

        // 先同步插入（保证日志必定落库）
        auto insRes = db.queryParams(
            "INSERT INTO sys_logininfor(user_name,ipaddr,login_location,browser,os,status,msg,login_time) "
            "VALUES($1,$2,$3,$4,$5,$6,$7,NOW()) RETURNING info_id",
            {username, ip, location, ua.browser, ua.os, status, msg});

        // 外网 IP：异步获取精确地址后 UPDATE
        if (!IpUtils::isIntranetIp(ip) && insRes.ok() && insRes.rows() > 0) {
            std::string infoId = insRes.str(0, 0);
            IpUtils::getIpLocationAsync(ip, [infoId](std::string loc) {
                DatabaseService::instance().execParams(
                    "UPDATE sys_logininfor SET login_location=$1 WHERE info_id=$2",
                    {loc, infoId});
            });
        }
    }
};
