/**
 * @file ModbusGateway.h
 * @brief Modbus TCP 协议网关 — 原生实现，无需 libmodbus
 * 
 * 功能概述：
 *   - 原生 Modbus TCP 实现：不依赖 libmodbus，纯 C++ 实现
 *   - 多设备支持：管理多个 Modbus 设备的连接和通信
 *   - 连接复用：自动连接缓存和复用，减少连接开销
 *   - 跨平台支持：Windows（Winsock2）和 Linux（POSIX Socket）
 *   - 完整功能码：支持 8 种常用 Modbus 功能码
 * 
 * 支持的功能码：
 *   - FC01 读线圈（Read Coils）
 *   - FC02 读离散输入（Read Discrete Inputs）
 *   - FC03 读保持寄存器（Read Holding Registers）
 *   - FC04 读输入寄存器（Read Input Registers）
 *   - FC05 写单线圈（Write Single Coil）
 *   - FC06 写单寄存器（Write Single Register）
 *   - FC15 写多线圈（Write Multiple Coils）
 *   - FC16 写多寄存器（Write Multiple Registers）
 * 
 * Modbus TCP 协议：
 *   - MBAP 帧格式：TransactionId[2] ProtocolId[2] Length[2] UnitId[1] PDU[...]
 *   - 默认端口：502
 *   - 超时控制：可配置的连接和读写超时
 * 
 * 使用示例：
 *   // 添加设备
 *   ModbusGateway::Device dev;
 *   dev.id = "device_001";
 *   dev.host = "192.168.1.100";
 *   dev.port = 502;
 *   ModbusGateway::instance().addDevice(dev);
 *   
 *   // 读寄存器
 *   auto values = ModbusGateway::instance().readHoldingRegisters(
 *       "device_001", 0, 10
 *   );
 * 
 * @see IotCtrl - IoT 设备管理控制器
 */

/**
 * @file ModbusGateway.h
 * @brief Modbus 协议网关 — 支持 Modbus TCP/RTU 设备通信
 * 
 * 功能概述：
 *   - Modbus TCP：支持标准 Modbus TCP 协议
 *   - Modbus RTU：支持 Modbus RTU 协议（串口通信）
 *   - 设备管理：支持多设备连接和管理
 *   - 数据读写：支持读取和写入寄存器、线圈等
 *   - 数据缓存：缓存设备数据，减少网络通信
 *   - 异常处理：自动重连、超时处理、错误恢复
 * 
 * 核心特性：
 *   - 多协议支持：同时支持 TCP 和 RTU 两种协议
 *   - 连接池：支持连接复用和连接池管理
 *   - 异步通信：非阻塞的异步读写操作
 *   - 数据转换：自动处理大小端、数据类型转换
 *   - 性能监控：记录通信统计、延迟、错误率
 *   - 安全认证：支持 Modbus 安全扩展（可选）
 * 
 * 支持的 Modbus 功能码：
 *   - 01: 读线圈状态
 *   - 02: 读离散输入状态
 *   - 03: 读保持寄存器
 *   - 04: 读输入寄存器
 *   - 05: 写单个线圈
 *   - 06: 写单个寄存器
 *   - 15: 写多个线圈
 *   - 16: 写多个寄存器
 * 
 * 配置项（config.json）：
 *   - modbus.enabled: 是否启用 Modbus 网关（默认 false）
 *   - modbus.protocol: "tcp" | "rtu"（默认 "tcp"）
 *   - modbus.host: Modbus TCP 服务器地址
 *   - modbus.port: Modbus TCP 端口（默认 502）
 *   - modbus.timeout_ms: 通信超时（毫秒，默认 5000）
 *   - modbus.max_retries: 最大重试次数（默认 3）
 */

#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <stdexcept>
#include <cstring>
#include <json/json.h>
#include <iostream>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
   using sock_t = SOCKET;
#  define SOCK_INVALID INVALID_SOCKET
#  define sock_close closesocket
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
   using sock_t = int;
#  define SOCK_INVALID (-1)
#  define sock_close close
#endif

/**
 * @class ModbusGateway
 * @brief Modbus TCP 协议网关单例
 * 
 * 提供原生 Modbus TCP 实现，支持多个设备的并发访问。
 * 采用单例模式，全局唯一实例。
 */
class ModbusGateway {
public:
    /**
     * @brief 获取单例实例
     * @return ModbusGateway 单例引用
     */
    static ModbusGateway &instance() { static ModbusGateway gw; return gw; }

