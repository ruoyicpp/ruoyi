# CertManager SSL 证书管理前端 (`certmanager-web`)

`certmanager-web` 是一个极简、极其轻量的单页面应用（SPA），专用于系统的 SSL 证书监控、申请、续签与统筹管理。

---

## 📦 目录结构

```
certmanager-web/
├── index.html          # 前端单页面主程序（整合了 Tailwind CSS 与 Alpine.js）
└── README.md           # 本文档
```

---

## ✨ 核心特性

1. **零构建步骤 (Zero Build Steps)**：
   * 采用纯 HTML 结构，采用 CDN 动态加载 **Tailwind CSS** 与 **Alpine.js** 响应式引擎。
   * 无需任何 `npm install`、`webpack` 或 `vite` 编译步骤，支持即改即生效、即开即用。
2. **仪表盘大盘监控 (Dashboard)**：
   * 实时分析并归类显示：SSL 证书总数、有效证书数（Valid）、即将过期数（Expiring Soon，以黄色高亮提示）以及已过期证书数（Expired，以红色高亮提示）。
3. **自动化 ACME 证书申请 (Obtain Certificate)**：
   * 支持通过 ACME 协议进行多域名自动化证书申请（对接 Let's Encrypt / ZeroSSL 等）。
   * 提供前端申请向导，支持一键配置并调用后端的自动化 HTTP-01 验证、生成与本地证书库存储。
4. **自适应暗黑模式 (Dark Mode)**：
   * 原生支持深色/浅色模式自适应切换，提供丝滑的 UI 交互视觉体验。

---

## 🌐 部署与运行说明

由于该模块是纯静态网页（不含复杂的资源打包），它拥有极高的部署灵活性：

### 1. Drogon 静态托管（推荐，零外部依赖）
在 RuoYi-Cpp 的 `config.json` 中配置 Drogon 的静态文件目录（`document_root`），将本目录放入静态资源树下，即可通过：
`http://localhost:18080/certmanager-web/index.html` 直接访问并与后端管理 API 通信。

### 2. Nginx 反向代理与静态托管
在生产环境中，可通过 Nginx 直接代理此 `index.html`，并将 API 请求转发到 `ruoyi-cpp` 容器或本地服务。
```nginx
server {
    listen 80;
    server_name cert.yourdomain.com;

    location / {
        root /opt/ruoyi-cpp/certmanager-web;
        index index.html;
    }

    # 后端 C++ 证书服务 API 代理
    location /api/ {
        proxy_pass http://127.0.0.1:18080/;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```
