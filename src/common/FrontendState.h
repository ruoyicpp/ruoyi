#pragma once
#include <string>

// 前端托管共享状态（由 main.cc 在前端模块初始化后设置）
// 供安全中间件（限流跳过静态资源、SPA 404 回退）等模块读取
inline bool        feHosted    = false;
inline bool        feSpaMode   = false;
inline bool        feEmbedded  = false;
inline std::string feApiPrefix;
inline std::string feIndexPath;
