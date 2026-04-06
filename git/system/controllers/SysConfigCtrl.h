#pragma once
#include "../../common/OperLogUtils.h"
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../common/PageUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"
#include "../services/SysConfigService.h"
#include "../../services/KoboldCppManager.h"
#include "../../services/KoboldCppService.h"
#include "../../services/NginxManager.h"
#include "../../services/DdnsGoManager.h"
#include <fstream>
#include <json/json.h>

class SysConfigCtrl : public drogon::HttpController<SysConfigCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysConfigCtrl::list,         "/system/config/list",              drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysConfigCtrl::getById,      "/system/config/{id}",              drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysConfigCtrl::getByKey,     "/system/config/configKey/{key}",   drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(SysConfigCtrl::add,          "/system/config",                   drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(SysConfigCtrl::edit,         "/system/config",                   drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(SysConfigCtrl::remove,       "/system/config/{ids}",             drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(SysConfigCtrl::refreshCache, "/system/config/refreshCache",      drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:config:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        std::string sql = "SELECT config_id,config_name,config_key,config_value,config_type,create_time FROM sys_config WHERE 1=1";
        std::vector<std::string> params;
        int idx = 1;
        auto configName = req->getParameter("configName");
        auto configKey  = req->getParameter("configKey");
        auto configType = req->getParameter("configType");
        if (!configName.empty()) { sql += " AND config_name LIKE $" + std::to_string(idx++); params.push_back("%" + configName + "%"); }
        if (!configKey.empty())  { sql += " AND config_key LIKE $" + std::to_string(idx++); params.push_back("%" + configKey + "%"); }
        if (!configType.empty()) { sql += " AND config_type=$" + std::to_string(idx++); params.push_back(configType); }

        std::string countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
        auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
        long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;

        sql += " ORDER BY config_id LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());

        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["configId"]    = res.intVal(i, 0);
            j["configName"]  = res.str(i, 1);
            j["configKey"]   = res.str(i, 2);
            j["configValue"] = res.str(i, 3);
            j["configType"]  = res.str(i, 4);
            j["createTime"]  = fmtTs(res.str(i, 5));
            rows.append(j);
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void getById(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, int id) {
        CHECK_PERM(req, cb, "system:config:query");
        auto res = DatabaseService::instance().queryParams(
            "SELECT config_id,config_name,config_key,config_value,config_type,remark FROM sys_config WHERE config_id=$1",
            {std::to_string(id)});
        if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "配置不存在"); return; }
        Json::Value j;
        j["configId"]    = res.intVal(0, 0);
        j["configName"]  = res.str(0, 1);
        j["configKey"]   = res.str(0, 2);
        j["configValue"] = res.str(0, 3);
        j["configType"]  = res.str(0, 4);
        j["remark"]      = res.str(0, 5);
        RESP_OK(cb, j);
    }

    void getByKey(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &key) {
        auto val = SysConfigService::instance().selectConfigByKey(key);
        RESP_OK(cb, Json::Value(val));
    }

    void add(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:config:add");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
        std::string configKey = (*body)["configKey"].asString();
        if (!SysConfigService::instance().checkConfigKeyUnique(configKey))
            { RESP_ERR(cb, "新增参数'" + (*body)["configName"].asString() + "'失败，参数键名已存在"); return; }
        DatabaseService::instance().execParams(
            "INSERT INTO sys_config(config_name,config_key,config_value,config_type,remark,create_by,create_time) VALUES($1,$2,$3,$4,$5,$6,NOW())",
            {(*body)["configName"].asString(), configKey, (*body)["configValue"].asString(),
             (*body).get("configType","N").asString(), (*body).get("remark","").asString(), GET_USER_NAME(req)});
        MemCache::instance().setString(Constants::SYS_CONFIG_KEY + configKey, (*body)["configValue"].asString());
        LOG_OPER(req, "参数设置", BusinessType::INSERT);
        RESP_MSG(cb, "操作成功");
    }

    void edit(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:config:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
        int configId = (*body)["configId"].asInt();
        std::string configKey = (*body)["configKey"].asString();
        if (!SysConfigService::instance().checkConfigKeyUnique(configKey, configId))
            { RESP_ERR(cb, "修改参数失败，参数键名已存在"); return; }
        std::string newValue = (*body)["configValue"].asString();
        DatabaseService::instance().execParams(
            "UPDATE sys_config SET config_name=$1,config_key=$2,config_value=$3,config_type=$4,remark=$5,update_by=$6,update_time=NOW() WHERE config_id=$7",
            {(*body)["configName"].asString(), configKey, newValue,
             (*body).get("configType","N").asString(), (*body).get("remark","").asString(),
             GET_USER_NAME(req), std::to_string(configId)});
        MemCache::instance().setString(Constants::SYS_CONFIG_KEY + configKey, newValue);
        // 子进程动态启停
        if (configKey.rfind("sys.subprocess.", 0) == 0) {
            std::string procName = configKey.substr(15);
            bool enable = (newValue == "true");
            std::string errMsg = applySubprocess(procName, enable);
            if (!errMsg.empty()) { RESP_ERR(cb, errMsg); return; }
        }
        LOG_OPER(req, "参数设置", BusinessType::UPDATE);
        RESP_MSG(cb, "操作成功");
    }

    void remove(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &ids) {
        CHECK_PERM(req, cb, "system:config:remove");
        auto& db = DatabaseService::instance();
        for (auto &idStr : splitIds(ids)) {
            auto r = db.queryParams("SELECT config_key,config_type FROM sys_config WHERE config_id=$1", {idStr});
            if (r.ok() && r.rows() > 0) {
                if (r.str(0, 1) == "Y") { RESP_ERR(cb, "系统参数不能删除"); return; }
                MemCache::instance().remove(Constants::SYS_CONFIG_KEY + r.str(0, 0));
            }
            db.execParams("DELETE FROM sys_config WHERE config_id=$1", {idStr});
        }
        LOG_OPER_PARAM(req, "参数设置", BusinessType::REMOVE, ids);
        RESP_MSG(cb, "操作成功");
    }

    void refreshCache(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "system:config:remove");
        SysConfigService::instance().resetConfigCache();
        LOG_OPER(req, "参数设置", BusinessType::CLEAN);
        RESP_MSG(cb, "操作成功");
    }

private:
    // 读 config.json，按子进程名称启动或停止对应管理器
    // 返回空字符串表示成功，非空表示错误消息
    std::string applySubprocess(const std::string& name, bool enable) {
        Json::Value root;
        {
            std::ifstream f("config.json");
            if (f.is_open()) {
                Json::CharReaderBuilder rb; std::string e;
                Json::parseFromStream(rb, f, &root, &e);
            }
        }

        if (name == "koboldcpp") {
            if (!enable) { KoboldCppManager::instance().stop(); return ""; }
            if (KoboldCppManager::instance().isRunning()) return "";
            auto& k = root["koboldcpp"];
            KoboldCppConfig kc;
            kc.launchCmd   = k.get("launch_cmd",  "").asString();
            kc.pythonExe   = k.get("python",      "python").asString();
            kc.scriptPath  = k.get("script",      "").asString();
            kc.modelPath   = k.get("model_path",  "").asString();
            kc.port        = k.get("port",         5001).asInt();
            kc.threads     = k.get("threads",      4).asInt();
            kc.contextSize = k.get("context_size", 2048).asInt();
            kc.blasBatch   = k.get("blas_batch",   512).asInt();
            kc.useGpu      = k.get("use_gpu",      false).asBool();
            kc.gpuLayers   = k.get("gpu_layers",   99).asInt();
            kc.workDir     = k.get("work_dir",     "").asString();
            kc.autoRestart = k.get("auto_restart", true).asBool();
            kc.showWindow  = k.get("show_window",  false).asBool();
            // 检查可执行文件 / 脚本是否存在
            if (!kc.launchCmd.empty()) {
                std::string exe = kc.launchCmd;
                auto sp = exe.find(' ');
                if (sp != std::string::npos) exe = exe.substr(0, sp);
                if (!exe.empty() && exe.front() == '"') exe = exe.substr(1, exe.rfind('"') - 1);
                if (!exe.empty() && !std::ifstream(exe).good())
                    return "无法启动 KoboldCpp：找不到可执行文件，请联系管理员";
            } else {
                std::string sf = kc.scriptPath;
                if (!kc.workDir.empty() && !kc.scriptPath.empty()) {
                    std::string c = kc.workDir + "/" + kc.scriptPath;
                    if (std::ifstream(c).good()) sf = c;
                }
                if (sf.empty() || !std::ifstream(sf).good())
                    return "无法启动 KoboldCpp：在目录中找不到脚本文件，请联系管理员";
            }
            KoboldCppService::instance().setPort(kc.port);
            KoboldCppManager::instance().start(kc);
            return "";
        }

        if (name == "nginx") {
            if (!enable) { NginxManager::instance().stop(); return ""; }
            if (NginxManager::instance().isRunning()) return "";
            auto& n = root["nginx"];
            NginxConfig nc;
            nc.exePath = n.get("exe_path", "nginx/nginx.exe").asString();
            nc.prefix  = n.get("prefix",   "nginx/").asString();
            nc.port    = n.get("port",      18081).asInt();
            if (!std::ifstream(nc.exePath).good())
                return "无法启动 Nginx：找不到可执行文件，请联系管理员";
            NginxManager::instance().init(nc);
            NginxManager::instance().start();
            return "";
        }

        if (name == "ddns") {
            if (!enable) { DdnsGoManager::instance().stop(); return ""; }
            if (DdnsGoManager::instance().isRunning()) return "";
            auto& d = root["ddns"];
            DdnsGoConfig dc;
            dc.enabled    = true;
            dc.exePath    = d.get("exe_path",     "").asString();
            dc.configPath = d.get("config_path",  "").asString();
            dc.frequency  = d.get("frequency",    300).asInt();
            dc.listenAddr = d.get("listen",        ":39876").asString();
            dc.noWeb      = d.get("no_web",        false).asBool();
            dc.skipVerify = d.get("skip_verify",   false).asBool();
            dc.showWindow = d.get("show_window",   false).asBool();
            if (!std::ifstream(dc.exePath).good())
                return "无法启动 DDNS-go：找不到可执行文件，请联系管理员";
            DdnsGoManager::instance().start(dc);
            return "";
        }

        return "";
    }

    std::vector<std::string> splitIds(const std::string &ids) {
        std::vector<std::string> result;
        std::stringstream ss(ids);
        std::string item;
        while (std::getline(ss, item, ','))
            if (!item.empty()) result.push_back(item);
        return result;
    }
};
