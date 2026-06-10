/**
 * @file StorageService.h
 * @brief 文件存储服务 — 支持本地、MinIO、AWS S3 三种存储后端
 * 
 * 功能概述：
 *   - 多存储后端支持：本地磁盘、MinIO、AWS S3
 *   - 统一的存储接口：上传、删除、列表、下载
 *   - 自动路由：根据配置自动选择存储后端
 *   - 签名支持：S3 SigV4 签名、MinIO HMAC 签名
 *   - 文件管理：支持目录结构、文件元数据
 * 
 * 存储后端对比：
 *   - local：本地磁盘存储，适合开发和小规模部署
 *   - minio：MinIO 对象存储，兼容 S3 API，适合中等规模
 *   - s3：AWS S3 或兼容 S3 的云存储，适合大规模和云部署
 * 
 * 配置示例（config.json）：
 *   {
 *     "storage": {
 *       "type": "local",
 *       "local_path": "./upload",
 *       "endpoint": "http://127.0.0.1:9000",
 *       "bucket": "ruoyi",
 *       "access_key": "minioadmin",
 *       "secret_key": "minioadmin",
 *       "region": "us-east-1",
 *       "public_url": "http://minio.example.com"
 *     }
 *   }
 * 
 * 使用示例：
 *   // 初始化
 *   StorageService::instance().init(config["storage"]);
 *   
 *   // 上传文件
 *   std::string url = StorageService::instance().upload(
 *       "2024/01/photo.jpg", fileData, "image/jpeg");
 *   
 *   // 删除文件
 *   StorageService::instance().remove("2024/01/photo.jpg");
 *   
 *   // 列出文件
 *   auto files = StorageService::instance().list("2024/01/");
 * 
 * @see MinIO C++ SDK - MinIO 客户端库
 * @see AWS SDK for C++ - AWS S3 客户端库
 */

#pragma once
#include <string>
#include <fstream>
#include <filesystem>
#include <json/json.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <curl/curl.h>
#include <iostream>

#ifdef RUOYI_USE_MINIO_CPP
#include <miniocpp/client.h>
#include <miniocpp/error.h>
#endif

/**
 * @class StorageService
 * @brief 文件存储服务
 * 
 * 单例模式，提供统一的文件存储接口。
 * 支持本地磁盘、MinIO、AWS S3 三种存储后端。
 */
class StorageService {
public:
    /**
     * @brief 获取单例实例
     * @return StorageService 单例引用
     */
    static StorageService &instance() { static StorageService s; return s; }

    /**
     * @brief 初始化存储服务
     * 
     * 从配置中读取存储后端类型和相关参数。
     * 如果是本地存储，自动创建目录。
     * 
     * @param cfg 存储配置（JSON 对象）
     */
    void init(const Json::Value &cfg) {
        // 读取存储类型和参数
        type_      = cfg.get("type", "local").asString();
        localPath_ = cfg.get("local_path", "./upload").asString();
        endpoint_  = cfg.get("endpoint", "").asString();
        bucket_    = cfg.get("bucket", "ruoyi").asString();
        accessKey_ = cfg.get("access_key", "").asString();
        secretKey_ = cfg.get("secret_key", "").asString();
        region_    = cfg.get("region", "us-east-1").asString();
        publicUrl_ = cfg.get("public_url", "").asString();
        
        // 如果是本地存储，创建目录
        if (type_ == "local")
            std::filesystem::create_directories(localPath_);
    }

    /**
     * @brief 上传文件
     * 
     * 根据配置的存储后端，将文件上传到相应的存储服务。
     * 返回文件的访问 URL。
     * 
     * @param filename 文件存储路径（如 "2024/01/photo.jpg"）
     * @param data 文件二进制内容
     * @param contentType 文件 MIME 类型（默认 "application/octet-stream"）
     * @return 文件访问 URL
     */
    std::string upload(const std::string &filename, const std::string &data,
                       const std::string &contentType = "application/octet-stream") {
        // 根据存储类型分发到相应的上传方法
        if (type_ == "local")   return uploadLocal(filename, data);
        if (type_ == "minio")  return uploadMinio(filename, data, contentType);
        if (type_ == "s3")     return uploadS3(filename, data, contentType);
        return uploadLocal(filename, data);
    }

