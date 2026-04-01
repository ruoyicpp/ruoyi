<div align="center">

# RuoYi-Cpp

**RuoYi 管理框架的 C++ 高性能完整复刻**

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

</div>

---

## 项目简介

RuoYi-Cpp 是 [若依（RuoYi-Vue）](https://gitee.com/y_project/RuoYi-Vue) 管理框架的 C++ 完整复刻，后端基于 Drogon 异步 HTTP 框架，数据库使用 PostgreSQL，与原版 RuoYi-Vue 前端保持完全 API 兼容。

> ✅ **平台支持**：已在 **Windows（MSYS2 MinGW64）** 上完整编译验证通过。数据库使用 **SQLite 内嵌模式**（无需安装 PostgreSQL 即可运行），PostgreSQL 作为可选主数据库。

**相比 Java 版本的优势：**

| 对比项 | Java (Spring Boot) | RuoYi-Cpp |
|--------|-------------------|-----------|
| 启动内存 | ~300–500 MB | **~3.2-10 MB** |
| 启动时间 | 5–15 秒 | **< 1 秒** |
| 运行时依赖 | JDK 17+ | 无（静态链接） |
| 部署方式 | JAR + JVM | **单个可执行文件** |
| 适用场景 | 云服务器 | 云服务器 / NAS / 嵌入式 |

---

## 功能模块

### 系统管理

| 模块 | API 路由 | 功能说明 |
|------|---------|---------|
| 用户管理 | `GET/POST/PUT/DELETE /system/user` | 增删改查、重置密码、导入导出、角色/岗位分配 |
| 角色管理 | `GET/POST/PUT/DELETE /system/role` | 增删改查、菜单权限分配、数据权限、用户授权 |
| 菜单管理 | `GET/POST/PUT/DELETE /system/menu` | 增删改查、动态路由树构建 |
| 部门管理 | `GET/POST/PUT/DELETE /system/dept` | 树形结构增删改查 |
| 岗位管理 | `GET/POST/PUT/DELETE /system/post` | 增删改查 |
| 参数配置 | `GET/POST/PUT/DELETE /system/config` | 系统参数 CRUD + 缓存刷新 |
| 字典管理 | `GET/POST/PUT/DELETE /system/dict` | 字典类型 + 字典数据 CRUD |
| 通知公告 | `GET/POST/PUT/DELETE /system/notice` | 公告 CRUD |
| 邮件配置 | `GET/POST /system/emailConfig` | SMTP 发件箱配置 + 测试发送 |

### 系统监控

| 模块 | API 路由 | 功能说明 |
|------|---------|---------|
| 操作日志 | `GET/DELETE /monitor/operlog` | 查询 / 删除 / 清空 / 导出 |
| 登录日志 | `GET/DELETE /monitor/logininfor` | 查询 / 删除 / 解锁账户 |
| 在线用户 | `GET/DELETE /monitor/online` | 查看在线会话 / 强制下线 |
| 定时任务 | `GET/POST/PUT/DELETE /monitor/job` | CRUD + 立即执行 + 暂停/恢复 |
| 服务监控 | `GET /monitor/server` | CPU、内存、磁盘、系统信息 |
| 缓存监控 | `GET /monitor/cache` | 查看缓存分类和键值 |

### 账号自助

| 功能 | API 路由 | 说明 |
|------|---------|-----|
| 登录 | `POST /login` | 用户名 + 密码 + 验证码，返回 JWT |
| 注册 | `POST /register` | 自助注册，支持邮箱验证码 |
| 忘记密码 | `POST /forgotPassword` | 通过邮箱发送重置验证码 |
| 重置密码 | `POST /resetPassword` | 凭重置令牌更新密码 |
| 发送验证码 | `POST /sendRegCode` | 注册前验证邮箱有效性 |

---

## 技术栈

| 组件 | 技术 |
|------|-----|
| HTTP 框架 | [Drogon](https://github.com/drogonframework/drogon) (C++17, 异步非阻塞) |
| 主数据库 | PostgreSQL (libpq 直连) |
| 本地缓存 | SQLite（进程内同步备份，重启恢复 Token） |
| 缓存层 | 进程内 MemCache / VramCache（可选 Redis） |
| 认证 | JWT（[jwt-cpp](https://github.com/Thalhammer/jwt-cpp)），PBKDF2-SHA256 密码哈希 |
| 邮件发送 | OpenSSL Implicit-TLS SMTP（支持 QQ/163/企业邮箱，多发件人轮转） |
| 前端 | RuoYi-Vue（Vue 2 + Element UI，**直接使用[若依官方前端](https://gitee.com/y_project/RuoYi-Vue)，无需修改任何代码**） |
| 反向代理 | Nginx（可选，项目内置启动管理） |

---

## 快速开始

### 前置依赖

**数据库**：需要运行中的 PostgreSQL 实例（版本 12+）

```sql
-- 创建数据库（首次运行自动建表，无需手动导入 SQL）
CREATE DATABASE "ruoyi.c";
```

**Redis**（可选）：不配置时自动退化为进程内缓存。

---

### Windows（MSYS2 MinGW64）

**1. 安装 MSYS2 依赖**

```bash
pacman -S --needed \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-ninja \
    mingw-w64-x86_64-openssl \
    mingw-w64-x86_64-jsoncpp \
    mingw-w64-x86_64-zlib \
    mingw-w64-x86_64-postgresql \
    mingw-w64-x86_64-brotli \
    mingw-w64-x86_64-c-ares \
    mingw-w64-x86_64-libuuid \
    mingw-w64-x86_64-hiredis
```

**2. 编译安装 Drogon**

```bash
git clone https://github.com/drogonframework/drogon
cd drogon && git submodule update --init
mkdir build && cd build
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_REDIS=ON \
    -DBUILD_MYSQL=OFF \
    -DBUILD_SQLITE=OFF \
    -DBUILD_POSTGRESQL=ON
ninja && ninja install
```

**3. 安装 jwt-cpp（Header-Only）**

```bash
git clone https://github.com/Thalhammer/jwt-cpp
cp -r jwt-cpp/include/jwt-cpp /mingw64/include/
```

**4. 编译项目**

```bash
git clone https://gitee.com/ruoyicpp/ruoyi ruoyi-cpp
cd ruoyi-cpp && mkdir build && cd build
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/mingw64
ninja
```

**5. 配置并运行**

```bash
# 编辑 config.json（数据库连接、JWT 密钥等）
# 首次运行自动建表 + 插入初始数据
./ruoyi-cpp.exe
```

---

## 配置说明

主配置文件：`config.json`

```jsonc
{
    "listeners": [{ "address": "0.0.0.0", "port": 18080, "https": false }],
    "database": {
        "host": "127.0.0.1",
        "port": 5432,
        "dbname": "ruoyi.c",
        "user": "postgres",
        "passwd": ""
    },
    "jwt": {
        "secret": "请填写随机字符串",  // ⚠️ 必填，生产环境请使用强随机密钥
        "expire_minutes": 30,
        "jwt_expire_days": 7
    },
    "redis": {
        "enabled": false,        // true 启用 Redis，false 使用内存缓存
        "host": "127.0.0.1",
        "port": 6379,
        "password": "",
        "db": 0,
        "key_prefix": ""
    },
    "captcha": {
        "enabled": true,
        "expire_seconds": 120
    },
    "user": {
        "max_retry_count": 5,    // 密码错误锁定阈值
        "lock_time_minutes": 15
    }
}
```

### 邮件配置（系统内配置）

登录后进入 **系统管理 → 邮件发件箱** 配置 SMTP，无需修改配置文件：

| 参数键 | 说明 | 示例值 |
|--------|-----|--------|
| `sys.email.host` | SMTP 服务器 | `smtp.qq.com` |
| `sys.email.port` | 端口（Implicit TLS）| `465` |
| `sys.email.fromName` | 发件人显示名 | `系统通知` |
| `sys.email.senders` | 发件人列表（JSON 数组）| `[{"email":"a@qq.com","authCode":"xxxx"}]` |

---

## 项目结构

```
ruoyi-cpp/
├── config.json                 # 主配置文件
├── src/
│   ├── main.cc                 # 入口：初始化、路由注册
│   ├── common/
│   │   ├── AjaxResult.h        # 统一 JSON 响应体
│   │   ├── Constants.h         # 全局常量
│   │   ├── LoginUser.h         # 登录用户模型
│   │   ├── TokenCache.h        # Token/KV 缓存（内存+Redis+VramCache）
│   │   ├── JwtUtils.h          # JWT 生成/解析
│   │   ├── SecurityUtils.h     # 密码加密、Token 提取、管理员判断
│   │   ├── SmtpUtils.h         # SMTP 邮件发送（OpenSSL Implicit-TLS）
│   │   ├── IpUtils.h           # IP 获取、黑名单匹配
│   │   ├── PageUtils.h         # 分页参数/结果封装
│   │   └── DatabaseInit.cc     # 自动建表 + 初始数据 + Schema 迁移
│   ├── filters/
│   │   ├── JwtAuthFilter.h     # JWT 认证过滤器（自动续期）
│   │   └── PermFilter.h        # 权限检查宏 CHECK_PERM
│   ├── services/
│   │   └── DatabaseService.h   # PostgreSQL + SQLite 双写服务
│   ├── system/
│   │   ├── services/
│   │   │   ├── TokenService.h       # Token 创建/刷新/删除
│   │   │   ├── SysLoginService.h    # 登录校验（验证码+密码+黑名单）
│   │   │   ├── SysConfigService.h   # 系统参数（带缓存）
│   │   │   ├── SysDictService.h     # 字典缓存
│   │   │   └── SysMenuService.h     # 菜单树/路由构建
│   │   └── controllers/
│   │       ├── SysLoginCtrl.h       # 登录/注册/忘记密码/路由
│   │       ├── SysUserCtrl.h        # 用户管理
│   │       ├── SysRoleCtrl.h        # 角色管理（含实时权限刷新）
│   │       ├── SysMenuCtrl.h        # 菜单管理
│   │       ├── SysDeptCtrl.h        # 部门管理
│   │       ├── SysConfigCtrl.h      # 参数配置
│   │       ├── SysDictCtrl.h        # 字典管理
│   │       ├── SysPostCtrl.h        # 岗位管理
│   │       ├── SysNoticeCtrl.h      # 通知公告
│   │       └── SysEmailConfigCtrl.h # 邮件发件箱配置
│   └── monitor/
│       └── controllers/
│           ├── SysLogCtrl.h         # 操作日志 + 登录日志
│           ├── SysOnlineCtrl.h      # 在线用户管理
│           ├── SysJobCtrl.h         # 定时任务
│           ├── ServerCtrl.h         # 服务器监控
│           └── CacheCtrl.h          # 缓存监控
└── ui/                         # RuoYi-Vue 前端源码（可选）
```

---

## 权限设计

- **超级管理员**（`user_id=1`）：拥有所有权限，不受 RBAC 限制
- **普通用户**：通过 `sys_user_role` 关联角色，角色关联菜单权限
- **菜单权限字符**：如 `system:user:list`，在 `CHECK_PERM` 宏中自动校验
- **角色权限实时生效**：修改角色菜单后，**无需在线用户重新登录**，后端自动刷新 Token 权限缓存和路由缓存

### 注册用户权限

通过系统参数 `sys.account.initRoleId` 控制：

| 参数值 | 效果 |
|--------|-----|
| 空（默认） | 注册后无角色，管理员手动分配 |
| 角色 ID（如 `2`） | 注册后自动分配指定角色 |

---

## 默认账号

| 用户名 | 密码 | 说明 |
|--------|------|-----|
| `admin` | `admin123` | 超级管理员，拥有全部权限 |

> ⚠️ **生产环境请立即修改默认密码！**

密码使用 OpenSSL PBKDF2-SHA256（10000 轮）存储，bcrypt 可选。

---

## 用户批量导入

支持通过 CSV 文件批量导入用户（**系统管理 → 用户管理 → 导入**）：

1. 点击"下载模板"获取 CSV 格式模板
2. 按模板填写用户数据（默认密码 `123456`）
3. 勾选"是否更新"可覆盖已有账号
4. 上传 CSV 文件，支持 UTF-8（含 BOM）编码

CSV 列格式：`登录账号, 用户昵称, 部门编号, 手机号码, 邮箱, 性别(0/1/2), 状态(0/1)`

---

## 前端说明

**前端直接使用若依官方源码：**

```bash
# 克隆若依官方前端
git clone https://gitee.com/y_project/RuoYi-Vue.git
cd RuoYi-Vue

# 修改 .env.development 中的后端地址
VUE_APP_BASE_API = 'http://127.0.0.1:18080'

# 安装依赖并启动
npm install
npm run dev
```

生产部署时将 `npm run build:prod` 生成的 `dist/` 目录部署到 Nginx 即可，后端地址指向本项目的监听端口（默认 `18080`）。

**Nginx 伪静态 + 反向代理配置：**

```nginx
# Vue 路由 history 模式伪静态
location / {
    try_files $uri $uri/ /index.html;
}

# 后端 API 代理（对应前端 VUE_APP_BASE_API = '/prod-api'）
location /prod-api/ {
    proxy_pass http://127.0.0.1:18080/;
    proxy_set_header Host              $host;
    proxy_set_header X-Real-IP         $remote_addr;
    proxy_set_header X-Forwarded-For   $proxy_add_x_forwarded_for;
    proxy_set_header X-Forwarded-Proto $scheme;
    proxy_connect_timeout 60s;
    proxy_send_timeout    60s;
    proxy_read_timeout    60s;
}

# WebSocket 通知
location /ws/ {
    proxy_pass http://127.0.0.1:18080/ws/;
    proxy_http_version 1.1;
    proxy_set_header Upgrade    $http_upgrade;
    proxy_set_header Connection "upgrade";
    proxy_set_header Host       $host;
    proxy_read_timeout 3600s;
}
```

---

## 与原版 RuoYi-Vue 的兼容性

- ✅ 所有 `/system/**`、`/monitor/**` API 路由与原版完全一致
- ✅ JWT Token 格式、`getInfo`、`getRouters` 响应结构完全兼容
- ✅ 直接克隆[若依官方前端](https://gitee.com/y_project/RuoYi-Vue)，只改后端地址即可运行
- ➕ 新增：邮件发件箱管理、忘记密码、注册邮箱验证码等功能

---

## 交流群

QQ 交流群：**7827982393**

广州市八股文科技官网：<http://www.gzbgw.com>

八股文题库平台：<http://question.gzbgw.com>

---

## 开源协议

本项目基于 [MIT License](LICENSE) 开源。

RuoYi-Vue 原项目版权归 [若依团队](https://gitee.com/y_project/RuoYi-Vue) 所有，遵循 MIT 协议。
