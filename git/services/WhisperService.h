#pragma once
#include <string>
#include <vector>

// Thin wrapper - proxies to koboldcpp's built-in Whisper HTTP API
class WhisperService {
public:
    static WhisperService& instance();

    void setPort(int port) { port_ = port; }
    bool isReady() const;

    // audioData: raw PCM float32 16kHz mono  (-1.0 ~ 1.0)
    std::string transcribe(const std::vector<float>& audioData,
                           const std::string& language = "auto");

    std::string lastError() const { return lastError_; }

private:
    WhisperService() = default;
    WhisperService(const WhisperService&) = delete;
    WhisperService& operator=(const WhisperService&) = delete;

    std::string httpPost(const std::string& path,
                         const std::string& body) const;

    int         port_ = 5001;
    std::string lastError_;
};
