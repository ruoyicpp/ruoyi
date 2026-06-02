/**
 * @file SysIpCtrl.h
 * @brief IP 封禁管理控制器
 * 
 * 功能概述：
 *   - 动态封禁列表：显示由速率限制自动封禁的 IP
 *   - 手动解封：管理员可手动解除 IP 封禁
 *   - 静态黑名单：管理系统配置中的 IP 黑名单
 *   - 黑名单增删：添加或删除 IP 黑名单条目
 * 
 * 数据来源：
 *   - 动态封禁：来自 RateLimiter 的内存缓存
 *   - 静态黑名单：来自 sys_config 表的 sys.login.blackIPList 配置
 */

#pragma once
#include <drogon/HttpController.h>      ///< Drogon HTTP 控制器基类
#include <sstream>                      ///< 字符串流处理
#include <algorithm>                    ///< 标准算法库（remove、find_if 等）
#include "../../common/AjaxResult.h"    ///< AJAX 响应结果
#include "../../common/RateLimiter.h"   ///< 速率限制器
#include "../../common/TokenCache.h"    ///< 令牌缓存
#include "../../filters/PermFilter.h"   ///< 权限过滤器
#include "../../services/DatabaseService.h"  ///< 数据库服务
#include "../../common/OperLogUtils.h"  ///< 操作日志工具

/**
 * @class SysIpCtrl
 * @brief IP 封禁管理控制器
 * 
 * 处理 IP 地址的动态封禁和静态黑名单管理。
 * 支持两种 IP 控制机制：
 *   1. 动态封禁：由速率限制器自动触发，临时封禁恶意 IP
 *   2. 静态黑名单：由管理员手动配置，永久性 IP 黑名单
 */
