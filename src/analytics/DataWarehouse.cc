#include "analytics/DataWarehouse.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

namespace Analytics {
namespace {

std::string trim(const std::string& input) {
    auto begin = std::find_if_not(input.begin(), input.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    auto end = std::find_if_not(input.rbegin(), input.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

std::string toLowerCopy(const std::string& value) {
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
}

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

std::vector<std::string> split(const std::string& input, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream ss(input);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        parts.push_back(trim(item));
    }
    return parts;
}

bool isNumber(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    char* end = nullptr;
    std::strtod(value.c_str(), &end);
    return end != value.c_str() && *end == '\0';
}

std::string normalizeKey(const std::vector<std::string>& values) {
    std::ostringstream oss;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            oss << '|';
        }
        oss << values[i];
    }
    return oss.str();
}

} // namespace

DataWarehouse& DataWarehouse::instance() {
    static DataWarehouse warehouse;
    return warehouse;
}

DataWarehouse::DataWarehouse() = default;

DataWarehouse::~DataWarehouse() {
    shutdown();
}

void DataWarehouse::init(const JsonValue& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    etlConfig_.enabled = config.get("enabled", true).asBool();
    etlConfig_.intervalSeconds = config.get("etl_interval", config.get("intervalSeconds", 3600)).asInt();
    etlConfig_.batchSize = config.get("batch_size", config.get("batchSize", 1000)).asInt();
    etlConfig_.removeDuplicates = config.get("remove_duplicates", config.get("removeDuplicates", true)).asBool();
    etlConfig_.missingValueStrategy = config.get("missing_value_strategy", config.get("missingValueStrategy", "null")).asString();
    etlConfig_.normalize = config.get("normalize", true).asBool();
    etlIntervalSeconds_ = std::max(1, etlConfig_.intervalSeconds);
    dbPath_ = config.get("database", config.get("dbPath", "analytics_dw.json")).asString();

    dataSources_.clear();
    const auto& dataSources = config["data_sources"];
    if (dataSources.isArray()) {
        for (const auto& node : dataSources) {
            DataSourceConfig source;
            source.name = node.get("name", "").asString();
            source.type = node.get("type", "memory").asString();
            source.connection = node.get("connection", "").asString();
            const auto& options = node["options"];
            if (options.isObject()) {
                for (const auto& key : options.getMemberNames()) {
                    source.options[key] = options[key].asString();
                }
            }
            if (!source.name.empty()) {
                dataSources_[source.name] = source;
            }
        }
    }

    std::filesystem::path dbFile(dbPath_);
    if (dbFile.has_parent_path()) {
        std::filesystem::create_directories(dbFile.parent_path());
    }
}

void DataWarehouse::shutdown() {
    stopETL();
}

void DataWarehouse::startETL() {
    if (etlRunning_.exchange(true)) {
        return;
    }
    etlThread_ = std::thread(&DataWarehouse::etlLoop, this);
}

void DataWarehouse::stopETL() {
    if (!etlRunning_.exchange(false)) {
        return;
    }
    etlCv_.notify_all();
    if (etlThread_.joinable()) {
        etlThread_.join();
    }
}

TableData DataWarehouse::collect(const DataSourceConfig& source,
                                 const std::vector<std::string>& columns,
                                 const std::string& filter) {
    TableData rows;
    const auto sourceType = toLowerCopy(source.type);

    if (sourceType == "memory") {
        RowData row;
        row["source"] = source.name;
        row["connection"] = source.connection;
        row["filter"] = filter;
        row["collected_at"] = nowString();
        for (const auto& column : columns) {
            if (!column.empty()) {
                row[column] = source.options.count(column) ? source.options.at(column) : (column + "_value");
            }
        }
        rows.push_back(std::move(row));
        return rows;
    }

    if (sourceType == "json_file" || sourceType == "file") {
        std::ifstream input(source.connection);
        if (!input.is_open()) {
            return rows;
        }
        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errors;
        if (!Json::parseFromStream(builder, input, &root, &errors) || !root.isArray()) {
            return rows;
        }
        for (const auto& item : root) {
            if (!item.isObject()) {
                continue;
            }
            RowData row;
            const auto members = item.getMemberNames();
            if (columns.empty()) {
                for (const auto& member : members) {
                    row[member] = item[member].isString() ? item[member].asString() : item[member].toStyledString();
                }
            } else {
                for (const auto& column : columns) {
                    if (item.isMember(column)) {
                        row[column] = item[column].isString() ? item[column].asString() : item[column].toStyledString();
                    }
                }
            }
            if (!filter.empty()) {
                const auto filterPos = filter.find('=');
                if (filterPos != std::string::npos) {
                    const auto key = trim(filter.substr(0, filterPos));
                    const auto expected = trim(filter.substr(filterPos + 1));
                    auto it = row.find(key);
                    if (it == row.end() || it->second != expected) {
                        continue;
                    }
                }
            }
            rows.push_back(std::move(row));
        }
    }

    return rows;
}

TableData DataWarehouse::clean(const TableData& raw, const ETLConfig& config) {
    TableData cleaned;
    std::set<std::string> seen;

    for (const auto& row : raw) {
        RowData normalizedRow;
        for (const auto& [key, value] : row) {
            std::string normalizedValue = config.normalize ? trim(value) : value;
            if (normalizedValue.empty()) {
                if (config.missingValueStrategy == "drop") {
                    normalizedRow.clear();
                    break;
                }
                if (config.missingValueStrategy == "fill_default") {
                    normalizedValue = "0";
                }
            }
            normalizedRow[key] = normalizedValue;
        }
        if (normalizedRow.empty()) {
            continue;
        }

        if (config.removeDuplicates) {
            std::vector<std::string> values;
            for (const auto& [key, value] : normalizedRow) {
                values.push_back(key + "=" + value);
            }
            const auto signature = normalizeKey(values);
            if (!seen.insert(signature).second) {
                continue;
            }
        }

        cleaned.push_back(std::move(normalizedRow));
        if (static_cast<int>(cleaned.size()) >= config.batchSize) {
            break;
        }
    }

    return cleaned;
}

TableData DataWarehouse::transform(const TableData& cleaned, const TransformConfig& config) {
    if (config.aggregations.empty()) {
        TableData result = cleaned;
        if (!config.orderBy.empty()) {
            std::sort(result.begin(), result.end(), [&](const RowData& lhs, const RowData& rhs) {
                for (const auto& field : config.orderBy) {
                    const auto lv = lhs.count(field) ? lhs.at(field) : std::string();
                    const auto rv = rhs.count(field) ? rhs.at(field) : std::string();
                    if (lv == rv) {
                        continue;
                    }
                    const bool asc = toLowerCopy(config.orderDirection) != "desc";
                    return asc ? lv < rv : lv > rv;
                }
                return false;
            });
        }
        if (config.limit > 0 && static_cast<int>(result.size()) > config.limit) {
            result.resize(static_cast<size_t>(config.limit));
        }
        return result;
    }

    struct AggregateState {
        RowData dimensions;
        std::map<std::string, double> sums;
        std::map<std::string, int64_t> counts;
        std::map<std::string, double> mins;
        std::map<std::string, double> maxs;
    };

    std::map<std::string, AggregateState> grouped;
    for (const auto& row : cleaned) {
        std::vector<std::string> keyValues;
        RowData dimensions;
        for (const auto& groupField : config.groupBy) {
            const auto value = row.count(groupField) ? row.at(groupField) : std::string();
            keyValues.push_back(value);
            dimensions[groupField] = value;
        }
        const auto groupKey = normalizeKey(keyValues);
        auto& state = grouped[groupKey];
        if (state.dimensions.empty()) {
            state.dimensions = std::move(dimensions);
        }

        for (const auto& agg : config.aggregations) {
            const auto rawValue = row.count(agg.field) ? row.at(agg.field) : std::string();
            const double numeric = isNumber(rawValue) ? std::stod(rawValue) : 0.0;
            const auto alias = agg.alias.empty() ? agg.field + "_" + agg.function : agg.alias;
            const auto func = toLowerCopy(agg.function);
            if (func == "count") {
                state.counts[alias] += 1;
            } else if (func == "sum" || func == "avg") {
                state.sums[alias] += numeric;
                state.counts[alias] += 1;
            } else if (func == "min") {
                if (!state.mins.count(alias) || numeric < state.mins[alias]) {
                    state.mins[alias] = numeric;
                }
            } else if (func == "max") {
                if (!state.maxs.count(alias) || numeric > state.maxs[alias]) {
                    state.maxs[alias] = numeric;
                }
            }
        }
    }

    TableData transformed;
    for (auto& [key, state] : grouped) {
        (void)key;
        RowData row = state.dimensions;
        for (const auto& agg : config.aggregations) {
            const auto alias = agg.alias.empty() ? agg.field + "_" + agg.function : agg.alias;
            const auto func = toLowerCopy(agg.function);
            std::ostringstream value;
            if (func == "count") {
                value << state.counts[alias];
            } else if (func == "sum") {
                value << state.sums[alias];
            } else if (func == "avg") {
                const auto count = state.counts[alias];
                value << (count == 0 ? 0.0 : state.sums[alias] / static_cast<double>(count));
            } else if (func == "min") {
                value << state.mins[alias];
            } else if (func == "max") {
                value << state.maxs[alias];
            }
            row[alias] = value.str();
        }
        transformed.push_back(std::move(row));
    }

    if (!config.orderBy.empty()) {
        std::sort(transformed.begin(), transformed.end(), [&](const RowData& lhs, const RowData& rhs) {
            for (const auto& field : config.orderBy) {
                const auto lv = lhs.count(field) ? lhs.at(field) : std::string();
                const auto rv = rhs.count(field) ? rhs.at(field) : std::string();
                if (lv == rv) {
                    continue;
                }
                const bool asc = toLowerCopy(config.orderDirection) != "desc";
                return asc ? lv < rv : lv > rv;
            }
            return false;
        });
    }
    if (config.limit > 0 && static_cast<int>(transformed.size()) > config.limit) {
        transformed.resize(static_cast<size_t>(config.limit));
    }
    return transformed;
}

bool DataWarehouse::load(const TableData& data, const LoadConfig& config) {
    if (config.targetTable.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    Json::Value root(Json::objectValue);
    {
        std::ifstream input(dbPath_);
        if (input.is_open()) {
            Json::CharReaderBuilder builder;
            std::string errors;
            Json::parseFromStream(builder, input, &root, &errors);
            if (!root.isObject()) {
                root = Json::Value(Json::objectValue);
            }
        }
    }

    Json::Value table(Json::arrayValue);
    const auto mode = toLowerCopy(config.mode);
    if (mode != "replace" && root.isMember(config.targetTable) && root[config.targetTable].isArray()) {
        table = root[config.targetTable];
    }

    if (mode == "upsert" && !config.uniqueKey.empty()) {
        std::map<std::string, Json::Value> byKey;
        for (const auto& existing : table) {
            if (existing.isObject() && existing.isMember(config.uniqueKey)) {
                byKey[existing[config.uniqueKey].asString()] = existing;
            }
        }
        for (const auto& row : data) {
            Json::Value jsonRow(Json::objectValue);
            for (const auto& [key, value] : row) {
                jsonRow[key] = value;
            }
            const auto key = row.count(config.uniqueKey) ? row.at(config.uniqueKey) : std::string();
            byKey[key] = jsonRow;
        }
        table = Json::Value(Json::arrayValue);
        for (const auto& [key, value] : byKey) {
            (void)key;
            table.append(value);
        }
    } else {
        for (const auto& row : data) {
            Json::Value jsonRow(Json::objectValue);
            for (const auto& [key, value] : row) {
                jsonRow[key] = value;
            }
            table.append(jsonRow);
        }
    }

    root[config.targetTable] = table;
    std::ofstream output(dbPath_, std::ios::trunc);
    output << root;
    return output.good();
}

TableData DataWarehouse::query(const std::string& sql) {
    const auto normalized = trim(sql);
    if (normalized.empty()) {
        return {};
    }

    const auto lower = toLowerCopy(normalized);
    if (lower.rfind("select * from ", 0) == 0) {
        const auto remainder = normalized.substr(std::string("select * from ").size());
        auto parts = split(remainder, ' ');
        if (!parts.empty()) {
            return query(parts.front(), {}, {}, 0, 0);
        }
    }
    return {};
}

TableData DataWarehouse::query(const std::string& table,
                               const std::map<std::string, std::string>& conditions,
                               const std::vector<std::string>& orderBy,
                               int limit,
                               int offset) {
    std::lock_guard<std::mutex> lock(mutex_);
    TableData result;

    std::ifstream input(dbPath_);
    if (!input.is_open()) {
        return result;
    }
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, input, &root, &errors) || !root.isObject()) {
        return result;
    }
    const auto& data = root[table];
    if (!data.isArray()) {
        return result;
    }

