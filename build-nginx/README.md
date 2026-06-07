# RuoYi-Cpp Nginx 与生产部署配置集 (`build-nginx`)

`build-nginx` 目录提供了 RuoYi-Cpp 生产部署所需的各种服务配置文件模板、Nginx 反向代理最佳实践以及详细的生产运行指南。

---

## 📦 目录结构

```
build-nginx/
├── 部署说明.md            # 系统全面部署文档（核心指南）
├── certmanager-web/       # 证书管理前端单页面（Nginx 静态代理版）
│   └── index.html
├── web/                   # 预留给前端编译产物（vue-c++/dist）的托管目录
└── README.md              # 本文档
```

---

## ✨ 核心组件与说明

### 1. 生产环境部署说明 (`部署说明.md`)
这是 RuoYi-Cpp 的核心系统运维说明书，详细讲解了以下内容：
* **快速启动**：本地一键启动。
* **访问模式选择**：
  * **模式 ①**：本地/局域网开发模式。
  * **模式 ②**：公网 HTTP 模式。
  * **模式 ③**：公网 HTTPS 强安全模式（包括**手动上传云厂商证书**与**自动 ACME Let's Encrypt 免费证书续签**两种方式）。
* **跨域 CORS 配置**：当前后端分离部署时的白名单域名管理。
* **常用端口矩阵表**。

### 2. Nginx 反向代理配置
在多服务或前后端分离部署场景中，通常在 C++ 后端前方挂载一层 Nginx。以下是典型的 `nginx.conf` 代理片段配置参考：

```nginx
server {
    listen 80;
    server_name yourdomain.com;

    # 1. 静态资源托管：代理前端 Vue 编译产物
    location / {
        root /opt/ruoyi-cpp/build-nginx/web;
        index index.html;
        try_files $uri $uri/ /index.html;
    }

    # 2. 动态接口代理：转发至 Drogon C++ 后端
    location /prod-api/ {
        proxy_pass http://127.0.0.1:18080/;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }

    # 3. 实时消息代理：WebSocket 监听
    location /ws/ {
        proxy_pass http://127.0.0.1:18080/ws/;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
    }
}
```

---

## 🚀 部署步骤概要

1. **编译前端**：
   在 `vue-c++` 目录下运行 `npm run build:prod`，将编译生成的 `dist/` 内所有文件拷贝至 `build-nginx/web/` 下。
2. **准备配置文件**：
   根据域名、数据库及 Redis 地址，修改并调整项目根目录下的 `config.json`，将其作为程序的运行配置文件。
3. **启动后端服务**：
   通过 `watchdog` 守护进程或 Docker-Compose 启动 `ruoyi-cpp` 后端程序。
4. **验证可用性**：
   访问配置的域名或 IP，查看管理后台及实时性能监控是否运行正常。
