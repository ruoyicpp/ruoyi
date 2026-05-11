<div align="center">

[English](README_EN.md) | 中文

# RuoYi-Cpp

**RuoYi 管理框架的 C++ 高性能版本**

基于 [Drogon](https://github.com/drogonframework/drogon) + PostgreSQL，与 RuoYi-Vue 前端 100% 兼容

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Drogon](https://img.shields.io/badge/Drogon-latest-green.svg)](https://github.com/drogonframework/drogon)
[![Platform](https://img.shields.io/badge/platform-Windows%20(MSYS2%20MinGW64)-brightgreen.svg)]()

[![RuoYi-Vue](https://img.shields.io/badge/RuoYi--Vue-3.8-red.svg)](https://gitee.com/y_project/RuoYi-Vue)
[![Vue](https://img.shields.io/badge/Vue-2.x-4FC08D.svg?logo=vue.js)](https://v2.vuejs.org)
[![Element UI](https://img.shields.io/badge/Element--UI-2.x-409EFF.svg)](https://element.eleme.io)
[![PostgreSQL](https://img.shields.io/badge/PostgreSQL-12%2B-336791.svg?logo=postgresql)](https://www.postgresql.org)
[![SQLite](https://img.shields.io/badge/SQLite-3-003B57.svg?logo=sqlite)](https://www.sqlite.org)
[![JWT](https://img.shields.io/badge/JWT-jwt--cpp-000000.svg?logo=jsonwebtokens)](https://github.com/Thalhammer/jwt-cpp)
[![OpenSSL](https://img.shields.io/badge/OpenSSL-3.x-721412.svg?logo=openssl)](https://www.openssl.org)
[![Nginx](https://img.shields.io/badge/Nginx-optional-009639.svg?logo=nginx)](https://nginx.org)
[![MinIO](https://img.shields.io/badge/MinIO%2FS3-optional-C72E49.svg)](https://min.io)
[![TOTP](https://img.shields.io/badge/TOTP-RFC6238-brightgreen.svg)](https://datatracker.ietf.org/doc/html/rfc6238)
[![OAuth2](https://img.shields.io/badge/OAuth2-GitHub%20%7C%20Google%20%7C%20%E9%92%89%E9%92%89%20%7C%20%E9%A3%9E%E4%B9%A6-4A90E2.svg)]()

</div>

---

## 项目简介

RuoYi-Cpp 是 [若依（RuoYi-Vue）](https://gitee.com/y_project/RuoYi-Vue) 管理框架的 C++ 高性能版本，后端基于 Drogon 异步 HTTP 框架，数据库使用 PostgreSQL，与原版 RuoYi-Vue 前端保持完全 API 兼容。

> ✅ **平台支持**：已在 **Windows（MSYS2 MinGW64）** 上完整编译验证通过。数据库使用 **SQLite 内嵌模式**（无需安装 PostgreSQL 即可运行），PostgreSQL 作为可选主数据库。

**相比 Java 版本的优势：**

| 对比项 | Java (Spring Boot) | RuoYi-Cpp |
|--------|-------------------|-----------|
| 启动内存 | ~300–500 MB | **~3.2-10 MB** |
| 启动时间 | 5–15 秒 | **< 1 秒** |
| 运行时依赖 | JDK 17+ | 无（静态链接） |
| 部署方式 | JAR + JVM | **单个可执行文件** |
| 适用场景 | 云服务器 | 云服务器 / NAS / 嵌入式 |
| Nginx 依赖 | **可选** | **可选**（内置前端托管）|

---

## 功能模块

### 系统管理

| 模块 | API 路由 | 功能说明 |
|------|---------|---------|
| 用户管理 | `GET/POST/PUT/DELETE /system/user` | 增删改查、重置密码、CSV 导入导出、角色/岗位分配 |
| 角色管理 | `GET/POST/PUT/DELETE /system/role` | 增删改查、菜单权限分配、数据权限、用户授权 |
| 菜单管理 | `GET/POST/PUT/DELETE /system/menu` | 增删改查、动态路由树构建 |
| 部门管理 | `GET/POST/PUT/DELETE /system/dept` | 树形结构增删改查 |
| 岗位管理 | `GET/POST/PUT/DELETE /system/post` | 增删改查 |
| 参数配置 | `GET/POST/PUT/DELETE /system/config` | 系统参数 CRUD + 缓存刷新 |
| 字典管理 | `GET/POST/PUT/DELETE /system/dict` | 字典类型 + 字典数据 CRUD |
| 通知公告 | `GET/POST/PUT/DELETE /system/notice` | 公告 CRUD + 已读状态 |
| 邮件配置 | `GET/POST /system/emailConfig` | SMTP 发件箱配置 + 测试发送 |
| 两步验证 | `POST /system/totp/*` | Google Authenticator TOTP 绑定/解绑 |
| 第三方登录 | `GET /oauth2/authorize/{p}` | GitHub / Google / 企业微信 / 钉钉 / 飞书 / QQ OAuth2 |
| 第三方回调 | `GET /oauth2/callback/{p}` | code → JWT，首次自动建号 |
| 第三方绑定 | `POST/DELETE /oauth2/bind/{p}` | 已登录账号绑定/解绑第三方 |

### 系统监控

| 模块 | API 路由 | 功能说明 |
|------|---------|---------|
| 操作日志 | `GET/DELETE /monitor/operlog` | 查询 / 删除 / 清空 / 导出 |
| 登录日志 | `GET/DELETE /monitor/logininfor` | 查询 / 删除 / 解锁账户 |
| 在线用户 | `GET/DELETE /monitor/online` | 查看在线会话 / 强制下线 |
| 定时任务 | `GET/POST/PUT/DELETE /monitor/job` | CRUD + 立即执行 + 暂停/恢复 + 执行日志 |
| 系统日志 | `GET /monitor/logfile` | 实时查看 `.log`/`.jsonl` 日志文件，支持删除 |
| 服务监控 | `GET /monitor/server` | CPU、内存、磁盘、系统信息、GPU VRAM |
| 缓存监控 | `GET /monitor/cache` | 查看缓存分类和键值 |
| 数据源监控 | `GET /monitor/druid` | DB 连接池状态、查询统计 |

### 账号自助

| 功能 | API 路由 | 说明 |
|------|---------|-----|
| 登录 | `POST /login` | 用户名 + 密码 + 验证码，返回 JWT |
| LDAP 登录 | `POST /login` | 配置 `ldap.enabled=true` 后自动走 AD/LDAP 认证 |
| 注册 | `POST /register` | 自助注册，支持邮箱验证码 |
| 忘记密码 | `POST /forgotPassword` | 通过邮箱发送重置验证码 |
| 重置密码 | `POST /resetPassword` | 凭重置令牌更新密码 |
| 发送验证码 | `POST /sendRegCode` | 注册前验证邮箱有效性 |

### 运维与可观测性

| 端点 | 方法 | 说明 |
|------|------|-----|
| `/actuator/health` | GET | 健康检查，JSON 格式 |
| `/actuator/metrics` | GET | Prometheus 文本格式指标（直接接 Grafana）|
| `/actuator/db` | GET | 数据库状态：后端类型、连接状态、待同步队列 |
| `/actuator/info` | GET | 应用信息 |
| `/actuator/reload` | POST | 热重载 `config.json`（无需重启）|
| `/swagger-ui/` | GET | Swagger UI API 文档 |
| `/v3/api-docs` | GET | OpenAPI 3.0 JSON 描述 |

> 详细的指标定义、PromQL 查询、Grafana 面板与告警规则见 [`docs/OBSERVABILITY.md`](docs/OBSERVABILITY.md)。

---

## 快速开始

### 环境要求

- **Windows**：MSYS2 + MinGW64 (gcc 13+) 或 MSVC 2022
- **Linux**：gcc 11+ / clang 14+
- CMake ≥ 3.16
- OpenSSL 3.x、PostgreSQL 12+ 客户端库（或仅用 SQLite 模式）
- 可选：Vault、Redis、Nginx（项目可嵌入运行无需外置）

### 编译

```bash
# 配置（启用单元测试 + 嵌入前端）
cmake -S . -B build -G Ninja \
    -DRUOYI_BUILD_TESTS=ON \
    -DRUOYI_EMBED_FRONTEND=ON \
    -DRUOYI_EMBED_FRONTEND_DIR=./web

# 编译
cmake --build build --parallel

# 运行单元测试
ctest --test-dir build --output-on-failure
```

### 运行

```bash
cp config.json.example config.json   # 按需修改
./build/ruoyi-cpp
```

默认监听 `0.0.0.0:18080`，首次启动自动建库、建表、初始化菜单与默认账号（admin / admin123）。

### 配置文件

核心配置 `config.json` 关键段：

| 段 | 说明 |
|----|------|
| `database` | PostgreSQL 连接（host/port/user/passwd/dbname），不配则用 SQLite |
| `redis` | Redis 缓存（不配则用进程内 LRU）|
| `jwt` | JWT 密钥、签发者、过期时间 |
| `security` | 验证码、签名验证、加密算法（含 SQLite 加密多种密钥来源）|
| `sqlite.encrypt_key` | 极简加密入口：填非空字符串即启用 SQLite 加密（详见 [`docs/SQLITE_ENCRYPTION.md`](docs/SQLITE_ENCRYPTION.md)）|
| `frontend` / `embedded_frontend` | 前端托管模式（外部目录 / 嵌入 exe）|
| `nginx_embedded` | 嵌入式 Nginx 静态库参数（HTTPS/反代）|
| `nginx_like` | proxy_pass / IP ACL / 连接数限流 / 访问日志 |
| `ai_fallback` | AI 远程 fallback 提供商链（图灵 / 讯飞星火 / ...）|
| `menu` | 动态菜单 URL 显式覆盖（如 `logfile_external_url` / `ai_page_url`）|

---

## 部署模式

RuoYi-Cpp 支持多种部署模式，**一个可执行文件**即可：

### 1. 前后端分离（开发）

后端监听 `:18080`，前端 vue-cli 在 `:3000` 跑 `pnpm dev`，自动通过 `/dev-api` 反代到后端。

### 2. 前端嵌入 exe（推荐生产）

```bash
cmake -DRUOYI_EMBED_FRONTEND=ON -DRUOYI_EMBED_FRONTEND_DIR=./web/dist ...
```

打包后单个 exe 包含前端静态资源，访问 `http://host:18080` 直接出登录页。

### 3. 外部前端目录托管

```json
"frontend": { "enabled": true, "path": "./web/dist", "api_prefix": "/prod-api" }
```

### 4. 嵌入式 Nginx（HTTPS / ACME 自动续期）

```json
"nginx_embedded": {
    "enabled": true,
    "https_port": 443,
    "cert_dir": "./certs",
    "acme": { "enabled": true, "email": "admin@example.com" }
}
```

### 5. 多进程编排（高并发）

```json
"worker_orchestrator": { "enabled": true, "workers": 4, "port_base": 18080 }
```

Worker 主进程通过 JobObject + SO_REUSEPORT 启 4 个 worker 监听同一端口，Redis 共享限流计数与会话缓存。

---

## 测试

```bash
# 仅跑单元测试（无需主项目编译，~5s）
cmake -S tests -B build-tests
cmake --build build-tests --parallel
ctest --test-dir build-tests --output-on-failure
```

CI 跑 `ubuntu-latest / windows-latest / macos-latest` 三平台 self-contained 测试。详见 `.github/workflows/ci.yml`。

---

## 目录结构

```
ruoyi-cpp/
├── src/
│   ├── main.cc                     ── 启动入口（多进程 / Vault / Nginx 嵌入）
│   ├── AppIncludes.h               ── 公共头汇总
│   ├── common/                     ── 工具：JWT/PBKDF2/Captcha/Metrics/HotConfig/Vault/...
│   ├── services/                   ── DatabaseService（PG + SQLite 自动切换）
│   ├── system/                     ── 用户/角色/菜单/字典/...
│   ├── monitor/                    ── 操作日志/在线用户/服务监控/定时任务
│   ├── tool/                       ── 代码生成器
│   └── ai/                         ── AI 助手（本地 KoboldCpp + 远程 fallback chain）
├── tests/                          ── doctest 单元测试
├── docs/                           ── 设计文档（OBSERVABILITY.md, SQLITE_ENCRYPTION.md）
├── config.json                     ── 主配置
├── CMakeLists.txt
└── .github/workflows/ci.yml        ── 三平台 CI
```

---

## 致谢

- [RuoYi-Vue](https://gitee.com/y_project/RuoYi-Vue) — 提供完整的前端 UI + RBAC 数据模型
- [Drogon](https://github.com/drogonframework/drogon) — 高性能 C++ 异步 HTTP 框架
- [jwt-cpp](https://github.com/Thalhammer/jwt-cpp) — JWT 实现
- [doctest](https://github.com/doctest/doctest) — 单测框架

---

## 许可证

MIT License — 详见 [LICENSE](LICENSE)