    for (const auto& item : data) {
        if (!item.isObject()) {
            continue;
        }
        RowData row;
        bool matches = true;
        for (const auto& member : item.getMemberNames()) {
            row[member] = item[member].isString() ? item[member].asString() : item[member].toStyledString();
        }
        for (const auto& [key, expected] : conditions) {
            auto it = row.find(key);
            if (it == row.end() || it->second != expected) {
                matches = false;
                break;
            }
        }
        if (matches) {
            result.push_back(std::move(row));
        }
    }

    if (!orderBy.empty()) {
        std::sort(result.begin(), result.end(), [&](const RowData& lhs, const RowData& rhs) {
            for (const auto& field : orderBy) {
                const auto lv = lhs.count(field) ? lhs.at(field) : std::string();
                const auto rv = rhs.count(field) ? rhs.at(field) : std::string();
                if (lv != rv) {
                    return lv < rv;
                }
            }
            return false;
        });
    }

    const auto start = static_cast<size_t>(std::max(0, offset));
    if (start >= result.size()) {
        return {};
    }
    auto end = result.size();
    if (limit > 0) {
        end = std::min(result.size(), start + static_cast<size_t>(limit));
    }
    return TableData(result.begin() + static_cast<std::ptrdiff_t>(start), result.begin() + static_cast<std::ptrdiff_t>(end));
}

