/**
 * @file KoboldCppManager.h
 * @brief KoboldCpp 大语言模型管理器 — 本地 LLM 推理引擎集成
 * 
 * 功能概述：
 *   - 模型推理：支持本地大语言模型推理
 *   - 进程管理：启动、停止、监控 KoboldCpp 进程
 *   - GPU 加速：支持 GPU 加速推理（CUDA、ROCm）
 *   - 模型管理：支持多个模型的加载和切换
 *   - API 接口：提供 HTTP API 进行推理请求
 * 
 * 核心特性：
 *   - 灵活启动：支持 bat/exe 直接运行或 Python 脚本启动
 *   - GPU 支持：支持 NVIDIA CUDA 和 AMD ROCm
 *   - 自动重启：进程崩溃时自动重启
 *   - 性能优化：支持批处理、上下文大小、线程数配置
 *   - 跨平台：支持 Windows 和 Linux
 * 
 * 启动方式：
 *   1. 直接运行：使用 launchCmd 直接运行 bat/exe
 *   2. Python 脚本：使用 pythonExe 运行 koboldcpp.py
 * 
 * 配置项（config.json）：
 *   - koboldcpp.enabled: 是否启用（默认 false）
 *   - koboldcpp.launchCmd: 启动命令（优先）
 *   - koboldcpp.pythonExe: Python 可执行文件路径
 *   - koboldcpp.scriptPath: koboldcpp.py 路径
 *   - koboldcpp.modelPath: 模型文件路径
 *   - koboldcpp.port: API 监听端口（默认 5001）
 *   - koboldcpp.threads: 推理线程数（默认 4）
 *   - koboldcpp.contextSize: 上下文大小（默认 2048）
 *   - koboldcpp.useGpu: 是否使用 GPU（默认 false）
 *   - koboldcpp.gpuLayers: GPU 层数（默认 99）
 * 
 * 支持的模型格式：
 *   - GGUF：量化模型格式（推荐）
 *   - GGML：原始模型格式
 *   - SafeTensors：Hugging Face 模型格式
 * 
 * GPU 加速：
 *   - NVIDIA CUDA：使用 CUDA 核心加速
 *   - AMD ROCm：使用 ROCm 加速
 *   - CPU：纯 CPU 推理（较慢）
 * 
 * API 端点：
 *   - POST /api/v1/generate - 生成文本
 *   - POST /api/v1/chat/completions - 聊天完成
 *   - GET /api/v1/model - 获取当前模型
 *   - POST /api/v1/model - 加载新模型
 * 
 * @see WhisperService - 语音识别服务
 * @see OnnxService - ONNX 推理服务
 */

#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#ifdef _WIN32
#include <windows.h>
#endif

struct KoboldCppConfig {
    bool        enabled      = false;

    // ── 方式一：直接指定 bat / exe（优先）─────────────────────────
    std::string launchCmd;   // cmd /c xxx.bat  或  xxx.exe  直接运行

    // ── 方式二：python 脚本（launchCmd 为空时使用）────────────────
    std::string pythonExe    = "python";
    std::string scriptPath;          // koboldcpp.py 路径
    std::string modelPath;
    std::string whisperModel;
    int         port         = 5001;
    int         threads      = 4;
    int         contextSize  = 2048;
    int         blasBatch    = 512;
    bool        useGpu       = false;
    int         gpuLayers    = 99;

    // ── 公共选项 ───────────────────────────────────────────────────
    std::string workDir;     // 子进程工作目录（为空时继承父进程）
    bool        autoRestart  = true;
    bool        showWindow   = false;
};

class KoboldCppManager {
public:
    static KoboldCppManager& instance();

    bool start(const KoboldCppConfig& cfg);
    void stop();
    bool isRunning() const;
    int  port() const { return cfg_.port; }

private:
    KoboldCppManager();
    ~KoboldCppManager();
    KoboldCppManager(const KoboldCppManager&) = delete;
    KoboldCppManager& operator=(const KoboldCppManager&) = delete;

    bool        spawnProcess();
    void        startMonitor();
    void        stopMonitor();

    KoboldCppConfig     cfg_;
    std::atomic<bool>   running_{false};
    std::atomic<bool>   monRunning_{false};
    std::thread         monThread_;
    mutable std::mutex  mu_;

#ifdef _WIN32
    HANDLE hProc_ = nullptr;
    HANDLE hJob_  = nullptr;
    DWORD  pid_   = 0;
#else
    pid_t  pid_   = 0;
#endif
};
