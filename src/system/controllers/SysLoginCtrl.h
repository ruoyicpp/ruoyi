#pragma once
#include <drogon/HttpController.h>
#include <drogon/utils/Utilities.h>
#include <sstream>
#include <random>
#include <unordered_map>
#include <algorithm>
#include "../../common/AjaxResult.h"
#include "../../common/TokenCache.h"
#include "../../common/SecurityUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"
#include "../services/SysLoginService.h"
#include "../services/TokenService.h"
#include "../services/SysMenuService.h"
#include "../services/SysConfigService.h"
#include "../services/SysPasswordService.h"
#include "../../common/IpUtils.h"
#include "../../common/SmtpUtils.h"
#include "SysEmailConfigCtrl.h"

// POST /login  POST /logout  GET /getInfo  GET /getRouters
// POST /register  GET /captchaImage  GET /challenge
// POST /forgotPassword  POST /resetPassword
// POST /sendRegCode  GET /health  GET /version
class SysLoginCtrl : public drogon::HttpController<SysLoginCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysLoginCtrl::login,           "/login",          drogon::Post);
        ADD_METHOD_TO(SysLoginCtrl::logout,          "/logout",         drogon::Post);
        ADD_METHOD_TO(SysLoginCtrl::getInfo,         "/getInfo",        drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(SysLoginCtrl::getRouters,      "/getRouters",     drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(SysLoginCtrl::doRegister,      "/register",       drogon::Post);
        ADD_METHOD_TO(SysLoginCtrl::captcha,         "/captchaImage",   drogon::Get);
        ADD_METHOD_TO(SysLoginCtrl::challenge,       "/challenge",      drogon::Get);
        ADD_METHOD_TO(SysLoginCtrl::forgotPassword,  "/forgotPassword", drogon::Post);
        ADD_METHOD_TO(SysLoginCtrl::resetPassword,   "/resetPassword",  drogon::Post);
        ADD_METHOD_TO(SysLoginCtrl::sendRegCode,     "/sendRegCode",    drogon::Post);
        ADD_METHOD_TO(SysLoginCtrl::health,          "/health",         drogon::Get);
        ADD_METHOD_TO(SysLoginCtrl::version,         "/version",        drogon::Get);
    METHOD_LIST_END

    // 密码复杂度校验：至少 8 位，含大小写字母、数字、特殊字符
    static std::string checkPasswordComplexity(const std::string& pwd) {
        if (pwd.size() < 8)  return "密码长度不能少于8位";
        if (pwd.size() > 20) return "密码长度不能超过20位";
        bool hasUpper = false, hasLower = false, hasDigit = false, hasSpecial = false;
        for (unsigned char c : pwd) {
            if (std::isupper(c))      hasUpper   = true;
            else if (std::islower(c)) hasLower   = true;
            else if (std::isdigit(c)) hasDigit   = true;
            else                       hasSpecial = true;
        }
        if (!hasUpper || !hasLower) return "密码必须包含大小写字母";
        if (!hasDigit)               return "密码必须包含数字";
        if (!hasSpecial)             return "密码必须包含特殊字符";
        return "";
    }

    // POST /login
    void login(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }

        std::string username = (*body).get("username", "").asString();
        std::string password = (*body).get("password", "").asString();
        std::string code     = (*body).get("code",     "").asString();
        std::string uuid     = (*body).get("uuid",     "").asString();

        try {
            auto token = SysLoginService::instance().login(username, password, code, uuid, req);
            auto result = AjaxResult::successMap();
            result["token"] = token;
            RESP_JSON(cb, result);
        } catch (const std::exception &e) {
            RESP_ERR(cb, e.what());
        }
    }

    // POST /logout
    void logout(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        try {
            auto userOpt = TokenService::instance().getLoginUser(req);
            if (userOpt) {
                TokenService::instance().delLoginUser(userOpt->token);
                std::string ip       = IpUtils::getIpAddr(req);
                std::string user     = userOpt->userName;
                std::string location = IpUtils::getIpLocation(ip);
                auto insRes = DatabaseService::instance().queryParams(
                    "INSERT INTO sys_logininfor(user_name,ipaddr,login_location,status,msg,login_time) "
                    "VALUES($1,$2,$3,'0','退出成功',NOW()) RETURNING info_id",
                    {user, ip, location});
                if (!IpUtils::isIntranetIp(ip) && insRes.ok() && insRes.rows() > 0) {
                    std::string infoId = insRes.str(0, 0);
                    IpUtils::getIpLocationAsync(ip, [infoId](std::string loc) {
                        DatabaseService::instance().execParams(
                            "UPDATE sys_logininfor SET login_location=$1 WHERE info_id=$2",
                            {loc, infoId});
                    });
                }
            }
        } catch (...) {}
        RESP_MSG(cb, "操作成功");
    }

    // GET /getInfo
    void getInfo(const drogon::HttpRequestPtr &req,
                 std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto user = GET_LOGIN_USER(req);
        long userId = user.userId;

        auto& db = DatabaseService::instance();
        // 注：user_id(0),dept_id(1),user_name(2),nick_name(3),email(4),phonenumber(5),
        //     sex(6),avatar(7),status(8),login_ip(9),login_date(10),create_time(11),
        //     remark(12),dept_id2(13),dept_name(14),leader(15)
        auto res = db.queryParams(
            "SELECT u.user_id,u.dept_id,u.user_name,u.nick_name,u.email,"
            "u.phonenumber,u.sex,u.avatar,u.status,u.login_ip,u.login_date,"
            "u.create_time,u.remark,"
            "d.dept_id,d.dept_name,COALESCE(d.leader,'') "
            "FROM sys_user u LEFT JOIN sys_dept d ON u.dept_id=d.dept_id "
            "WHERE u.user_id=$1", {std::to_string(userId)});

        if (!res.ok() || res.rows() == 0) { RESP_401(cb); return; }

        Json::Value userJson;
        userJson["userId"]      = (Json::Int64)res.longVal(0, 0);
        userJson["deptId"]      = (Json::Int64)res.longVal(0, 1);
        userJson["userName"]    = res.str(0, 2);
        userJson["nickName"]    = res.str(0, 3);
        userJson["email"]       = res.str(0, 4);
        userJson["phonenumber"] = res.str(0, 5);
        userJson["sex"]         = res.str(0, 6);
        userJson["avatar"]      = res.str(0, 7);
        userJson["status"]      = res.str(0, 8);
        userJson["loginIp"]     = res.str(0, 9);
        userJson["loginDate"]   = fmtTs(res.str(0, 10));
        userJson["createTime"]  = fmtTs(res.str(0, 11));
        userJson["remark"]      = res.str(0, 12);
        userJson["admin"]       = SecurityUtils::isAdmin(userId);

        // 嵌套 dept 字段（供前端 profile 页面 user.dept.deptName）
        Json::Value dept;
        dept["deptId"]   = (Json::Int64)res.longVal(0, 13);
        dept["deptName"] = res.str(0, 14);
        dept["leader"]   = res.str(0, 15);
        userJson["dept"] = dept;

        // 角色列表（嵌套到 user）供 authRole 页面用
        auto roleRes = db.queryParams(
            "SELECT r.role_id,r.role_name,r.role_key,r.role_sort,r.status "
            "FROM sys_role r INNER JOIN sys_user_role ur ON r.role_id=ur.role_id "
            "WHERE ur.user_id=$1 AND r.del_flag='0'", {std::to_string(userId)});
        Json::Value userRoles(Json::arrayValue);
        if (roleRes.ok()) for (int i = 0; i < roleRes.rows(); ++i) {
            Json::Value r;
            r["roleId"]   = (Json::Int64)roleRes.longVal(i, 0);
            r["roleName"] = roleRes.str(i, 1);
            r["roleKey"]  = roleRes.str(i, 2);
            r["roleSort"] = roleRes.str(i, 3);
            r["status"]   = roleRes.str(i, 4);
            r["admin"]    = (roleRes.longVal(i, 0) == 1);
            userRoles.append(r);
        }
        userJson["roles"] = userRoles;

        // 单独输出 roles/角色权限标识列表，即 permissions
        Json::Value roles(Json::arrayValue);
        for (auto &r : user.roles) roles.append(r);
        Json::Value perms(Json::arrayValue);
        for (auto &p : user.permissions) perms.append(p);

        auto result = AjaxResult::successMap();
        result["user"]        = userJson;
        result["roles"]       = roles;
        result["permissions"] = perms;
        RESP_JSON(cb, result);
    }

    // GET /getRouters
    void getRouters(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        long userId = GET_USER_ID(req);
        auto cacheKey = "routers:" + std::to_string(userId);
        auto cached = MemCache::instance().getJson(cacheKey);
        if (cached) { RESP_OK(cb, *cached); return; }
        auto menus = SysMenuService::instance().buildMenusForUser(userId);
        MemCache::instance().setJson(cacheKey, menus, 1800); // 30min TTL
        RESP_OK(cb, menus);
    }

    // 使角色/菜单变更后失效该用户的菜单缓存
    static void invalidateRouterCache(long userId) {
        MemCache::instance().remove("routers:" + std::to_string(userId));
    }

    // 失效所有用户的菜单缓存（角色权限全局变更时调用）
    static void invalidateAllRouterCache() {
        MemCache::instance().removeByPrefix("routers:");
    }

    // POST /register
    void doRegister(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        if (!SysConfigService::instance().isRegisterEnabled()) {
            RESP_ERR(cb, "当前系统没有开启注册功能！");
            return;
        }
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }

        std::string username  = (*body).get("username",  "").asString();
        std::string password  = (*body).get("password",  "").asString();
        std::string code      = (*body).get("code",      "").asString();
        std::string uuid      = (*body).get("uuid",      "").asString();
        std::string email     = (*body).get("email",     "").asString();
        std::string emailCode = (*body).get("emailCode", "").asString();

        // 1. 验证码（C# 顺序：先验证码，再内容校验）
        if (SysConfigService::instance().isCaptchaEnabled()) {
            auto cacheKey = Constants::CAPTCHA_CODE_KEY + uuid;
            auto cached = MemCache::instance().getString(cacheKey);
            MemCache::instance().remove(cacheKey);
            if (!cached || *cached != code) {
                RESP_ERR(cb, "验证码错误"); return;
            }
        }

        // 2. 内容校验
        if (username.empty()) { RESP_ERR(cb, "用户名不能为空"); return; }
        if (password.empty()) { RESP_ERR(cb, "用户密码不能为空"); return; }
        if (username.size() < 2 || username.size() > 20)
            { RESP_ERR(cb, "账户长度必须在2到20个字符之间"); return; }
        {
            auto err = checkPasswordComplexity(password);
            if (!err.empty()) { RESP_ERR(cb, err); return; }
        }

        // 2a. email 选填：仅在非空时做格式和唯一性校验（旧前端不传 email 时直接跳过）
        if (!email.empty()) {
            auto at = email.find('@'), dot = email.rfind('.');
            if (at == std::string::npos || dot == std::string::npos || dot < at)
                { RESP_ERR(cb, "邮箱格式不正确"); return; }
            auto eExist = DatabaseService::instance().queryParams(
                "SELECT user_id FROM sys_user WHERE email=$1 AND del_flag='0' LIMIT 1", {email});
            if (eExist.ok() && eExist.rows() > 0)
                { RESP_ERR(cb, "该邮箱已被注册"); return; }
        }

        // 2b. 邮箱验证码（sys_config sys.account.emailVerify=true 时启用）
        {
            auto emailVerify = DatabaseService::instance().queryParams(
                "SELECT config_value FROM sys_config WHERE config_key='sys.account.emailVerify' LIMIT 1", {});
            bool requireEmail = emailVerify.ok() && emailVerify.rows() > 0
                && emailVerify.str(0,0) == "true";
            if (requireEmail) {
                if (email.empty()) { RESP_ERR(cb, "请填写邮箱地址"); return; }
                if (emailCode.empty()) { RESP_ERR(cb, "请输入邮箱验证码"); return; }
                auto cached = MemCache::instance().getString("regcode:" + email);
                if (!cached || *cached != emailCode) {
                    RESP_ERR(cb, "邮箱验证码错误或已过期"); return;
                }
                MemCache::instance().remove("regcode:" + email);
            }
        }

        // 3. 用户名唯一性（对应 C# CheckUserNameUniqueAsync）
        auto& db = DatabaseService::instance();
        auto exist = db.queryParams(
            "SELECT user_id FROM sys_user WHERE user_name=$1 AND del_flag!='2' LIMIT 1", {username});
        if (exist.ok() && exist.rows() > 0)
            { RESP_ERR(cb, "保存用户'" + username + "'失败，注册账号已存在"); return; }

        // 4. 注册（对应 C# RegisterUserAsync）
        std::string encPwd = SecurityUtils::encryptPassword(password);
        auto ins = db.queryParams(
            "INSERT INTO sys_user(user_name,nick_name,password,email,status,del_flag,create_time) "
            "VALUES($1,$1,$2,$3,'0','0',NOW()) RETURNING user_id",
            {username, encPwd, email});
        if (!ins.ok() || ins.rows() == 0) { RESP_ERR(cb, "注册失败，请联系系统管理人员"); return; }

        // 5. 绑定默认角色（如果管理员在系统参数中配置了 sys.account.initRoleId）
        long initRoleId = SysConfigService::instance().getInitRoleId();
        if (initRoleId > 0) {
            std::string newUserId = ins.str(0, 0);
            db.execParams(
                "INSERT INTO sys_user_role(user_id,role_id) VALUES($1,$2) ON CONFLICT DO NOTHING",
                {newUserId, std::to_string(initRoleId)});
        }

        // 6. 注册成功写登录日志（对应 C# SysLogininforService.AddAsync）
        std::string ip = IpUtils::getIpAddr(req);
        db.execParams(
            "INSERT INTO sys_logininfor(user_name,ipaddr,login_location,browser,os,status,msg,login_time) "
            "VALUES($1,$2,'','','','0','注册成功',NOW())",
            {username, ip});
        RESP_MSG(cb, "操作成功");
    }

    // GET /health — Nginx 健康探测 + DB ping
    void health(const drogon::HttpRequestPtr&,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto& db = DatabaseService::instance();
        bool dbOk = db.query("SELECT 1").ok();
        auto result = AjaxResult::successMap();
        result["status"] = dbOk ? "ok" : "degraded";
        result["db"]     = dbOk ? "ok" : "error";
        auto now = std::chrono::system_clock::now();
        auto t   = std::chrono::system_clock::to_time_t(now);
        char buf[32]; std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        result["time"] = buf;
        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(dbOk ? drogon::k200OK : drogon::k503ServiceUnavailable);
        cb(resp);
    }

    // GET /version — 返回构建版本信息
    void version(const drogon::HttpRequestPtr&,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto& cfg = drogon::app().getCustomConfig();
        auto result = AjaxResult::successMap();
        if (cfg.isMember("ruoyi")) {
            result["name"]    = cfg["ruoyi"].get("name",    "RuoYi-Cpp").asString();
            result["version"] = cfg["ruoyi"].get("version", "unknown").asString();
        } else {
            result["name"]    = "RuoYi-Cpp";
            result["version"] = "unknown";
        }
        result["buildTime"] = __DATE__ " " __TIME__;
        RESP_JSON(cb, result);
    }

    // POST /sendRegCode — 发送注册邮箱验证码（6位，5min TTL）
    void sendRegCode(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
        std::string email = (*body).get("email", "").asString();
        if (email.empty() || email.find('@') == std::string::npos) {
            RESP_ERR(cb, "邮箱格式不正确"); return;
        }
        // 60s 内不重复发送
        auto limitKey = "regcode_limit:" + email;
        if (MemCache::instance().getString(limitKey)) {
            RESP_ERR(cb, "发送过于频繁，请60秒后重试"); return;
        }
        // 生成 6 位数字验证码
        std::random_device rd; std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(100000, 999999);
        std::string code = std::to_string(dis(gen));
        MemCache::instance().setString("regcode:" + email, code, 300);  // 5min
        MemCache::instance().setString(limitKey, "1", 60);              // 60s 防重发
        SysEmailConfigCtrl::reloadSmtp();
        bool sent = SmtpUtils::instance().send(email, "注册验证码",
            "您的验证码是" + code + "，有效期为5分钟，请勿泄露给他人！");
        if (!sent)
            LOG_INFO << "[RegCode] email=" << email << " code=" << code
                     << " ip=" << IpUtils::getIpAddr(req) << " (SMTP 未配置，验证码已记录到日志)";
        RESP_MSG(cb, sent ? "验证码发送成功，请注意查收邮件" : "验证码发送失败，请稍后重试");
    }

    // GET /challenge — 生成一次性挑战令牌（60s TTL），浏览器动能接口用此替代 Secret
    void challenge(const drogon::HttpRequestPtr&,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto token = drogon::utils::getUuid();
        MemCache::instance().setString("challenge:" + token, "1", 60);
        auto result = AjaxResult::successMap();
        result["token"] = token;
        RESP_JSON(cb, result);
    }

    // POST /forgotPassword — 通过邮箱发送6位重置验证码（15min TTL，60s 防重发）
    void forgotPassword(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        if (!SysConfigService::instance().isForgotPwdEnabled()) {
            RESP_ERR(cb, "当前系统未开启忘记密码功能！");
            return;
        }
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
        std::string email = (*body).get("email", "").asString();
        if (email.empty()) { RESP_ERR(cb, "请输入邮箱地址"); return; }

        // 60s 防重发
        auto limitKey = "reset_limit:" + email;
        if (MemCache::instance().getString(limitKey)) {
            RESP_ERR(cb, "发送过于频繁，请60秒后重试"); return;
        }

        auto& db = DatabaseService::instance();
        auto res = db.queryParams(
            "SELECT user_name FROM sys_user WHERE email=$1 AND del_flag='0' LIMIT 1", {email});
        // 不泄露邮箱是否存在，统一回复相同消息
        if (res.ok() && res.rows() > 0) {
            std::string code;
            for (int i = 0; i < 6; ++i)
                code += std::to_string(std::rand() % 10);
            MemCache::instance().setString("reset_code:" + email, code, 900); // 15min
            MemCache::instance().setString(limitKey, "1", 60);
            SysEmailConfigCtrl::reloadSmtp();
            bool sent = SmtpUtils::instance().send(email, "忘记密码",
                "您的验证码是" + code + "，有效期为15分钟，请勿泄露给他人！");
            if (!sent)
                LOG_INFO << "[ForgotPwd] email=" << email << " code=" << code
                         << " ip=" << IpUtils::getIpAddr(req) << " (SMTP 未配置，已记录日志)";
        }
        RESP_MSG(cb, "验证码发送成功，请注意查收邮件");
    }

    // POST /resetPassword — 邮箱 + 验证码 + 新密码 三要素重置
    void resetPassword(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        if (!SysConfigService::instance().isForgotPwdEnabled()) {
            RESP_ERR(cb, "当前系统未开启忘记密码功能！");
            return;
        }
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
        std::string email  = (*body).get("email",       "").asString();
        std::string code   = (*body).get("code",        "").asString();
        std::string newPwd = (*body).get("newPassword", "").asString();
        if (email.empty()) { RESP_ERR(cb, "请输入邮箱地址"); return; }
        if (code.empty())  { RESP_ERR(cb, "请输入验证码"); return; }

        auto cacheKey = "reset_code:" + email;
        auto cached   = MemCache::instance().getString(cacheKey);
        if (!cached || *cached != code) { RESP_ERR(cb, "验证码错误或已过期"); return; }

        auto err = checkPasswordComplexity(newPwd);
        if (!err.empty()) { RESP_ERR(cb, err); return; }

        auto& db = DatabaseService::instance();
        auto res = db.queryParams(
            "SELECT user_name FROM sys_user WHERE email=$1 AND del_flag='0' LIMIT 1", {email});
        if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "邮箱未注册"); return; }
        std::string username = res.str(0, 0);

        std::string encPwd = SecurityUtils::encryptPassword(newPwd);
        db.execParams("UPDATE sys_user SET password=$1,update_time=NOW() WHERE user_name=$2",
                      {encPwd, username});
        MemCache::instance().remove(cacheKey);
        MemCache::instance().remove("reset_limit:" + email);
        LOG_INFO << "[ResetPwd] 密码已重置 user=" << username
                 << " ip=" << IpUtils::getIpAddr(req);
        RESP_MSG(cb, "密码重置成功");
    }

    // GET /captchaImage — 3种验证码随机：数字加减 / 字母 / 中文数字
    void captcha(const drogon::HttpRequestPtr &req,
                 std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto uuid = drogon::utils::getUuid();
        int mode = randInt(0, 2); // 0:数字加减 1:字母 2:中文数字

        std::string answer;
        std::vector<RenderCmd> cmds;
        switch (mode) {
            case 0: makeMathCaptcha(cmds, answer);    break;
            case 1: makeLetterCaptcha(cmds, answer);  break;
            case 2: makeChineseCaptcha(cmds, answer);  break;
        }

        MemCache::instance().setString(Constants::CAPTCHA_CODE_KEY + uuid, answer, 120);

        std::string gifData = generateCaptchaGif(cmds);
        std::string base64Img = drogon::utils::base64Encode(
            (const unsigned char*)gifData.data(), gifData.size());

        Json::Value result = AjaxResult::successMap();
        result["uuid"]            = uuid;
        result["img"]             = base64Img;
        result["captchaEnabled"]    = SysConfigService::instance().isCaptchaEnabled();
        result["registerEnabled"]   = SysConfigService::instance().isRegisterEnabled();
        result["forgotPwdEnabled"]  = SysConfigService::instance().isForgotPwdEnabled();
        RESP_JSON(cb, result);
    }

