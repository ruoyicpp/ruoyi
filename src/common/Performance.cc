#include "Performance.h"
#include <sstream>
#include <iomanip>

std::string PerformanceMetrics::toJson() const {
    std::ostringstream ss;
    ss << "{";

    bool first = true;

    // 计数器
    ss << "\"counters\":{";
    first = true;
    for (auto& [name, counter] : counters_) {
        if (!first) ss << ",";
        ss << "\"" << name << "\":" << counter.load();
        first = false;
    }
    ss << "},";

    // 仪表
    ss << "\"gauges\":{";
    first = true;
    for (auto& [name, gauge] : gauges_) {
        if (!first) ss << ",";
        ss << "\"" << name << "\":" << std::fixed << std::setprecision(2) << gauge.load();
        first = false;
    }
    ss << "},";

    // 直方图
    ss << "\"histograms\":{";
    first = true;
    for (auto& [name, histData] : histograms_) {
        if (!first) ss << ",";
        ss << "\"" << name << "\":";
        auto stats = getHistogramStats(name);
        ss << "{\"count\":" << stats.count
           << ",\"min\":" << stats.min
           << ",\"max\":" << stats.max
           << ",\"avg\":" << stats.avg
           << ",\"p50\":" << stats.p50
           << ",\"p95\":" << stats.p95
           << ",\"p99\":" << stats.p99
           << "}";
        first = false;
    }
    ss << "}";

    ss << "}";
    return ss.str();
}
