#pragma once
#include <string>
#include <cstdio>
#include <iostream>

// 通过子进程调用 vault-with-ui.exe kv get 取单个字段值
// 失败或 Vault 不可用时返回空字符串（调用方自行降级处理）
namespace VaultClient {

    struct Config {
        std::string exePath;    // vault-with-ui.exe 全路径
        std::string addr;       // http://127.0.0.1:8200
        std::string token;      // Root Token / AppRole token
        bool        enabled = false;
    };

    inline std::string getField(const Config& cfg,
                                const std::string& secretPath,
                                const std::string& field) {
        if (!cfg.enabled || cfg.exePath.empty()) return "";

#ifdef _WIN32
        // Windows: CMD 拼 set 环境变量 + 调用 vault
        std::string cmd =
            "cmd /C \"set VAULT_ADDR=" + cfg.addr +
            " & set VAULT_TOKEN=" + cfg.token +
            " & \"\"" + cfg.exePath + "\"\" kv get -field=" + field +
            " " + secretPath + " 2>nul\"";
        FILE* pipe = _popen(cmd.c_str(), "r");
#else
        std::string cmd =
            "VAULT_ADDR=\"" + cfg.addr + "\" VAULT_TOKEN=\"" + cfg.token +
            "\" \"" + cfg.exePath + "\" kv get -field=" + field +
            " " + secretPath + " 2>/dev/null";
        FILE* pipe = popen(cmd.c_str(), "r");
#endif
        if (!pipe) {
            std::cout << "[VaultClient] 无法启动 Vault 子进程" << std::endl;
            return "";
        }

        std::string result;
        char buf[256];
        while (fgets(buf, sizeof(buf), pipe)) result += buf;

#ifdef _WIN32
        _pclose(pipe);
#else
        pclose(pipe);
#endif
        // 去掉尾部换行/空格
        while (!result.empty() &&
               (result.back() == '\n' || result.back() == '\r' || result.back() == ' '))
            result.pop_back();

        if (result.empty())
            std::cout << "[VaultClient] 字段 " << field << " 为空（Vault 未启动或未解封？）" << std::endl;

        return result;
    }
}
