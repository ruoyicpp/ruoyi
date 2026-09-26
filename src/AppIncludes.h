/**
 * @file AppIncludes.h
 * @brief RuoYi-C++ 应用程序统一头文件包含
 * 
 * 功能概述：
 *   - 集中管理所有项目头文件的包含
 *   - 按功能模块分类组织（安全、认证、配置、日志、缓存等）
 *   - 避免循环依赖和重复包含
 *   - 提供清晰的模块依赖关系
 * 
 * 包含的模块：
 *   1. **标准库** - C++ 标准库和平台特定库
 *   2. **框架库** - Drogon HTTP 框架和 Trantor 网络库
 *   3. **数据库** - SQL 方言兼容层和数据库适配器
 *   4. **安全模块** - JWT、OAuth2、LDAP、加密等
 *   5. **配置与日志** - 配置加载、日志输出、指标收集
 *   6. **监控运维** - 链路追踪、数据脱敏、错误处理
 *   7. **性能优化** - 缓存、并发控制、批处理
 *   8. **服务层** - 业务服务（Nginx、DDNS、存储等）
 *   9. **控制器** - HTTP 控制器（系统、监控、工具等）
 *   10. **中间件** - 过滤器、拦截器、调度器
 * 
 * 使用方式：
 *   在所有 .cc 文件中包含此头文件即可获得所有依赖：
 *   ```cpp
 *   #include "AppIncludes.h"
 *   ```
 * 
 * 模块依赖关系：
 *   ```
 *   标准库 + Drogon
 *       ↓
 *   数据库层（SQL 兼容、适配器）
 *       ↓
 *   基础工具（安全、加密、配置、日志）
 *       ↓
 *   服务层（业务服务、缓存、存储）
 *       ↓
 *   控制器层（HTTP 端点）
 *       ↓
 *   中间件（过滤器、拦截器）
 *   ```
 * 
 * 编译优化：
 *   - 使用 #pragma once 防止重复包含
 *   - 按包含顺序避免前向声明问题
 *   - 条件编译支持 Windows/Linux 平台差异
 * 
 * @note 
 *   - 此文件应在所有 .cc 文件中首先包含
 *   - 不应在 .h 文件中包含（避免循环依赖）
 *   - 如需添加新模块，请在相应分类下添加
 * 
 * @see main.cc - 应用程序入口
 * @see CMakeLists.txt - 编译配置
 */

#pragma once

/**
 * @defgroup StdLib 标准库
 * @brief C++ 标准库和平台特定库
 * @{
 */

// ── 标准库 ────────────────────────────────────────────────────────
#include <iostream>      ///< 标准输入输出
#include <fstream>       ///< 文件输入输出
#include <filesystem>    ///< 文件系统操作（C++17）
#include <sstream>       ///< 字符串流
#include <chrono>        ///< 时间和计时
#include <ctime>         ///< C 风格时间函数
#include <thread>        ///< 多线程支持
#include <iomanip>       ///< 输入输出操纵符
#include <mutex>         ///< 互斥锁
#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
#  include <shlobj.h>
#else
#  include <unistd.h>
#  include <sys/statvfs.h>
#  include <sys/resource.h>
#endif

/// @}

/**
 * @defgroup Framework 框架库
 * @brief HTTP 框架和网络库
 * @{
 */

// ── Drogon / Trantor ─────────────────────────────────────────────
#include <drogon/drogon.h>           ///< Drogon HTTP 框架
#include <trantor/utils/Logger.h>    ///< Trantor 日志库

/// @}

/**
 * @defgroup Database 数据库层
 * @brief SQL 方言兼容和数据库适配
 * @{
 */

// ── 数据库 SQL 宏定义 ─────────────────────────────────────────────
// MySQL 和 PostgreSQL 兼容层（编译时自动转换）
#include "mysql/db_sql_map.h"        ///< SQL 方言宏定义（编译时转换）

// ── 数据库运行时适配层 ───────────────────────────────────────────
// 支持 MySQL 和 PostgreSQL 同时使用（运行时动态选择）
#include "mysql/DatabaseAdapter.h"   ///< 数据库适配器（运行时选择驱动）

