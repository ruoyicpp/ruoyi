#pragma once
#include <drogon/HttpController.h>
#include "../AjaxResult.h"
#include "../SecurityUtils.h"
#include "../../filters/PermFilter.h"
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
        static const std::set<std::string> allowed = {
            ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".webp", ".svg",
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
        // 保存uploads 目录
        std::string uploadDir = "uploads/";
        std::filesystem::create_directories(uploadDir);
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        std::string newName = std::to_string(ms) + ext;
        std::string filePath = uploadDir + newName;
        file.saveAs(filePath);

        std::string url = "/profile/upload/" + newName;
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
        // 防路径穿越：不允许 .. 和绝对路径
        if (fileName.find("..") != std::string::npos ||
            fileName.find('/') != std::string::npos  ||
            fileName.find('\\') != std::string::npos) {
            RESP_ERR(cb, "非法文件名"); return;
        }
        std::string filePath = "uploads/" + fileName;
        if (!std::filesystem::exists(filePath)) { RESP_ERR(cb, "文件不存在"); return; }
        // 读取文件内容
        std::ifstream ifs(filePath, std::ios::binary);
        if (!ifs) { RESP_ERR(cb, "文件读取失败"); return; }
        std::string content((std::istreambuf_iterator<char>(ifs)),
                             std::istreambuf_iterator<char>());
        // 根据扩展名设置 Content-Type
        std::string ext = std::filesystem::path(fileName).extension().string();
        for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
        std::string mime = "application/octet-stream";
        if (ext==".jpg"||ext==".jpeg") mime="image/jpeg";
        else if (ext==".png")  mime="image/png";
        else if (ext==".gif")  mime="image/gif";
        else if (ext==".pdf")  mime="application/pdf";
        else if (ext==".txt")  mime="text/plain; charset=utf-8";
        else if (ext==".csv")  mime="text/csv; charset=utf-8";
        else if (ext==".mp4")  mime="video/mp4";
        else if (ext==".mp3")  mime="audio/mpeg";
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeString(mime);
        resp->addHeader("Content-Disposition",
            "attachment; filename=\"" + fileName + "\"");
        resp->setBody(std::move(content));
        cb(resp);
        // 下载后可选删除
        if (deleteStr == "true" || deleteStr == "1")
            std::filesystem::remove(filePath);
    }

    void fileList(const drogon::HttpRequestPtr &req,
                  std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "common:file:list");
        std::string uploadDir = "uploads/";
        Json::Value rows(Json::arrayValue);
        std::error_code ec;
        if (std::filesystem::exists(uploadDir, ec)) {
            for (auto& entry : std::filesystem::directory_iterator(uploadDir, ec)) {
                if (!entry.is_regular_file()) continue;
                Json::Value f;
                f["fileName"]  = entry.path().filename().string();
                f["fileSize"]  = (Json::Int64)entry.file_size();
                f["url"]       = "/profile/upload/" + entry.path().filename().string();
                auto ftime = entry.last_write_time();
                auto sctp  = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - std::filesystem::file_time_type::clock::now()
                    + std::chrono::system_clock::now());
                std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
                char buf[20]; std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&tt));
                f["uploadTime"] = buf;
                rows.append(f);
            }
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
        if (fileName.find("..") != std::string::npos ||
            fileName.find('/') != std::string::npos  ||
            fileName.find('\\') != std::string::npos) {
            RESP_ERR(cb, "非法文件名"); return;
        }
        std::string filePath = "uploads/" + fileName;
        std::error_code ec;
        if (!std::filesystem::exists(filePath, ec)) { RESP_ERR(cb, "文件不存在"); return; }
        std::filesystem::remove(filePath, ec);
        if (ec) { RESP_ERR(cb, "删除失败: " + ec.message()); return; }
        RESP_MSG(cb, "删除成功");
    }
};
