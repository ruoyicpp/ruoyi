# RuoYi-Cpp 前端

<<<<<<< HEAD
基于 [RuoYi-Vue](https://gitee.com/y_project/RuoYi-Vue) 前端改造，适配 C++ 后端（Drogon 框架）。
=======
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

---
>>>>>>> 8696020 (update README.md.)

## 技术栈

- Vue 2.6 + Vue Router 3 + Vuex 3
- Element UI 2.15
- ECharts 5.4
- Axios

## 环境要求

- Node ≥ 18
- pnpm（推荐）/ npm

## 开发

```bash
# 安装依赖
pnpm install
# 或使用国内镜像
pnpm install --registry=https://registry.npmmirror.com

# 启动开发服务器（默认端口 3000）
pnpm dev
```

浏览器访问 http://localhost:3000

开发环境自动代理后端接口（`/dev-api` → `http://localhost:18080`），同时代理 WebSocket（`/ws/`）。

## 构建

```bash
# 构建测试环境
pnpm build:stage

# 构建生产环境
pnpm build:prod
```

产物输出到 `dist/` 目录，静态资源在 `dist/static/` 下，生产环境开启 gzip 压缩且不生成 source map。

## 环境配置

| 文件 | 环境 | API 前缀 |
|------|------|----------|
| `.env.development` | 开发 | `/dev-api` |
| `.env.staging` | 测试 | `/stage-api` |
| `.env.production` | 生产 | `/prod-api` |

## 部署

构建后将 `dist/` 目录部署到 Web 服务器，Nginx 示例：

```nginx
location / {
    try_files $uri $uri/ /index.html;
}

location /prod-api/ {
    proxy_pass http://127.0.0.1:18080/;
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    proxy_set_header X-Forwarded-Proto $scheme;
    proxy_connect_timeout 60s;
    proxy_send_timeout 60s;
    proxy_read_timeout 60s;
}

location /api/ {
    proxy_pass http://127.0.0.1:18080/api/;
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    proxy_set_header X-Forwarded-Proto $scheme;
    proxy_connect_timeout 60s;
    proxy_send_timeout 60s;
    proxy_read_timeout 60s;
}
```