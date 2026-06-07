# Alert System Module
# 性能告警系统模块

# 源文件
set(ALERT_SOURCES
    AlertEngine.cc
    AlertAggregator.cc
    AlertNotifier.cc
    AlertCtrl.cc
)

# 头文件
set(ALERT_HEADERS
    AlertEngine.h
    AlertAggregator.h
    AlertNotifier.h
    AlertRule.h
    AlertCtrl.h
)

# 添加到父项目的源文件列表
# 在主 CMakeLists.txt 中包含此文件后，会自动添加这些源文件
