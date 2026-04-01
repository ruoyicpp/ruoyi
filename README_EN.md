<div align="center">

English | [中文](README.md)

# RuoYi-Cpp

**A High-Performance C++ Reimplementation of the RuoYi Management Framework**

Based on [Drogon](https://github.com/drogonframework/drogon) + PostgreSQL, 100% compatible with the RuoYi-Vue frontend

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

## Overview

RuoYi-Cpp is a complete C++ reimplementation of the [RuoYi-Vue](https://gitee.com/y_project/RuoYi-Vue) management framework. The backend is built on the Drogon asynchronous HTTP framework with PostgreSQL as the primary database, maintaining full API compatibility with the original RuoYi-Vue frontend.

> ✅ **Platform**: Fully compiled and verified on **Windows (MSYS2 MinGW64)**. Supports **embedded SQLite mode** (no PostgreSQL required to run), with PostgreSQL as an optional primary database.

**Advantages over the Java version:**

| Metric | Java (Spring Boot) | RuoYi-Cpp |
|--------|-------------------|-----------|
| Memory usage | ~300–500 MB | **~3.2–10 MB** |
| Startup time | 5–15 s | **< 1 s** |
| Runtime dependency | JDK 17+ | None (statically linked) |
| Deployment | JAR + JVM | **Single executable** |
| Target environment | Cloud servers | Cloud / NAS / Embedded |

---

## Modules

### System Management

| Module | API Route | Description |
|--------|-----------|-------------|
| User Management | `GET/POST/PUT/DELETE /system/user` | CRUD, password reset, import/export, role/post assignment |
| Role Management | `GET/POST/PUT/DELETE /system/role` | CRUD, menu permission assignment, data scope, user authorization |
| Menu Management | `GET/POST/PUT/DELETE /system/menu` | CRUD, dynamic route tree building |
| Department Management | `GET/POST/PUT/DELETE /system/dept` | Tree-structured CRUD |
| Post Management | `GET/POST/PUT/DELETE /system/post` | CRUD |
| Config Management | `GET/POST/PUT/DELETE /system/config` | System parameter CRUD + cache refresh |
| Dictionary Management | `GET/POST/PUT/DELETE /system/dict` | Dict type + dict data CRUD |
| Notice Management | `GET/POST/PUT/DELETE /system/notice` | Announcement CRUD |
| Email Config | `GET/POST /system/emailConfig` | SMTP sender configuration + test send |

### System Monitor

| Module | API Route | Description |
|--------|-----------|-------------|
| Operation Log | `GET/DELETE /monitor/operlog` | Query / delete / clear / export |
| Login Log | `GET/DELETE /monitor/logininfor` | Query / delete / unlock account |
| Online Users | `GET/DELETE /monitor/online` | View sessions / force logout |
| Scheduled Jobs | `GET/POST/PUT/DELETE /monitor/job` | CRUD + run once + pause/resume |
| Server Monitor | `GET /monitor/server` | CPU, memory, disk, system info |
| Cache Monitor | `GET /monitor/cache` | View cache categories and key-values |

### Account Self-Service

| Feature | API Route | Description |
|---------|-----------|-------------|
| Login | `POST /login` | Username + password + captcha, returns JWT |
| Register | `POST /register` | Self-registration with email verification |
| Forgot Password | `POST /forgotPassword` | Send reset code via email |
| Reset Password | `POST /resetPassword` | Update password with reset token |
| Send Verification Code | `POST /sendRegCode` | Validate email before registration |

---

## Tech Stack

| Component | Technology |
|-----------|-----------|
| HTTP Framework | [Drogon](https://github.com/drogonframework/drogon) (C++17, async non-blocking) |
| Primary Database | PostgreSQL (libpq direct connection) |
| Local Cache | SQLite (in-process sync backup, restores tokens on restart) |
| Cache Layer | In-process MemCache / VramCache (optional Redis) |
| Authentication | JWT ([jwt-cpp](https://github.com/Thalhammer/jwt-cpp)), PBKDF2-SHA256 password hashing |
| Email | OpenSSL Implicit-TLS SMTP (QQ/163/enterprise mail, multi-sender rotation) |
| Frontend | RuoYi-Vue (Vue 2 + Element UI, **use [official RuoYi frontend](https://gitee.com/y_project/RuoYi-Vue) directly**) |
| Reverse Proxy | Nginx (optional, built-in startup management) |

---

## Quick Start

### Prerequisites

**Database**: A running PostgreSQL instance (version 12+)

```sql
-- Create database (tables are created automatically on first run)
CREATE DATABASE "ruoyi.c";
```

**Redis** (optional): Falls back to in-process cache if not configured.

---

### Windows (MSYS2 MinGW64)

**1. Install MSYS2 dependencies**

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

**2. Build and install Drogon**

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

**3. Install jwt-cpp (header-only)**

```bash
git clone https://github.com/Thalhammer/jwt-cpp
cp -r jwt-cpp/include/jwt-cpp /mingw64/include/
```

**4. Build the project**

```bash
git clone https://gitee.com/ruoyicpp/ruoyi ruoyi-cpp
cd ruoyi-cpp && mkdir build && cd build
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/mingw64
ninja
```

**5. Configure and run**

```bash
# Edit config.json (database connection, JWT secret, etc.)
# Tables and initial data are created automatically on first run
./ruoyi-cpp.exe
```

---

## Configuration

Main config file: `config.json`

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
        "secret": "fill-in-a-random-string",  // ⚠️ Required — use a strong random key in production
        "expire_minutes": 30,
        "jwt_expire_days": 7
    },
    "redis": {
        "enabled": false,        // true = use Redis, false = in-process cache
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
        "max_retry_count": 5,    // Lock threshold for wrong password attempts
        "lock_time_minutes": 15
    }
}
```

### Email Configuration (in-app)

After logging in, go to **System Management → Email Senders** to configure SMTP — no config file changes needed:

| Key | Description | Example |
|-----|-------------|---------|
| `sys.email.host` | SMTP server | `smtp.qq.com` |
| `sys.email.port` | Port (Implicit TLS) | `465` |
| `sys.email.fromName` | Sender display name | `System Notification` |
| `sys.email.senders` | Sender list (JSON array) | `[{"email":"a@qq.com","authCode":"xxxx"}]` |

---

## Project Structure

```
ruoyi-cpp/
├── config.json                 # Main config file
├── src/
│   ├── main.cc                 # Entry point: init, route registration
│   ├── common/
│   │   ├── AjaxResult.h        # Unified JSON response body
│   │   ├── Constants.h         # Global constants
│   │   ├── LoginUser.h         # Logged-in user model
│   │   ├── TokenCache.h        # Token/KV cache (memory + Redis + VramCache)
│   │   ├── JwtUtils.h          # JWT generation/parsing
│   │   ├── SecurityUtils.h     # Password hashing, token extraction
│   │   ├── SmtpUtils.h         # SMTP email (OpenSSL Implicit-TLS)
│   │   ├── IpUtils.h           # IP retrieval, blacklist matching
│   │   ├── PageUtils.h         # Pagination params/results
│   │   └── DatabaseInit.cc     # Auto table creation + initial data + schema migration
│   ├── filters/
│   │   ├── JwtAuthFilter.h     # JWT auth filter (auto token renewal)
│   │   └── PermFilter.h        # Permission check macro CHECK_PERM
│   ├── services/
│   │   └── DatabaseService.h   # PostgreSQL + SQLite dual-write service
│   ├── system/
│   │   ├── services/
│   │   │   ├── TokenService.h       # Token create/refresh/delete
│   │   │   ├── SysLoginService.h    # Login validation (captcha + password + blacklist)
│   │   │   ├── SysConfigService.h   # System parameters (with cache)
│   │   │   ├── SysDictService.h     # Dictionary cache
│   │   │   └── SysMenuService.h     # Menu tree / route building
│   │   └── controllers/
│   │       ├── SysLoginCtrl.h       # Login / register / forgot password / routes
│   │       ├── SysUserCtrl.h        # User management
│   │       ├── SysRoleCtrl.h        # Role management (with real-time permission refresh)
│   │       ├── SysMenuCtrl.h        # Menu management
│   │       ├── SysDeptCtrl.h        # Department management
│   │       ├── SysConfigCtrl.h      # Parameter configuration
│   │       ├── SysDictCtrl.h        # Dictionary management
│   │       ├── SysPostCtrl.h        # Post management
│   │       ├── SysNoticeCtrl.h      # Notice management
│   │       └── SysEmailConfigCtrl.h # Email sender configuration
│   └── monitor/
│       └── controllers/
│           ├── SysLogCtrl.h         # Operation log + login log
│           ├── SysOnlineCtrl.h      # Online user management
│           ├── SysJobCtrl.h         # Scheduled jobs
│           ├── ServerCtrl.h         # Server monitor
│           └── CacheCtrl.h          # Cache monitor
└── ui/                         # RuoYi-Vue frontend source (optional)
```

---

## Permission Design

- **Super Admin** (`user_id=1`): All permissions, bypasses RBAC
- **Regular Users**: Assigned roles via `sys_user_role`, roles linked to menu permissions
- **Permission strings**: e.g. `system:user:list`, auto-checked by the `CHECK_PERM` macro
- **Real-time permission updates**: Modifying role menus takes effect immediately — **online users do not need to re-login**

### Default Role for Registered Users

Controlled by system parameter `sys.account.initRoleId`:

| Value | Effect |
|-------|--------|
| Empty (default) | No role after registration; admin assigns manually |
| Role ID (e.g. `2`) | Automatically assigned to the specified role |

---

## Default Account

| Username | Password | Description |
|----------|----------|-------------|
| `admin` | `admin123` | Super admin, full permissions |

> ⚠️ **Change the default password immediately in production!**

Passwords are stored using OpenSSL PBKDF2-SHA256 (10,000 rounds).

---

## Bulk User Import

Batch import users via CSV (**System Management → User Management → Import**):

1. Click "Download Template" to get the CSV format
2. Fill in user data (default password: `123456`)
3. Check "Update existing" to overwrite existing accounts
4. Upload the CSV file (UTF-8 with or without BOM)

CSV columns: `Login Name, Display Name, Dept ID, Phone, Email, Gender(0/1/2), Status(0/1)`

---

## Frontend

**Use the official RuoYi-Vue frontend:**

```bash
# Clone the official RuoYi frontend
git clone https://gitee.com/y_project/RuoYi-Vue.git
cd RuoYi-Vue

# Set the backend address in .env.development
VUE_APP_BASE_API = 'http://127.0.0.1:18080'

# Install dependencies and start
npm install
npm run dev
```

For production, deploy the `dist/` folder (built with `npm run build:prod`) to Nginx, pointing the backend address to this project's port (default `18080`).

**Nginx pseudo-static + reverse proxy configuration:**

```nginx
# Vue Router history mode
location / {
    try_files $uri $uri/ /index.html;
}

# Backend API proxy (matches VUE_APP_BASE_API = '/prod-api')
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

# WebSocket notifications
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

## Compatibility with RuoYi-Vue

- ✅ All `/system/**` and `/monitor/**` API routes are identical to the original
- ✅ JWT token format, `getInfo`, `getRouters` response structures are fully compatible
- ✅ Clone the [official RuoYi frontend](https://gitee.com/y_project/RuoYi-Vue), change only the backend URL, and run
- ➕ Added: email sender management, forgot password, registration email verification

---

## FAQ

**Q: Startup fails with `cannot connect to database`?**
> Check `database.host/port/dbname/user/passwd` in `config.json` and ensure PostgreSQL is running. For SQLite mode, leave `host` empty.

**Q: Login says captcha is wrong?**
> Ensure `captcha.enabled` is `true` and the system clock is correct (captcha expires after 120 seconds).

**Q: Frontend CORS error?**
> In dev mode, check that `devServer.proxy` in `vue.config.js` points to the correct backend port (`18080`). In production, verify the Nginx `/prod-api/` proxy config.

**Q: Can the server start with an empty JWT secret?**
> Yes, but all tokens will be signed with an empty key — **a serious security risk**. Always set a strong random secret in production.

**Q: Role permission changes not taking effect?**
> The backend automatically refreshes online users' permission cache. If it still doesn't work, check that `MemCache` / Redis is functioning correctly.

---

## Contributing

Issues and Pull Requests are welcome!

1. Fork the repository: <https://gitee.com/ruoyicpp/ruoyi>
2. Create a branch: `git checkout -b feat/your-feature`
3. Commit your changes: `git commit -m "feat: describe your change"`
4. Push the branch: `git push origin feat/your-feature`
5. Open a Pull Request

**Code style:**
- C++ code follows the existing project style (header-only implementation, Drogon async callbacks)
- New API endpoints must include a permission string (e.g. `system:user:add`)
- No hardcoded secrets — use `config.json` or database configuration

---

## Changelog

### v1.0.0
- Full implementation of all RuoYi-Vue system management and monitoring APIs
- Embedded SQLite mode — no external database required to run
- PBKDF2-SHA256 password hashing, JWT auto-renewal
- Built-in Nginx reverse proxy startup management
- Email sender management (OpenSSL Implicit-TLS SMTP)
- Forgot password / registration email verification
- WebSocket real-time notifications (one-time ticket authentication)
- IP rate limiting, bot UA blocking, XSS filtering, CORS configuration
- Role permission changes take effect in real time — no re-login required

---

## Community

QQ Group: **7827982393**

Official Website: <http://www.gzbgw.com>

Question Bank Platform: <http://question.gzbgw.com>

---

## License

This project is open-sourced under the [MIT License](LICENSE).

RuoYi-Vue is copyright of the [RuoYi Team](https://gitee.com/y_project/RuoYi-Vue), licensed under MIT.
