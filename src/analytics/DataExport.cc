#include "analytics/DataExport.h"

#include "analytics/DataWarehouse.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace Analytics {
namespace {

std::string nowString() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string escapeCsv(const std::string& value) {
    if (value.find_first_of(",\"\n") == std::string::npos) {
        return value;
    }
    std::string escaped = "\"";
    for (char ch : value) {
        if (ch == '\"') {
            escaped += "\"\"";
        } else {
            escaped += ch;
        }
    }
    escaped += '\"';
    return escaped;
}

} // namespace

DataExport& DataExport::instance() {
    static DataExport exporter;
    return exporter;
}

DataExport::DataExport() = default;

DataExport::~DataExport() {
    shutdown();
}

void DataExport::init(const JsonValue& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    exportDir_ = config.get("export_dir", config.get("exportDir", "exports")).asString();
    dbPath_ = config.get("database", config.get("dbPath", "analytics_exports.json")).asString();
    std::filesystem::create_directories(exportDir_);
}

void DataExport::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }
    cv_.notify_all();
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

bool DataExport::exportToCSV(const std::string& query,
                             const std::string& outputPath,
                             const std::vector<ExportColumn>& columns,
                             const std::string& delimiter) {
    return exportToCSVImpl(query, outputPath, columns, delimiter);
}

bool DataExport::exportToExcel(const std::string& query,
                               const std::string& outputPath,
                               const std::vector<ExportColumn>& columns) {
    return exportToCSVImpl(query, outputPath, columns, ",");
}

bool DataExport::exportToJSON(const std::string& query, const std::string& outputPath) {
    return exportToJSONImpl(query, outputPath);
}

bool DataExport::exportToPdf(const std::string& query,
                             const std::string& outputPath,
                             const std::string& title) {
    const auto rows = DataWarehouse::instance().query(query, {}, {}, 0, 0);
    std::filesystem::create_directories(std::filesystem::path(outputPath).parent_path());
    std::ofstream output(outputPath, std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    output << title << "\n\n";
    for (const auto& row : rows) {
        for (const auto& [key, value] : row) {
            output << key << ": " << value << "\n";
        }
        output << "-------------------------\n";
    }
    return output.good();
}

int64_t DataExport::submitExportTask(const std::string& name,
                                     const std::string& query,
                                     const std::string& format,
                                     const std::string& createdBy,
                                     const std::vector<ExportColumn>& columns) {
    (void)columns;
    ExportTask task;
    task.id = nextTaskId_.fetch_add(1);
    task.name = name;
    task.query = query;
    task.format = format;
    task.createdBy = createdBy;
    task.createdAt = nowString();
    task.filePath = (std::filesystem::path(exportDir_) / ("export_" + std::to_string(task.id) + "." + format)).string();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_[task.id] = task;
        taskQueue_.push(task);
        if (!running_.load()) {
            running_ = true;
            workerThread_ = std::thread(&DataExport::workerLoop, this);
        }
    }
    cv_.notify_one();
    return task.id;
}

ExportTask DataExport::getTaskStatus(int64_t taskId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tasks_.find(taskId);
    return it == tasks_.end() ? ExportTask{} : it->second;
}

std::vector<ExportTask> DataExport::listTasks(const std::string& createdBy, int page, int pageSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ExportTask> list;
    for (const auto& [id, task] : tasks_) {
        (void)id;
        if (!createdBy.empty() && task.createdBy != createdBy) {
            continue;
        }
        list.push_back(task);
    }
    const auto start = static_cast<size_t>(std::max(0, (page - 1) * pageSize));
    if (start >= list.size()) {
        return {};
    }
    const auto end = std::min(list.size(), start + static_cast<size_t>(pageSize));
    return std::vector<ExportTask>(list.begin() + static_cast<std::ptrdiff_t>(start),
                                   list.begin() + static_cast<std::ptrdiff_t>(end));
}

bool DataExport::cancelTask(int64_t taskId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tasks_.find(taskId);
    if (it == tasks_.end() || it->second.status >= 2) {
        return false;
    }
    it->second.status = 3;
    it->second.errorMessage = "cancelled";
    it->second.completedAt = nowString();
    return true;
}