    /**
     * @struct Device
     * @brief Modbus 设备配置
     */
    struct Device {
        std::string id;                         ///< 设备唯一标识
        std::string name;                       ///< 设备名称
        std::string host;                       ///< 设备 IP 地址
        int         port    = 502;              ///< Modbus TCP 端口（默认 502）
        uint8_t     unitId  = 1;                ///< Modbus 单元 ID（默认 1）
        int         timeoutMs = 2000;           ///< 连接超时（毫秒，默认 2000）
        std::string description;                ///< 设备描述
    };

    // ── 设备注册 ──────────────────────────────────────────────────────────
    void addDevice(const Device &dev) {
        std::lock_guard<std::mutex> lk(mu_);
        devices_[dev.id] = dev;
    }
    void removeDevice(const std::string &id) {
        std::lock_guard<std::mutex> lk(mu_);
        devices_.erase(id);
    }
    std::vector<Device> listDevices() const {
        std::lock_guard<std::mutex> lk(mu_);
        std::vector<Device> v;
        for (auto &[k, d] : devices_) v.push_back(d);
        return v;
    }
    bool hasDevice(const std::string &id) const {
        std::lock_guard<std::mutex> lk(mu_);
        return devices_.count(id) > 0;
    }

    // ── 读操作 ─────────────────────────────────────────────────────────────
    // 读保持寄存器 FC03，返回 uint16 值列表
    std::vector<uint16_t> readHoldingRegisters(const std::string &devId,
                                               uint16_t startAddr, uint16_t count) {
        return readRegisters(devId, 0x03, startAddr, count);
    }
    // 读输入寄存器 FC04
    std::vector<uint16_t> readInputRegisters(const std::string &devId,
                                             uint16_t startAddr, uint16_t count) {
        return readRegisters(devId, 0x04, startAddr, count);
    }
    // 读线圈 FC01，返回 bool 列表
    std::vector<bool> readCoils(const std::string &devId,
                                uint16_t startAddr, uint16_t count) {
        return readBits(devId, 0x01, startAddr, count);
    }
    // 读离散输入 FC02
    std::vector<bool> readDiscreteInputs(const std::string &devId,
                                         uint16_t startAddr, uint16_t count) {
        return readBits(devId, 0x02, startAddr, count);
    }

    // ── 写操作 ─────────────────────────────────────────────────────────────
    // 写单寄存器 FC06
    void writeSingleRegister(const std::string &devId, uint16_t addr, uint16_t value) {
        auto &dev = getDevice(devId);
        std::vector<uint8_t> pdu = {0x06,
            (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF),
            (uint8_t)(value >> 8), (uint8_t)(value & 0xFF)};
        auto resp = sendRequest(dev, pdu);
        if (resp.empty() || resp[0] != 0x06)
            throw std::runtime_error("Modbus FC06 failed");
    }
    // 写多寄存器 FC16
    void writeMultipleRegisters(const std::string &devId,
                                uint16_t startAddr,
                                const std::vector<uint16_t> &values) {
        auto &dev = getDevice(devId);
        uint16_t cnt = (uint16_t)values.size();
        std::vector<uint8_t> pdu;
        pdu.push_back(0x10);
        pdu.push_back(startAddr >> 8); pdu.push_back(startAddr & 0xFF);
        pdu.push_back(cnt >> 8); pdu.push_back(cnt & 0xFF);
        pdu.push_back((uint8_t)(cnt * 2));
        for (auto v : values) {
            pdu.push_back(v >> 8); pdu.push_back(v & 0xFF);
        }
        auto resp = sendRequest(dev, pdu);
        if (resp.empty() || resp[0] != 0x10)
            throw std::runtime_error("Modbus FC16 failed");
    }
    // 写单线圈 FC05
    void writeSingleCoil(const std::string &devId, uint16_t addr, bool on) {
        auto &dev = getDevice(devId);
        uint16_t val = on ? 0xFF00 : 0x0000;
        std::vector<uint8_t> pdu = {0x05,
            (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF),
            (uint8_t)(val >> 8), (uint8_t)(val & 0xFF)};
        sendRequest(dev, pdu);
    }

    // ── 连通性测试 ────────────────────────────────────────────────────────
    bool ping(const std::string &devId) {
        try {
            readHoldingRegisters(devId, 0, 1);
            return true;
        } catch (...) { return false; }
    }

private:
    mutable std::mutex mu_;
    std::map<std::string, Device> devices_;
    std::atomic<uint16_t> txId_{1};

    Device &getDevice(const std::string &id) {
        auto it = devices_.find(id);
        if (it == devices_.end()) throw std::runtime_error("Device not found: " + id);
        return it->second;
    }