/// @}

/**
 * @defgroup CommonTools 公共工具
 * @brief 基础工具类和通用功能
 * @{
 */

/**
 * @defgroup Security 安全模块
 * @brief 认证、授权、加密相关功能
 * @{
 */

// ── 公共基础 ─────────────────────────────────────────────────────
// 【安全模块】— 认证、授权、加密
#include "common/CrashHandler.h"       ///< 崩溃捕获（SEH/VEH/terminate）+ 生成 dump
#include "common/JwtUtils.h"           ///< JWT 生成/解析（HS256/RS256）
#include "common/TokenCache.h"         ///< Token 缓存（内存+Redis 双后端）
#include "common/RuoYiException.h"     ///< 统一异常类（Auth/Validate/NotFound 等）
#include "common/ApiVersion.h"         ///< API 版本控制 + CSRF 防护 + 安全响应头 + 请求签名
#include "common/SignUtils.h"          ///< API 请求签名验证
#include "common/RateLimiter.h"        ///< IP 限流（滑动窗口 + 自动封禁）
#include "common/XssUtils.h"           ///< XSS 过滤 + SQL 注入检测
#include "common/DeviceBinding.h"      ///< 设备绑定（硬件指纹）
#include "common/SslManager.h"         ///< SSL 证书管理（Let's Encrypt/商业证书）
#include "common/TotpUtils.h"          ///< TOTP 两步验证（RFC6238）

/// @}

/**
 * @defgroup Encryption 加密模块
 * @brief 国密和国际加密算法
 * @{
 */

// 【加密模块】— 国密与国际算法
#include "common/GmCrypto.h"           ///< 国密算法（SM2/SM3/SM4、SM9）

/// @}

/**
 * @defgroup Authentication 认证授权
 * @brief 第三方登录和许可证管理
 * @{
 */

// 【认证授权】— 第三方登录
#include "common/LdapAuth.h"           ///< LDAP/Active Directory 认证
#include "common/OAuth2Manager.h"      ///< OAuth2 第三方登录（GitHub/Google/钉钉等）
#include "common/LicenseManager.h"     ///< 软件许可证管理

/// @}

/**
 * @defgroup Config 配置与日志
 * @brief 配置管理、日志输出、指标收集
 * @{
 */

// 【配置与日志】— 配置管理、日志输出
#include "common/ConfigLoader.h"       ///< YAML/JSON 配置文件加载
#include "common/ConfigValidator.h"    ///< 配置验证（环境变量+多环境 dev/staging/prod）
#include "common/HotConfig.h"          ///< 配置文件热重载
#include "common/JsonLogger.h"         ///< JSON 结构化日志（.jsonl 格式，ELK 友好）
#include "common/StructuredLogger.h"   ///< 增强型结构化日志（多输出+脱敏+异步）
#include "common/ErrorLogger.h"        ///< 错误日志记录（error.log）
#include "common/ColorLogger.h"        ///< 控制台彩色日志
#include "common/MetricsCollector.h"   ///< Prometheus 指标收集

/// @}

/**
 * @defgroup Monitoring 监控运维
 * @brief 链路追踪、数据脱敏、错误处理
 * @{
 */

// 【监控运维】— 追踪、脱敏、托管
#include "common/RequestTracing.h"     ///< X-Request-ID 全链路追踪
#include "common/DataMaskUtils.h"      ///< 敏感数据脱敏（手机号/身份证/银行卡）
#include "common/FrontendHost.h"       ///< 前端静态文件托管
#include "common/HttpStatus.h"         ///< HTTP 状态码全集 + 二进制风格别名（_11011=404）
#include "common/TraceContext.h"       ///< 分布式追踪上下文（OpenTelemetry 兼容）
#include "common/DatabaseInit.h"       ///< 自动建表 + 初始数据
#include "error_pages/ErrorPage.h"     ///< 统一错误页面（HTML+CSS 内嵌）

/// @}

/**
 * @defgroup Performance 性能优化
 * @brief 并发控制、缓存、批处理
 * @{
 */

