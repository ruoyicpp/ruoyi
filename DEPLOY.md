# RuoYi-Cpp 部署指南

[English](DEPLOY_EN.md) | 中文

本文档说明如何直接使用预编译二进制文件部署 RuoYi-Cpp，无需自行编译。

---

## 一、下载发布包

从 [Releases](../../releases) 页面下载对应平台的压缩包：

| 文件名 | 平台 |
|--------|------|
| `ruoyi-cpp-windows-x64.zip` | Windows 10/11 x64 |
| `ruoyi-cpp-linux.tar.gz` | Linux x64，glibc ≥ 2.39（Ubuntu 24.04 / Debian 13 / RHEL 9.4+）|

---

## 二、Linux 部署

### 1. 解压

```bash
tar -xzf ruoyi-cpp-linux.tar.gz
cd ruoyi-cpp
```

发布包目录结构：

```
ruoyi-cpp/
├── ruoyi-cpp          # 可执行文件
├── config.json        # 配置文件（按需修改）
├── web/               # 前端静态文件
└── upload/            # 上传文件目录（自动创建）
```

### 2. 配置

编辑 `config.json`，至少修改以下字段：

```json
{
    "app": {
        "base_url": "https://your-domain.com"
    },
    "database": {
        "host": "127.0.0.1",
        "port": 5432,
        "dbname": "ruoyi",
        "user": "postgres",
        "passwd": "your-password"
    }
}
```

> **提示**：不安装 PostgreSQL 也可运行，系统会自动使用内嵌 SQLite 数据库。

### 3. 启动

```bash
chmod +x ruoyi-cpp
nohup ./ruoyi-cpp > ruoyi.log 2>&1 &
```

默认监听 `0.0.0.0:18080`，浏览器访问 `http://服务器IP:18080`。

### 4. 配置 Nginx 反向代理（推荐）

**宝塔面板**：网站 → 反向代理 → 添加，目标 URL 填 `http://127.0.0.1:18080`。

**手动配置**：

```nginx
location / {
    proxy_pass http://127.0.0.1:18080;
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_http_version 1.1;
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection "upgrade";
}
```

### 5. 停止 / 重启

```bash
# 停止
pkill ruoyi-cpp

# 重启
pkill ruoyi-cpp && sleep 1 && nohup ./ruoyi-cpp > ruoyi.log 2>&1 &

# 查看日志
tail -f ruoyi.log
```

---

## 三、Windows 部署

### 1. 解压

解压 `ruoyi-cpp-windows-x64.zip` 到任意目录。

### 2. 配置

编辑 `config.json`，修改数据库连接等配置。

### 3. 启动

双击 `ruoyi-cpp.exe`，或在命令行中运行：

```cmd
ruoyi-cpp.exe
```

浏览器访问 `http://localhost:18080`。

---

## 四、默认账号

| 账号 | 密码 |
|------|------|
| `admin` | `admin123` |

> **首次登录后请立即修改密码。**

---

## 五、数据库说明

| 场景 | 数据库 | 说明 |
|------|--------|------|
| 未安装 PostgreSQL | SQLite（内嵌） | 零依赖，开箱即用，数据存于 `ruoyi.db` |
| 已安装 PostgreSQL | PostgreSQL | 生产推荐，支持高并发 |
| 双写模式 | PostgreSQL + SQLite | PG 断线时自动降级 SQLite，恢复后同步回写 |

---

## 六、常见问题

**Q: 启动后访问 502？**  
A: 检查 Nginx 反向代理目标端口是否为 `18080`，或直接访问 `http://IP:18080` 确认后端是否正常。

**Q: 数据库连接失败怎么办？**  
A: 检查 `config.json` 中的数据库配置，或将 `sqlite.enabled` 设为 `true` 先用 SQLite 运行。

**Q: 如何查看后端日志？**  
```bash
tail -f ruoyi.log
# 或
tail -f logs/ruoyi.*.log
```

**Q: 菜单中的功能页面打不开（localhost 拒绝连接）？**  
A: 在 `config.json` 的 `app.base_url` 填入你的实际域名，重启后自动修复。
