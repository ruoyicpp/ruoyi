#pragma once

#include <json/json.h>

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace Analytics {

using JsonValue = Json::Value;

struct ReportTemplate {
    std::string name;
    std::string description;

    struct Metric {
        std::string name;
        std::string label;
        std::string format;
        std::string unit;
        std::string query;
    };

    struct ChartConfig {
        std::string name;
        std::string title;
        std::string type;          // line | bar | pie | scatter | table
        std::string query;
        std::string xAxis;
        std::vector<std::string> yAxes;
    };

    struct Section {
        std::string name;
        std::string title;
        std::vector<Metric> metrics;
        std::vector<ChartConfig> charts;
    };

    std::vector<Section> sections;
};

struct ScheduleConfig {
    bool enabled = false;
    std::string cron;
    std::string format;        // html | csv | json | pdf
    std::string timezone;
    std::vector<std::string> recipients;
};

struct ReportResult {
    int64_t reportId = 0;
    std::string name;
    std::string format;
    std::string generatedAt;
    std::string filePath;
    int64_t fileSize = 0;
    bool success = false;
    std::string errorMessage;
    std::map<std::string, std::string> metadata;
};

class ReportGenerator {
public:
    static ReportGenerator& instance();

    void init(const JsonValue& config);
    void shutdown();

    bool registerTemplate(const ReportTemplate& tmpl);
    ReportTemplate getTemplate(const std::string& name);
    std::vector<std::string> listTemplates();
    bool deleteTemplate(const std::string& name);

    ReportResult generate(const std::string& templateName,
                          const std::map<std::string, std::string>& params = {},
                          const std::string& format = "html");
    ReportResult generate(int64_t reportId,
                          const std::map<std::string, std::string>& params = {},
                          const std::string& format = "html");

    JsonValue previewData(const std::string& templateName,
                          const std::map<std::string, std::string>& params = {});

    void startScheduler();
    void stopScheduler();
    bool updateSchedule(int64_t reportId, const ScheduleConfig& schedule);

    std::vector<ReportResult> listGeneratedReports(int64_t reportId = 0, int page = 1, int pageSize = 20);
    bool deleteGeneratedReport(int64_t generatedId);

private:
    ReportGenerator() = default;
    ~ReportGenerator() = default;
    ReportGenerator(const ReportGenerator&) = delete;
    ReportGenerator& operator=(const ReportGenerator&) = delete;

    std::string getReportPath(int64_t reportId);

    std::string renderHtml(const ReportTemplate& tmpl,
                           const std::map<std::string, std::string>& params,
                           const JsonValue& data);
    std::string renderCsv(const ReportTemplate& tmpl,
                          const std::map<std::string, std::string>& params,
                          const JsonValue& data);
    std::string renderJson(const ReportTemplate& tmpl,
                           const std::map<std::string, std::string>& params,
                           const JsonValue& data);

    bool saveReport(const ReportResult& result);
    std::string expandQuery(const std::string& query,
                            const std::map<std::string, std::string>& params);
    JsonValue executeQuery(const std::string& query);

    mutable std::mutex mutex_;
    std::map<std::string, ReportTemplate> templates_;
    std::atomic<int64_t> nextGeneratedId_{1};
    bool schedulerRunning_ = false;

    std::string outputDir_ = "reports";
    std::string templateDir_ = "templates";
    std::string dbPath_;
};

} // namespace Analytics