// 【性能优化】— 并发、缓存、批处理
#include "common/Performance.h"        ///< 性能工具（读写锁/对象池/PreparedStatement 缓存/批量处理）

/// @}
/// @}

/**
 * @defgroup Services 服务层
 * @brief 业务服务、DevOps 服务、系统服务
 * @{
 */

/**
 * @defgroup DevOpsServices DevOps 服务
 * @brief Nginx、DDNS、容器管理等基础设施服务
 * @{
 */

// ── 服务层 ────────────────────────────────────────────────────────
// 【DevOps服务】— Nginx、DDNS、容器管理
#include "services/NginxManager.h"      ///< Nginx 进程管理（配置热加载）
#include "services/DdnsGoManager.h"     ///< 动态 DNS 客户端管理
#include "services/KoboldCppManager.h"  ///< KoboldCpp 服务管理
#include "services/KoboldCppService.h"  ///< KoboldCpp API 封装
#include "services/WhisperService.h"    ///< Whisper 语音识别服务
#include "services/VaultManager.h"      ///< HashiCorp Vault 密钥管理
#include "services/StorageService.h"    ///< 文件存储服务（S3/本地）
#include "services/ExternalServiceManager.h"  ///< 外部服务管理器（进程监控 + 代理路由）

/// @}

/**
 * @defgroup SystemServices 系统服务
 * @brief 系统级服务（配置、字典、令牌等）
 * @{
 */

// ── System服务 ────────────────────────────────────────────────────
#include "system/services/SysConfigService.h"  ///< 系统配置服务
#include "system/services/SysDictService.h"    ///< 字典数据服务
#include "system/services/TokenService.h"      ///< Token 管理服务

/// @}
/// @}

/**
 * @defgroup Middleware 中间件
 * @brief 过滤器、拦截器、调度器
 * @{
 */

// ── 过滤器 / 调度器 ──────────────────────────────────────────────
#include "filters/JwtAuthFilter.h"    ///< JWT 认证过滤器
#include "monitor/JobScheduler.h"      ///< 定时任务调度器

/// @}

/**
 * @defgroup Controllers 控制器
 * @brief HTTP 控制器（系统、监控、工具等）
 * @{
 */

/**
 * @defgroup SystemControllers 系统控制器
 * @brief 用户、角色、菜单、部门等系统管理
 * @{
 */

// ── System 控制器 ────────────────────────────────────────────────
#include "system/controllers/SysLoginCtrl.h"    ///< 登录/登出/注册
#include "system/controllers/SysUserCtrl.h"     ///< 用户管理
#include "system/controllers/SysRoleCtrl.h"     ///< 角色管理
#include "system/controllers/SysMenuCtrl.h"     ///< 菜单权限
#include "system/controllers/SysDeptCtrl.h"     ///< 部门管理
#include "system/controllers/SysConfigCtrl.h"   ///< 系统配置
#include "system/controllers/SysDictCtrl.h"     ///< 字典数据
#include "system/controllers/SysPostCtrl.h"     ///< 岗位管理
#include "system/controllers/SysNoticeCtrl.h"   ///< 通知公告
#include "system/controllers/OaCtrl.h"          ///< 轻量 OA
#include "system/controllers/ApiKeyCtrl.h"      ///< API 密钥管理

/// @}

/**
 * @defgroup NotificationControllers 通知控制器
 * @brief 站内通知、邮件、SSL 证书等
 * @{
 */

#include "system/controllers/NotifyCtrl.h"              ///< 站内通知
#include "system/controllers/SysEmailConfigCtrl.h"      ///< 邮件配置
#include "system/controllers/SysSslConfigCtrl.h"        ///< SSL 证书配置
#include "system/controllers/SysCertManagerCtrl.h"      ///< 证书管理
#include "system/controllers/SysTotpCtrl.h"             ///< TOTP 两步验证
#include "system/controllers/OAuth2Ctrl.h"              ///< OAuth2 第三方登录
#include "system/controllers/SysMonitorConfigCtrl.h"    ///< 监控配置
#include "system/controllers/UnlockScreenCtrl.h"        ///< 解锁屏幕
#include "system/controllers/WsTicketCtrl.h"            ///< WebSocket 工单

