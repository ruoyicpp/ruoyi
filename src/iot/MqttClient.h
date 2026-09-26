/**
 * @file MqttClient.h
 * @brief 精简 MQTT 3.1.1 客户端 — 设备接入/消息订阅转发/在线管理
 *
 * 功能概述：
 *   - CONNECT/PUBLISH(QoS0)/SUBSCRIBE/PINGREQ 最小协议实现
 *   - 后台读线程：自动重连 + 消息分发到 topic 回调
 *   - 设备在线管理：订阅 device/+/status 通配符，更新 iot_device 表
 *   - 消息转发：收到设备上行消息 → WsBus 推送前端 + 可选写库
 *
 * 协议说明：
 *   - 仅实现 QoS 0（物联网遥测场景够用，不保证送达）
 *   - 不支持 TLS（内网部署；需加密走 broker 侧 stunnel/nginx 终结）
 *   - 不支持 QoS 1/2 的 PUBACK/PUBREC 流程
 *
 * 配置项（config.json → mqtt）：
 *   - enabled: 总开关（默认 false）
 *   - host/port: broker 地址（默认 127.0.0.1:1883）
 *   - client_id: 客户端标识（默认 ruoyi-cpp-<pid>）
 *   - username/password: 认证（可空）
 *   - keepalive: 心跳秒数（默认 60）
 *   - topics: 订阅主题数组（默认 ["device/+/status","device/+/data"]）
 *   - forward_ws: 收到消息是否推 WsBus（默认 true）
 *   - persist: 是否写 iot_message 表（默认 false）
 */

#pragma once
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cstring>
#include <ctime>
#include <json/json.h>
#include <trantor/utils/Logger.h>
#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <process.h>   // _getpid
#  pragma comment(lib, "ws2_32.lib")
   using socklen_t = int;
   using mqtt_sock_t = SOCKET;
#  define MQTT_INVALID_SOCK INVALID_SOCKET
#  define mqtt_getpid _getpid
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netdb.h>
#  include <unistd.h>
   using mqtt_sock_t = int;
#  define MQTT_INVALID_SOCK (-1)
#  define mqtt_getpid ::getpid
#endif
#include "../services/DatabaseService.h"
#include "../common/WsBus.h"

/**
 * @class MqttClient
 * @brief 精简 MQTT 客户端单例
 */
class MqttClient {
public:
    struct Config {
        bool        enabled   = false;
        std::string host      = "127.0.0.1";
        int         port      = 1883;
        std::string clientId;
        std::string username, password;
        int         keepalive = 60;
        std::vector<std::string> topics = {"device/+/status", "device/+/data"};
        bool        forwardWs = true;
        bool        persist   = false;
    };

    using MsgHandler = std::function<void(const std::string& topic,
                                          const std::string& payload)>;

    static MqttClient& instance() {
        static MqttClient inst;
        return inst;
    }

    void init(const Config& cfg) {
        cfg_ = cfg;
        if (!cfg_.enabled) { LOG_INFO << "[MQTT] disabled"; return; }
        if (cfg_.clientId.empty())
            cfg_.clientId = "ruoyi-cpp-" + std::to_string(mqtt_getpid());
        running_ = true;
        worker_ = std::thread(&MqttClient::run, this);
        LOG_INFO << "[MQTT] connecting " << cfg_.host << ":" << cfg_.port
                 << " clientId=" << cfg_.clientId;
    }

    void stop() {
        running_ = false;
        closeSock();
        if (worker_.joinable()) worker_.join();
    }

    bool isConnected() const { return connected_; }

    /// 发布消息（QoS 0）
    bool publish(const std::string& topic, const std::string& payload) {
        if (!connected_) return false;
        std::vector<uint8_t> pkt;
        pkt.push_back(0x30);   // PUBLISH QoS0
        std::string body = encodeStr(topic) + payload;
        encodeVarint(pkt, (int)body.size());
        pkt.insert(pkt.end(), body.begin(), body.end());
        return sendAll(pkt);
    }

