# RuoYi-Cpp 前端

基于 [RuoYi-Vue](https://gitee.com/y_project/RuoYi-Vue) 前端改造，适配 C++ 后端（Drogon 框架）。

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