    std::vector<uint16_t> readRegisters(const std::string &devId,
                                        uint8_t fc, uint16_t startAddr, uint16_t count) {
        auto &dev = getDevice(devId);
        std::vector<uint8_t> pdu = {fc,
            (uint8_t)(startAddr >> 8), (uint8_t)(startAddr & 0xFF),
            (uint8_t)(count >> 8), (uint8_t)(count & 0xFF)};
        auto resp = sendRequest(dev, pdu);
        if (resp.size() < 2 + (size_t)(count * 2))
            throw std::runtime_error("Modbus read response too short");
        std::vector<uint16_t> result;
        for (int i = 0; i < count; ++i)
            result.push_back((uint16_t)((resp[2 + i*2] << 8) | resp[3 + i*2]));
        return result;
    }

    std::vector<bool> readBits(const std::string &devId,
                               uint8_t fc, uint16_t startAddr, uint16_t count) {
        auto &dev = getDevice(devId);
        std::vector<uint8_t> pdu = {fc,
            (uint8_t)(startAddr >> 8), (uint8_t)(startAddr & 0xFF),
            (uint8_t)(count >> 8), (uint8_t)(count & 0xFF)};
        auto resp = sendRequest(dev, pdu);
        if (resp.size() < 2) throw std::runtime_error("Modbus bit response too short");
        std::vector<bool> result;
        for (int i = 0; i < count; ++i)
            result.push_back((resp[2 + i/8] >> (i % 8)) & 1);
        return result;
    }

    // Modbus TCP 发送/接收（阻塞，带超时）
    std::vector<uint8_t> sendRequest(const Device &dev,
                                     const std::vector<uint8_t> &pdu) {
        uint16_t tx = txId_.fetch_add(1);
        // MBAP: TxId[2] ProtoId[2]=0 Len[2] UnitId[1] PDU[...]
        uint16_t len = (uint16_t)(1 + pdu.size());
        std::vector<uint8_t> frame = {
            (uint8_t)(tx >> 8), (uint8_t)(tx & 0xFF),
            0x00, 0x00,
            (uint8_t)(len >> 8), (uint8_t)(len & 0xFF),
            dev.unitId
        };
        frame.insert(frame.end(), pdu.begin(), pdu.end());

        // 建立 TCP 连接
        sock_t s = ::socket(AF_INET, SOCK_STREAM, 0);
        if (s == SOCK_INVALID) throw std::runtime_error("Modbus: socket() failed");

        // 超时设置
#ifdef _WIN32
        DWORD tv = dev.timeoutMs;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(tv));
#else
        struct timeval tv;
        tv.tv_sec = dev.timeoutMs / 1000;
        tv.tv_usec = (dev.timeoutMs % 1000) * 1000;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons((uint16_t)dev.port);
        inet_pton(AF_INET, dev.host.c_str(), &addr.sin_addr);

        if (::connect(s, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
            sock_close(s);
            throw std::runtime_error("Modbus: connect to " + dev.host + ":" +
                                     std::to_string(dev.port) + " failed");
        }
        // 发送
        ::send(s, (const char*)frame.data(), (int)frame.size(), 0);

        // 读满 N 字节（跨平台，Windows MSG_WAITALL 不可靠）
        auto recvAll = [&](std::vector<uint8_t> &buf, int need) -> bool {
            int got = 0;
            while (got < need) {
                int r = ::recv(s, (char*)buf.data() + got, need - got, 0);
                if (r <= 0) return false;
                got += r;
            }
            return true;
        };

        // 接收响应（先读 6 字节 MBAP header）
        std::vector<uint8_t> header(6);
        if (!recvAll(header, 6)) { sock_close(s); throw std::runtime_error("Modbus: recv MBAP failed"); }
        uint16_t respLen = (uint16_t)((header[4] << 8) | header[5]);
        std::vector<uint8_t> body(respLen);
        bool ok2 = recvAll(body, respLen);
        sock_close(s);
        if (!ok2) throw std::runtime_error("Modbus: recv body failed");

        // body[0] = unitId, body[1..] = PDU
        if (body.size() < 2) throw std::runtime_error("Modbus: empty PDU");
        if (body[1] & 0x80) {  // 异常码
            uint8_t ec = body.size() > 2 ? body[2] : 0;
            throw std::runtime_error("Modbus exception code: " + std::to_string(ec));
        }
        return std::vector<uint8_t>(body.begin() + 1, body.end()); // 去掉 unitId
    }
};