std::vector<Report> DataWarehouse::listReports(const std::string& category, int page, int pageSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Report> reports;
    std::ifstream input(dbPath_);
    if (!input.is_open()) {
        return reports;
    }
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, input, &root, &errors)) {
        return reports;
    }
    const auto& list = root["reports"];
    if (!list.isArray()) {
        return reports;
    }
    for (const auto& node : list) {
        Report report;
        report.id = node.get("id", 0).asInt64();
        report.name = node.get("name", "").asString();
        report.description = node.get("description", "").asString();
        report.category = node.get("category", "").asString();
        report.templateName = node.get("templateName", "").asString();
        report.createdBy = node.get("createdBy", "system").asString();
        report.createdAt = node.get("createdAt", "").asString();
        report.updatedAt = node.get("updatedAt", "").asString();
        report.status = node.get("status", 0).asInt();
        if (!category.empty() && report.category != category) {
            continue;
        }
        if (node["parameters"].isObject()) {
            for (const auto& key : node["parameters"].getMemberNames()) {
                report.parameters[key] = node["parameters"][key].asString();
            }
        }
        reports.push_back(std::move(report));
    }
    const auto start = static_cast<size_t>(std::max(0, (page - 1) * pageSize));
    if (start >= reports.size()) {
        return {};
    }
    const auto end = std::min(reports.size(), start + static_cast<size_t>(pageSize));
    return std::vector<Report>(reports.begin() + static_cast<std::ptrdiff_t>(start), reports.begin() + static_cast<std::ptrdiff_t>(end));
}

