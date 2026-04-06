#pragma once
#include <string>
#include <vector>

namespace Constants {
    // Token 相关
    inline const std::string TOKEN_PREFIX      = "Bearer ";
    inline const std::string LOGIN_USER_KEY    = "login_user_key";
    inline const std::string LOGIN_TOKEN_KEY   = "login_tokens:";
    inline const std::string PWD_ERR_CNT_KEY   = "pwd_err_cnt:";
    inline const std::string SYS_CONFIG_KEY    = "sys_config:";
    inline const std::string SYS_DICT_KEY      = "sys_dict:";
    inline const std::string CAPTCHA_CODE_KEY  = "captcha_codes:";

    // 登录状态
    inline const std::string LOGIN_SUCCESS = "Success";
    inline const std::string LOGOUT        = "Logout";
    inline const std::string REGISTER      = "Register";
    inline const std::string LOGIN_FAIL    = "Error";

    // 用户唯一性
    inline const std::string UNIQUE     = "0";
    inline const std::string NOT_UNIQUE = "1";
    inline const std::string YES        = "Y";
    inline const std::string NO         = "N";

    // 部门状态
    inline const std::string DEPT_NORMAL  = "0";
    inline const std::string DEPT_DISABLE = "1";

    // 删除标志
    inline const std::string DEL_FLAG_NO  = "0";
    inline const std::string DEL_FLAG_YES = "2";

    // 状态
    inline const std::string STATUS_NORMAL  = "0";
    inline const std::string STATUS_DISABLE = "1";

    // 菜单类型
    inline const std::string TYPE_DIR    = "M";
    inline const std::string TYPE_MENU   = "C";
    inline const std::string TYPE_BUTTON = "F";

    // 是否外链
    inline const std::string YES_FRAME = "0";
    inline const std::string NO_FRAME  = "1";

    // 路由类型
    inline const std::string LAYOUT      = "Layout";
    inline const std::string PARENT_VIEW = "ParentView";
    inline const std::string INNER_LINK  = "InnerLink";

    // 超级管理员 userId
    inline const long ADMIN_USER_ID = 1L;
    inline const long ADMIN_ROLE_ID = 1L;

    // 用户名/密码长度限制
    inline const int USERNAME_MIN_LENGTH = 2;
    inline const int USERNAME_MAX_LENGTH = 20;
    inline const int PASSWORD_MIN_LENGTH = 5;
    inline const int PASSWORD_MAX_LENGTH = 20;

    // 定时任务白名单前缀
    inline const std::string JOB_WHITELIST_PREFIX = "RuoYi";

    // 禁止调用的字符串
    inline const std::vector<std::string> JOB_ERROR_STR = {
        "java.net.URL", "javax.script", "org.yaml.snakeyaml",
        "org.springframework", "org.apache"
    };
    inline const std::string LOOKUP_RMI   = "rmi:";
    inline const std::string LOOKUP_LDAP  = "ldap:";
    inline const std::string LOOKUP_LDAPS = "ldaps:";
    inline const std::string HTTP  = "http://";
    inline const std::string HTTPS = "https://";
}