    /**
     * @brief 删除文件
     * 
     * 从存储服务中删除指定的文件。
     * 
     * @param filename 文件存储路径
     * @return 是否删除成功
     */
    bool remove(const std::string &filename) {
        // 根据存储类型分发到相应的删除方法
        if (type_ == "local") {
            std::error_code ec;
            return std::filesystem::remove(localPath_ + "/" + filename, ec);
        }
        if (type_ == "minio") {
#ifdef RUOYI_USE_MINIO_CPP
            return removeMinio(filename);
#else
            return false;
#endif
        }
        if (type_ == "s3")     return deleteS3(filename);
        return false;
    }

    /**
     * @brief 列出文件
     * 
     * 列出存储服务中指定前缀下的所有文件。
     * 
     * @param prefix 文件前缀（如 "2024/01/"）
     * @return 文件列表（文件名或 key）
     */
    std::vector<std::string> list(const std::string &prefix = "") {
        // 根据存储类型分发到相应的列表方法
        if (type_ == "local") {
            std::vector<std::string> v;
            std::error_code ec;
            for (auto &e : std::filesystem::directory_iterator(localPath_, ec))
                if (e.is_regular_file()) v.push_back(e.path().filename().string());
            return v;
        }
        if (type_ == "minio") {
#ifdef RUOYI_USE_MINIO_CPP
            return listMinio(prefix);
#else
            return {};
#endif
        }
        // S3/MinIO: GET /?list-type=2&prefix=...
        return listS3(prefix);
    }

    std::string getPublicUrl(const std::string &filename) {
        if (type_ == "local") return "/profile/" + filename;
        std::string base = publicUrl_.empty() ? endpoint_ : publicUrl_;
        return base + "/" + bucket_ + "/" + filename;
    }

    std::string type() const { return type_; }

private:
    std::string type_      = "local";
    std::string localPath_ = "./upload";
    std::string endpoint_;
    std::string bucket_;
    std::string accessKey_;
    std::string secretKey_;
    std::string region_    = "us-east-1";
    std::string publicUrl_;

    std::string uploadLocal(const std::string &filename, const std::string &data) {
        auto path = std::filesystem::path(localPath_) / filename;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream f(path, std::ios::binary);
        f.write(data.data(), data.size());
        return "/profile/" + filename;
    }

    // ── minio-cpp SDK 封装（type=minio）───────────────────────
#ifdef RUOYI_USE_MINIO_CPP
    std::string uploadMinio(const std::string &filename, const std::string &data,
                            const std::string &contentType) {
        try {
            minio::s3::BaseUrl baseUrl(endpoint_);
            minio::creds::StaticProvider provider(accessKey_, secretKey_);
            minio::s3::Client client(baseUrl, &provider);

            std::stringstream ss;
            ss.write(data.data(), static_cast<std::streamsize>(data.size()));

            minio::s3::PutObjectArgs args(ss,
                static_cast<long>(data.size()), -1);
            args.bucket        = bucket_;
            args.object        = filename;
            args.content_type  = contentType;

            auto resp = client.PutObject(args);
            if (resp) {
                return getPublicUrl(filename);
            } else {
                std::cerr << "[Storage] MinIO PUT failed: " << resp.Error().String() << std::endl;
                return "";
            }
        } catch (const std::exception &e) {
            std::cerr << "[Storage] MinIO exception: " << e.what() << std::endl;
            return "";
        }
    }

