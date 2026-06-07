# Watchdog 跨平台守护进程

`watchdog` 是一个轻量级、跨平台、零依赖的守护进程管理器，完全基于纯 C++17 标准库编写。它专门设计用于保障 RuoYi-Cpp 服务端（或其他应用程序）的 7×24 小时高可用性运行。

---

## 📦 目录结构

```
watchdog/
├── watchdog.cc         # 守护进程主程序源码（跨平台实现）
├── watchdog.ini        # 守护进程配置文件（INI 格式）
└── README.md           # 本文档
```

---

## ✨ 核心特性

1. **跨平台支持**：
   * **Windows**：原生使用 `CreateProcess`、`WaitForSingleObject` 等 Win32 API 维护生命周期。
   * **Linux / macOS**：原生通过 `fork`、`execvp`、`waitpid` 和系统信号处理实现守护。
2. **多实例并行监控 (`count`)**：
   * 支持通过配置文件或命令行设定 `count` 参数，启动并监控多个完全平行的应用程序副本（多实例模式）。
3. **双重可用性判定（生命周期 + 活跃心跳）**：
   * **状态探测**：当子进程意外崩溃或退出（根据 PID 和 Exit Code），守护进程会在设定的延迟后自动拉起它。
   * **心跳健康检查 (`heartbeat_file`)**：子进程定期更新指定的隐藏心跳文件。当心跳文件更新延迟超过阈值，即使子进程仍在运行（但可能已假死或陷入死循环），守护进程也会将其强行 `kill` 并拉起。
4. **单实例锁 (Windows)**：
   * 在 Windows 环境下使用 Named Mutex (`ruoyi-watchdog-singleton`) 保证在一台机器上只会运行一个 watchdog 主进程。
5. **高性能日志轮转 (Log Rotation)**：
   * 支持指定最大日志文件大小（`max_log_size`）与备份保留数量（`max_log_files`），自动滚动轮转，防止磁盘空间被写满。

---

## ⚙️ 配置文件说明 (`watchdog.ini`)

```ini
# watchdog 配置文件

exe               = ruoyi-cpp.exe           # 待守护的子进程可执行文件名/路径
workdir           = .                       # 子进程的工作目录
args              = --config config.json    # 传递给子进程的启动命令行参数
restart_delay     = 3                       # 进程退出后，等待重新拉起的延迟（秒）
max_restarts      = 0                       # 最大重启上限。0 表示无限重启
log_file          = watchdoglogs/watchdog.log # 守护进程运行日志输出路径
check_interval    = 1000                    # 进程状态轮询检测间隔（毫秒）
count             = 1                       # 同时监控和运行的子进程实例数量（多实例模式）
heartbeat_timeout = 10                      # 心跳超时时间（秒）。0 表示不进行心跳检查
heartbeat_file    = .watchdog_heartbeat     # 心跳检测文件路径，子进程需定时刷新此文件
grace_seconds     = 30                      # 启动宽限期（秒）。在此期间不进行心跳检查
max_log_size      = 10485760                # 单个日志文件上限（字节），默认 10MB
max_log_files     = 5                       # 日志轮转时最多保留的历史备份数
```

---

## 🔨 编译与运行

由于 watchdog 零依赖，因此可以直接通过普通的 C++ 编译器（支持 C++17 即可）完成单文件编译。

### 1. 编译命令

#### Windows (GCC/MinGW)
```powershell
g++ -std=c++17 -O2 -static-libgcc -static-libstdc++ -o watchdog.exe watchdog.cc
```

#### Linux
```bash
g++ -std=c++17 -O2 -o watchdog watchdog.cc
```

### 2. 运行用法

```bash
# 方式 A：读取同目录下的默认配置文件 watchdog.ini（推荐）
./watchdog

# 方式 B：读取指定路径的配置文件
./watchdog --config /path/to/my_config.ini

# 方式 C：通过命令行参数直接覆盖参数运行
./watchdog --exe my_server --count 2 --restart-delay 5
```

---

## 💡 心跳健康检查对接指南

要在子进程（如您的业务主程序）中集成心跳：
1. 业务主程序在核心事件循环或定时器中，定期（例如每隔 3-5 秒）往 `.watchdog_heartbeat`（即 `heartbeat_file` 配置）写入任意数据或仅刷新其修改时间（如调用 `std::filesystem::last_write_time` 或重写文件）。
2. Watchdog 会不断检测该文件的最后修改时间（mtime）。
3. 如果子进程因死锁、假死、网络阻塞等原因卡死，导致超过 `heartbeat_timeout` 秒未刷新该文件，Watchdog 会强行 `kill` 假死进程，防止僵尸服务长时间挂起。
4. 刚启动子进程时，会有 `grace_seconds` 秒（默认 30s）的宽限期，在宽限期内即使没有心跳也不会触发重启，保证主程序有足够的冷启动和连接数据库的初始化时间。