bool DataExport::deleteTask(int64_t taskId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tasks_.find(taskId);
    if (it == tasks_.end()) {
        return false;
    }
    if (!it->second.filePath.empty()) {
        std::error_code ec;
        std::filesystem::remove(it->second.filePath, ec);
    }
    tasks_.erase(it);
    return true;
}

void DataExport::workerLoop() {
    while (true) {
        ExportTask task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() {
                return !taskQueue_.empty() || !running_.load();
            });
            if (!running_.load() && taskQueue_.empty()) {
                break;
            }
            task = taskQueue_.front();
            taskQueue_.pop();
            auto it = tasks_.find(task.id);
            if (it != tasks_.end() && it->second.status == 3) {
                continue;
            }
            tasks_[task.id].status = 1;
        }
        processTask(task);
    }
}

void DataExport::processTask(const ExportTask& task) {
    bool success = false;
    auto format = task.format;
    if (format == "csv") {
        success = writeCSV(task);
    } else if (format == "json") {
        success = writeJSON(task);
    } else if (format == "excel") {
        success = exportToExcel(task.query, task.filePath, {});
    } else if (format == "pdf") {
        success = exportToPdf(task.query, task.filePath, task.name);
    }

    const auto rows = DataWarehouse::instance().query(task.query, {}, {}, 0, 0);

    std::lock_guard<std::mutex> lock(mutex_);
    auto& current = tasks_[task.id];
    current.totalRows = static_cast<int64_t>(rows.size());
    current.exportedRows = success ? current.totalRows : 0;
    current.status = success ? 2 : 3;
    current.completedAt = nowString();
    current.fileSize = success && std::filesystem::exists(current.filePath)
        ? static_cast<int64_t>(std::filesystem::file_size(current.filePath))
        : 0;
    if (!success && current.errorMessage.empty()) {
        current.errorMessage = "export failed";
    }
}

bool DataExport::writeCSV(const ExportTask& task) {
    return exportToCSVImpl(task.query, task.filePath, {}, ",");
}

bool DataExport::writeJSON(const ExportTask& task) {
    return exportToJSONImpl(task.query, task.filePath);
}

bool DataExport::exportToCSVImpl(const std::string& query,
                                 const std::string& outputPath,
                                 const std::vector<ExportColumn>& columns,
                                 const std::string& delimiter) {
    const auto rows = DataWarehouse::instance().query(query, {}, {}, 0, 0);
    std::filesystem::path outputFile(outputPath);
    if (outputFile.has_parent_path()) {
        std::filesystem::create_directories(outputFile.parent_path());
    }
    std::ofstream output(outputPath, std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    if (rows.empty()) {
        return true;
    }

    std::vector<std::string> fields;
    if (!columns.empty()) {
        for (const auto& column : columns) {
            if (!column.hidden) {
                fields.push_back(column.field);
            }
        }
    } else {
        for (const auto& [field, value] : rows.front()) {
            (void)value;
            fields.push_back(field);
        }
    }

    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            output << delimiter;
        }
        output << escapeCsv(fields[i]);
    }
    output << '\n';

    for (const auto& row : rows) {
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i > 0) {
                output << delimiter;
            }
            auto it = row.find(fields[i]);
            output << escapeCsv(it == row.end() ? std::string() : it->second);
        }
        output << '\n';
    }
    return output.good();
}

bool DataExport::exportToJSONImpl(const std::string& query, const std::string& outputPath) {
    const auto rows = DataWarehouse::instance().query(query, {}, {}, 0, 0);
    std::filesystem::path outputFile(outputPath);
    if (outputFile.has_parent_path()) {
        std::filesystem::create_directories(outputFile.parent_path());
    }

    JsonValue root(Json::arrayValue);
    for (const auto& row : rows) {
        JsonValue item(Json::objectValue);
        for (const auto& [key, value] : row) {
            item[key] = value;
        }
        root.append(item);
    }

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    std::ofstream output(outputPath, std::ios::trunc);
    output << Json::writeString(builder, root);
    return output.good();
}

} // namespace Analytics
