/**
 * @file main.cc
 * @brief RuoYi-C++ 应用程序主入口
 * 
 * 功能概述：
 *   - 应用初始化：OpenSSL、日志、配置加载、数据库连接
 *   - 多进程编排：支持多 Worker 进程，由 WorkerOrchestrator 管理
 *   - Watchdog 集成：自动启动 watchdog 进程，防止单实例冲突
 *   - 许可证校验：企业版许可证验证和监控
 *   - HTTP 服务器：基于 Drogon 框架的高性能 Web 服务
 *   - 缓存预热：启动时预加载配置、字典、菜单等数据
 *   - 优雅关闭：支持 SIGTERM/SIGINT 信号，确保资源正确释放
 * 
 * 启动流程：
 *   1. 初始化日志系统（ColorLogger）和崩溃处理（CrashHandler）
 *   2. OpenSSL 全局初始化（主线程单线程完成）
 *   3. Watchdog 检测和移交（直接双击 exe 时转交给 watchdog）
 *   4. 单实例锁检查（防止多个主进程同时运行）
 *   5. 多进程编排（如果 worker_processes > 1，由 WorkerOrchestrator 管理）
 *   6. 许可证校验（企业版功能）
 *   7. 配置加载（config.json）
 *   8. 数据库连接（PostgreSQL 或 SQLite）
 *   9. 缓存预热（配置、字典、菜单等）
 *   10. HTTP 服务器启动（监听指定端口）
 *   11. 信号处理和优雅关闭
 * 
 * 关键特性：
 *   - 多进程支持：通过 WorkerOrchestrator 管理多个 Worker 进程
 *   - 热重载：支持 config.json 热重载（不需要重启应用）
 *   - 缓存策略：多层缓存（内存、Redis）加速数据访问
 *   - 日志系统：彩色控制台输出、结构化日志、日志搜索
 *   - 监控告警：性能指标收集、实时监控、告警规则
 *   - 安全防护：XSS 防御、SQL 注入防御、限流、签名验证
 *   - 企业功能：许可证管理、硬件指纹、LDAP 认证、OAuth2
 * 
 * 配置文件（config.json）：
 *   - app.port: HTTP 服务器监听端口（默认 18080）
 *   - app.worker_processes: Worker 进程数（默认 1）
 *   - database.type: 数据库类型（postgresql/sqlite）
 *   - database.host: 数据库主机
 *   - database.port: 数据库端口
 *   - database.dbname: 数据库名称
 *   - database.user: 数据库用户
 *   - database.passwd: 数据库密码
 *   - cache.type: 缓存类型（memory/redis）
 *   - cache.redis.host: Redis 主机
 *   - cache.redis.port: Redis 端口
 * 
 * 环境变量：
 *   - RUOYI_WORKER_INDEX: Worker 进程索引（由 WorkerOrchestrator 设置）
 *   - RUOYI_NO_PAUSE: 禁用错误时的暂停（用于自动化脚本）
 * 
 * 编译选项：
 *   - RUOYI_USE_EMBEDDED_FRONTEND: 嵌入前端资源（减少外部依赖）
 *   - _WIN32: Windows 平台特定代码
 * 
 * 依赖模块：
 *   - Drogon: HTTP 框架
 *   - PostgreSQL libpq: 数据库驱动
 *   - OpenSSL: 加密和 TLS 支持
 *   - JSON: 配置和数据处理
 * 
 * 拆分说明（main/ 目录）：
 *   - main/EarlyInit.cc      —— 早期初始化：横幅、工作目录、watchdog 移交、
 *                               单实例锁、WorkerOrchestrator 多进程编排
 *   - main/ConfigInit.cc     —— 配置阶段：--config、DefaultConfig、许可证、
 *                               集群、schema 校验、日志系统、stdout 重定向、
 *                               ConfigLoader、Vault、缓存、JWT、安全配置
 *   - main/HttpSetup.cc      —— HTTP 装配：CORS、前端托管、Metrics、HotConfig、
 *                               NginxLike、WAF、限流、Bot UA、错误处理、
 *                               安全头、API 鉴权 advice、各类定时任务
 *   - main/RoutesSetup.cc    —— 内置路由：/profile、iconfont、/health、
 *                               /version、/api/video/*、/ssl-config
 *   - main/CertRoutes.cc     —— /certmanager 页面 + API、/api/ssl/*
 *   - main/StartupAdvice.cc  —— registerBeginningAdvice：DB 初始化、
 *                               菜单 URL 校正、设备绑定、子进程、启动横幅
 *   - main/RuntimeSetup.cc   —— JobScheduler、Token 恢复、日志归档、HTTPS、
 *                               NginxEmbedded、ACME、TaskQueue、心跳、退出清理
 *   - main/DbConnStr.cc      —— buildDbConnStr（libpq 连接串）
 *   - main/AppBootstrap.h    —— 共享头：includes、AppContext、AuthHelper、声明
 * 
 * @see WorkerOrchestrator - 多进程编排器
 * @see DatabaseService - 数据库服务
 * @see ConfigLoader - 配置加载器
 * @see CacheStrategy - 缓存策略
 * @see LicenseManager - 许可证管理器
 */

#include "main/main/AppBootstrap.h"