/// @}

/**
 * @defgroup MonitorControllers 监控控制器
 * @brief 日志、在线用户、定时任务、服务器监控等
 * @{
 */

// ── Monitor 控制器 ────────────────────────────────────────────────
#include "monitor/controllers/SysLogCtrl.h"        ///< 操作日志
#include "monitor/controllers/SysLogFileCtrl.h"    ///< 日志文件
#include "monitor/controllers/SysOnlineCtrl.h"     ///< 在线用户
#include "monitor/controllers/WsNotifyCtrl.h"      ///< WebSocket 通知
#include "monitor/controllers/SysJobCtrl.h"        ///< 定时任务
#include "monitor/controllers/ServerCtrl.h"        ///< 服务器监控
#include "monitor/controllers/CacheCtrl.h"         ///< 缓存监控
#include "monitor/controllers/DruidCtrl.h"         ///< 数据库连接池
#include "monitor/controllers/SysIpCtrl.h"         ///< IP 黑名单
#include "monitor/controllers/SysRestartCtrl.h"    ///< 服务重启
#include "monitor/controllers/OpsCtrl.h"           ///< 运维工具

/// @}

/**
 * @defgroup TaskQueue 任务队列
 * @brief 异步任务处理系统
 * @{
 */

// ── 任务队列 ──────────────────────────────────────────────────────
#include "taskqueue/TaskQueue.h"        ///< 异步任务队列核心
#include "taskqueue/TaskQueueCtrl.h"    ///< 任务队列管理 API
#include "taskqueue/TaskQueueExample.h" ///< 任务处理器实现（email/export/process/notification）

/// @}

/**
 * @defgroup WafModule WAF 安全模块
 * @brief Web 应用防火墙（规则匹配、风控存储、内核封禁）
 * @{
 */

// ── WAF 安全模块 ──────────────────────────────────────────────────
// 【WAF】— 请求拦截、IP封禁、CIDR匹配、风控持久化
// Windows 下仅编译应用层部分（nftables/RocksDB 自动跳过）
#include "waf/CidrMatcher.h"           ///< CIDR 网段匹配（IPv4/IPv6）
#include "waf/RiskStore.h"            ///< 风控状态存储（SQLite/RocksDB）
#include "waf/NftBan.h"               ///< nftables 内核层封禁（Linux）
#include "waf/WafEngine.h"            ///< WAF 规则匹配引擎
#include "waf/WafCtrl.h"              ///< WAF 管理 API 控制器

/// @}

/**
 * @defgroup AuditModule 安全审计模块
 * @brief Manticore 审计日志上报与检索
 * @{
 */

// ── 安全审计 ──────────────────────────────────────────────────────
// 【审计】— Manticore HTTP JSON API，异步批量上报，仅 Linux
#include "audit/ManticoreClient.h"    ///< Manticore HTTP JSON 客户端
#include "audit/AuditQueue.h"         ///< 审计事件异步批量队列
#include "audit/AuditCtrl.h"          ///< 审计检索/聚合 API 控制器

/// @}

/**
 * @defgroup SsoModule SSO 单点登录模块
 * @brief OAuth2.0/OIDC 服务端（授权码流程 + 票据存储 + 接入方管理）
 * @{
 */

// ── SSO 单点登录 ──────────────────────────────────────────────────
// 【SSO】— OAuth2.0 授权码流程，票据双模（local/redis），仅 Linux
#include "sso/SsoTicketStore.h"       ///< SSO 票据存储（授权码/刷新令牌）
#include "sso/SsoServer.h"            ///< SSO 服务端核心逻辑
#include "sso/SsoCtrl.h"              ///< SSO 端点控制器

/// @}

/**
 * @defgroup RiskModule AI 风控与日志留存
 * @brief AI 网关风控 + 日志标准化清理
 * @{
 */

