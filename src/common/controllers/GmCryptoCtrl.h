#pragma once
// ════════════════════════════════════════════════════════════════════════════
// GmCryptoCtrl.h — 国密算法 REST API
//
//   POST /gm/sm3          body: {"data":"..."} → {"hash":"<hex>"}
//   POST /gm/sm4/encrypt  body: {"data":"...","key":"<16char>"} → {"cipher":"..."}
//   POST /gm/sm4/decrypt  body: {"cipher":"...","key":"<16char>"} → {"data":"..."}
//   GET  /gm/sm2/genkey   → {"privPem":"...","pubPem":"..."}
//   POST /gm/sm2/sign     body: {"privPem":"...","msg":"..."} → {"sig":"..."}
//   POST /gm/sm2/verify   body: {"pubPem":"...","msg":"...","sig":"..."} → {"valid":bool}
//   POST /gm/password/hash    body: {"password":"..."} → {"hash":"...","algo":"sm3pbkdf2"}
//   POST /gm/password/verify  body: {"password":"...","hash":"..."} → {"valid":bool}
// ════════════════════════════════════════════════════════════════════════════
#include <drogon/drogon.h>
#include "../GmCrypto.h"
#include "../AjaxResult.h"

class GmCryptoCtrl : public drogon::HttpController<GmCryptoCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(GmCryptoCtrl::sm3Hash,       "/gm/sm3",             drogon::Post, "JwtAuthFilter");
        ADD_METHOD_TO(GmCryptoCtrl::sm4Encrypt,    "/gm/sm4/encrypt",     drogon::Post, "JwtAuthFilter");
        ADD_METHOD_TO(GmCryptoCtrl::sm4Decrypt,    "/gm/sm4/decrypt",     drogon::Post, "JwtAuthFilter");
        ADD_METHOD_TO(GmCryptoCtrl::sm2GenKey,     "/gm/sm2/genkey",      drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(GmCryptoCtrl::sm2Sign,       "/gm/sm2/sign",        drogon::Post, "JwtAuthFilter");
        ADD_METHOD_TO(GmCryptoCtrl::sm2Verify,     "/gm/sm2/verify",      drogon::Post, "JwtAuthFilter");
        ADD_METHOD_TO(GmCryptoCtrl::pwdHash,       "/gm/password/hash",   drogon::Post, "JwtAuthFilter");
        ADD_METHOD_TO(GmCryptoCtrl::pwdVerify,     "/gm/password/verify", drogon::Post, "JwtAuthFilter");
    METHOD_LIST_END

    // POST /gm/sm3
    void sm3Hash(const drogon::HttpRequestPtr &req,
                 std::function<void(const drogon::HttpResponsePtr&)> &&cb) {
        auto body = req->getJsonObject();
        if (!body || !(*body)["data"].isString()) { RESP_ERR(cb, "缺少 data 字段"); return; }
        try {
            Json::Value r;
            r["hash"] = GmCrypto::sm3Hex((*body)["data"].asString());
            r["algo"] = "SM3";
            RESP_OK(cb, r);
        } catch (const std::exception &e) { RESP_ERR(cb, e.what()); }
    }

    // POST /gm/sm4/encrypt
    void sm4Encrypt(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr&)> &&cb) {
        auto body = req->getJsonObject();
        if (!body || !(*body)["data"].isString() || !(*body)["key"].isString())
            { RESP_ERR(cb, "缺少 data / key 字段"); return; }
        std::string key = (*body)["key"].asString();
        if (key.size() != 16) { RESP_ERR(cb, "SM4 key 必须恰好 16 字节"); return; }
        try {
            Json::Value r;
            r["cipher"] = GmCrypto::sm4Encrypt((*body)["data"].asString(), key);
            r["algo"]   = "SM4-GCM";
            RESP_OK(cb, r);
        } catch (const std::exception &e) { RESP_ERR(cb, e.what()); }
    }

    // POST /gm/sm4/decrypt
    void sm4Decrypt(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr&)> &&cb) {
        auto body = req->getJsonObject();
        if (!body || !(*body)["cipher"].isString() || !(*body)["key"].isString())
            { RESP_ERR(cb, "缺少 cipher / key 字段"); return; }
        std::string key = (*body)["key"].asString();
        if (key.size() != 16) { RESP_ERR(cb, "SM4 key 必须恰好 16 字节"); return; }
        try {
            Json::Value r;
            r["data"] = GmCrypto::sm4Decrypt((*body)["cipher"].asString(), key);
            RESP_OK(cb, r);
        } catch (const std::exception &e) { RESP_ERR(cb, e.what()); }
    }

    // GET /gm/sm2/genkey
    void sm2GenKey(const drogon::HttpRequestPtr&,
                   std::function<void(const drogon::HttpResponsePtr&)> &&cb) {
        try {
            auto kp = GmCrypto::sm2GenKey();
            Json::Value r;
            r["privPem"] = kp.privPem;
            r["pubPem"]  = kp.pubPem;
            r["algo"]    = "SM2";
            RESP_OK(cb, r);
        } catch (const std::exception &e) { RESP_ERR(cb, e.what()); }
    }

    // POST /gm/sm2/sign
    void sm2Sign(const drogon::HttpRequestPtr &req,
                 std::function<void(const drogon::HttpResponsePtr&)> &&cb) {
        auto body = req->getJsonObject();
        if (!body || !(*body)["privPem"].isString() || !(*body)["msg"].isString())
            { RESP_ERR(cb, "缺少 privPem / msg 字段"); return; }
        try {
            Json::Value r;
            r["sig"]  = GmCrypto::sm2Sign((*body)["privPem"].asString(),
                                           (*body)["msg"].asString());
            r["algo"] = "SM2-SM3";
            RESP_OK(cb, r);
        } catch (const std::exception &e) { RESP_ERR(cb, e.what()); }
    }

    // POST /gm/sm2/verify
    void sm2Verify(const drogon::HttpRequestPtr &req,
                   std::function<void(const drogon::HttpResponsePtr&)> &&cb) {
        auto body = req->getJsonObject();
        if (!body || !(*body)["pubPem"].isString() ||
            !(*body)["msg"].isString() || !(*body)["sig"].isString())
            { RESP_ERR(cb, "缺少 pubPem / msg / sig 字段"); return; }
        try {
            bool ok = GmCrypto::sm2Verify((*body)["pubPem"].asString(),
                                          (*body)["msg"].asString(),
                                          (*body)["sig"].asString());
            Json::Value r;
            r["valid"] = ok;
            RESP_OK(cb, r);
        } catch (const std::exception &e) { RESP_ERR(cb, e.what()); }
    }

    // POST /gm/password/hash
    void pwdHash(const drogon::HttpRequestPtr &req,
                 std::function<void(const drogon::HttpResponsePtr&)> &&cb) {
        auto body = req->getJsonObject();
        if (!body || !(*body)["password"].isString())
            { RESP_ERR(cb, "缺少 password 字段"); return; }
        try {
            Json::Value r;
            r["hash"] = GmCrypto::hashPassword((*body)["password"].asString());
            r["algo"] = "SM3-PBKDF2";
            RESP_OK(cb, r);
        } catch (const std::exception &e) { RESP_ERR(cb, e.what()); }
    }

    // POST /gm/password/verify
    void pwdVerify(const drogon::HttpRequestPtr &req,
                   std::function<void(const drogon::HttpResponsePtr&)> &&cb) {
        auto body = req->getJsonObject();
        if (!body || !(*body)["password"].isString() || !(*body)["hash"].isString())
            { RESP_ERR(cb, "缺少 password / hash 字段"); return; }
        try {
            bool ok = GmCrypto::verifyPassword((*body)["password"].asString(),
                                               (*body)["hash"].asString());
            Json::Value r;
            r["valid"] = ok;
            RESP_OK(cb, r);
        } catch (const std::exception &e) { RESP_ERR(cb, e.what()); }
    }
};