    /// 注册额外消息处理器（业务侧自定义）
    void onMessage(MsgHandler h) {
        std::lock_guard<std::mutex> lk(mu_);
        handlers_.push_back(std::move(h));
    }

private:
    MqttClient() = default;
    ~MqttClient() { stop(); }

    // ── 主循环：连接 → 订阅 → 读包 → 断线重连 ─────────────────────────
    void run() {
        while (running_) {
            if (!connectAndSubscribe()) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }
            readLoop();
            connected_ = false;
            closeSock();
            if (running_) {
                LOG_WARN << "[MQTT] disconnected, reconnect in 5s";
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
    }

    bool connectAndSubscribe() {
        sock_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sock_ == MQTT_INVALID_SOCK) return false;
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons((uint16_t)cfg_.port);
        if (::inet_pton(AF_INET, cfg_.host.c_str(), &addr.sin_addr) <= 0) {
            // 域名解析
            auto* he = ::gethostbyname(cfg_.host.c_str());
            if (!he) { closeSock(); return false; }
            memcpy(&addr.sin_addr, he->h_addr, he->h_length);
        }
        if (::connect(sock_, (sockaddr*)&addr, sizeof(addr)) < 0) {
            closeSock(); return false;
        }

        // CONNECT 包
        std::string body = encodeStr("MQTT") + std::string(1, 0x04)  // v3.1.1
                         + std::string(1, 0x02)                       // clean session
                         + std::string(1, (char)(cfg_.keepalive >> 8))
                         + std::string(1, (char)(cfg_.keepalive & 0xFF))
                         + encodeStr(cfg_.clientId);
        if (!cfg_.username.empty()) {
            body[7] |= 0x80;   // username flag
            if (!cfg_.password.empty()) body[7] |= 0x40;
            body += encodeStr(cfg_.username);
            if (!cfg_.password.empty()) body += encodeStr(cfg_.password);
        }
        std::vector<uint8_t> pkt = {0x10};
        encodeVarint(pkt, (int)body.size());
        pkt.insert(pkt.end(), body.begin(), body.end());
        if (!sendAll(pkt)) { closeSock(); return false; }

        // 等 CONNACK
        uint8_t ack[4];
        if (!recvAll(ack, 4) || ack[0] != 0x20 || ack[3] != 0x00) {
            closeSock(); return false;
        }
        connected_ = true;
        lastPing_ = std::chrono::steady_clock::now();
        LOG_INFO << "[MQTT] connected " << cfg_.host << ":" << cfg_.port;

        // SUBSCRIBE 所有主题
        for (auto& t : cfg_.topics) subscribe(t);
        return true;
    }

    void subscribe(const std::string& topic) {
        std::string body = std::string(1, 0) + std::string(1, 1)  // packet id=1
                         + encodeStr(topic) + std::string(1, 0);   // QoS0
        std::vector<uint8_t> pkt = {0x82};
        encodeVarint(pkt, (int)body.size());
        pkt.insert(pkt.end(), body.begin(), body.end());
        sendAll(pkt);
        LOG_INFO << "[MQTT] subscribed " << topic;
    }

    void readLoop() {
        while (running_ && connected_) {
            uint8_t hdr;
            if (!recvAll(&hdr, 1)) break;
            int remLen = readVarint();
            if (remLen < 0 || remLen > 1024 * 1024) break;
            std::vector<uint8_t> body(remLen);
            if (remLen > 0 && !recvAll(body.data(), remLen)) break;

            uint8_t type = hdr >> 4;
            if (type == 3) {   // PUBLISH
                int tlen = (body[0] << 8) | body[1];
                std::string topic((char*)body.data() + 2, tlen);
                std::string payload((char*)body.data() + 2 + tlen,
                                    remLen - 2 - tlen);
                dispatch(topic, payload);
            }
            // PINGRESP/SUBACK 等忽略

            // 心跳
            auto now = std::chrono::steady_clock::now();
            if (now - lastPing_ > std::chrono::seconds(cfg_.keepalive)) {
                uint8_t ping[] = {0xC0, 0x00};
                if (!sendAll(std::vector<uint8_t>(ping, ping + 2))) break;
                lastPing_ = now;
            }
        }
    }