/**
 * @brief RuoYi-C++ 应用程序主函数
 * 
 * 应用程序的入口点，负责初始化所有系统组件并启动 HTTP 服务器。
 * 
 * 启动流程：
 *   1. **日志和崩溃处理初始化**
 *      - ColorLogger::install() - 启用彩色控制台输出
 *      - CrashHandler::install() - 安装崩溃处理器
 *   
 *   2. **OpenSSL 初始化**
 *      - ruoyi_init_openssl_provider() - 主线程单线程初始化
 *   
 *   3. **工作目录设置**
 *      - 将工作目录切换到 exe 所在目录
 *      - 确保能找到 config.json 和其他配置文件
 *   
 *   4. **Watchdog 检测和移交**
 *      - 如果检测到 watchdog，自动移交给 watchdog 进程
 *      - 防止直接双击 exe 时的多实例问题
 *   
 *   5. **单实例锁检查**
 *      - 仅在由 watchdog 启动时检查
 *      - 防止多个主进程同时运行
 *   
 *   6. **多进程编排**
 *      - 如果 worker_processes > 1，由 WorkerOrchestrator 管理
 *      - 否则进入单进程模式
 *   
 *   7. **许可证校验**
 *      - 检查企业版许可证有效性
 *      - 启动许可证监控线程
 *   
 *   8. **配置加载**
 *      - 从 config.json 加载配置
 *      - 支持环境变量覆盖
 *      - 支持 Vault 密钥管理
 *   
 *   9. **数据库连接**
 *      - 连接 PostgreSQL 或 SQLite
 *      - 初始化连接池
 *   
 *   10. **缓存预热**
 *       - 加载配置、字典、菜单等数据到缓存
 *       - 加速首次请求
 *   
 *   11. **HTTP 服务器启动**
 *       - 启动 Drogon HTTP 服务器
 *       - 监听指定端口
 *   
 *   12. **信号处理和优雅关闭**
 *       - 捕获 SIGTERM/SIGINT 信号
 *       - 优雅关闭所有资源
 * 
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 *            - --config <file>: 指定配置文件路径（默认 config.json）
 *            - --launched-by-watchdog: 由 watchdog 启动的标记（内部使用）
 * 
 * @return 退出码
 *         - 0: 正常退出
 *         - 1: 初始化失败
 *         - 2: 单实例锁冲突（已有其他实例在运行）
 *         - 其他: 运行时错误
 * 
 * @note 
 *   - 此函数不会返回，除非发生错误或收到关闭信号
 *   - 所有异常都会被捕获并记录到日志
 *   - Windows 下会自动设置控制台 UTF-8 编码
 *   - 支持 RUOYI_WORKER_INDEX 环境变量（由 WorkerOrchestrator 设置）
 *   - 具体实现已拆分到 main/ 目录下的各阶段文件中，
 *     本函数只负责按原顺序编排调用
 * 
 * @see WorkerOrchestrator - 多进程编排器
 * @see ConfigLoader - 配置加载器
 * @see DatabaseService - 数据库服务
 * @see CacheStrategy - 缓存策略
 * @see LicenseManager - 许可证管理器
 */
int main(int argc, char* argv[]) {
    ColorLogger::install(); // 开启彩色控制台输出（Windows VT + trantor 拦截）
    // 最先安装崩溃日志捕获，确保任何时刻崩溃都有记录
    CrashHandler::install("./logs");

    // ── 主线程单线程下完成 OpenSSL 全局 init ─────────────────────
    // 不能延迟到 drogon worker 线程 lazy 加载，否则与 libpq.dll 间接依赖的
    // system OpenSSL DLL 在多线程并发时引发 RtlReAllocateHeap 堆损坏崩溃
    ruoyi_init_openssl_provider();

// 单实例锁已移至 watchdog 检测之后（见 EarlyInit.cc），确保双击 exe 时始终能正确移交给 watchdog

    AppContext ctx;

    try {
        // 各阶段函数返回 std::optional<int>：
        //   std::nullopt → 继续后续阶段
        //   有值         → 该值即进程退出码（对应原 main() 中的 return 语句）

        // ── 早期初始化：横幅、工作目录、watchdog 移交、单实例锁、多进程编排 ──
        if (auto code = boot::earlyInit(argc, argv, ctx))   return *code;

        // ── 配置阶段：--config、许可证、集群、schema、日志、Vault、缓存、安全 ──
        if (auto code = boot::configInit(argc, argv, ctx))  return *code;

        // ── HTTP 装配：CORS、前端托管、WAF、限流、错误处理、定时任务 ──────────
        if (auto code = boot::setupHttp(ctx))               return *code;

        // ── 路由注册：内置路由 + certmanager/ssl API ──────────────────────
        boot::registerBuiltinRoutes(ctx);
        boot::registerCertRoutes(ctx);

        // ── BeginningAdvice：DB 初始化、菜单 URL 校正、子进程启动 ──────────
        boot::registerStartupAdvice(ctx);

        // ── 运行时服务：JobScheduler、HTTPS、ACME、TaskQueue、心跳 ─────────
        boot::startRuntimeServices(ctx);

        // ── 进入 drogon 事件循环（阻塞直至退出）────────────────────────────
        drogon::app().run();

        // ── 退出清理：心跳线程、TaskQueue、ACME、NginxEmbedded ─────────────
        boot::shutdownCleanup();
    } catch (const std::exception &e) {
        std::cerr << "[致命错误] " << e.what() << std::endl;
        ELOG_FATAL("main", std::string("未捕获异常: ") + e.what());
        std::cout << "按回车键退出..." << std::endl;
        std::cin.get();
        return 1;
    } catch (...) {
        std::cerr << "[致命错误] 未知异常" << std::endl;
        ELOG_FATAL("main", "未捕获未知异常");
        std::cout << "按回车键退出..." << std::endl;
        std::cin.get();
        return 1;
    }
    return 0;
}