Report DataWarehouse::getReport(int64_t id) {
    for (auto& report : listReports("", 1, 10000)) {
        if (report.id == id) {
            return report;
        }
    }
    return {};
}

int64_t DataWarehouse::createReport(const Report& report) {
    std::lock_guard<std::mutex> lock(mutex_);
    Json::Value root(Json::objectValue);
    {
        std::ifstream input(dbPath_);
        if (input.is_open()) {
            Json::CharReaderBuilder builder;
            std::string errors;
            Json::parseFromStream(builder, input, &root, &errors);
            if (!root.isObject()) {
                root = Json::Value(Json::objectValue);
            }
        }
    }
    Json::Value node(Json::objectValue);
    const auto id = nextId();
    node["id"] = static_cast<Json::Int64>(id);
    node["name"] = report.name;
    node["description"] = report.description;
    node["category"] = report.category;
    node["templateName"] = report.templateName;
    node["createdBy"] = report.createdBy.empty() ? "system" : report.createdBy;
    node["createdAt"] = report.createdAt.empty() ? nowString() : report.createdAt;
    node["updatedAt"] = report.updatedAt.empty() ? node["createdAt"].asString() : report.updatedAt;
    node["status"] = report.status;
    Json::Value parameters(Json::objectValue);
    for (const auto& [key, value] : report.parameters) {
        parameters[key] = value;
    }
    node["parameters"] = parameters;
    root["reports"].append(node);
    std::ofstream output(dbPath_, std::ios::trunc);
    output << root;
    return id;
}