// ── AI 风控 / 日志留存 ────────────────────────────────────────────
// 【AI风控】— 请求特征异步评分，高风险 IP 自动封禁，仅 Linux
#include "risk/AiRiskEngine.h"        ///< AI 风控引擎（采样+批量评分+自动封禁）
// 【日志留存】— PG日志表/本地文件/审计日志统一清理策略
#include "log/LogRetention.h"         ///< 日志留存清理（每日定时任务）
#include "log/LogIndexer.h"           ///< 日志文件→Manticore 全文索引器

/// @}

/**
 * @defgroup EnterpriseModule 企业通用增强
 * @brief 多数据源 / 工作流 / 短信通道 / WS消息推送
 * @{
 */

// ── 多数据源管理 ──────────────────────────────────────────────────
// 【多数据源】— 动态注册 PG/SQLite 数据源、连通性测试、只读查询路由
#include "ds/DataSourceManager.h"     ///< 数据源注册表 + queryOn 路由
#include "ds/DataSourceCtrl.h"        ///< 数据源管理 API

// ── 工作流引擎 ────────────────────────────────────────────────────
// 【工作流】— 顺序审批流 + 或签/会签 + 流程变量，wf_* 三表
#include "flow/FlowEngine.h"          ///< 流程引擎（定义/实例/任务推进）
#include "flow/FlowCtrl.h"            ///< 流程管理 + 审批 API

// ── 短信通道 ──────────────────────────────────────────────────────
// 【短信】— 阿里云/腾讯云签名发送 + 验证码 + 模板/日志
#include "sms/SmsService.h"           ///< 短信服务（双厂商签名）
#include "sms/SmsCtrl.h"              ///< 短信 API（验证码/模板/日志）

// ── WS 消息管理 ───────────────────────────────────────────────────
// 【WS消息】— 在线用户/全员广播/定向推送（站内信走 NotifyService::sendInbox）
#include "monitor/controllers/WsMsgCtrl.h"  ///< WS 消息管理 API

/// @}

/**
 * @defgroup OpsModule 运维&可靠性
 * @brief 分布式锁 / 集群会话 / 慢SQL审计 / 备份管理
 * @{
 */

// ── 分布式锁 ──────────────────────────────────────────────────────
// 【分布式锁】— Redis SET NX EX + Lua 原子释放/续期 + 本地降级 + RAII
#include "common/DistLock.h"          ///< DistLock / DistLockGuard

// ── 集群会话同步 ──────────────────────────────────────────────────
// 【集群会话】— 跨节点在线用户注册表（Redis SET + 心跳续期）
#include "common/ClusterSession.h"    ///< 在线用户跨节点同步

// ── 缓存一致性 ────────────────────────────────────────────────────
// 【缓存同步】— Redis pub/sub 失效广播（多节点本地缓存同步）
#include "common/CacheSync.h"         ///< MemCache 失效跨节点广播

// ── 配置热更新 ────────────────────────────────────────────────────
// 【热重载】— config.json mtime 监听 + /actuator/reload 手动触发
#include "common/ConfigReloader.h"    ///< 配置热更新（无需重启）

// ── 行为验证码（go-captcha gRPC，Linux only）──────────────────────
// 【滑块/旋转验证码】— captcha-server 侧车，/captcha/get + /captcha/check
#include "captcha/CaptchaClient.h"    ///< gRPC 客户端封装（stub 安全降级）
#include "captcha/CaptchaCtrl.h"      ///< 行为验证码 API

// ── SQL 慢查询审计 ────────────────────────────────────────────────
// 【慢查询】— DbMetricsHook::slowHook → 异步队列 → sys_slow_log + 告警
#include "log/SlowLogQueue.h"         ///< 慢查询异步落库队列
#include "log/SlowLogCtrl.h"          ///< 慢查询列表/统计/清理 API

// ── 备份管理 ──────────────────────────────────────────────────────
// 【备份】— pg_dump 定时备份 + 文件管理 + pg_restore 一键恢复
#include "backup/BackupService.h"     ///< 备份服务（定时/滚动清理/恢复）
#include "backup/BackupCtrl.h"        ///< 备份管理 API

/// @}