class SysIpCtrl : public drogon::HttpController<SysIpCtrl> {
public:
    /// @brief 路由映射表
    METHOD_LIST_BEGIN
        /// GET /monitor/ip/banned - 查询动态封禁列表（需要 monitor:ip:list 权限）
        ADD_METHOD_TO(SysIpCtrl::bannedList,    "/monitor/ip/banned",           drogon::Get,    "JwtAuthFilter");
        /// DELETE /monitor/ip/banned/{ip} - 手动解除 IP 封禁（需要 monitor:ip:unban 权限）
        ADD_METHOD_TO(SysIpCtrl::unban,         "/monitor/ip/banned/{ip}",      drogon::Delete, "JwtAuthFilter");
        /// GET /monitor/ip/blacklist - 查询静态黑名单（需要 monitor:ip:list 权限）
        ADD_METHOD_TO(SysIpCtrl::blacklist,     "/monitor/ip/blacklist",        drogon::Get,    "JwtAuthFilter");
        /// POST /monitor/ip/blacklist - 添加 IP 到黑名单（需要 monitor:ip:edit 权限）
        ADD_METHOD_TO(SysIpCtrl::addBlacklist,  "/monitor/ip/blacklist",        drogon::Post,   "JwtAuthFilter");
        /// DELETE /monitor/ip/blacklist/{ip} - 从黑名单删除 IP（需要 monitor:ip:edit 权限）
        ADD_METHOD_TO(SysIpCtrl::delBlacklist,  "/monitor/ip/blacklist/{ip}",   drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    /**
     * @brief 查询动态封禁列表
     * 
     * 返回由速率限制器自动封禁的 IP 列表。
     * 每个 IP 包含剩余封禁时间和被封禁次数。
     * 
     * @param req HTTP 请求对象
     * @param cb 响应回调函数
     * 
     * @note 需要权限：monitor:ip:list
     */
    void bannedList(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:ip:list");  ///< 权限检查
        auto list = RateLimiter::instance().bannedList();  ///< 获取动态封禁列表
        Json::Value arr(Json::arrayValue);  ///< 创建 JSON 数组
        for (auto& e : list) {  ///< 遍历每个被封禁的 IP
            Json::Value j;  ///< 创建 JSON 对象
            j["ip"]          = e.ip;  ///< IP 地址
            j["remainSecs"]  = (Json::Int64)e.remainSecs;  ///< 剩余封禁时间（秒）
            j["banCount"]    = e.banCount;  ///< 被封禁次数
            arr.append(j);  ///< 添加到数组
        }
        RESP_OK(cb, arr);  ///< 返回成功响应
    }

    /**
     * @brief 手动解除 IP 封禁
     * 
     * 管理员可以手动解除某个 IP 的动态封禁。
     * 操作会被记录到操作日志。
     * 
     * @param req HTTP 请求对象
     * @param cb 响应回调函数
     * @param ip 要解禁的 IP 地址
     * 
     * @note 需要权限：monitor:ip:unban
     */
    void unban(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb,
               const std::string& ip) {
        CHECK_PERM(req, cb, "monitor:ip:unban");  ///< 权限检查
        RateLimiter::instance().unban(ip);  ///< 调用速率限制器解禁 IP
        LOG_OPER_PARAM(req, "IP封禁管理", BusinessType::CLEAN, ip);  ///< 记录操作日志
        RESP_MSG(cb, "解封成功");  ///< 返回成功消息
    }

    /**
     * @brief 查询静态黑名单
     * 
     * 返回系统配置中的 IP 黑名单（sys.login.blackIPList）。
     * 黑名单中的 IP 将被永久拒绝登录。
     * 
     * @param req HTTP 请求对象
     * @param cb 响应回调函数
     * 
     * @note 需要权限：monitor:ip:list
     */
    void blacklist(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:ip:list");  ///< 权限检查
        std::string raw = getBlacklistRaw();  ///< 从数据库获取黑名单（逗号分隔）
        Json::Value arr(Json::arrayValue);  ///< 创建 JSON 数组
        if (!raw.empty()) {  ///< 如果黑名单不为空
            std::istringstream ss(raw);  ///< 创建字符串流
            std::string token;  ///< 临时变量存储每个 IP
            while (std::getline(ss, token, ',')) {  ///< 按逗号分割
                auto ip = trim(token);  ///< 去除空白字符
                if (!ip.empty()) arr.append(ip);  ///< 添加到数组
            }
        }
        RESP_OK(cb, arr);  ///< 返回成功响应
    }

    /**
     * @brief 添加 IP 到静态黑名单
     * 
     * 将新的 IP 地址添加到黑名单。
     * 操作会更新数据库和缓存，并记录到操作日志。
     * 
     * 请求体格式：{ "ip": "192.168.1.1" }
     * 
     * @param req HTTP 请求对象
     * @param cb 响应回调函数
     * 
     * @note 需要权限：monitor:ip:edit
     */
    void addBlacklist(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "monitor:ip:edit");  ///< 权限检查
        auto body = req->getJsonObject();  ///< 获取请求体 JSON
        if (!body || !(*body).isMember("ip")) { RESP_ERR(cb, "缺少 ip 参数"); return; }  ///< 验证 ip 参数
        std::string newIp = trim((*body)["ip"].asString());  ///< 获取并清理 IP
        if (newIp.empty()) { RESP_ERR(cb, "ip 不能为空"); return; }  ///< 验证 IP 非空

        std::string raw = getBlacklistRaw();  ///< 获取当前黑名单
        auto parts = split(raw);  ///< 分割为 IP 列表
        for (auto& p : parts) if (p == newIp) { RESP_ERR(cb, "该 IP 已在黑名单中"); return; }  ///< 检查重复

        parts.push_back(newIp);  ///< 添加新 IP
        std::string newVal = join(parts);  ///< 重新合并为字符串
        saveBlacklist(newVal);  ///< 保存到数据库和缓存
        LOG_OPER_PARAM(req, "IP黑名单", BusinessType::INSERT, newIp);  ///< 记录操作日志
        RESP_MSG(cb, "添加成功");  ///< 返回成功消息
    }

