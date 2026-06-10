/**
 * @file KoboldCppService.h
 * @brief KoboldCpp 本地 LLM 服务 — 集成本地大语言模型
 * 
 * 功能概述：
 *   - 本地 LLM 集成：与 KoboldCpp 服务器通信
 *   - 聊天完成：支持 OpenAI 兼容的聊天接口
 *   - 文本生成：支持原始文本生成
 *   - 参数控制：支持温度、最大令牌数等参数
 * 
 * KoboldCpp 说明：
 *   - 开源本地 LLM 推理引擎
 *   - 支持多种模型（Llama、Mistral 等）
 *   - 提供 OpenAI 兼容的 API
 *   - 可在 CPU/GPU 上运行
 * 
 * 使用示例：
 *   // 设置端口
 *   KoboldCppService::instance().setPort(5001);
 *   
 *   // 检查就绪状态
 *   if (KoboldCppService::instance().isReady()) {
 *       // 聊天完成
 *       std::string response = KoboldCppService::instance().chat(
 *           "你好，请介绍一下自己",
 *           "你是一个有帮助的 AI 助手",
 *           0.7f,
 *           512
 *       );
 *   }
 * 
 * 配置示例（config.json）：
 *   {
 *     "koboldcpp": {
 *       "enabled": true,
 *       "port": 5001,
 *       "model": "mistral-7b",
 *       "gpu_layers": 32
 *     }
 *   }
 * 
 * @see WhisperService - 语音识别服务
 */

#pragma once
#include <string>

/**
 * @class KoboldCppService
 * @brief KoboldCpp 本地 LLM 服务单例
 * 
 * 与 KoboldCpp HTTP 服务器通信，提供本地 LLM 推理能力。
 * 采用单例模式，全局唯一实例。
 */
class KoboldCppService {
public:
    /**
     * @brief 获取单例实例
     * @return KoboldCppService 单例引用
     */
    static KoboldCppService& instance();

    /**
     * @brief 设置 KoboldCpp 服务端口
     * @param port 端口号（默认 5001）
     */
    void setPort(int port) { port_ = port; }

    /**
     * @brief 检查 KoboldCpp 服务是否就绪
     * @return 是否就绪
     */
    bool isReady() const;

    /**
     * @brief 聊天完成（OpenAI 兼容接口）
     * 
     * 发送聊天消息到 KoboldCpp，获取 AI 回复。
     * 
     * @param message 用户消息
     * @param systemPrompt 系统提示词（可选）
     * @param temperature 温度参数（0.0-2.0，默认 0.7）
     * @param maxTokens 最大生成令牌数（默认 512）
     * @return AI 回复内容
     */
    std::string chat(const std::string& message,
                     const std::string& systemPrompt = "",
                     float temperature = 0.7f, int maxTokens = 512);

    /**
     * @brief 文本生成（原始完成）
     * 
     * 发送提示词到 KoboldCpp，获取生成的文本。
     * 
     * @param prompt 提示词
     * @param temperature 温度参数（0.0-2.0，默认 0.7）
     * @param maxTokens 最大生成令牌数（默认 512）
     * @return 生成的文本
     */
    std::string generate(const std::string& prompt,
                         float temperature = 0.7f, int maxTokens = 512);

    /**
     * @brief 获取最后的错误信息
     * @return 错误信息字符串
     */
    std::string lastError() const { return lastError_; }

private:
    KoboldCppService() = default;
    KoboldCppService(const KoboldCppService&) = delete;
    KoboldCppService& operator=(const KoboldCppService&) = delete;

    /**
     * @brief 发送 HTTP POST 请求
     * 
     * @param path API 路径
     * @param body 请求体（JSON）
     * @return 响应内容
     */
    std::string httpPost(const std::string& path,
                         const std::string& body) const;

    int         port_      = 5001;                 ///< KoboldCpp 服务端口
    std::string lastError_;                        ///< 最后的错误信息
};
