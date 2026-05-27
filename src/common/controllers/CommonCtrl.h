#pragma once
#include <drogon/HttpController.h>
#include "../AjaxResult.h"
#include "../SecurityUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/StorageService.h"
#include <filesystem>
#include <fstream>
#include <set>
#include <chrono>
#include <ctime>

// common controller: file upload
class CommonCtrl : public drogon::HttpController<CommonCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(CommonCtrl::upload,     "/common/upload",   drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(CommonCtrl::download,   "/common/download", drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(CommonCtrl::fileList,   "/common/files",    drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(CommonCtrl::fileDelete, "/common/file",     drogon::Delete, "JwtAuthFilter");
    METHOD_LIST_END

    void upload(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        drogon::MultiPartParser parser;
        if (parser.parse(req) != 0) { RESP_ERR(cb, "上传文件异常"); return; }
        auto &files = parser.getFiles();
        if (files.empty()) { RESP_ERR(cb, "上传文件不能为空"); return; }

        auto &file = files[0];
        std::string ext = std::filesystem::path(file.getFileName()).extension().string();
        for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
        // 注意：.svg 已移除——SVG 可嵌入 <script>，被浏览器以 image/svg+xml 直接渲染时执行
        static const std::set<std::string> allowed = {
            ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".webp",
            ".pdf", ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx",
            ".txt", ".csv", ".zip", ".rar", ".7z", ".mp4", ".mp3"
        };
        if (ext.empty() || allowed.find(ext) == allowed.end()) {
            RESP_ERR(cb, "不允许上传该类型文件: " + ext);
            return;
        }
        // 文件大小限制（从 config.json upload.max_file_mb 读取，默认 10MB）
        {
            size_t maxMb = 10;
            auto& cfg = drogon::app().getCustomConfig();
            if (cfg.isMember("upload")) maxMb = (size_t)cfg["upload"].get("max_file_mb", 10).asInt();
            if (file.fileLength() > maxMb * 1024 * 1024) {
                RESP_ERR(cb, "文件大小超过限制 (" + std::to_string(maxMb) + "MB)");
                return;
            }
        }
        // 生成文件名：日期目录/ms_rnd.ext
        auto now = std::chrono::system_clock::now();
        auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        char rnd[5]; for (int i = 0; i < 4; ++i) rnd[i] = (char)('a' + (std::rand() % 26)); rnd[4] = 0;
        time_t tt = std::chrono::system_clock::to_time_t(now);
        char dateBuf[16];
#ifdef _WIN32
        { struct tm tm_; localtime_s(&tm_, &tt); std::strftime(dateBuf, sizeof(dateBuf), "%Y%m%d", &tm_); }
#else
        { struct tm tm_; localtime_r(&tt, &tm_); std::strftime(dateBuf, sizeof(dateBuf), "%Y%m%d", &tm_); }
#endif
        std::string newName = std::string(dateBuf) + "/" + std::to_string(ms) + "_" + rnd + ext;

        // MIME
        std::string mime = "application/octet-stream";
        if (ext==".jpg"||ext==".jpeg") mime="image/jpeg";
        else if (ext==".png")  mime="image/png";
        else if (ext==".gif")  mime="image/gif";
        else if (ext==".webp") mime="image/webp";
        else if (ext==".bmp")  mime="image/bmp";
        else if (ext==".pdf")  mime="application/pdf";
        else if (ext==".mp4")  mime="video/mp4";
        else if (ext==".mp3")  mime="audio/mpeg";

        auto sv = file.fileContent();
        std::string data(sv.data(), sv.size());
        auto& storage = StorageService::instance();
        std::string url;

        // 优先走 StorageService（MinIO/S3/local 由配置决定）
        url = storage.upload(newName, data, mime);

        // MinIO/S3 失败时降级本地
        if (url.empty()) {
            std::string localDir = "uploads/" + std::string(dateBuf) + "/";
            std::filesystem::create_directories(localDir);
            std::string localPath = localDir + std::to_string(ms) + "_" + rnd + ext;
            std::ofstream lf(localPath, std::ios::binary);
            lf.write(data.data(), data.size());
            url = "/profile/upload/" + newName;
            LOG_WARN << "[Storage] 远端上传失败，已降级到本地: " << localPath;
        }

        auto result = AjaxResult::successMap();
        result["url"]              = url;
        result["fileName"]         = url;
        result["newFileName"]      = newName;
        result["originalFilename"] = file.getFileName();
        RESP_JSON(cb, result);
    }

    void download(const drogon::HttpRequestPtr &req,
                  std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        std::string fileName  = req->getParameter("fileName");
        std::string deleteStr = req->getParameter("delete");
        if (fileName.empty()) { RESP_ERR(cb, "文件名不能为空"); return; }
        if (fileName.find("..") != std::string::npos) { RESP_ERR(cb, "非法文件名"); return; }

        // MinIO/S3：重定向到公开 URL
        auto& storage = StorageService::instance();
        if (storage.type() != "local") {
            auto pubUrl = storage.getPublicUrl(fileName);
            auto resp = drogon::HttpResponse::newRedirectionResponse(pubUrl);
            cb(resp);
            return;
        }

        // 本地模式
        if (fileName.find('/') != std::string::npos || fileName.find('\\') != std::string::npos) {
            RESP_ERR(cb, "非法文件名"); return;
        }
        std::string filePath = "uploads/" + fileName;
        if (!std::filesystem::exists(filePath)) { RESP_ERR(cb, "文件不存在"); return; }
        std::ifstream ifs(filePath, std::ios::binary);
        if (!ifs) { RESP_ERR(cb, "文件读取失败"); return; }
        std::string content((std::istreambuf_iterator<char>(ifs)), {});
        std::string ext2 = std::filesystem::path(fileName).extension().string();
        for (auto& c : ext2) c = (char)std::tolower((unsigned char)c);
        std::string mime = "application/octet-stream";
        if (ext2==".jpg"||ext2==".jpeg") mime="image/jpeg";
        else if (ext2==".png")  mime="image/png";
        else if (ext2==".gif")  mime="image/gif";
        else if (ext2==".pdf")  mime="application/pdf";
        else if (ext2==".txt")  mime="text/plain; charset=utf-8";
        else if (ext2==".csv")  mime="text/csv; charset=utf-8";
        else if (ext2==".mp4")  mime="video/mp4";
        else if (ext2==".mp3")  mime="audio/mpeg";
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeString(mime);
        resp->addHeader("Content-Disposition", "attachment; filename=\"" + fileName + "\"");
        resp->setBody(std::move(content));
        cb(resp);
        if (deleteStr == "true" || deleteStr == "1")
            std::filesystem::remove(filePath);
    }

    void fileList(const drogon::HttpRequestPtr &req,
                  std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "common:file:list");
        auto& storage = StorageService::instance();
        auto keys = storage.list();
        Json::Value rows(Json::arrayValue);
        for (auto& key : keys) {
            Json::Value f;
            f["fileName"] = key;
            f["url"]      = storage.getPublicUrl(key);
            rows.append(f);
        }
        auto result = AjaxResult::successMap();
        result["rows"]  = rows;
        result["total"] = (Json::Int)rows.size();
        RESP_JSON(cb, result);
    }

    void fileDelete(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "common:file:delete");
        std::string fileName = req->getParameter("fileName");
        if (fileName.empty()) { RESP_ERR(cb, "文件名不能为空"); return; }
        if (fileName.find("..") != std::string::npos) { RESP_ERR(cb, "非法文件名"); return; }
        bool ok = StorageService::instance().remove(fileName);
        if (!ok) { RESP_ERR(cb, "删除失败或文件不存在"); return; }
        RESP_MSG(cb, "删除成功");
    }
};
