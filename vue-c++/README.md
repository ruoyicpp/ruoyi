# RuoYi-Cpp 前端管理系统 (`vue-c++`)

`vue-c++` 是 RuoYi-Cpp 后台管理系统的专用前端 UI，基于 Vue 2.x 和 Element UI 构建。它与基于 C++ (Drogon 框架) 开发的高性能后端无缝集成，实现了流畅的用户认证、动态权限菜单加载、性能监控等全套管理功能。

---

## 🛠️ 技术栈与依赖

* **前端框架**：Vue 2.6.12
* **路由与状态管理**：Vue Router 3.4.9 + Vuex 3.6.0
* **UI 组件库**：Element-UI 2.15.14
* **网络请求**：Axios 0.30.3
* **打包构建**：Vue CLI 4.4.6 + Webpack

---

## ⚙️ 核心配置说明 (`vue.config.js`)

前端默认配置已经针对 RuoYi-Cpp 进行了深度定制：

1. **服务端口**：
   * 前端本地开发服务器默认运行在：`http://localhost:3000`。
2. **后端代理 (API Proxy)**：
   * 后端 Drogon 服务默认运行在：`http://localhost:18080`。
   * 开发环境下所有 API 请求会自动代理至该地址，并进行了路径重写。
3. **WebSocket 代理**：
   * 支持通过 `/ws/` 前缀代理，提供实时的通知和工单长连接交互（`/ws/notify`、`/ws/ticket`）。
4. **高性能 Gzip 压缩**：
   * 生产环境打包时使用 `CompressionPlugin` 自动对 JS/CSS/SVG 等资源进行 Gzip 压缩（生成 `.gz` 文件），显著提升页面加载速度。

---

## 🚀 快速开始

### 1. 环境准备
确保您的开发机已安装：
* [Node.js](https://nodejs.org/) (建议版本：`>= 14.x` 且 `< 18.x`)
* npm (建议 `>= 6.x`)

### 2. 依赖安装
进入前端目录并使用国内的高速 npm 源安装依赖，避免安装卡顿或丢包：

```bash
# 进入前端目录
cd vue-c++

# 设定国内镜像源并安装依赖
npm install --registry=https://registry.npmmirror.com
```

### 3. 本地开发启动
启动前端热热加载服务：

```bash
npm run dev
```
启动成功后，浏览器会自动打开 `http://localhost:3000`。

---

## 📦 生产环境打包

打包前端静态资源用于部署到 Nginx 或由 Drogon 静态服务器托管：

```bash
# 构建并压缩生产环境产物
npm run build:prod
```

编译完成后，将在 `vue-c++/dist` 目录下生成打包好的静态资源。
