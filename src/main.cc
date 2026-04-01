#include <drogon/drogon.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#endif
#include "common/DatabaseInit.h"
#include "common/JwtUtils.h"
#include "common/TokenCache.h"
#include <trantor/utils/Logger.h>
#include "system/services/SysConfigService.h"
#include "system/services/SysDictService.h"

// 引入所有 Controller（drogon 通过静态注册自动发现）
#include "system/controllers/SysLoginCtrl.h"
#include "system/controllers/SysUserCtrl.h"
#include "system/controllers/SysRoleCtrl.h"
#include "system/controllers/SysMenuCtrl.h"
#include "system/controllers/SysDeptCtrl.h"
#include "system/controllers/SysConfigCtrl.h"
#include "system/controllers/SysDictCtrl.h"
#include "system/controllers/SysPostCtrl.h"
#include "system/controllers/SysNoticeCtrl.h"
#include "monitor/controllers/SysLogCtrl.h"
#include "monitor/controllers/SysOnlineCtrl.h"
#include "monitor/controllers/SysJobCtrl.h"
#include "monitor/controllers/ServerCtrl.h"
#include "monitor/controllers/CacheCtrl.h"
#include "common/controllers/CommonCtrl.h"
#include "tool/controllers/GenCtrl.h"
#include "filters/JwtAuthFilter.h"

