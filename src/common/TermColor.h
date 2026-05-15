// 终端彩色输出工具——按截图调色板对齐桌面发布版
//   TermColor::init()         —— 启用 Windows VT 序列；并把 std::cout 替换为
//                                  彩色 streambuf，自动给 [TAG] 标签和
//                                  INFO/WARN/ERROR/DEBUG/TRACE/FATAL 关键字着色
//   TermColor::writeColored() —— 供 ColorLogger 拦截 trantor 日志后直接调用
//   TermColor::_decorate()    —— 行级染色函数，供 JsonLogger 复用
#pragma once

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <ostream>
#include <streambuf>
#include <string>
#include <string_view>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace TermColor {

// ── ANSI 颜色码（与桌面版一致）──────────────────────────────────────────
namespace _ansi {
    // 标准 16 色
    constexpr const char* RESET   = "\x1b[0m";
    constexpr const char* BLACK   = "\x1b[30m";
    constexpr const char* RED     = "\x1b[31m";
    constexpr const char* GREEN   = "\x1b[32m";
    constexpr const char* YELLOW  = "\x1b[33m";
    constexpr const char* BLUE    = "\x1b[34m";
    constexpr const char* MAGENTA = "\x1b[35m";
    constexpr const char* CYAN    = "\x1b[36m";
    constexpr const char* WHITE   = "\x1b[37m";
    constexpr const char* GRAY    = "\x1b[90m";
    // 高亮 16 色
    constexpr const char* B_RED     = "\x1b[1;31m";
    constexpr const char* B_GREEN   = "\x1b[1;32m";
    constexpr const char* B_YELLOW  = "\x1b[1;33m";
    constexpr const char* B_BLUE    = "\x1b[1;34m";
    constexpr const char* B_MAGENTA = "\x1b[1;35m";
    constexpr const char* B_CYAN    = "\x1b[1;36m";
    constexpr const char* B_WHITE   = "\x1b[1;97m";
    // 24-bit 真彩色（按截图 RGB）
    constexpr const char* ORANGE = "\x1b[38;2;255;165;0m";    // [HardwareBind][DeviceBinding]
    constexpr const char* PURPLE = "\x1b[38;2;148;0;211m";    // [VramCache][KoboldCpp]
    constexpr const char* VIOLET = "\x1b[38;2;238;130;238m";  // [Cache][WsNotify]
    constexpr const char* BROWN  = "\x1b[38;2;139;69;19m";    // [LicenseWatcher][LicenseManager]
    constexpr const char* GOLD   = "\x1b[38;2;255;215;0m";    // [Vault][VaultClient][Unlock]
    constexpr const char* PINK   = "\x1b[38;2;255;105;180m";  // [ResetPwd][ForgotPwd]
    constexpr const char* SALMON = "\x1b[38;2;250;128;114m";  // [SMTP]
    constexpr const char* LIME   = "\x1b[38;2;0;255;128m";    // [TokenRestore]
    constexpr const char* SKY    = "\x1b[38;2;135;206;235m";  // [Config][ConfigLoader][DDNS]
    constexpr const char* NAVY   = "\x1b[38;2;0;0;128m";      // [System]
    constexpr const char* TEAL   = "\x1b[38;2;0;128;128m";    // [DB][DatabaseInit][SQLite]
    constexpr const char* OLIVE  = "\x1b[38;2;128;128;0m";    // [Migration]
}

// ── [TAG] → 颜色 映射（按截图右侧注释整理）──────────────────────────────
inline const char* _tagColor(std::string_view tag) {
    using namespace _ansi;
    // 数据库 / 持久化
    if (tag == "[DB]" || tag == "[DatabaseInit]" || tag == "[SQLite]") return TEAL;
    if (tag == "[Migration]") return OLIVE;
    // 许可证
    if (tag == "[LicenseWatcher]" || tag == "[LicenseManager]") return BROWN;
    // 密钥库 / 解锁
    if (tag == "[Vault]" || tag == "[VaultClient]" || tag == "[Unlock]") return GOLD;
    // 缓存 / 通知
    if (tag == "[Cache]" || tag == "[WsNotify]") return VIOLET;
    if (tag == "[VramCache]" || tag == "[KoboldCpp]") return PURPLE;
    // 硬件指纹 / 设备绑定
    if (tag == "[HardwareBind]" || tag == "[DeviceBinding]") return ORANGE;
    // 配置中心
    if (tag == "[Config]" || tag == "[ConfigLoader]" || tag == "[DDNS]") return SKY;
    // 任务调度器
    if (tag == "[JobScheduler]") return B_MAGENTA;
    // 集群
    if (tag == "[Cluster]") return BLUE;
    // 系统级
    if (tag == "[System]") return NAVY;
    // 安全相关
    if (tag == "[SSL]") return B_BLUE;
    if (tag == "[Security]") return B_WHITE;
    // 邮件
    if (tag == "[SMTP]") return SALMON;
    // Token / 会话
    if (tag == "[TokenRestore]") return LIME;
    // 密码相关流程
    if (tag == "[ResetPwd]" || tag == "[ForgotPwd]") return PINK;
    // 注册码
    if (tag == "[RegCode]") return MAGENTA;
    // 限流 / 警告类
    if (tag == "[RateLimit]") return YELLOW;
    // 菜单 / 启动配置
    if (tag == "[Menu]") return B_CYAN;
    // 启动 / 服务正常
    if (tag == "[NGINX]" || tag == "[SysDictService]") return GREEN;
    // 服务器信息
    if (tag == "[Server]") return WHITE;
    // 日志系统
    if (tag == "[Log]") return GRAY;
    // 崩溃
    if (tag == "[CRASH]") return B_RED;
    // 默认（未映射 TAG 兜底）：CYAN
    return CYAN;
}