bool DataWarehouse::updateReport(int64_t id, const Report& report) {
    std::lock_guard<std::mutex> lock(mutex_);
    Json::Value root;
    std::ifstream input(dbPath_);
    if (!input.is_open()) {
        return false;
    }
    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, input, &root, &errors) || !root["reports"].isArray()) {
        return false;
    }
    bool updated = false;
    for (auto& node : root["reports"]) {
        if (node.get("id", 0).asInt64() != id) {
            continue;
        }
        node["name"] = report.name;
        node["description"] = report.description;
        node["category"] = report.category;
        node["templateName"] = report.templateName;
        node["updatedAt"] = nowString();
        node["status"] = report.status;
        Json::Value parameters(Json::objectValue);
        for (const auto& [key, value] : report.parameters) {
            parameters[key] = value;
        }
        node["parameters"] = parameters;
        updated = true;
        break;
    }
    if (!updated) {
        return false;
    }
    std::ofstream output(dbPath_, std::ios::trunc);
    output << root;
    return output.good();
}

bool DataWarehouse::deleteReport(int64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    Json::Value root;
    std::ifstream input(dbPath_);
    if (!input.is_open()) {
        return false;
    }
    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, input, &root, &errors) || !root["reports"].isArray()) {
        return false;
    }
    Json::Value filtered(Json::arrayValue);
    bool removed = false;
    for (const auto& node : root["reports"]) {
        if (node.get("id", 0).asInt64() == id) {
            removed = true;
            continue;
        }
        filtered.append(node);
    }
    if (!removed) {
        return false;
    }
    root["reports"] = filtered;
    std::ofstream output(dbPath_, std::ios::trunc);
    output << root;
    return output.good();
}

std::vector<ChartDefinition> DataWarehouse::listCharts(int64_t reportId) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ChartDefinition> charts;
    std::ifstream input(dbPath_);
    if (!input.is_open()) {
        return charts;
    }
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, input, &root, &errors) || !root["charts"].isArray()) {
        return charts;
    }
    for (const auto& node : root["charts"]) {
        ChartDefinition chart;
        chart.id = node.get("id", 0).asInt64();
        chart.reportId = node.get("reportId", "").asString();
        if (reportId > 0 && chart.reportId != std::to_string(reportId)) {
            continue;
        }
        chart.name = node.get("name", "").asString();
        chart.type = node.get("type", "table").asString();
        chart.query = node.get("query", "").asString();
        chart.xAxis = node.get("xAxis", "").asString();
        chart.yAxis = node.get("yAxis", "").asString();
        if (node["series"].isArray()) {
            for (const auto& series : node["series"]) {
                chart.series.push_back(series.asString());
            }
        }
        if (node["options"].isObject()) {
            for (const auto& key : node["options"].getMemberNames()) {
                chart.options[key] = node["options"][key].asString();
            }
        }
        charts.push_back(std::move(chart));
    }
    return charts;
}

ChartDefinition DataWarehouse::getChart(int64_t id) {
    for (auto& chart : listCharts(0)) {
        if (chart.id == id) {
            return chart;
        }
    }
    return {};
}

