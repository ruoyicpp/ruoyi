/**
 * @file CaptchaClient.h
 * @brief 行为验证码 gRPC 客户端 — 对接 go-captcha captcha-server（Linux only）
 *
 * 功能概述：
 *   - 封装 captcha.CaptchaService 的 4 个 RPC：GetSlide/VerifySlide/GetRotate/VerifyRotate
 *   - 连接管理：惰性建连 + 断线自动重连（channel 失效时重建）
 *   - 超时控制：每次 RPC 默认 3s deadline，防 captcha-server 挂起拖垮登录
 *   - 线程安全：stub 可并发调用（gRPC channel 本身线程安全）
 *
 * 配置项（config.json → captcha.behavioral）：
 *   - enabled:      是否启用行为验证码（默认 false，关闭时走原 GIF 验证码）
 *   - server_addr:  captcha-server gRPC 地址（默认 127.0.0.1:18090）
 *   - timeout_ms:   RPC 超时毫秒（默认 3000）
 *   - type:         "slide" | "rotate"（默认 slide）
 *
 * 依赖：libgrpc++ + libprotobuf（apt install libgrpc++-dev protobuf-compiler-grpc）
 * 编译：CMake 选项 RUOYI_USE_GRPC_CAPTCHA=ON（仅 Linux；未装 grpc 时编译为 stub）
 */

// 传统 include guard：WSL1 DrvFs(/mnt/g) inode 合成会让 #pragma once 误判跳过
#ifndef RUOYI_CAPTCHA_CLIENT_H
#define RUOYI_CAPTCHA_CLIENT_H

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <json/json.h>
#include <trantor/utils/Logger.h>
#include "../system/services/SysConfigService.h"

#ifdef RUOYI_USE_GRPC_CAPTCHA
#include <grpcpp/grpcpp.h>
#include "proto/captcha.grpc.pb.h"
#endif

class CaptchaClient {
public:
    struct Config {
        bool        enabled    = false;
        std::string serverAddr = "127.0.0.1:18090";
        int         timeoutMs  = 3000;
        std::string type       = "slide";   ///< slide | rotate
    };

    /// 滑块验证码数据（透传 proto 字段）
    struct SlideData {
        std::string key;
        std::string masterImage;   ///< data:image/jpeg;base64,...
        std::string tileImage;     ///< data:image/png;base64,...
        int tileX = 0, tileY = 0;
        int tileWidth = 0, tileHeight = 0;
        int masterWidth = 0, masterHeight = 0;
    };

    /// 旋转验证码数据
    struct RotateData {
        std::string key;
        std::string masterImage;   ///< data:image/png;base64,...
        std::string thumbImage;    ///< data:image/png;base64,...
    };

    static CaptchaClient& instance() {
        static CaptchaClient inst;
        return inst;
    }

    /// 初始化（幂等，可热重载）
    void init(const Config& cfg) {
        std::lock_guard<std::mutex> lk(mu_);
        cfg_ = cfg;
#ifdef RUOYI_USE_GRPC_CAPTCHA
        if (cfg_.enabled) {
            channel_ = grpc::CreateChannel(cfg_.serverAddr,
                                           grpc::InsecureChannelCredentials());
            stub_ = captcha::CaptchaService::NewStub(channel_);
            LOG_INFO << "[Captcha] gRPC client -> " << cfg_.serverAddr
                     << " type=" << cfg_.type;
        } else {
            stub_.reset();
            channel_.reset();
            LOG_INFO << "[Captcha] behavioral captcha disabled";
        }
#else
        if (cfg_.enabled)
            LOG_WARN << "[Captcha] behavioral captcha requires RUOYI_USE_GRPC_CAPTCHA (Linux + libgrpc++-dev)";
#endif
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lk(mu_);
        return cfg_.enabled;
    }
    std::string type() const {
        std::lock_guard<std::mutex> lk(mu_);
        return cfg_.type;   // 返回值拷贝，防热重载时引用悬垂
    }
    /// 配置快照（一次性拿 enabled+type，避免两次加锁间被 init 改）
    Config config() const {
        std::lock_guard<std::mutex> lk(mu_);
        return cfg_;
    }

    /// 从 sys_config 同步前端可配项（系统管理→参数设置 可改）。
    /// DB 有值时覆盖 config.json；空值回退文件配置。
    /// 每请求调用一次即可——SysConfigService 内部走 MemCache，开销可忽略。
    void syncFromDb() {
        auto& sc = SysConfigService::instance();
        std::string en  = sc.selectConfigByKey("sys.captcha.behavioral.enabled");
        std::string typ = sc.selectConfigByKey("sys.captcha.behavioral.type");
        std::lock_guard<std::mutex> lk(mu_);
        if (!en.empty()) {
            bool b = (en == "true");
            if (b != cfg_.enabled) {
                cfg_.enabled = b;
#ifdef RUOYI_USE_GRPC_CAPTCHA
                if (!b) { stub_.reset(); channel_.reset(); }  // 关闭即断连；开启由 ensureStub 懒建
#endif
                LOG_INFO << "[Captcha] sys_config enabled=" << en;
            }
        }
        if (typ == "slide" || typ == "rotate") cfg_.type = typ;
    }

    /// 健康检查（返回 false 表示 captcha-server 不可达）
    bool ping() {
#ifdef RUOYI_USE_GRPC_CAPTCHA
        auto stub = ensureStub();
        if (!stub) return false;
        captcha::PingRequest req;
        captcha::PingResponse resp;
        grpc::ClientContext ctx;
        setDeadline(ctx);
        auto st = stub->Ping(&ctx, req, &resp);
        if (!st.ok()) {
            LOG_WARN << "[Captcha] ping failed: " << st.error_message();
            return false;
        }
        return true;
#else
        return false;
#endif
    }

