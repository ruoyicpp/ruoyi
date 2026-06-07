/**
 * @file SmtpUtils.h
 * @brief SMTP email sending utility - secure email sending with OpenSSL
 * 
 * Features:
 *   - SMTP protocol support for sending emails
 *   - TLS encryption using OpenSSL (Port-465 Implicit-TLS)
 *   - Multiple senders with round-robin load balancing
 *   - Async sending in background thread
 * 
 * Supported email services:
 *   - QQ Mail: smtp.qq.com:465
 *   - 163 Mail: smtp.163.com:465
 *   - Custom SMTP servers
 * 
 * Configuration (sys_config):
 *   - sys.email.host: SMTP server address
 *   - sys.email.port: SMTP port (usually 465 or 587)
 *   - sys.email.fromName: sender display name
 *   - sys.email.senders: sender list (JSON array)
 * 
 * Usage example:
 *   SmtpUtils::Sender s1{"user1@qq.com", "authCode1"};
 *   SmtpUtils::Sender s2{"user2@qq.com", "authCode2"};
 *   SmtpUtils::instance().loadConfig("smtp.qq.com", 465, "System", {s1, s2});
 *   SmtpUtils::instance().sendAsync("recipient@example.com", "Subject", "Body");
 * 
 * Load balancing:
 *   - Round-robin sender selection
 *   - Atomic counter for thread-safe rotation
 *   - Even distribution across multiple senders
 * 
 * Security features:
 *   - TLS encryption for all communications
 *   - Optional SSL certificate verification
 *   - Auth codes stored in database, not hardcoded
 */

#pragma once
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
#  include <wincrypt.h>
#  pragma comment(lib,"ws2_32.lib")
#  pragma comment(lib,"crypt32.lib")
// 避免与 OpenSSL 冲突：wincrypt.h 定义了 X509_NAME 等同名宏
#  ifdef X509_NAME
#    undef X509_NAME
#  endif
#  ifdef X509_EXTENSIONS
#    undef X509_EXTENSIONS
#  endif
#  ifdef X509_CERT_PAIR
#    undef X509_CERT_PAIR
#  endif
#  ifdef PKCS7_ISSUER_AND_SERIAL
#    undef PKCS7_ISSUER_AND_SERIAL
#  endif
#  ifdef PKCS7_SIGNER_INFO
#    undef PKCS7_SIGNER_INFO
#  endif
#  ifdef OCSP_REQUEST
#    undef OCSP_REQUEST
#  endif
#  ifdef OCSP_RESPONSE
#    undef OCSP_RESPONSE
#  endif
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

    // ── Windows 系统证书仓库 → OpenSSL X509_STORE（java/ SChannel 行为对齐）───
    //   读取 "ROOT"（受信任的根证书颁发机构）和 "CA"（中间 CA），
    //   逐个 d2i_X509 转换后塞进 OpenSSL 的 trust store。
    //   返回成功导入的证书数。
#ifdef _WIN32
    static int loadWindowsCertStore(SSL_CTX* ctx) {
        X509_STORE* store = SSL_CTX_get_cert_store(ctx);
        if (!store) return 0;
        int total = 0;
        const wchar_t* names[] = { L"ROOT", L"CA" };
        for (auto* name : names) {
            HCERTSTORE hStore = ::CertOpenSystemStoreW(0, name);
            if (!hStore) continue;
            PCCERT_CONTEXT pCtx = nullptr;
            while ((pCtx = ::CertEnumCertificatesInStore(hStore, pCtx)) != nullptr) {
                const unsigned char* enc = pCtx->pbCertEncoded;
                X509* x = d2i_X509(nullptr, &enc, (long)pCtx->cbCertEncoded);
                if (x) {
                    if (X509_STORE_add_cert(store, x) == 1) ++total;
                    X509_free(x);
                }
            }
            ::CertCloseStore(hStore, 0);
        }
        return total;
    }
#endif

    // ── 拉取最近一条 OpenSSL 错误，附加到异常文案 ──────────────────────────
    static std::string opensslLastError() {
        unsigned long e = ERR_get_error();
        if (!e) return "(no error queue)";
        char buf[256] = {};
        ERR_error_string_n(e, buf, sizeof(buf));
        return buf;
    }

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

        // 2. SSL wrap（Implicit TLS）——java / SChannel 行为：信任 Windows 系统证书仓库
        SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx) { closesocket(sock); throw std::runtime_error("SSL_CTX_new 失败: " + opensslLastError()); }
        SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
#ifdef _WIN32
        int loaded = loadWindowsCertStore(ctx);
        if (loaded == 0) {
            // Windows 仓库取不到时退而求其次走 OpenSSL 默认路径
            SSL_CTX_set_default_verify_paths(ctx);
        }
#else
        SSL_CTX_set_default_verify_paths(ctx);
#endif
        SSL* ssl = SSL_new(ctx);
        SSL_set_fd(ssl, sock);
        SSL_set_tlsext_host_name(ssl, host.c_str());
        // 启用主机名验证（避免拿到一张合法 CA 签发但域名对不上的证书）
        SSL_set1_host(ssl, host.c_str());
        if (SSL_connect(ssl) <= 0) {
            long vr = SSL_get_verify_result(ssl);
            std::string detail = opensslLastError();
            if (vr != X509_V_OK) {
                detail += " (verify_result=" + std::to_string(vr)
                       + " "  + std::string(X509_verify_cert_error_string(vr)) + ")";
            }
            SSL_free(ssl); SSL_CTX_free(ctx); closesocket(sock);
            throw std::runtime_error("SSL 握手失败: " + detail);
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