int64_t DataWarehouse::createChart(const ChartDefinition& chart) {
    std::lock_guard<std::mutex> lock(mutex_);
    Json::Value root(Json::objectValue);
    {
        std::ifstream input(dbPath_);
        if (input.is_open()) {
            Json::CharReaderBuilder builder;
            std::string errors;
            Json::parseFromStream(builder, input, &root, &errors);
            if (!root.isObject()) {
                root = Json::Value(Json::objectValue);
            }
        }
    }
    const auto id = nextId();
    Json::Value node(Json::objectValue);
    node["id"] = static_cast<Json::Int64>(id);
    node["reportId"] = chart.reportId;
    node["name"] = chart.name;
    node["type"] = chart.type;
    node["query"] = chart.query;
    node["xAxis"] = chart.xAxis;
    node["yAxis"] = chart.yAxis;
    Json::Value series(Json::arrayValue);
    for (const auto& item : chart.series) {
        series.append(item);
    }
    node["series"] = series;
    Json::Value options(Json::objectValue);
    for (const auto& [key, value] : chart.options) {
        options[key] = value;
    }
    node["options"] = options;
    root["charts"].append(node);
    std::ofstream output(dbPath_, std::ios::trunc);
    output << root;
    return id;
}

bool DataWarehouse::updateChart(int64_t id, const ChartDefinition& chart) {
    std::lock_guard<std::mutex> lock(mutex_);
    Json::Value root;
    std::ifstream input(dbPath_);
    if (!input.is_open()) {
        return false;
    }
    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, input, &root, &errors) || !root["charts"].isArray()) {
        return false;
    }
    bool updated = false;
    for (auto& node : root["charts"]) {
        if (node.get("id", 0).asInt64() != id) {
            continue;
        }
        node["reportId"] = chart.reportId;
        node["name"] = chart.name;
        node["type"] = chart.type;
        node["query"] = chart.query;
        node["xAxis"] = chart.xAxis;
        node["yAxis"] = chart.yAxis;
        Json::Value series(Json::arrayValue);
        for (const auto& item : chart.series) {
            series.append(item);
        }
        node["series"] = series;
        Json::Value options(Json::objectValue);
        for (const auto& [key, value] : chart.options) {
            options[key] = value;
        }
        node["options"] = options;
        updated = true;
        break;
    }
    if (!updated) {
        return false;
    }
    std::ofstream output(dbPath_, std::ios::trunc);
    output << root;
    return output.good();
}

bool DataWarehouse::deleteChart(int64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    Json::Value root;
    std::ifstream input(dbPath_);
    if (!input.is_open()) {
        return false;
    }
    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, input, &root, &errors) || !root["charts"].isArray()) {
        return false;
    }
    Json::Value filtered(Json::arrayValue);
    bool removed = false;
    for (const auto& node : root["charts"]) {
        if (node.get("id", 0).asInt64() == id) {
            removed = true;
            continue;
        }
        filtered.append(node);
    }
    if (!removed) {
        return false;
    }
    root["charts"] = filtered;
    std::ofstream output(dbPath_, std::ios::trunc);
    output << root;
    return output.good();
}

std::vector<Dashboard> DataWarehouse::listDashboards() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Dashboard> dashboards;
    std::ifstream input(dbPath_);
    if (!input.is_open()) {
        return dashboards;
    }
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, input, &root, &errors) || !root["dashboards"].isArray()) {
        return dashboards;
    }
    for (const auto& node : root["dashboards"]) {
        Dashboard dashboard;
        dashboard.id = node.get("id", 0).asInt64();
        dashboard.name = node.get("name", "").asString();
        dashboard.description = node.get("description", "").asString();
        dashboard.layout = node.get("layout", "").asString();
        if (node["chartIds"].isArray()) {
            for (const auto& id : node["chartIds"]) {
                dashboard.chartIds.push_back(id.asInt64());
            }
        }
        dashboards.push_back(std::move(dashboard));
    }
    return dashboards;
}

Dashboard DataWarehouse::getDashboard(int64_t id) {
    for (auto& dashboard : listDashboards()) {
        if (dashboard.id == id) {
            return dashboard;
        }
    }
    return {};
}

