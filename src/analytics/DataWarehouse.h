#pragma once

#include <json/json.h>

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Analytics {

using JsonValue = Json::Value;
using RowData = std::map<std::string, std::string>;
using TableData = std::vector<RowData>;

struct DataSourceConfig {
    std::string name;
    std::string type;        // memory | json_file | database
    std::string connection;
    std::map<std::string, std::string> options;
};

struct ETLConfig {
    bool enabled = true;
    int intervalSeconds = 3600;
    int batchSize = 1000;
    bool removeDuplicates = true;
    std::string missingValueStrategy;  // null | drop | fill_default
    bool normalize = true;
};

struct TransformConfig {
    std::vector<std::string> groupBy;
    struct Aggregation {
        std::string field;
        std::string function;  // count | sum | avg | min | max
        std::string alias;
    };
    std::vector<Aggregation> aggregations;
    std::vector<std::string> orderBy;
    std::string orderDirection = "asc";
    int limit = 0;
};

struct LoadConfig {
    std::string targetTable;
    std::string mode;          // append | replace | upsert
    std::string uniqueKey;
};

struct Report {
    int64_t id = 0;
    std::string name;
    std::string description;
    std::string category;
    std::string templateName;
    std::string createdBy;
    std::string createdAt;
    std::string updatedAt;
    int status = 0;
    std::map<std::string, std::string> parameters;
};

struct ChartDefinition {
    int64_t id = 0;
    std::string reportId;
    std::string name;
    std::string type;          // table | line | bar | pie | scatter
    std::string query;
    std::string xAxis;
    std::string yAxis;
    std::vector<std::string> series;
    std::map<std::string, std::string> options;
};

struct Dashboard {
    int64_t id = 0;
    std::string name;
    std::string description;
    std::string layout;
    std::vector<int64_t> chartIds;
};

class DataWarehouse {
public:
    static DataWarehouse& instance();

    void init(const JsonValue& config);
    void shutdown();

    void startETL();
    void stopETL();

    TableData collect(const DataSourceConfig& source,
                      const std::vector<std::string>& columns = {},
                      const std::string& filter = "");
    TableData clean(const TableData& raw, const ETLConfig& config);
    TableData transform(const TableData& cleaned, const TransformConfig& config);
    bool load(const TableData& data, const LoadConfig& config);

    TableData query(const std::string& sql);
    TableData query(const std::string& table,
                    const std::map<std::string, std::string>& conditions = {},
                    const std::vector<std::string>& orderBy = {},
                    int limit = 0,
                    int offset = 0);

    std::vector<Report> listReports(const std::string& category = "", int page = 1, int pageSize = 20);
    Report getReport(int64_t id);
    int64_t createReport(const Report& report);
    bool updateReport(int64_t id, const Report& report);
    bool deleteReport(int64_t id);

    std::vector<ChartDefinition> listCharts(int64_t reportId = 0);
    ChartDefinition getChart(int64_t id);
    int64_t createChart(const ChartDefinition& chart);
    bool updateChart(int64_t id, const ChartDefinition& chart);
    bool deleteChart(int64_t id);

    std::vector<Dashboard> listDashboards();
    Dashboard getDashboard(int64_t id);
    int64_t createDashboard(const Dashboard& dashboard);
    bool updateDashboard(int64_t id, const Dashboard& dashboard);
    bool deleteDashboard(int64_t id);

    bool isETLRunning() const { return etlRunning_.load(); }
    JsonValue getStats();

private:
    DataWarehouse();
    ~DataWarehouse();
    DataWarehouse(const DataWarehouse&) = delete;
    DataWarehouse& operator=(const DataWarehouse&) = delete;

    void etlLoop();
    bool runETLStep(const std::string& step);
    void scheduleETL();
    int64_t nextId();

    mutable std::mutex mutex_;
    std::thread etlThread_;
    std::condition_variable etlCv_;
    std::atomic<bool> etlRunning_{false};
    std::atomic<int64_t> nextId_{1};

    ETLConfig etlConfig_;
    int etlIntervalSeconds_ = 3600;
    std::map<std::string, DataSourceConfig> dataSources_;
    std::string dbPath_;
};

} // namespace Analytics