private:
    // ===================== 随机数工具 =====================
    static std::mt19937& rng() {
        static std::mt19937 gen(std::random_device{}());
        return gen;
    }
    static int randInt(int lo, int hi) {
        std::uniform_int_distribution<int> dist(lo, hi);
        return dist(rng());
    }

    // ===================== 渲染命令定义 =====================
    struct RenderCmd {
        enum Type { ASCII, CN_DIGIT } type;
        char ch;      // ASCII 字符
        int  digit;   // 中文数字字符编号 1-9
    };
    static RenderCmd asciiCmd(char c)  { return {RenderCmd::ASCII,    c, 0}; }
    static RenderCmd cnCmd(int d)      { return {RenderCmd::CN_DIGIT, 0, d}; }

    // ===================== 验证码模式1：数字运算 =====================
    static void makeMathCaptcha(std::vector<RenderCmd>& cmds, std::string& answer) {
        int a = randInt(1, 10), b = randInt(1, 10);
        int op = randInt(0, 2);
        std::string expr;
        switch (op) {
            case 0: expr = std::to_string(a) + "+" + std::to_string(b) + "=?";
                    answer = std::to_string(a + b); break;
            case 1: if (a < b) std::swap(a, b);
                    expr = std::to_string(a) + "-" + std::to_string(b) + "=?";
                    answer = std::to_string(a - b); break;
            default: a = randInt(1,9); b = randInt(1,9);
                     expr = std::to_string(a) + "x" + std::to_string(b) + "=?";
                     answer = std::to_string(a * b); break;
        }
        for (char c : expr) cmds.push_back(asciiCmd(c));
    }

    // ===================== 验证码模式2：字母 =====================
    static void makeLetterCaptcha(std::vector<RenderCmd>& cmds, std::string& answer) {
        static const char pool[] = "ABCDEFGHJKMNPRSTUWXYZ"; // 排除易混淆字符 I/L/O/Q/V
        answer.clear();
        for (int i = 0; i < 4; i++) {
            char c = pool[randInt(0, (int)sizeof(pool) - 2)];
            cmds.push_back(asciiCmd(c));
            answer += (char)std::tolower(c);
        }
    }

    // ===================== 验证码模式3：中文数字算式 =====================
    static void makeChineseCaptcha(std::vector<RenderCmd>& cmds, std::string& answer) {
        int a = randInt(1, 9), b = randInt(1, 9);
        int op = randInt(0, 1); // 0:加法 1:减法
        if (op == 1 && a < b) std::swap(a, b);
        cmds.push_back(cnCmd(a));
        cmds.push_back(asciiCmd(op == 0 ? '+' : '-'));
        cmds.push_back(cnCmd(b));
        cmds.push_back(asciiCmd('='));
        cmds.push_back(asciiCmd('?'));
        answer = std::to_string(op == 0 ? a + b : a - b);
    }

    // ===================== 5x7 ASCII 点阵 =====================
    static const std::vector<uint8_t>& getGlyph(char c) {
        static const std::unordered_map<char, std::vector<uint8_t>> font = {
            {'0', {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}},
            {'1', {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}},
            {'2', {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F}},
            {'3', {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E}},
            {'4', {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}},
            {'5', {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}},
            {'6', {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}},
            {'7', {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}},
            {'8', {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}},
            {'9', {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}},
            {'+', {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}},
            {'-', {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}},
            {'x', {0x00,0x11,0x0A,0x04,0x0A,0x11,0x00}},
            {'=', {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}},
            {'?', {0x0E,0x11,0x01,0x06,0x04,0x00,0x04}},
            {'A', {0x04,0x0A,0x11,0x1F,0x11,0x11,0x11}},
            {'B', {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}},
            {'C', {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}},
            {'D', {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}},
            {'E', {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}},
            {'F', {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}},
            {'G', {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}},
            {'H', {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}},
            {'J', {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}},
            {'K', {0x11,0x12,0x14,0x18,0x14,0x12,0x11}},
            {'M', {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}},
            {'N', {0x11,0x19,0x15,0x13,0x11,0x11,0x11}},
            {'P', {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}},
            {'R', {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},
            {'S', {0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E}},
            {'T', {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}},
            {'U', {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}},
            {'W', {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}},
            {'X', {0x11,0x0A,0x04,0x04,0x0A,0x11,0x11}},
            {'Y', {0x11,0x0A,0x04,0x04,0x04,0x04,0x04}},
            {'Z', {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}},
        };
        static const std::vector<uint8_t> blank = {0,0,0,0,0,0,0};
        auto it = font.find(c);
        return it != font.end() ? it->second : blank;
    }

    // ===================== 中文数字笔画定义（12x12 简化字形）=====================
    struct Stroke { int x0, y0, x1, y1; };
    using StrokeVec = std::vector<Stroke>;

    static const StrokeVec& getCnStrokes(int d) {
        // 简化笔画，仅用于验证码可辨识显示
        static const std::vector<StrokeVec> defs = {
            {},                                                        // 0 (unused)
            {{1,5, 10,5}},                                             // 一
            {{2,3, 9,3}, {1,8, 10,8}},                                 // 二
            {{2,1, 9,1}, {3,5, 8,5}, {1,10, 10,10}},                   // 三
            {{1,0,10,0},{1,0,1,10},{10,0,10,10},{1,10,10,10},           // 四
             {4,0,4,7},{7,0,7,7},{4,7,7,7}},
            {{1,0,10,0},{5,0,5,5},{2,5,9,5},{2,5,1,10},{1,10,10,10}},  // 五
            {{5,0,6,1},{1,3,10,3},{4,4,1,10},{7,4,10,10}},             // 六
            {{1,4,10,4},{6,0,6,10},{6,10,10,10}},                      // 七
            {{5,1,1,10},{6,1,10,10}},                                  // 八
            {{2,2,10,2},{4,2,4,6},{4,6,1,10},{4,6,10,9}},              // 九
        };
        if (d >= 1 && d <= 9) return defs[(size_t)d];
        static const StrokeVec empty;
        return empty;
    }

    // ===================== GIF 生成 =====================
    static std::string generateCaptchaGif(const std::vector<RenderCmd>& cmds) {
        const int W = 160, H = 60;
        struct Color { uint8_t r, g, b; };
        std::vector<Color> palette(256);
        palette[0] = {240, 240, 240};
        palette[1] = {0, 0, 0};
        for (int i = 2; i < 8; ++i)
            palette[i] = {(uint8_t)randInt(40,220), (uint8_t)randInt(40,220), (uint8_t)randInt(40,220)};
        for (int i = 8; i < 16; ++i)
            palette[i] = {(uint8_t)randInt(20,140), (uint8_t)randInt(20,140), (uint8_t)randInt(20,140)};
        for (int i = 16; i < 256; ++i)
            palette[i] = {240, 240, 240};

        std::vector<uint8_t> pixels(W * H, 0);

        // Bresenham 画线
        auto drawLine = [&](int x0, int y0, int x1, int y1, uint8_t color) {
            int dx = abs(x1-x0), dy = abs(y1-y0);
            int sx = x0<x1?1:-1, sy = y0<y1?1:-1;
            int err = dx-dy;
            while (true) {
                if (x0>=0 && x0<W && y0>=0 && y0<H) pixels[y0*W+x0] = color;
                if (x0==x1 && y0==y1) break;
                int e2 = 2*err;
                if (e2 > -dy) { err -= dy; x0 += sx; }
                if (e2 < dx)  { err += dx; y0 += sy; }
            }
        };

        // 干扰线
        for (int i = 0; i < 3; ++i) {
            int x0 = randInt(0,W-1), y0 = randInt(0,H-1);
            int x1 = std::min(W-1, std::max(0, x0+randInt(-30,30)));
            int y1 = std::min(H-1, std::max(0, y0+randInt(-15,15)));
            drawLine(x0, y0, x1, y1, 2+(i%6));
        }
        // 噪点
        for (int i = 0; i < 30; ++i) {
            int px = randInt(0,W-1), py = randInt(0,H-1);
            pixels[py*W+px] = 2 + randInt(0,5);
        }

        // 居中布局字符
        const int asciiCharW = 17; // 5*3+2
        const int cnCharW    = 26; // 12*2+2
        int totalW = 0;
        for (auto& cmd : cmds)
            totalW += (cmd.type == RenderCmd::ASCII ? asciiCharW : cnCharW);
        int curX = (W - totalW) / 2;

        // 绘制字符
        for (size_t ci = 0; ci < cmds.size(); ++ci) {
            uint8_t color = 8 + (ci % 8);
            int jitter = randInt(-3, 3);

            if (cmds[ci].type == RenderCmd::ASCII) {
                // 5x7 ASCII 字形，放大 3 倍
                int scale = 3;
                auto& glyph = getGlyph(cmds[ci].ch);
                int oy = (H - 7*scale) / 2 + jitter;
                for (int row = 0; row < 7; ++row)
                    for (int col = 0; col < 5; ++col)
                        if (glyph[row] & (0x10 >> col))
                            for (int sy = 0; sy < scale; ++sy)
                                for (int sx = 0; sx < scale; ++sx) {
                                    int px = curX + col*scale + sx;
                                    int py = oy + row*scale + sy;
                                    if (px>=0 && px<W && py>=0 && py<H)
                                        pixels[py*W+px] = color;
                                }
                curX += asciiCharW;
            } else {
                // 中文数字按笔画绘制，缩放 scale=2
                int scale = 2;
                auto& strokes = getCnStrokes(cmds[ci].digit);
                int oy = (H - 12*scale) / 2 + jitter;
                for (auto& s : strokes) {
                    for (int t = 0; t < scale; ++t) {
                        drawLine(curX+s.x0*scale, oy+s.y0*scale+t,
                                 curX+s.x1*scale, oy+s.y1*scale+t, color);
                        drawLine(curX+s.x0*scale+t, oy+s.y0*scale,
                                 curX+s.x1*scale+t, oy+s.y1*scale, color);
                    }
                }
                curX += cnCharW;
            }
        }

        // ========== GIF89a 编码 ==========
        std::string gif;
        auto w8  = [&](uint8_t  v) { gif += (char)v; };
        auto w16 = [&](uint16_t v) { gif += (char)(v&0xFF); gif += (char)((v>>8)&0xFF); };

        gif += "GIF89a";
        w16(W); w16(H);
        w8(0xF7); w8(0); w8(0);
        for (int i = 0; i < 256; ++i) { w8(palette[i].r); w8(palette[i].g); w8(palette[i].b); }
        w8(0x2C); w16(0); w16(0); w16(W); w16(H); w8(0);

        const int minCodeSize = 8;
        w8((uint8_t)minCodeSize);
        const int clearCode = 1 << minCodeSize; // 256
        const int eoiCode   = clearCode + 1;    // 257

        // --- 最小实现的 LZW 编码 ---
        std::vector<uint8_t> lzwBytes;
        uint32_t bitBuf = 0; int bitCnt = 0; int codeSize = minCodeSize + 1;
        int nextCode = eoiCode + 1, maxCode = 1 << codeSize;

        auto emitCode = [&](int code) {
            bitBuf |= ((uint32_t)code << bitCnt); bitCnt += codeSize;
            while (bitCnt >= 8) { lzwBytes.push_back(bitBuf & 0xFF); bitBuf >>= 8; bitCnt -= 8; }
        };

        // 字典项：key = prefix<<8 | suffix，value = code
        std::unordered_map<uint32_t,int> dict;
        auto resetDict = [&]() {
            dict.clear(); codeSize = minCodeSize + 1;
            nextCode = eoiCode + 1; maxCode = 1 << codeSize;
        };

        resetDict();
        emitCode(clearCode);

        int prefix = pixels[0];
        for (size_t i = 1; i < pixels.size(); ++i) {
            uint32_t key = ((uint32_t)prefix << 8) | pixels[i];
            auto it = dict.find(key);
            if (it != dict.end()) {
                prefix = it->second;
            } else {
                emitCode(prefix);
                if (nextCode < 4096) {
                    dict[key] = nextCode++;
                    if (nextCode > maxCode && codeSize < 12) { ++codeSize; maxCode <<= 1; }
                } else {
                    emitCode(clearCode); resetDict();
                }
                prefix = pixels[i];
            }
        }
        emitCode(prefix);
        emitCode(eoiCode);
        if (bitCnt > 0) lzwBytes.push_back(bitBuf & 0xFF);

        size_t pos = 0;
        while (pos < lzwBytes.size()) {
            size_t chunk = std::min<size_t>(255, lzwBytes.size() - pos);
            w8((uint8_t)chunk);
            for (size_t j = 0; j < chunk; ++j) w8(lzwBytes[pos + j]);
            pos += chunk;
        }
        w8(0);
        w8(0x3B);
        return gif;
    }
};
