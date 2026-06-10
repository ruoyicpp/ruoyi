# Linux 运行环境限制：严格要求 Ubuntu 24.04 LTS

---

### ⚠️ 重要公告：Linux 运行环境限制
本项目的 Linux 二进制编译版本与运行环境**严格要求且仅支持 Ubuntu 24.04 LTS (Noble Numbat)** 或更高版本。
请勿在 Ubuntu 22.04、Debian 12 或更低版本的 Linux 系统中直接运行编译后的程序，否则会导致严重的运行时错误或崩溃。

---

### 🛠️ 为什么严格限制在 Ubuntu 24.04？

1. **GLIBC 2.39+ 强依赖**
   - Ubuntu 24.04 默认搭载了 **GLIBC 2.39**。
   - `ruoyi-cpp` 编译时链接了 GLIBC 2.39 特有的底层符号（如数学库和线程库的新特性）。
   - 如果在低版本系统（如仅搭载 GLIBC 2.35 的 Ubuntu 22.04）中运行，会直接报错：
     ```bash
     /lib/x86_64-linux-gnu/libc.so.6: version `GLIBC_2.39' not found
     ```

2. **C++20 标准库特性（GCC 13 / libstdc++ 13+）**
   - 本项目采用了大量现代 **C++20 核心特性**（如 `std::ranges`、`std::format`、高级协程以及原子屏障等）。
   - 低于 Ubuntu 24.04 的系统（如 Ubuntu 22.04 默认使用 GCC 11）其附带的 `libstdc++.so.6` 并不完整支持 C++20 的这些高级运行时特性，会导致链接或运行时异常。

3. **OpenSSL 3.3.0 与安全符号对齐**
   - 本项目的加密、HTTPS 证书申请及安全认证模块深度依赖系统的 OpenSSL 库。
   - Ubuntu 24.04 采用了最新的 OpenSSL 3.3.0 以及具有 `t64` 后缀的现代化库包（例如 `libssl3t64`）。这些动态链接符号在旧系统上无法解析。

4. **现代化第三方依赖 ABI 一致性**
   - 项目用到的数据库连接驱动（`libpq`）、Redis 驱动（`libhiredis`）以及 `libjsoncpp` 在 Ubuntu 24.04 上的编译 ABI 标准（具有 64 位 time_t 支持等特性）与旧版本不兼容。

---

### 💡 解决方案

#### 方案一：在 Ubuntu 24.04 环境中直接运行（推荐）
请确保您的目标生产服务器或开发机已安装并更新至 **Ubuntu 24.04 LTS**，然后直接运行：
```bash
./ruoyi-cpp
```

#### 方案二：使用 Docker 容器化部署（跨平台通用）
如果您的宿主机无法升级到 Ubuntu 24.04（例如必须使用 CentOS, Debian 或 Ubuntu 22.04），请使用我们提供的 **Docker 部署方案**。
Docker 镜像内部包含了完整的 Ubuntu 24.04 运行时环境，这能让您在**任意** Linux 宿主机上完美运行本项目：
```bash
# 启动项目容器（其内部基于 Ubuntu 24.04 镜像构建）
docker-compose up -d
```

#### 方案三：自主本地源码编译（最彻底的兼容方案）
上述限制**仅针对直接使用官方预编译的 Linux 二进制安装包**。如果您在自己的 Linux 系统（如 Ubuntu 22.04, Debian 12, CentOS 等）上直接通过**源码编译**，编译器将自动匹配您本地系统的 GLIBC、OpenSSL 与 C++20 运行时库，**完全可以在各种 Linux 操作系统中完美运行**：
```bash
# 本地编译步骤：
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja ruoyi-cpp
```
