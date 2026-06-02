/**
 * @file Constants.h
 * @brief 系统常量定义
 * 
 * 集中定义系统中使用的所有常量，包括：
 *   - 缓存键前缀
 *   - 登录状态标志
 *   - 数据库状态值
 *   - 菜单和路由类型
 *   - 用户和角色 ID
 *   - 字符串长度限制
 *   - 安全相关常量
 */

#pragma once
#include <string>
#include <vector>

/**
 * @namespace Constants
 * @brief 系统常量命名空间
 * 
 * 所有常量都定义为 inline，支持 C++17 及以上版本。
 */
namespace Constants {
    /// @name 缓存键相关常量
    /// @{
    inline const std::string TOKEN_PREFIX      = "Bearer ";           ///< JWT 令牌前缀
    inline const std::string LOGIN_USER_KEY    = "login_user_key";    ///< 登录用户缓存键
    inline const std::string LOGIN_TOKEN_KEY   = "login_tokens:";     ///< 登录令牌缓存键前缀
    inline const std::string PWD_ERR_CNT_KEY   = "pwd_err_cnt:";      ///< 密码错误计数缓存键前缀
    inline const std::string SYS_CONFIG_KEY    = "sys_config:";       ///< 系统配置缓存键前缀
    inline const std::string SYS_DICT_KEY      = "sys_dict:";         ///< 系统字典缓存键前缀
    inline const std::string CAPTCHA_CODE_KEY  = "captcha_codes:";    ///< 验证码缓存键前缀
    /// @}

    /// @name 登录状态常量
    /// @{
    inline const std::string LOGIN_SUCCESS = "Success";   ///< 登录成功
    inline const std::string LOGOUT        = "Logout";    ///< 登出
    inline const std::string REGISTER      = "Register";  ///< 注册
    inline const std::string LOGIN_FAIL    = "Error";     ///< 登录失败
    /// @}

    /// @name 通用状态常量
    /// @{
    inline const std::string UNIQUE     = "0";   ///< 唯一（用于用户名、邮箱等唯一性检查）
    inline const std::string NOT_UNIQUE = "1";   ///< 不唯一
    inline const std::string YES        = "Y";   ///< 是
    inline const std::string NO         = "N";   ///< 否
    /// @}

    /// @name 部门状态常量
    /// @{
    inline const std::string DEPT_NORMAL  = "0";  ///< 部门正常
    inline const std::string DEPT_DISABLE = "1";  ///< 部门禁用
    /// @}

    /// @name 删除标志常量
    /// @{
    inline const std::string DEL_FLAG_NO  = "0";  ///< 未删除
    inline const std::string DEL_FLAG_YES = "2";  ///< 已删除
    /// @}

    /// @name 通用状态常量
    /// @{
    inline const std::string STATUS_NORMAL  = "0";  ///< 正常
    inline const std::string STATUS_DISABLE = "1";  ///< 禁用
    /// @}

    /// @name 菜单类型常量
    /// @{
    inline const std::string TYPE_DIR    = "M";  ///< 目录
    inline const std::string TYPE_MENU   = "C";  ///< 菜单
    inline const std::string TYPE_BUTTON = "F";  ///< 按钮
    /// @}

    /// @name 外链标志常量
    /// @{
    inline const std::string YES_FRAME = "0";  ///< 是外链
    inline const std::string NO_FRAME  = "1";  ///< 不是外链
    /// @}

    /// @name 路由类型常量
    /// @{
    inline const std::string LAYOUT      = "Layout";      ///< 布局组件
    inline const std::string PARENT_VIEW = "ParentView";  ///< 父视图
    inline const std::string INNER_LINK  = "InnerLink";   ///< 内部链接
    /// @}

    /// @name 用户和角色常量
    /// @{
    inline const long ADMIN_USER_ID = 1L;  ///< 超级管理员用户 ID
    inline const long ADMIN_ROLE_ID = 1L;  ///< 超级管理员角色 ID
    /// @}

    /// @name 字符串长度限制常量
    /// @{
    inline const int USERNAME_MIN_LENGTH = 2;   ///< 用户名最小长度
    inline const int USERNAME_MAX_LENGTH = 20;  ///< 用户名最大长度
    inline const int PASSWORD_MIN_LENGTH = 5;   ///< 密码最小长度
    inline const int PASSWORD_MAX_LENGTH = 20;  ///< 密码最大长度
    /// @}

    /// @name 定时任务相关常量
    /// @{
    inline const std::string JOB_WHITELIST_PREFIX = "RuoYi";  ///< 定时任务白名单前缀
    /// @}

    /// @name 安全相关常量（禁止调用的字符串）
    /// @{
    inline const std::vector<std::string> JOB_ERROR_STR = {
        "java.net.URL", "javax.script", "org.yaml.snakeyaml",
        "org.springframework", "org.apache"
    };  ///< 定时任务中禁止调用的类名前缀
    inline const std::string LOOKUP_RMI   = "rmi:";    ///< RMI 协议前缀（禁止）
    inline const std::string LOOKUP_LDAP  = "ldap:";   ///< LDAP 协议前缀（禁止）
    inline const std::string LOOKUP_LDAPS = "ldaps:";  ///< LDAPS 协议前缀（禁止）
    /// @}

    /// @name 协议常量
    /// @{
    inline const std::string HTTP  = "http://";   ///< HTTP 协议
    inline const std::string HTTPS = "https://";  ///< HTTPS 协议
    /// @}
}
