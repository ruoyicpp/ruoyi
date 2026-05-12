# Drogon 预编译库使用指南

本目录包含 Drogon 框架的预编译静态库，用于快速编译 RuoYi-Cpp 项目，无需自行编译 Drogon。

## 📦 目录结构

```
drogon-release/
├── windows/          # Windows 平台 (MSYS2 UCRT64)
│   ├── include/      # 头文件 (1.1 MB)
│   │   ├── drogon/
│   │   ├── trantor/
│   │   └── json/
│   └── lib/          # 静态库 (12 MB)
│       ├── libdrogon.a   (9.6 MB)
│       ├── libtrantor.a  (1.4 MB)
│       └── libjsoncpp.a  (380 KB)
│
└── linux/            # Linux 平台 (Ubuntu/Debian)
    ├── include/      # 头文件 (1.5 MB)
    │   ├── drogon/
    │   └── trantor/
    └── lib/          # 静态库 (12 MB)
        ├── libdrogon.a   (11 MB)
        └── libtrantor.a  (1.4 MB)
```

**总大小**: 约 26 MB (Windows 13 MB + Linux 13 MB)

## 🚀 快速开始

### 1. 放置预编译库

将 `drogon-release` 文件夹放到 `ruoyi-cpp` 项目根目录：

```
ruoyi-cpp/
├── drogon-release/    ← 放在这里
│   ├── windows/
│   └── linux/
├── src/
├── CMakeLists.txt
└── ...
```

### 2. 修改 CMakeLists.txt

打开 `ruoyi-cpp/CMakeLists.txt`，找到第 17 行左右的 `DROGON_INSTALL_PREFIX`，修改为：

**Windows 平台：**
```cmake
set(DROGON_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/drogon-release/windows"
    CACHE PATH "Path where drogon was installed")
```

**Linux 平台：**
```cmake
set(DROGON_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/drogon-release/linux"
    CACHE PATH "Path where drogon was installed")
```

或者使用自动检测：
```cmake
if(WIN32)
    set(DROGON_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/drogon-release/windows")
else()
    set(DROGON_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/drogon-release/linux")
endif()
```

### 3. 安装系统依赖

#### Windows (MSYS2 UCRT64)

```bash
# 打开 MSYS2 UCRT64 终端
pacman -S mingw-w64-ucrt-x86_64-gcc \
          mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-openssl \
          mingw-w64-ucrt-x86_64-postgresql \
          mingw-w64-ucrt-x86_64-hiredis
```

**⚠️ 重要**: 必须使用 UCRT64 工具链，不能使用 MinGW64！

#### Linux (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libssl-dev \
    libpq-dev \
    libjsoncpp-dev \
    libhiredis-dev \
    uuid-dev \
    zlib1g-dev
```

### 4. 编译项目

#### Windows

```bash
# 在 MSYS2 UCRT64 终端中
cd /g/back/recovered/ruoyi-cpp
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

#### Linux

```bash
cd /path/to/ruoyi-cpp
cmake -B build-Linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-Linux -j$(nproc)
```

## ✅ 验证安装

编译成功后，会生成以下文件：

- Windows: `build/ruoyi-cpp.exe` (约 9-10 MB)
- Linux: `build-Linux/ruoyi-cpp` (约 9-10 MB)

运行程序测试：
```bash
# Windows
./build/ruoyi-cpp.exe

# Linux
./build-Linux/ruoyi-cpp
```

如果提示缺少 `license.lic`，说明编译成功！

## 📋 版本信息

- **Drogon 版本**: 最新稳定版
- **编译日期**: 2026-05-13
- **C++ 标准**: C++17
- **编译器**: 
  - Windows: GCC 13.x (MSYS2 UCRT64)
  - Linux: GCC 13.3.0

## ❓ 常见问题

### Q1: Windows 编译报错 "ENTRYPOINT_NOT_FOUND"

**原因**: 使用了错误的工具链（MinGW64 而非 UCRT64）

**解决**: 
1. 确保使用 MSYS2 UCRT64 终端
2. 检查 PATH 环境变量，确保 `H:/msys64/ucrt64/bin` 在最前面
3. 删除 `build` 目录重新编译

### Q2: Linux 编译找不到头文件

**原因**: 系统依赖未安装

**解决**:
```bash
sudo apt-get install -y libssl-dev libpq-dev libjsoncpp-dev
```

### Q3: 链接时报错 "undefined reference"

**原因**: 静态库顺序或依赖缺失

**解决**: 确保 CMakeLists.txt 中链接顺序正确：
```cmake
target_link_libraries(ruoyi-cpp
    drogon
    trantor
    # 其他库...
)
```

### Q4: 想要重新编译 Drogon

如果预编译库不适合你的环境，可以自行编译：

**Windows:**
```bash
git clone https://github.com/drogonframework/drogon.git
cd drogon
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_INSTALL_PREFIX=../install \
         -DBUILD_SHARED_LIBS=OFF
make -j$(nproc)
make install
```

**Linux:**
```bash
git clone https://github.com/drogonframework/drogon.git
cd drogon
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
make -j$(nproc)
sudo make install
```

## 📝 注意事项

1. **工具链兼容性**: Windows 必须使用 MSYS2 UCRT64，不能混用其他工具链
2. **静态链接**: 这些是静态库 (.a)，会被完整链接到最终可执行文件中
3. **依赖库**: 虽然 Drogon 是静态库，但仍需要系统的 OpenSSL、PostgreSQL 等动态库
4. **跨平台**: Windows 和 Linux 的库不能混用，必须使用对应平台的版本

## 🔗 相关链接

- [Drogon 官方网站](https://drogon.org/)
- [Drogon GitHub](https://github.com/drogonframework/drogon)
- [Drogon 文档](https://drogon.docsforge.com/)
- [MSYS2 官网](https://www.msys2.org/)

## 📄 许可证

Drogon 框架采用 MIT 许可证。

---

**如有问题，请查看项目 README.md 或提交 Issue。**
