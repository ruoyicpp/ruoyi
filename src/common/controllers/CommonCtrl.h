#pragma once
#include <drogon/HttpController.h>
#include "../AjaxResult.h"
#include "../SecurityUtils.h"
#include "../../filters/PermFilter.h"
#include <filesystem>
#include <fstream>

// common controller: file upload
class CommonCtrl : public drogon::HttpController<CommonCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(CommonCtrl::upload, "/common/upload", drogon::Post, "JwtAuthFilter");
    METHOD_LIST_END

    void upload(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        drogon::MultiPartParser parser;
        if (parser.parse(req) != 0) { RESP_ERR(cb, "上传文件异常"); return; }
        auto &files = parser.getFiles();
        if (files.empty()) { RESP_ERR(cb, "上传文件不能为空"); return; }

        auto &file = files[0];
        // 保存�?uploads 目录
        std::string uploadDir = "uploads/";
        std::filesystem::create_directories(uploadDir);
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        std::string ext = std::filesystem::path(file.getFileName()).extension().string();
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
};
