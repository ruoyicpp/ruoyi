/**
 * @file WhisperService.h
 * @brief Whisper 语音识别服务 — 集成 OpenAI Whisper 模型
 * 
 * 功能概述：
 *   - 语音识别：将音频转录为文本
 *   - 多语言支持：自动检测或指定语言
 *   - KoboldCpp 集成：通过 KoboldCpp 的 Whisper API
 *   - 实时处理：支持流式音频处理
 * 
 * Whisper 说明：
 *   - OpenAI 开源语音识别模型
 *   - 支持 99 种语言
 *   - 鲁棒性强，支持背景噪音
 *   - 可通过 KoboldCpp 本地运行
 * 
 * 音频格式：
 *   - 格式：PCM float32
 *   - 采样率：16 kHz
 *   - 声道：单声道
 *   - 范围：-1.0 ~ 1.0
 * 
 * 使用示例：
 *   // 设置端口
 *   WhisperService::instance().setPort(5001);
 *   
 *   // 检查就绪状态
 *   if (WhisperService::instance().isReady()) {
 *       // 转录音频
 *       std::vector<float> audioData = loadAudioFile("audio.wav");
 *       std::string text = WhisperService::instance().transcribe(
 *           audioData,
 *           "zh"  // 中文
 *       );
 *   }
 * 
 * 配置示例（config.json）：
 *   {
 *     "whisper": {
 *       "enabled": true,
 *       "port": 5001,
 *       "model": "base",
 *       "language": "auto"
 *     }
 *   }
 * 
 * @see KoboldCppService - KoboldCpp 本地 LLM 服务
 */

#pragma once
#include <string>
#include <vector>

/**
 * @class WhisperService
 * @brief Whisper 语音识别服务单例
 * 
 * 与 KoboldCpp 的 Whisper API 通信，提供语音识别能力。
 * 采用单例模式，全局唯一实例。
 */
class WhisperService {
public:
    /**
     * @brief 获取单例实例
     * @return WhisperService 单例引用
     */
    static WhisperService& instance();

    /**
     * @brief 设置 Whisper 服务端口
     * @param port 端口号（默认 5001）
     */
    void setPort(int port) { port_ = port; }

    /**
     * @brief 检查 Whisper 服务是否就绪
     * @return 是否就绪
     */
    bool isReady() const;

    /**
     * @brief 转录音频为文本
     * 
     * 将 PCM 音频数据转录为文本。
     * 
     * 音频格式要求：
     *   - 格式：PCM float32
     *   - 采样率：16 kHz
     *   - 声道：单声道
     *   - 范围：-1.0 ~ 1.0
     * 
     * @param audioData 音频数据（PCM float32）
     * @param language 语言代码（"auto" 自动检测，"zh" 中文等）
     * @return 转录的文本
     */
    std::string transcribe(const std::vector<float>& audioData,
                           const std::string& language = "auto");

    /**
     * @brief 获取最后的错误信息
     * @return 错误信息字符串
     */
    std::string lastError() const { return lastError_; }

private:
    WhisperService() = default;
    WhisperService(const WhisperService&) = delete;
    WhisperService& operator=(const WhisperService&) = delete;

    /**
     * @brief 发送 HTTP POST 请求
     * 
     * @param path API 路径
     * @param body 请求体
     * @return 响应内容
     */
    std::string httpPost(const std::string& path,
                         const std::string& body) const;

    int         port_ = 5001;                 ///< Whisper 服务端口
    std::string lastError_;                   ///< 最后的错误信息
};