    /// 消息分发：设备状态更新 + WsBus 转发 + 自定义处理器
    void dispatch(const std::string& topic, const std::string& payload) {
        // device/<id>/status → 更新 iot_device 在线状态（PK 列是 id）
        if (topic.rfind("device/", 0) == 0) {
            auto parts = splitTopic(topic);
            if (parts.size() == 3 && parts[2] == "status") {
                bool online = (payload == "online" || payload == "1");
                DatabaseService::instance().execParams(
                    "UPDATE iot_device SET status=$1,last_seen=CURRENT_TIMESTAMP "
                    "WHERE id=$2",
                    {online ? "online" : "offline", parts[1]});
            }
        }
        // WsBus 转发前端
        if (cfg_.forwardWs) {
            Json::Value msg;
            msg["type"]    = "mqtt";
            msg["topic"]   = topic;
            msg["payload"] = payload;
            msg["ts"]      = (Json::Int64)std::time(nullptr);
            WsBus::instance().broadcast("live:", msg);
        }
        // 可选持久化
        if (cfg_.persist) {
            DatabaseService::instance().execParams(
                "INSERT INTO iot_message(topic,payload) VALUES($1,$2)",
                {topic, payload});
        }
        // 自定义处理器
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& h : handlers_) {
            try { h(topic, payload); } catch (...) {}
        }
    }

    // ── 协议工具 ──────────────────────────────────────────────────────
    static std::string encodeStr(const std::string& s) {
        return std::string(1, (char)(s.size() >> 8))
             + std::string(1, (char)(s.size() & 0xFF)) + s;
    }

    static void encodeVarint(std::vector<uint8_t>& out, int v) {
        do {
            uint8_t b = v % 128; v /= 128;
            if (v > 0) b |= 0x80;
            out.push_back(b);
        } while (v > 0);
    }

    int readVarint() {
        int mult = 1, val = 0;
        uint8_t b;
        do {
            if (!recvAll(&b, 1)) return -1;
            val += (b & 0x7F) * mult;
            mult *= 128;
        } while (b & 0x80);
        return val;
    }

    static std::vector<std::string> splitTopic(const std::string& t) {
        std::vector<std::string> out;
        size_t pos = 0;
        while (true) {
            auto p = t.find('/', pos);
            if (p == std::string::npos) { out.push_back(t.substr(pos)); break; }
            out.push_back(t.substr(pos, p - pos));
            pos = p + 1;
        }
        return out;
    }

    bool sendAll(const std::vector<uint8_t>& data) {
        size_t sent = 0;
        while (sent < data.size()) {
            int n = (int)::send(sock_, (const char*)data.data() + sent,
                                (int)(data.size() - sent), 0);
            if (n <= 0) return false;
            sent += n;
        }
        return true;
    }

    bool recvAll(uint8_t* buf, int len) {
        int got = 0;
        while (got < len) {
            int n = (int)::recv(sock_, (char*)buf + got, len - got, 0);
            if (n <= 0) return false;
            got += n;
        }
        return true;
    }

    void closeSock() {
        if (sock_ != MQTT_INVALID_SOCK) {
#ifdef _WIN32
            ::closesocket(sock_);
#else
            ::close(sock_);
#endif
            sock_ = MQTT_INVALID_SOCK;
        }
    }

    Config cfg_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    mqtt_sock_t sock_ = MQTT_INVALID_SOCK;
    std::thread worker_;
    std::chrono::steady_clock::time_point lastPing_;
    std::vector<MsgHandler> handlers_;
    std::mutex mu_;
};