    bool removeMinio(const std::string &filename) {
        try {
            minio::s3::BaseUrl baseUrl(endpoint_);
            minio::creds::StaticProvider provider(accessKey_, secretKey_);
            minio::s3::Client client(baseUrl, &provider);
            minio::s3::RemoveObjectArgs remArgs;
            remArgs.bucket = bucket_;
            remArgs.object = filename;
            auto resp = client.RemoveObject(remArgs);
            return resp.operator bool();
        } catch (const std::exception &e) {
            std::cerr << "[Storage] MinIO remove error: " << e.what() << std::endl;
            return false;
        }
    }

    std::vector<std::string> listMinio(const std::string &prefix) {
        try {
            minio::s3::BaseUrl baseUrl(endpoint_);
            minio::creds::StaticProvider provider(accessKey_, secretKey_);
            minio::s3::Client client(baseUrl, &provider);

            minio::s3::ListObjectsArgs listArgs;
            listArgs.bucket     = bucket_;
            listArgs.prefix     = prefix;
            listArgs.recursive  = true;
            std::vector<std::string> keys;
            for (auto it = client.ListObjects(listArgs); it; ++it) {
                keys.push_back((*it).name);
            }
            return keys;
        } catch (const std::exception &e) {
            std::cerr << "[Storage] MinIO list error: " << e.what() << std::endl;
            return {};
        }
    }
#else
    // 无 minio-cpp 时，minio 类型回退到手写 SigV4（兼容 MinIO）
    std::string uploadMinio(const std::string &filename, const std::string &data,
                            const std::string &contentType) {
        std::cerr << "[Storage] minio-cpp not available, falling back to SigV4 for type=minio" << std::endl;
        return uploadS3(filename, data, contentType);
    }
    bool removeMinio(const std::string &filename) { return deleteS3(filename); }
    std::vector<std::string> listMinio(const std::string &prefix) { return listS3(prefix); }
#endif