/**
 * @defgroup IntegrationModule 配套扩展
 * @brief MQTT 设备接入 / 大屏可视化 / API 开放平台
 * @{
 */

// ── MQTT 客户端 ───────────────────────────────────────────────────
// 【MQTT】— 精简 MQTT 3.1.1 客户端：设备接入/订阅转发/在线管理
#include "iot/MqttClient.h"           ///< MQTT 客户端（自动重连+消息分发）
#include "iot/MqttCtrl.h"             ///< MQTT 设备管理 API

// ── 大屏可视化 ────────────────────────────────────────────────────
// 【大屏】— 统计指标统一输出（MetricsCollector + DB + WS + 慢查询聚合）
#include "screen/ScreenCtrl.h"        ///< /screen/overview + /screen/realtime

/// @}

/**
 * @defgroup CommonControllers 通用控制器
 * @brief 文件上传、仪表盘、许可证、国密等
 * @{
 */

// ── Common 控制器 ────────────────────────────────────────────────
#include "common/controllers/CommonCtrl.h"       ///< 通用接口（文件上传/下载）
#include "common/controllers/DashboardCtrl.h"    ///< 仪表盘
#include "common/controllers/LicenseApiCtrl.h"   ///< 许可证 API
#include "common/controllers/GmCryptoCtrl.h"     ///< 国密 API
#include "common/controllers/HealthCtrl.h"       ///< 健康检查

/// @}

/**
 * @defgroup ToolControllers 工具控制器
 * @brief 代码生成、项目构建、视频处理等
 * @{
 */

// ── Tool 控制器 ──────────────────────────────────────────────────
#include "tool/controllers/GenCtrl.h"            ///< 代码生成
#include "tool/controllers/BuildCtrl.h"          ///< 项目构建
#include "tool/controllers/WebsiteInfoCtrl.h"    ///< 网站信息
#include "tool/controllers/VideoCtrl.h"          ///< 视频处理
#include "tool/controllers/SwaggerCtrl.h"        ///< Swagger 文档

/// @}

/**
 * @defgroup AIControllers AI 控制器
 * @brief 大模型、语音识别、向量模型等 AI 服务
 * @{
 */

// ── AI ────────────────────────────────────────────────────────────
// 【AI服务】— 大模型、语音识别、向量模型
#include "ai/OnnxService.h"            ///< ONNX 向量模型服务（embedding 生成）
#include "ai/controllers/AiCtrl.h"     ///< AI 对话接口
#include "ai/AiChatCtrl.h"             ///< AI 聊天控制器
#include "ai/OnnxCtrl.h"               ///< ONNX 模型管理

/// @}

/**
 * @defgroup IoTControllers IoT 控制器
 * @brief 物联网设备管理和控制
 * @{
 */

// ── IoT ──────────────────────────────────────────────────────────
#include "iot/ModbusGateway.h"        ///< Modbus TCP/RTU 网关
#include "iot/IotCtrl.h"              ///< IoT 设备控制器

/// @}

/**
 * @defgroup IMControllers IM 控制器
 * @brief 即时通讯系统（唐僧叨叨 CGO 集成）
 * @{
 */

// ── IM 即时通讯 ──────────────────────────────────────────────────
// 【IM系统】— 唐僧叨叨 CGO DLL 集成（运行时动态加载）
#include "im/TangSengDaoDaoWrapper.h"  ///< IM 系统 C++ 包装类（单例）
#include "im/ImProxy.h"                ///< IM 系统代理（生命周期管理）
#include "im/ImCtrl.h"                 ///< IM HTTP 控制器（RESTful API）
#include "im/ImPublicCtrl.h"           ///< IM 公开控制器（无需认证）

/// @}

/**
 * @defgroup PluginSystem 插件系统
 * @brief 用户 DLL/SO 运行时加载
 * @{
 */

// ── 插件系统 ─────────────────────────────────────────────────────
// 【插件】— 用户 DLL/SO 运行时加载（前端触发热激活）
#include "libs/plugin/PluginCtrl.h"   ///< 插件管理 API

/// @}
/// @}
