#pragma once

#include <json/json.h>

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace Analytics {

using JsonValue = Json::Value;

struct ExportTask {
    int64_t id = 0;
    std::string name;
    std::string query;
    std::string format;       // csv | excel | json | pdf
    std::string filePath;
    int64_t totalRows = 0;
    int64_t exportedRows = 0;
    int status = 0;           // 0=pending, 1=running, 2=completed, 3=failed
    std::string errorMessage;
    std::string createdBy;
    std::string createdAt;
    std::string completedAt;
    int64_t fileSize = 0;
};

struct ExportColumn {
    std::string field;
    std::string label;
    std::string format;       // string | number | date | bool
    int width = 0;
    bool hidden = false;
};

class DataExport {
public:
    static DataExport& instance();

    void init(const JsonValue& config);
    void shutdown();

    bool exportToCSV(const std::string& query, const std::string& outputPath,
                     const std::vector<ExportColumn>& columns = {},
                     const std::string& delimiter = ",");
    bool exportToExcel(const std::string& query, const std::string& outputPath,
                       const std::vector<ExportColumn>& columns = {});
    bool exportToJSON(const std::string& query, const std::string& outputPath);
    bool exportToPdf(const std::string& query, const std::string& outputPath,
                     const std::string& title = "");

    int64_t submitExportTask(const std::string& name,
                             const std::string& query,
                             const std::string& format,
                             const std::string& createdBy,
                             const std::vector<ExportColumn>& columns = {});
    ExportTask getTaskStatus(int64_t taskId);
    std::vector<ExportTask> listTasks(const std::string& createdBy = "",
                                      int page = 1, int pageSize = 20);
    bool cancelTask(int64_t taskId);
    bool deleteTask(int64_t taskId);

    std::string getExportDir() const { return exportDir_; }

private:
    DataExport();
    ~DataExport();
    DataExport(const DataExport&) = delete;
    DataExport& operator=(const DataExport&) = delete;

    void workerLoop();
    void processTask(const ExportTask& task);
    bool writeCSV(const ExportTask& task);
    bool writeJSON(const ExportTask& task);

    bool exportToCSVImpl(const std::string& query,
                         const std::string& outputPath,
                         const std::vector<ExportColumn>& columns,
                         const std::string& delimiter);
    bool exportToJSONImpl(const std::string& query,
                          const std::string& outputPath);

    mutable std::mutex mutex_;
    std::thread workerThread_;
    std::condition_variable cv_;
    std::queue<ExportTask> taskQueue_;
    std::atomic<bool> running_{false};
    std::atomic<int64_t> nextTaskId_{1};
    std::map<int64_t, ExportTask> tasks_;

    std::string exportDir_ = "exports";
    std::string dbPath_;
};

} // namespace Analytics