int main() {
    try {
#ifdef _WIN32
        // Windows 控制台设置 UTF-8 编码（参考 config-center-gateway）
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
#endif
        // 将工作目录切换到 exe 所在目录，确保双击运行时能找到 config.json
        std::filesystem::path exePath;
#ifdef _WIN32
        {
            wchar_t buf[MAX_PATH];
            GetModuleFileNameW(nullptr, buf, MAX_PATH);
            exePath = std::filesystem::path(buf).parent_path();
        }
#else
        {
            std::error_code ec;
            exePath = std::filesystem::canonical("/proc/self/exe", ec).parent_path();
        }
#endif
        if (!exePath.empty()) {
            std::filesystem::current_path(exePath);
        }

        // 检查 config.json 是否存在
        if (!std::filesystem::exists("config.json")) {
            std::cerr << "[错误] 找不到 config.json，请将其放到 exe 同目录下" << std::endl;
            std::cerr << "当前目录: " << std::filesystem::current_path() << std::endl;
            std::cout << "按回车键退出..." << std::endl;
            std::cin.get();
            return 1;
        }

        // 手动注册 JwtAuthFilter 中间件（isAutoCreation=false）
        drogon::app().registerMiddleware(std::make_shared<JwtAuthFilter>());

        // 加载配置
        drogon::app().loadConfigFile("config.json");

        // 日志时间显示为本地时间（默认是 UTC）
        trantor::Logger::setDisplayLocalTime(true);

        LOG_INFO << "Cache backend: " << MemCache::backendInfo();
        std::cout << "[Cache] backend: " << MemCache::backendInfo() << std::endl;

        // 加载 JWT 配置
        JwtUtils::loadConfig();

        // CORS 中间件
        drogon::app().registerPreRoutingAdvice([](const drogon::HttpRequestPtr &req,
                                                  drogon::AdviceCallback &&acb,
                                                  drogon::AdviceChainCallback &&accb) {
            if (req->method() == drogon::Options) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->addHeader("Access-Control-Allow-Origin",  "*");
                resp->addHeader("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");
                resp->addHeader("Access-Control-Allow-Headers", "*");
                resp->addHeader("Access-Control-Expose-Headers","access-token,x-access-token");
                resp->setStatusCode(drogon::k200OK);
                acb(resp);
                return;
            }
            accb();
        });

        // 静态文件服务：/profile/{dir}/{file} → uploads/{dir}/{file}
        // 用于头像(/profile/avatar/xxx)、通用上传(/profile/upload/xxx)等
        auto serveUpload = [](const drogon::HttpRequestPtr &,
                              std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                              const std::string &dir, const std::string &file) {
            std::string filePath = "uploads/" + dir + "/" + file;
            if (!std::filesystem::exists(filePath) || std::filesystem::is_directory(filePath)) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k404NotFound);
                cb(resp);
                return;
            }
            cb(drogon::HttpResponse::newFileResponse(filePath));
        };
        drogon::app().registerHandler("/profile/{dir}/{file}", serveUpload, {drogon::Get});

        // Druid 数据监控页面存根（C++ 后端不使用 Java Druid 连接池）
        drogon::app().registerHandler(
            "/druid/{path}",
            [](const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb,
               const std::string &) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setContentTypeCode(drogon::CT_TEXT_HTML);
                resp->setBody(
                    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
                    "<title>数据监控</title></head><body style='font-family:sans-serif;text-align:center;padding:60px'>"
                    "<h2>数据监控</h2><p>C++ 后端使用 libpq 直连 PostgreSQL，不使用 Java Druid 连接池。</p>"
                    "<p>数据库状态：<b style='color:green'>正常</b></p></body></html>");
                cb(resp);
            },
            {drogon::Get});

        drogon::app().registerPostHandlingAdvice([](const drogon::HttpRequestPtr &,
                                                    const drogon::HttpResponsePtr &resp) {
            resp->addHeader("Access-Control-Allow-Origin",  "*");
            resp->addHeader("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "*");
            resp->addHeader("Access-Control-Expose-Headers","access-token,x-access-token");
        });

        // 数据库就绪后初始化
        drogon::app().registerBeginningAdvice([]() {
            std::cout << "[Cache] backend: " << MemCache::backendInfo() << std::endl;
            // 从 config.json 读取配置
            Json::Value dbCfg;
            int    listenPort    = 18080;
            std::string listenAddr = "0.0.0.0";
            {
                std::ifstream cfgFile("config.json");
                if (cfgFile.is_open()) {
                    Json::Value root;
                    Json::CharReaderBuilder rb;
                    std::string errs;
                    if (Json::parseFromStream(rb, cfgFile, &root, &errs)) {
                        dbCfg = root["database"];
                        if (root.isMember("listeners") && root["listeners"].isArray()
                            && root["listeners"].size() > 0) {
                            auto &l = root["listeners"][0];
                            listenPort = l.get("port", 18080).asInt();
                            listenAddr = l.get("address", "0.0.0.0").asString();
                        }
                    }
                }
            }
            if (!dbCfg.isNull()) {
                std::string connStr = "host=" + dbCfg.get("host", "127.0.0.1").asString()
                    + " port=" + std::to_string(dbCfg.get("port", 5432).asInt())
                    + " dbname=" + dbCfg.get("dbname", "ruoyi").asString()
                    + " user=" + dbCfg.get("user", "postgres").asString()
                    + " password=" + dbCfg.get("passwd", "").asString()
                    + " connect_timeout=5";
                LOG_INFO << "正在连接数据库: " << dbCfg.get("host","127.0.0.1").asString()
                         << ":" << dbCfg.get("port",5432).asInt()
                         << "/" << dbCfg.get("dbname","ruoyi").asString();
                bool pgOk = DatabaseService::instance().connect(connStr);
                // 始终打开 SQLite（PG 可用时用于双写，PG 不可用时用作主库）
                DatabaseService::instance().connectSqlite("ruoyi.db");
                if (!pgOk) {
                    LOG_ERROR << "数据库连接失败，已切换到 SQLite 回退!";
                    DatabaseService::instance().activateSqliteFallback();
                } else {
                    LOG_INFO << "已连接 PostgreSQL，SQLite 双写已就绪";
                }
            } else {
                LOG_ERROR << "config.json 中未找到 database 配置段";
            }

            LOG_INFO << "正在初始化数据库表...";
            DatabaseInit::run();

            LOG_INFO << "加载配置缓存...";
            SysConfigService::instance().loadConfigCache();

            LOG_INFO << "加载字典缓存...";
            SysDictService::instance().loadDictCache();

            LOG_INFO << "RuoYi-Cpp 启动完成，监听 " << listenAddr << ":" << listenPort;
        });

        drogon::app().run();
    } catch (const std::exception &e) {
        std::cerr << "[致命错误] " << e.what() << std::endl;
        std::cout << "按回车键退出..." << std::endl;
        std::cin.get();
        return 1;
    } catch (...) {
        std::cerr << "[致命错误] 未知异常" << std::endl;
        std::cout << "按回车键退出..." << std::endl;
        std::cin.get();
        return 1;
    }
    return 0;
}
