// AI 生成轨迹逐字恢复 - 来自 cb68a004 Step 7597
#pragma once
#include "TermColor.h"
#include <trantor/utils/Logger.h>

namespace ColorLogger {

// 初始化终端颜色 + 安装 trantor 彩色日志拦截器
// 在 main() 最顶部调用一次即可
inline void install() {
    TermColor::init();
    trantor::Logger::setOutputFunction(
        [](const char* msg, const uint64_t len) {
            TermColor::writeColored(msg, static_cast<size_t>(len));
        },
        []() { std::cout.flush(); }
    );
}

} // namespace ColorLogger