// ── 行级染色：按级别整行 + 行首 [TAG] 单独高亮 ──────────────────────────
inline std::string _decorate(const char* data, std::size_t len) {
    std::string_view line(data, len);
    if (line.empty()) return std::string(line);
    // 行内已含 ANSI 序列则直接透传，避免二次包装
    if (line.find("\x1b[") != std::string_view::npos) return std::string(line);

    auto isWord = [](unsigned char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
            || (c >= '0' && c <= '9') || c == '_';
    };
    auto findWord = [&](const char* kw) -> bool {
        std::size_t klen = std::strlen(kw);
        std::size_t start = 0;
        while (true) {
            auto p = line.find(kw, start);
            if (p == std::string_view::npos) return false;
            const bool lOk = (p == 0) || !isWord((unsigned char)line[p - 1]);
            const bool rOk = (p + klen >= line.size()) || !isWord((unsigned char)line[p + klen]);
            if (lOk && rOk) return true;
            start = p + 1;
        }
    };

    // 整行级别颜色（trantor 行：含 INFO/WARN/ERROR/...）
    const char* lineColor = nullptr;
    if      (findWord("FATAL"))                       lineColor = _ansi::B_RED;
    else if (findWord("ERROR"))                       lineColor = _ansi::RED;
    else if (findWord("WARN") || findWord("WARNING")) lineColor = _ansi::YELLOW;
    else if (findWord("DEBUG"))                       lineColor = _ansi::GRAY;
    else if (findWord("TRACE"))                       lineColor = _ansi::GRAY;
    else if (findWord("NOTICE"))                      lineColor = _ansi::GRAY;
    else if (findWord("INFO"))                        lineColor = _ansi::GREEN;

    std::string out;
    out.reserve(len + 32);

    // 高亮特定字符串："RuoYi-Cpp started" → B_GREEN
    if (line.find("RuoYi-Cpp started") != std::string_view::npos) {
        out.append(_ansi::B_GREEN);
        out.append(line.data(), line.size());
        out.append(_ansi::RESET);
        return out;
    }

    // 行首 [TAG] 单独染色，剩余部分按级别色（若有）
    if (line.front() == '[') {
        auto rb = line.find(']');
        if (rb != std::string_view::npos && rb < 64) {
            std::string_view tag = line.substr(0, rb + 1);
            const char* tc = _tagColor(tag);
            out.append(tc);
            out.append(tag.data(), tag.size());
            out.append(_ansi::RESET);
            std::string_view rest = line.substr(rb + 1);
            out.append(lineColor ? lineColor : _ansi::CYAN);
            out.append(rest.data(), rest.size());
            out.append(_ansi::RESET);
            return out;
        }
    }

    // 普通行：仅按级别整行染色
    if (lineColor) {
        out.append(lineColor);
        out.append(line.data(), line.size());
        out.append(_ansi::RESET);
    } else {
        out.append(line.data(), line.size());
    }
    return out;
}

// ── 彩色 streambuf：行缓冲 + 末尾染色 ────────────────────────────────────
class ColorStreambuf : public std::streambuf {
public:
    explicit ColorStreambuf(FILE* sink) : sink_(sink) {}

protected:
    int overflow(int ch) override {
        if (ch == EOF) return 0;
        char c = static_cast<char>(ch);
        if (c == '\n') {
            flushLine();
            std::fputc('\n', sink_);
            std::fflush(sink_);
        } else {
            buf_.push_back(c);
        }
        return ch;
    }

    std::streamsize xsputn(const char* s, std::streamsize n) override {
        const char* p   = s;
        const char* end = s + n;
        while (p < end) {
            const char* nl = static_cast<const char*>(std::memchr(p, '\n', end - p));
            if (!nl) { buf_.append(p, end - p); break; }
            buf_.append(p, nl - p);
            flushLine();
            std::fputc('\n', sink_);
            p = nl + 1;
        }
        return n;
    }

    int sync() override {
        if (!buf_.empty()) flushLine();
        std::fflush(sink_);
        return 0;
    }

private:
    void flushLine() {
        if (buf_.empty()) return;
        std::string colored = _decorate(buf_.data(), buf_.size());
        std::fwrite(colored.data(), 1, colored.size(), sink_);
        buf_.clear();
    }

    FILE* sink_;
    std::string buf_;
};

inline ColorStreambuf& _outBuf() {
    static ColorStreambuf inst(stdout);
    return inst;
}
inline ColorStreambuf& _errBuf() {
    static ColorStreambuf inst(stderr);
    return inst;
}

inline void init() {
#ifdef _WIN32
    auto enableVT = [](DWORD which) {
        HANDLE h = ::GetStdHandle(which);
        if (h == INVALID_HANDLE_VALUE || h == nullptr) return;
        DWORD mode = 0;
        if (!::GetConsoleMode(h, &mode)) return;
#       ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#         define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#       endif
        ::SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    };
    enableVT(STD_OUTPUT_HANDLE);
    enableVT(STD_ERROR_HANDLE);
    ::SetConsoleOutputCP(65001);
#endif
    std::cout.rdbuf(&_outBuf());
    std::cout.setf(std::ios::unitbuf);
    std::cerr.rdbuf(&_errBuf());
    std::cerr.setf(std::ios::unitbuf);
}

inline void writeColored(const char* msg, std::size_t len) {
    if (msg == nullptr || len == 0) return;
    std::size_t body = len;
    bool nl = false;
    if (body > 0 && msg[body - 1] == '\n') { --body; nl = true; }
    std::string colored = _decorate(msg, body);
    std::fwrite(colored.data(), 1, colored.size(), stdout);
    if (nl) std::fputc('\n', stdout);
    std::fflush(stdout);
}

} // namespace TermColor
