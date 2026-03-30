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

// ��Ӧ RuoYi.Net SysLoginService��ʹ������ libpq ������
class SysLoginService {
public:
    static SysLoginService &instance() {
        static SysLoginService inst;
        return inst;
    }

    // ��¼������ JWT token �ַ���
    std::string login(const std::string &username, const std::string &password,
                      const std::string &code, const std::string &uuid,
                      const drogon::HttpRequestPtr &req) {
        // 1. ��֤��У��
        try {
            validateCaptcha(username, code, uuid);
        } catch (const std::exception &e) {
            insertLoginLog(username, "1", e.what(), req);
            throw;
        }

        // 2. ǰ��У��
        try {
            loginPreCheck(username, password, req);
        } catch (const std::exception &e) {
            insertLoginLog(username, "1", e.what(), req);
            throw;
        }

        // 3. ��ѯ�û���ֱ�� libpq��
        auto& db = DatabaseService::instance();
        auto res = db.queryParams(
            "SELECT user_id, dept_id, user_name, nick_name, password, "
            "status, del_flag, email, phonenumber, avatar "
            "FROM sys_user WHERE user_name=$1 AND del_flag='0' LIMIT 1",
            {username});

        if (!res.ok() || res.rows() == 0) {
            insertLoginLog(username, "1", "�û�������/�������", req);
            throw std::runtime_error("�û�������/�������");
        }

        std::string status  = res.str(0, 5);  // status
        std::string delFlag = res.str(0, 6);  // del_flag

        if (delFlag == "2") {
            insertLoginLog(username, "1", "�Բ��������˺��ѱ�ɾ��", req);
            throw std::runtime_error("�Բ��������˺��ѱ�ɾ��");
        }
        if (status  == "1") {
            insertLoginLog(username, "1", "�û��ѷ��������ϵ����Ա", req);
            throw std::runtime_error("�û��ѷ��������ϵ����Ա");
        }

        std::string encodedPwd = res.str(0, 4);  // password

        // 4. ������֤��������������ƣ�
        // ���� ip/ua��SysPasswordService �ڲ���д���Ѵ���N�Ρ���־����Ӧ C# ������־��Ϊ��
        try {
            std::string ip = IpUtils::getIpAddr(req);
            std::string ua = req->getHeader("User-Agent");
            if (ua.size() > 50) ua = ua.substr(0, 50);
            SysPasswordService::instance().validate(username, password, encodedPwd, ip, ua);
        } catch (const std::exception &e) {
            insertLoginLog(username, "1", e.what(), req);
            throw;
        }

        // 5. ���� LoginUser
        LoginUser user;
        user.userId   = res.longVal(0, 0);   // user_id
        user.deptId   = res.longVal(0, 1);   // dept_id
        user.userName = username;
        user.password = encodedPwd;

        // ���ز�������
        auto deptRes = db.queryParams(
            "SELECT dept_name FROM sys_dept WHERE dept_id=$1",
            {std::to_string(user.deptId)});
        if (deptRes.ok() && deptRes.rows() > 0)
            user.deptName = deptRes.str(0, 0);

        // ����Ȩ��
        loadPermissions(user);

        // 6. ��¼��¼��Ϣ
        recordLoginInfo(user.userId, IpUtils::getIpAddr(req));

        // 7. ��¼��¼��־
        insertLoginLog(username, "0", "��¼�ɹ�", req);

        // 8. ���� token
        return TokenService::instance().createToken(user, req);
    }

private:
    void validateCaptcha(const std::string &username,
                         const std::string &code, const std::string &uuid) {
        if (!SysConfigService::instance().isCaptchaEnabled()) return;
        if (code.empty() || uuid.empty())
            throw std::runtime_error("��֤�벻��Ϊ��");

        auto cacheKey = Constants::CAPTCHA_CODE_KEY + uuid;
        auto cached = MemCache::instance().getString(cacheKey);
        MemCache::instance().remove(cacheKey);

        if (!cached)
            throw std::runtime_error("��֤����ʧЧ");
        // ��Сд����бȽϣ���ĸ��֤�룩
        auto toLower = [](std::string s) { for (auto& c : s) c = std::tolower(c); return s; };
        if (toLower(*cached) != toLower(code))
            throw std::runtime_error("��֤�����");
    }

    void loginPreCheck(const std::string &username, const std::string &password,
                       const drogon::HttpRequestPtr &req) {
        if (username.empty() || password.empty())
            throw std::runtime_error("�û�/���������д");

        auto userCfg = drogon::app().getCustomConfig()["user"];
        int minPwd = userCfg.get("password_min_length", 5).asInt();
        int maxPwd = userCfg.get("password_max_length", 20).asInt();
        int minUsr = userCfg.get("username_min_length", 2).asInt();
        int maxUsr = userCfg.get("username_max_length", 20).asInt();

        if ((int)password.size() < minPwd || (int)password.size() > maxPwd)
            throw std::runtime_error("�û�������/�������");
        if ((int)username.size() < minUsr || (int)username.size() > maxUsr)
            throw std::runtime_error("�û�������/�������");

        auto blackList = SysConfigService::instance().selectConfigByKey("sys.login.blackIPList");
        if (IpUtils::isMatchedIp(blackList, IpUtils::getIpAddr(req)))
            throw std::runtime_error("���ź�������IP�ѱ�����ϵͳ������");
    }

    void loadPermissions(LoginUser &user) {
        if (SecurityUtils::isAdmin(user.userId)) {
            user.permissions = {"*:*:*"};
            user.roles       = {"admin"};
            return;
        }
        auto& db = DatabaseService::instance();
        // ���ɫ key
        auto roleRes = db.queryParams(
            "SELECT r.role_key FROM sys_role r "
            "INNER JOIN sys_user_role ur ON r.role_id=ur.role_id "
            "WHERE ur.user_id=$1 AND r.status='0' AND r.del_flag='0'",
            {std::to_string(user.userId)});
        if (roleRes.ok()) {
            for (int i = 0; i < roleRes.rows(); ++i)
                user.roles.push_back(roleRes.str(i, 0));
        }
        // ��˵�Ȩ��
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
        std::string ip = IpUtils::getIpAddr(req);
        auto ua = UaUtils::parse(req->getHeader("User-Agent"));
        DatabaseService::instance().execParams(
            "INSERT INTO sys_logininfor(user_name,ipaddr,login_location,browser,os,status,msg,login_time) "
            "VALUES($1,$2,$3,$4,$5,$6,$7,NOW())",
            {username, ip, "", ua.browser, ua.os, status, msg});
    }
};
