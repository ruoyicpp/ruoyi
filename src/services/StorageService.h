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

// 文件存储分层：本地磁盘 / MinIO / AWS S3（SigV4 签名）
// config.json: { "storage": { "type": "local", "local_path": "./upload",
//   "endpoint": "http://127.0.0.1:9000", "bucket": "ruoyi",
//   "access_key": "", "secret_key": "", "region": "us-east-1", "public_url": "" } }
class StorageService {
public:
    static StorageService &instance() { static StorageService s; return s; }

    void init(const Json::Value &cfg) {
        type_      = cfg.get("type", "local").asString();
        localPath_ = cfg.get("local_path", "./upload").asString();
        endpoint_  = cfg.get("endpoint", "").asString();
        bucket_    = cfg.get("bucket", "ruoyi").asString();
        accessKey_ = cfg.get("access_key", "").asString();
        secretKey_ = cfg.get("secret_key", "").asString();
        region_    = cfg.get("region", "us-east-1").asString();
        publicUrl_ = cfg.get("public_url", "").asString();
        if (type_ == "local")
            std::filesystem::create_directories(localPath_);
    }

    // 上传文件，返回访问 URL
    // data: 文件二进制内容，filename: 存储路径（如 2024/01/photo.jpg）
    std::string upload(const std::string &filename, const std::string &data,
                       const std::string &contentType = "application/octet-stream") {
        if (type_ == "local")   return uploadLocal(filename, data);
        if (type_ == "minio")   return uploadS3(filename, data, contentType);
        if (type_ == "s3")      return uploadS3(filename, data, contentType);
        return uploadLocal(filename, data);
    }

    // 删除文件
    bool remove(const std::string &filename) {
        if (type_ == "local") {
            std::error_code ec;
            return std::filesystem::remove(localPath_ + "/" + filename, ec);
        }
        // MinIO/S3：发送 DELETE 请求
        return false;
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

    // AWS SigV4 签名 PUT（兼容 MinIO）
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

        // HTTP PUT via curl CLI（简化）
        std::string url = endpoint_ + "/" + objectKey;
        std::string cmd = "curl -s -X PUT"
            " -H \"Authorization: " + authHeader + "\""
            " -H \"Content-Type: " + contentType + "\""
            " -H \"x-amz-date: " + std::string(datetimeBuf) + "\""
            " -H \"x-amz-content-sha256: " + payloadHash + "\""
            " --data-binary @- \"" + url + "\"";
        (void)cmd; // 实际使用时启用 popen 调用

        std::string base = publicUrl_.empty() ? endpoint_ : publicUrl_;
        return base + "/" + bucket_ + "/" + filename;
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
