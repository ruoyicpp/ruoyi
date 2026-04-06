#pragma once

// ── 标准库 ────────────────────────────────────────────────────────
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <chrono>
#include <ctime>
#include <thread>
#include <iomanip>
#include <mutex>
#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
#  include <shlobj.h>
#else
#  include <unistd.h>
#  include <sys/statvfs.h>
#  include <sys/resource.h>
#endif

// ── Drogon / Trantor ─────────────────────────────────────────────
#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>

// ── 公共基础 ─────────────────────────────────────────────────────
#include "common/CrashHandler.h"
#include "common/DatabaseInit.h"
#include "common/JwtUtils.h"
#include "common/TokenCache.h"
#include "common/XssUtils.h"
#include "common/DeviceBinding.h"
#include "common/ConfigLoader.h"
#include "common/RateLimiter.h"
#include "common/SignUtils.h"
#include "common/JsonLogger.h"
#include "common/SslManager.h"

// ── 服务层 ───────────────────────────────────────────────────────
#include "services/NginxManager.h"
#include "services/DdnsGoManager.h"
#include "services/KoboldCppManager.h"
#include "services/KoboldCppService.h"
#include "services/WhisperService.h"
#include "services/VaultManager.h"
#include "system/services/SysConfigService.h"
#include "system/services/SysDictService.h"
#include "system/services/TokenService.h"

// ── 过滤器 / 调度器 ──────────────────────────────────────────────
#include "filters/JwtAuthFilter.h"
#include "monitor/JobScheduler.h"

// ── System 控制器 ────────────────────────────────────────────────
#include "system/controllers/SysLoginCtrl.h"
#include "system/controllers/SysUserCtrl.h"
#include "system/controllers/SysRoleCtrl.h"
#include "system/controllers/SysMenuCtrl.h"
#include "system/controllers/SysDeptCtrl.h"
#include "system/controllers/SysConfigCtrl.h"
#include "system/controllers/SysDictCtrl.h"
#include "system/controllers/SysPostCtrl.h"
#include "system/controllers/SysNoticeCtrl.h"
#include "system/controllers/SysEmailConfigCtrl.h"
#include "system/controllers/SysSslConfigCtrl.h"

// ── Monitor 控制器 ───────────────────────────────────────────────
#include "monitor/controllers/SysLogCtrl.h"
#include "monitor/controllers/SysOnlineCtrl.h"
#include "monitor/controllers/WsNotifyCtrl.h"
#include "monitor/controllers/SysJobCtrl.h"
#include "monitor/controllers/ServerCtrl.h"
#include "monitor/controllers/CacheCtrl.h"
#include "monitor/controllers/DruidCtrl.h"
#include "monitor/controllers/SysIpCtrl.h"

// ── Common 控制器 ────────────────────────────────────────────────
#include "common/controllers/CommonCtrl.h"
#include "common/controllers/DashboardCtrl.h"

// ── Tool 控制器 ──────────────────────────────────────────────────
#include "tool/controllers/GenCtrl.h"
#include "tool/controllers/BuildCtrl.h"
#include "tool/controllers/WebsiteInfoCtrl.h"
#include "tool/controllers/VideoCtrl.h"
#include "tool/controllers/SwaggerCtrl.h"

// ── AI ───────────────────────────────────────────────────────────
#include "ai/controllers/AiCtrl.h"
#include "ai/AiChatCtrl.h"
