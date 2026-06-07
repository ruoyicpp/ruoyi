# RuoYi-Cpp 核心架构升级与新特性发布说明 (Update & Release Notes)

我们非常高兴地向大家宣布，**RuoYi-Cpp (若依 C++ 版)** 迎来了一次重大的架构升级与核心特性迭代！本次更新主要围绕**企业级分布式缓存策略**、**高性能 Prometheus 指标监控**、**独立安全/守护工具链编译**等核心模块进行了全方位集成与优化，致力于打造极低内存占用 (30~40MB)、极高吞吐、单文件一键分发的企业级 C++ 后端系统。

以下为本次更新及集成特性的详细说明，便于共享至 Gitee 社区：

---

## 🚀 核心新增功能

### 1. 企业级多层分布式缓存策略 (`src/cache/`)
为解决高并发场景下的数据读取瓶颈、保护数据库免受瞬时流量冲击，本次正式集成并启用了多层分布式缓存架构。
* **多层架构**：支持 **L1: 本地高性能内存缓存 (Shared Memory)** 与 **L2: 进程外分布式缓存 (Redis / Redis Cluster)** 协同工作。
* **高可用防击穿/防穿透机制**：
  * **防穿透**：引入智能空值缓存 (Null Value Caching) 机制，对不存在的 Key 缓存特定空对象，并支持设定短 TTL。
  * **防击穿/雪崩**：基础获取操作引入互斥锁机制，确保同一热点 Key 在失效时只有一个请求回源加载数据库；引入过期时间随机扰动 (Jitter)，防止大量 Key 同时过期。
* **模式匹配清理**：支持高效率的 Pattern 批量 Key 扫描与删除。

### 2. Prometheus 规范化指标监控与度量收集 (`src/monitor/`)
深度集成 Prometheus 度量标准，为主程序注入原生的生产级监控与微服务观测能力。
* **全面度量覆盖**：
  * **HTTP 流量**：统计请求总数 (`http_requests_total`)、请求错误数 (`http_request_errors_total`)、处理耗时分布直方图 (`http_request_duration_seconds`) 以及当前在途请求数 (`http_requests_in_flight`)。
  * **数据库吞吐**：实时监控 DB 查询总数 (`db_query_total`)、错误数 (`db_query_errors_total`) 以及耗时直方图。
  * **Redis 状态**：监控操作总数、错误数、耗时分布直方图以及连接池大小。
* **标准 Prometheus 导出器**：内置 `GET /metrics` 与 `/monitor/stats` 接口，支持被外部 Prometheus 监控系统直接拉取 (Pull) 并无缝对接 **Grafana 仪表盘**。

### 3. 独立工具链与守护进程完美编译
通过对底层 CMakeLists 的深度优化，正式支持除主程序外的另外两个关键 EXE 独立编译：
* **`watchdog.exe` (守护进程)**：零依赖、超轻量的进程监控伴侣。支持双击防误触、自动对 `ruoyi-cpp.exe` 进行心跳观测与异常崩溃自动拉起，保障生产环境 7×24h 不间断运行。
* **`sqlite_cipher_tool.exe` (安全加密工具)**：支持对 SQLite3 数据库进行全库级/页级的 AES-256 (SQLCipher 兼容) 与 ChaCha20 加密、解密、密钥轮转 (Rekey) 与自检，为系统底层数据资产安全护航。

---

## 🛠️ 底层技术演进与 C++20 性能调优

在本次将新增模块编译进 `ruoyi-cpp` 的过程中，针对 Windows (MinGW/UCRT64) 与 C++20 标准进行了深度调优：
1. **Labels 结构体列表初始化优化**：
   * 针对代码中常用的 `Labels{{"method", "GET"}, {"path", "/login"}}` 语法，为 `Labels` 专门实现了双键值对 `std::initializer_list` 构造函数，避免旧编译器在转换多参数时抛出匹配错误。
2. **克服原子成员不可复制/移动的编译瓶颈**：
   * 直方图内部结构 `Bucket` 包含了 `std::atomic<uint64_t>`（计数器），导致默认的拷贝/移动构造函数被禁用。
   * 我们为 `Bucket` 重新设计并手写了**自定义的拷贝构造、移动构造函数以及拷贝/移动赋值运算符**。通过原子级的值加载（`load`）与存储（`store`），彻底消除了 `std::vector` 在扩容重新分配内存时的编译冲突，代码编译立刻畅通无阻。
3. **Json 依赖解耦**：
   * 在缓存模块的泛型定义中，支持智能引入并解耦 `Json::Value`，保证在 variant 缓存类型转换时的类型安全。

---

## 📦 现有强大集成特性回顾 (为什么我们如此优秀？)

* **进程内 Nginx 深度嵌入 (`RUOYI_USE_NGINX=ON`)**：
  * 主程序启动时会自动拉起独立线程，通过高性能 **IOCP 异步事件循环** 运行嵌入式 Nginx，直接在 EXE 内部实现 HTTP/HTTPS/HTTP2、反向代理（直连后端 18080 端口）、Gzip/Brotli 压缩、静态资源托管，实现真正意义上的**单 EXE 极简部署**。
* **全面兼容若依官方前后端生态**：
  * 完美适配官方 Vue2 / Vue3 移动端与桌面端。
  * 支持带 `/dev-api` 或 `/prod-api` 前缀的路由自动剥离，自动修正 Bot 用户代理拦截白名单，支持在 `config.json` 中一键开启/关闭接口签名认证。
* **国密/多数据源支持**：
  * 内置国密 SM2/SM3/SM4 加解密，支持 PostgreSQL、MySQL、SQLite3 等多层混合数据源及自研的高性能 PostgreSQL 异步连接池驱动。

---

## 📂 文件变更清单 (Files Touched)

* `@/g:\back\recovered\ruoyi-cpp\src\cache\CacheStrategy.h`：引入 `<json/json.h>`，定义多层缓存数据结构与锁机制。
* `@/g:\back\recovered\ruoyi-cpp\src\monitor\MetricsCollector.h`：增加 `Labels` 初始化列表支持，手写 `Bucket` 的原子安全拷贝/移动构造器。
* `@/g:\back\recovered\ruoyi-cpp\src\monitor\MetricsCollector.cc`：优化 Histogram 对象的内存装配逻辑，全面采用 `emplace_back`。
* `@/g:\back\recovered\ruoyi-cpp\CMakeLists.txt`：激活 `watchdog` 及 `sqlite_cipher_tool` 的编译任务。

---

## 🎯 编译与运行说明

### 1. 快速编译所有组件 (主程序 + 守护进程 + 加密工具)
```powershell
cd g:\back\recovered\ruoyi-cpp
# 创建编译目录
mkdir build-nginx
cd build-nginx
# 配置开启 SQLite 强加密支持
cmake .. -G "Ninja" -DENABLE_SQLITE_ENCRYPTION=ON
# 并行编译所有目标
ninja
```

### 2. 独立运行与测试
* 启动主服务：双击运行 `ruoyi-cpp.exe`（自动拉起内置 Nginx 并在本地 18080 端口监听）。
* 启用守护：双击运行 `watchdog.exe`，它将自动进入后台守护主程序。