int64_t DataWarehouse::createDashboard(const Dashboard& dashboard) {
    std::lock_guard<std::mutex> lock(mutex_);
    Json::Value root(Json::objectValue);
    {
        std::ifstream input(dbPath_);
        if (input.is_open()) {
            Json::CharReaderBuilder builder;
            std::string errors;
            Json::parseFromStream(builder, input, &root, &errors);
            if (!root.isObject()) {
                root = Json::Value(Json::objectValue);
            }
        }
    }
    const auto id = nextId();
    Json::Value node(Json::objectValue);
    node["id"] = static_cast<Json::Int64>(id);
    node["name"] = dashboard.name;
    node["description"] = dashboard.description;
    node["layout"] = dashboard.layout;
    Json::Value chartIds(Json::arrayValue);
    for (auto chartId : dashboard.chartIds) {
        chartIds.append(static_cast<Json::Int64>(chartId));
    }
    node["chartIds"] = chartIds;
    root["dashboards"].append(node);
    std::ofstream output(dbPath_, std::ios::trunc);
    output << root;
    return id;
}

bool DataWarehouse::updateDashboard(int64_t id, const Dashboard& dashboard) {
    std::lock_guard<std::mutex> lock(mutex_);
    Json::Value root;
    std::ifstream input(dbPath_);
    if (!input.is_open()) {
        return false;
    }
    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, input, &root, &errors) || !root["dashboards"].isArray()) {
        return false;
    }
    bool updated = false;
    for (auto& node : root["dashboards"]) {
        if (node.get("id", 0).asInt64() != id) {
            continue;
        }
        node["name"] = dashboard.name;
        node["description"] = dashboard.description;
        node["layout"] = dashboard.layout;
        Json::Value chartIds(Json::arrayValue);
        for (auto chartId : dashboard.chartIds) {
            chartIds.append(static_cast<Json::Int64>(chartId));
        }
        node["chartIds"] = chartIds;
        updated = true;
        break;
    }
    if (!updated) {
        return false;
    }
    std::ofstream output(dbPath_, std::ios::trunc);
    output << root;
    return output.good();
}

bool DataWarehouse::deleteDashboard(int64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    Json::Value root;
    std::ifstream input(dbPath_);
    if (!input.is_open()) {
        return false;
    }
    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, input, &root, &errors) || !root["dashboards"].isArray()) {
        return false;
    }
    Json::Value filtered(Json::arrayValue);
    bool removed = false;
    for (const auto& node : root["dashboards"]) {
        if (node.get("id", 0).asInt64() == id) {
            removed = true;
            continue;
        }
        filtered.append(node);
    }
    if (!removed) {
        return false;
    }
    root["dashboards"] = filtered;
    std::ofstream output(dbPath_, std::ios::trunc);
    output << root;
    return output.good();
}

JsonValue DataWarehouse::getStats() {
    JsonValue stats(Json::objectValue);
    const auto reports = listReports("", 1, 10000);
    const auto charts = listCharts(0);
    const auto dashboards = listDashboards();

    stats["reportCount"] = static_cast<Json::UInt64>(reports.size());
    stats["chartCount"] = static_cast<Json::UInt64>(charts.size());
    stats["dashboardCount"] = static_cast<Json::UInt64>(dashboards.size());
    stats["etlRunning"] = etlRunning_.load();
    stats["dataSourceCount"] = static_cast<Json::UInt64>(dataSources_.size());
    stats["dbPath"] = dbPath_;
    stats["lastComputedAt"] = nowString();
    return stats;
}

void DataWarehouse::etlLoop() {
    std::unique_lock<std::mutex> lock(mutex_, std::defer_lock);
    while (etlRunning_.load()) {
        runETLStep("collect");
        runETLStep("clean");
        runETLStep("transform");
        runETLStep("load");

        lock.lock();
        etlCv_.wait_for(lock, std::chrono::seconds(etlIntervalSeconds_), [this]() {
            return !etlRunning_.load();
        });
        lock.unlock();
    }
}

bool DataWarehouse::runETLStep(const std::string& step) {
    (void)step;
    return true;
}

void DataWarehouse::scheduleETL() {
    if (!etlRunning_.load()) {
        startETL();
    }
}

int64_t DataWarehouse::nextId() {
    return nextId_.fetch_add(1);
}

} // namespace Analytics