    // ── 手写 SigV4（type=s3 或 minio 回退）──────────────────────
    std::string uploadS3(const std::string &filename, const std::string &data,
                         const std::string &contentType) {
        // 计算 payload SHA256
        auto payloadHash = sha256Hex(data);

        // 日期
        time_t now = time(nullptr);
        char dateBuf[9], datetimeBuf[17];
        struct tm *gmt = gmtime(&now);
        strftime(dateBuf,     sizeof(dateBuf),     "%Y%m%d",       gmt);
        strftime(datetimeBuf, sizeof(datetimeBuf), "%Y%m%dT%H%M%SZ", gmt);

        std::string host = endpoint_;
        if (host.rfind("http://",  0) == 0) host = host.substr(7);
        if (host.rfind("https://", 0) == 0) host = host.substr(8);

        std::string objectKey = bucket_ + "/" + filename;
        std::string canonicalUri = "/" + objectKey;

        // Canonical request
        std::string canonReq =
            "PUT\n" + canonicalUri + "\n\n" +
            "content-type:" + contentType + "\n" +
            "host:" + host + "\n" +
            "x-amz-content-sha256:" + payloadHash + "\n" +
            "x-amz-date:" + std::string(datetimeBuf) + "\n\n" +
            "content-type;host;x-amz-content-sha256;x-amz-date\n" + payloadHash;

        std::string credScope = std::string(dateBuf) + "/" + region_ + "/s3/aws4_request";
        std::string strToSign = "AWS4-HMAC-SHA256\n" + std::string(datetimeBuf) + "\n" +
                                credScope + "\n" + sha256Hex(canonReq);

        // Signing key
        auto sigKey = hmacSha256(hmacSha256(hmacSha256(hmacSha256(
            "AWS4" + secretKey_, dateBuf), region_), "s3"), "aws4_request");
        std::string signature = hmacSha256Hex(sigKey, strToSign);

        std::string authHeader = "AWS4-HMAC-SHA256 Credential=" + accessKey_ + "/" + credScope +
            ", SignedHeaders=content-type;host;x-amz-content-sha256;x-amz-date"
            ", Signature=" + signature;

        // HTTP PUT via libcurl
        std::string url = endpoint_ + "/" + objectKey;
        std::string respBody;
        long httpCode = 0;

        CURL *curl = curl_easy_init();
        if (!curl) return "";
        struct curl_slist *hdrs = nullptr;
        hdrs = curl_slist_append(hdrs, ("Authorization: " + authHeader).c_str());
        hdrs = curl_slist_append(hdrs, ("Content-Type: " + contentType).c_str());
        hdrs = curl_slist_append(hdrs, ("x-amz-date: " + std::string(datetimeBuf)).c_str());
        hdrs = curl_slist_append(hdrs, ("x-amz-content-sha256: " + payloadHash).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)data.size());
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        // Read callback
        struct ReadCtx { const char *ptr; size_t rem; };
        ReadCtx rctx{data.data(), data.size()};
        curl_easy_setopt(curl, CURLOPT_READFUNCTION,
            +[](char *buf, size_t, size_t n, void *u) -> size_t {
                auto *c = (ReadCtx*)u;
                size_t cp = std::min(n, c->rem);
                memcpy(buf, c->ptr, cp); c->ptr += cp; c->rem -= cp; return cp;
            });
        curl_easy_setopt(curl, CURLOPT_READDATA, &rctx);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
            +[](char *p, size_t, size_t n, void *u) -> size_t {
                ((std::string*)u)->append(p, n); return n;
            });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &respBody);
        curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_slist_free_all(hdrs);
        curl_easy_cleanup(curl);

        if (httpCode < 200 || httpCode >= 300) {
            std::cerr << "[Storage] S3 PUT failed HTTP " << httpCode << " " << respBody << std::endl;
            return "";
        }
        std::string base = publicUrl_.empty() ? endpoint_ : publicUrl_;
        return base + "/" + bucket_ + "/" + filename;
    }

    bool deleteS3(const std::string &filename) {
        std::string objectKey = bucket_ + "/" + filename;
        time_t now = time(nullptr);
        char dateBuf[9], datetimeBuf[17];
        struct tm *gmt = gmtime(&now);
        strftime(dateBuf,     sizeof(dateBuf),     "%Y%m%d",       gmt);
        strftime(datetimeBuf, sizeof(datetimeBuf), "%Y%m%dT%H%M%SZ", gmt);

        std::string host = endpoint_;
        if (host.rfind("http://",  0) == 0) host = host.substr(7);
        if (host.rfind("https://", 0) == 0) host = host.substr(8);

        std::string canonReq =
            "DELETE\n/" + objectKey + "\n\n" +
            "host:" + host + "\n" +
            "x-amz-date:" + std::string(datetimeBuf) + "\n\n" +
            "host;x-amz-date\n" +
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

        std::string credScope = std::string(dateBuf) + "/" + region_ + "/s3/aws4_request";
        std::string strToSign = "AWS4-HMAC-SHA256\n" + std::string(datetimeBuf) + "\n" +
                                credScope + "\n" + sha256Hex(canonReq);
        auto sigKey = hmacSha256(hmacSha256(hmacSha256(hmacSha256(
            "AWS4" + secretKey_, dateBuf), region_), "s3"), "aws4_request");
        std::string authHeader = "AWS4-HMAC-SHA256 Credential=" + accessKey_ + "/" + credScope +
            ", SignedHeaders=host;x-amz-date, Signature=" + hmacSha256Hex(sigKey, strToSign);

        std::string url = endpoint_ + "/" + objectKey;
        long httpCode = 0;
        CURL *curl = curl_easy_init(); if (!curl) return false;
        struct curl_slist *hdrs = nullptr;
        hdrs = curl_slist_append(hdrs, ("Authorization: " + authHeader).c_str());
        hdrs = curl_slist_append(hdrs, ("x-amz-date: " + std::string(datetimeBuf)).c_str());
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_slist_free_all(hdrs);
        curl_easy_cleanup(curl);
        return httpCode >= 200 && httpCode < 300;
    }

    std::vector<std::string> listS3(const std::string &prefix) {
        std::string objectPath = bucket_ + "/?list-type=2" +
            (prefix.empty() ? "" : "&prefix=" + prefix);
        time_t now = time(nullptr);
        char dateBuf[9], datetimeBuf[17];
        struct tm *gmt = gmtime(&now);
        strftime(dateBuf,     sizeof(dateBuf),     "%Y%m%d",       gmt);
        strftime(datetimeBuf, sizeof(datetimeBuf), "%Y%m%dT%H%M%SZ", gmt);

        std::string host = endpoint_;
        if (host.rfind("http://",  0) == 0) host = host.substr(7);
        if (host.rfind("https://", 0) == 0) host = host.substr(8);

        std::string credScope = std::string(dateBuf) + "/" + region_ + "/s3/aws4_request";
        std::string canonReq =
            "GET\n/" + bucket_ + "/\nlist-type=2" + (prefix.empty() ? "" : "&prefix=" + prefix) + "\n" +
            "host:" + host + "\nx-amz-date:" + std::string(datetimeBuf) + "\n\n" +
            "host;x-amz-date\n" +
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
        std::string strToSign = "AWS4-HMAC-SHA256\n" + std::string(datetimeBuf) + "\n" +
                                credScope + "\n" + sha256Hex(canonReq);
        auto sigKey = hmacSha256(hmacSha256(hmacSha256(hmacSha256(
            "AWS4" + secretKey_, dateBuf), region_), "s3"), "aws4_request");
        std::string authHeader = "AWS4-HMAC-SHA256 Credential=" + accessKey_ + "/" + credScope +
            ", SignedHeaders=host;x-amz-date, Signature=" + hmacSha256Hex(sigKey, strToSign);

        std::string url = endpoint_ + "/" + objectPath;
        std::string body;
        CURL *curl = curl_easy_init(); if (!curl) return {};
        struct curl_slist *hdrs = nullptr;
        hdrs = curl_slist_append(hdrs, ("Authorization: " + authHeader).c_str());
        hdrs = curl_slist_append(hdrs, ("x-amz-date: " + std::string(datetimeBuf)).c_str());
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
            +[](char *p, size_t, size_t n, void *u) -> size_t {
                ((std::string*)u)->append(p, n); return n;
            });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
        curl_easy_perform(curl);
        curl_slist_free_all(hdrs);
        curl_easy_cleanup(curl);

        // 简单解析 XML <Key>...</Key>
        std::vector<std::string> keys;
        size_t pos = 0;
        while ((pos = body.find("<Key>", pos)) != std::string::npos) {
            pos += 5;
            auto end = body.find("</Key>", pos);
            if (end == std::string::npos) break;
            keys.push_back(body.substr(pos, end - pos));
            pos = end + 6;
        }
        return keys;
    }

    static std::string sha256Hex(const std::string &data) {
        unsigned char h[SHA256_DIGEST_LENGTH];
        SHA256((const unsigned char *)data.data(), data.size(), h);
        std::ostringstream ss;
        for (auto b : h) ss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
        return ss.str();
    }

    static std::vector<uint8_t> hmacSha256(const std::string &key, const std::string &msg) {
        unsigned char h[EVP_MAX_MD_SIZE]; unsigned int len = 0;
        HMAC(EVP_sha256(), key.data(), (int)key.size(),
             (const unsigned char *)msg.data(), msg.size(), h, &len);
        return {h, h + len};
    }
    static std::vector<uint8_t> hmacSha256(const std::vector<uint8_t> &key, const std::string &msg) {
        unsigned char h[EVP_MAX_MD_SIZE]; unsigned int len = 0;
        HMAC(EVP_sha256(), key.data(), (int)key.size(),
             (const unsigned char *)msg.data(), msg.size(), h, &len);
        return {h, h + len};
    }
    static std::string hmacSha256Hex(const std::vector<uint8_t> &key, const std::string &msg) {
        auto h = hmacSha256(key, msg);
        std::ostringstream ss;
        for (auto b : h) ss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
        return ss.str();
    }
};
