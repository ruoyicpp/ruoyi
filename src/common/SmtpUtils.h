#pragma once
/**
 * SmtpUtils — 基于 OpenSSL 的 SMTP 发件工具（QQ/163/企业邮箱 Port-465 Implicit-TLS）
 *
 * 配置存储在 sys_config（通过 /system/emailConfig API 管理）：
 *   sys.email.host     = "smtp.qq.com"
 *   sys.email.port     = "465"
 *   sys.email.fromName = "系统通知"
 *   sys.email.senders  = JSON array: [{"email":"a@qq.com","authCode":"xxxx"}, ...]
 *
 * 轮询策略：每次发送时选下一个发件人（原子计数器 % senders.size()）
 */
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <cstring>
#include <json/json.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <trantor/utils/Logger.h>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib,"ws2_32.lib")
#else
#  include <sys/socket.h>
#  include <netdb.h>
#  include <unistd.h>
#  define closesocket close
#endif

class SmtpUtils {
public:
    struct Sender {
        std::string email;
        std::string authCode;
    };

    static SmtpUtils& instance() {
        static SmtpUtils inst;
        return inst;
    }

    // 从 sys_config 重新加载配置（每次发送前也可调用）
    void loadConfig(const std::string& host, int port, const std::string& fromName,
                    const std::vector<Sender>& senders) {
        std::lock_guard<std::mutex> lk(mu_);
        host_     = host;
        port_     = port;
        fromName_ = fromName;
        senders_  = senders;
    }

    bool isConfigured() const {
        std::lock_guard<std::mutex> lk(mu_);
        return !host_.empty() && !senders_.empty();
    }

    // 发送邮件，返回 true 成功
    bool send(const std::string& to, const std::string& subject, const std::string& body) {
        Sender sender;
        std::string host;
        int port;
        std::string fromName;
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (senders_.empty()) {
                LOG_WARN << "[SMTP] 未配置发件人，邮件未发送 to=" << to;
                return false;
            }
            size_t idx = counter_.fetch_add(1) % senders_.size();
            sender   = senders_[idx];
            host     = host_;
            port     = port_;
            fromName = fromName_;
        }
        try {
            sendImpl(host, port, fromName, sender, to, subject, body);
            LOG_INFO << "[SMTP] 发送成功 from=" << sender.email << " to=" << to;
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR << "[SMTP] 发送失败 from=" << sender.email << " to=" << to << ": " << e.what();
            return false;
        }
    }

private:
    mutable std::mutex mu_;
    std::string host_     = "smtp.qq.com";
    int         port_     = 465;
    std::string fromName_ = "系统通知";
    std::vector<Sender> senders_;
    std::atomic<size_t> counter_{0};

    // ── Base64 编码（不依赖 BIO，避免换行符问题）────────────────────────────
    static std::string b64(const std::string& s) {
        static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string r;
        int val=0, bits=-6;
        for (unsigned char c : s) {
            val = (val << 8) + c; bits += 8;
            while (bits >= 0) { r += t[(val >> bits) & 0x3F]; bits -= 6; }
        }
        if (bits > -6) r += t[((val << 8) >> (bits + 8)) & 0x3F];
        while (r.size() % 4) r += '=';
        return r;
    }

    // ── MIME 邮件头编码（UTF-8 subject）─────────────────────────────────────
    static std::string mimeWord(const std::string& s) {
        return "=?UTF-8?B?" + b64(s) + "?=";
    }

    // ── 核心发送实现（Implicit TLS on port 465）──────────────────────────────
    void sendImpl(const std::string& host, int port,
                  const std::string& fromName, const Sender& sender,
                  const std::string& to,
                  const std::string& subject, const std::string& body) {
#ifdef _WIN32
        WSADATA wd; WSAStartup(MAKEWORD(2,2), &wd);
#endif
        // 1. TCP connect
        struct addrinfo hints{}, *res = nullptr;
        hints.ai_family   = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0)
            throw std::runtime_error("DNS 解析失败: " + host);

        int sock = -1;
        for (auto* p = res; p; p = p->ai_next) {
            sock = (int)socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (sock < 0) continue;
            if (connect(sock, p->ai_addr, (int)p->ai_addrlen) == 0) break;
            closesocket(sock); sock = -1;
        }
        freeaddrinfo(res);
        if (sock < 0) throw std::runtime_error("TCP 连接失败: " + host);

        // 2. SSL wrap（Implicit TLS）
        SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx) { closesocket(sock); throw std::runtime_error("SSL_CTX_new 失败"); }
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
        SSL_CTX_set_default_verify_paths(ctx);  // 加载系统根证书
        SSL* ssl = SSL_new(ctx);
        SSL_set_fd(ssl, sock);
        SSL_set_tlsext_host_name(ssl, host.c_str());
        if (SSL_connect(ssl) <= 0) {
            SSL_free(ssl); SSL_CTX_free(ctx); closesocket(sock);
            throw std::runtime_error("SSL 握手失败");
        }

        auto W = [&](const std::string& s) {
            SSL_write(ssl, s.c_str(), (int)s.size());
        };
        auto R = [&]() -> std::string {
            char buf[4096] = {};
            SSL_read(ssl, buf, sizeof(buf)-1);
            return buf;
        };
        auto expect = [&](const std::string& prefix) {
            auto resp = R();
            if (resp.substr(0, prefix.size()) != prefix) {
                SSL_free(ssl); SSL_CTX_free(ctx); closesocket(sock);
                throw std::runtime_error("SMTP 协议错误: " + resp);
            }
        };

        // 3. SMTP 握手
        expect("220");
        W("EHLO ruoyi-cpp\r\n");  expect("250");
        W("AUTH LOGIN\r\n");       expect("334");
        W(b64(sender.email) + "\r\n"); expect("334");
        W(b64(sender.authCode) + "\r\n"); expect("235");

        // 4. 发送信封
        std::string fromDisp = "\"" + fromName + "\" <" + sender.email + ">";
        W("MAIL FROM:<" + sender.email + ">\r\n"); expect("250");
        W("RCPT TO:<" + to + ">\r\n");             expect("250");
        W("DATA\r\n");                             expect("354");

        // 5. 邮件内容
        std::string msg =
            "From: " + fromDisp + "\r\n"
            "To: <" + to + ">\r\n"
            "Subject: " + mimeWord(subject) + "\r\n"
            "MIME-Version: 1.0\r\n"
            "Content-Type: text/plain; charset=UTF-8\r\n"
            "Content-Transfer-Encoding: base64\r\n"
            "\r\n" +
            b64(body) + "\r\n"
            ".\r\n";
        W(msg); expect("250");

        W("QUIT\r\n");
        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        closesocket(sock);
    }
};