    /// 获取滑块验证码
    bool getSlide(SlideData& out, int width = 0, int height = 0) {
#ifdef RUOYI_USE_GRPC_CAPTCHA
        auto stub = ensureStub();
        if (!stub) return false;
        captcha::GetSlideRequest req;
        if (width > 0)  req.set_width(width);
        if (height > 0) req.set_height(height);
        captcha::GetSlideResponse resp;
        grpc::ClientContext ctx;
        setDeadline(ctx);
        auto st = stub->GetSlide(&ctx, req, &resp);
        if (!st.ok()) {
            LOG_WARN << "[Captcha] GetSlide failed: " << st.error_message();
            onRpcError();
            return false;
        }
        out.key          = resp.key();
        out.masterImage  = resp.master_image();
        out.tileImage    = resp.tile_image();
        out.tileX        = resp.tile_x();
        out.tileY        = resp.tile_y();
        out.tileWidth    = resp.tile_width();
        out.tileHeight   = resp.tile_height();
        out.masterWidth  = resp.master_width();
        out.masterHeight = resp.master_height();
        return true;
#else
        (void)out; (void)width; (void)height;
        return false;
#endif
    }

    /// 校验滑块（一次性）。y=0 时只校验水平位置
    bool verifySlide(const std::string& key, int x, int y = 0) {
#ifdef RUOYI_USE_GRPC_CAPTCHA
        auto stub = ensureStub();
        if (!stub) return false;
        captcha::VerifySlideRequest req;
        req.set_key(key);
        req.set_x(x);
        req.set_y(y);
        captcha::VerifySlideResponse resp;
        grpc::ClientContext ctx;
        setDeadline(ctx);
        auto st = stub->VerifySlide(&ctx, req, &resp);
        if (!st.ok()) {
            LOG_WARN << "[Captcha] VerifySlide failed: " << st.error_message();
            onRpcError();
            return false;
        }
        return resp.result();
#else
        (void)key; (void)x; (void)y;
        return false;
#endif
    }

    /// 获取旋转验证码
    bool getRotate(RotateData& out, int size = 0) {
#ifdef RUOYI_USE_GRPC_CAPTCHA
        auto stub = ensureStub();
        if (!stub) return false;
        captcha::GetRotateRequest req;
        if (size > 0) req.set_size(size);
        captcha::GetRotateResponse resp;
        grpc::ClientContext ctx;
        setDeadline(ctx);
        auto st = stub->GetRotate(&ctx, req, &resp);
        if (!st.ok()) {
            LOG_WARN << "[Captcha] GetRotate failed: " << st.error_message();
            onRpcError();
            return false;
        }
        out.key         = resp.key();
        out.masterImage = resp.master_image();
        out.thumbImage  = resp.thumb_image();
        return true;
#else
        (void)out; (void)size;
        return false;
#endif
    }

    /// 校验旋转（一次性）。angle 为用户旋转角度（0-360）
    bool verifyRotate(const std::string& key, int angle) {
#ifdef RUOYI_USE_GRPC_CAPTCHA
        auto stub = ensureStub();
        if (!stub) return false;
        captcha::VerifyRotateRequest req;
        req.set_key(key);
        req.set_angle(angle);
        captcha::VerifyRotateResponse resp;
        grpc::ClientContext ctx;
        setDeadline(ctx);
        auto st = stub->VerifyRotate(&ctx, req, &resp);
        if (!st.ok()) {
            LOG_WARN << "[Captcha] VerifyRotate failed: " << st.error_message();
            onRpcError();
            return false;
        }
        return resp.result();
#else
        (void)key; (void)angle;
        return false;
#endif
    }

private:
    CaptchaClient() = default;

#ifdef RUOYI_USE_GRPC_CAPTCHA
    void setDeadline(grpc::ClientContext& ctx) {
        int ms;
        { std::lock_guard<std::mutex> lk(mu_); ms = cfg_.timeoutMs; }
        ctx.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::milliseconds(ms));
    }

    /// 获取 stub 快照（断线时重建 channel）。
    /// 返回 shared_ptr 拷贝：调用方在锁外做 RPC 期间，
    /// init()/onRpcError() reset stub_ 不会影响在飞的调用（无 UAF）。
    std::shared_ptr<captcha::CaptchaService::Stub> ensureStub() {
        std::lock_guard<std::mutex> lk(mu_);
        if (!cfg_.enabled) return nullptr;
        if (!stub_) {
            channel_ = grpc::CreateChannel(cfg_.serverAddr,
                                           grpc::InsecureChannelCredentials());
            stub_ = std::shared_ptr<captcha::CaptchaService::Stub>(
                captcha::CaptchaService::NewStub(channel_).release());
            LOG_INFO << "[Captcha] reconnect -> " << cfg_.serverAddr;
        }
        return stub_;
    }

    /// RPC 失败后标记 stub 失效，下次调用时重建
    void onRpcError() {
        std::lock_guard<std::mutex> lk(mu_);
        stub_.reset();
        channel_.reset();
    }
#endif

    Config cfg_;
    mutable std::mutex mu_;   // mutable：const 方法（isEnabled/type/config）也要加锁
#ifdef RUOYI_USE_GRPC_CAPTCHA
    std::shared_ptr<grpc::Channel> channel_;
    std::shared_ptr<captcha::CaptchaService::Stub> stub_;   // shared_ptr：RPC 快照防 UAF
#endif
};

#endif // RUOYI_CAPTCHA_CLIENT_H