    /**
     * @brief 从静态黑名单删除 IP
     * 
     * 将 IP 地址从黑名单中移除。
     * 操作会更新数据库和缓存，并记录到操作日志。
     * 
     * @param req HTTP 请求对象
     * @param cb 响应回调函数
     * @param ip 要删除的 IP 地址
     * 
     * @note 需要权限：monitor:ip:edit
     */
    void delBlacklist(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                      const std::string& ip) {
        CHECK_PERM(req, cb, "monitor:ip:edit");  ///< 权限检查
        std::string raw = getBlacklistRaw();  ///< 获取当前黑名单
        auto parts = split(raw);  ///< 分割为 IP 列表
        parts.erase(std::remove(parts.begin(), parts.end(), ip), parts.end());  ///< 删除指定 IP
        saveBlacklist(join(parts));  ///< 保存到数据库和缓存
        LOG_OPER_PARAM(req, "IP黑名单", BusinessType::REMOVE, ip);  ///< 记录操作日志
        RESP_MSG(cb, "删除成功");  ///< 返回成功消息
    }

private:
    /**
     * @brief 从数据库获取黑名单原始值
     * 
     * 查询 sys_config 表中 sys.login.blackIPList 配置项的值。
     * 返回逗号分隔的 IP 字符串。
     * 
     * @return 黑名单字符串（逗号分隔），如果不存在返回空字符串
     */
    std::string getBlacklistRaw() {
        auto res = DatabaseService::instance().queryParams(  ///< 执行数据库查询
            "SELECT config_value FROM sys_config WHERE config_key='sys.login.blackIPList' LIMIT 1", {});
        return (res.ok() && res.rows() > 0) ? res.str(0, 0) : "";  ///< 返回第一行第一列，或空字符串
    }

    /**
     * @brief 保存黑名单到数据库和缓存
     * 
     * 更新 sys_config 表中的黑名单配置，同时更新内存缓存。
     * 
     * @param val 新的黑名单值（逗号分隔的 IP 字符串）
     */
    void saveBlacklist(const std::string& val) {
        DatabaseService::instance().execParams(  ///< 执行数据库更新
            "UPDATE sys_config SET config_value=$1 WHERE config_key='sys.login.blackIPList'",
            {val});
        MemCache::instance().setString("sys_config:sys.login.blackIPList", val);  ///< 更新缓存
    }

    /**
     * @brief 按逗号分割字符串为 IP 列表
     * 
     * 将逗号分隔的 IP 字符串分割为向量，自动去除空白和空字符串。
     * 
     * @param s 逗号分隔的 IP 字符串
     * @return IP 地址向量
     */
    std::vector<std::string> split(const std::string& s) {
        std::vector<std::string> v;  ///< 结果向量
        std::istringstream ss(s);  ///< 创建字符串流
        std::string t;  ///< 临时变量
        while (std::getline(ss, t, ',')) {  ///< 按逗号分割
            auto p = trim(t);  ///< 去除空白
            if (!p.empty()) v.push_back(p);  ///< 添加非空 IP
        }
        return v;  ///< 返回 IP 列表
    }

    /**
     * @brief 将 IP 列表合并为逗号分隔字符串
     * 
     * @param v IP 地址向量
     * @return 逗号分隔的 IP 字符串
     */
    std::string join(const std::vector<std::string>& v) {
        std::string r;  ///< 结果字符串
        for (size_t i = 0; i < v.size(); i++) {  ///< 遍历每个 IP
            if (i) r += ',';  ///< 添加分隔符（第一个 IP 前不添加）
            r += v[i];  ///< 添加 IP
        }
        return r;  ///< 返回合并后的字符串
    }

    /**
     * @brief 去除字符串首尾的空白字符
     * 
     * 移除字符串开头和结尾的空格、制表符、换行符等。
     * 
     * @param s 输入字符串
     * @return 去除空白后的字符串
     */
    static std::string trim(std::string s) {
        /// 去除开头的空白字符
        s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            [](unsigned char c){ return !std::isspace(c); }));
        /// 去除结尾的空白字符
        s.erase(std::find_if(s.rbegin(), s.rend(),
            [](unsigned char c){ return !std::isspace(c); }).base(), s.end());
        return s;  ///< 返回修剪后的字符串
    }
};
