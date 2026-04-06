#pragma once
#include <string>

// Thin wrapper – calls the koboldcpp HTTP server running as a subprocess.
class KoboldCppService {
public:
    static KoboldCppService& instance();

    void setPort(int port) { port_ = port; }
    bool isReady() const;

    // OpenAI-compatible chat completions
    std::string chat(const std::string& message,
                     const std::string& systemPrompt = "",
                     float temperature = 0.7f, int maxTokens = 512);

    // Raw text completion
    std::string generate(const std::string& prompt,
                         float temperature = 0.7f, int maxTokens = 512);

    std::string lastError() const { return lastError_; }

private:
    KoboldCppService() = default;
    KoboldCppService(const KoboldCppService&) = delete;
    KoboldCppService& operator=(const KoboldCppService&) = delete;

    std::string httpPost(const std::string& path,
                         const std::string& body) const;

    int         port_      = 5001;
    std::string lastError_;
};
