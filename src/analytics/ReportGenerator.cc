#include "analytics/ReportGenerator.h"

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

std::string replaceAll(std::string text, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return text;
    }
    size_t start = 0;
    while ((start = text.find(from, start)) != std::string::npos) {
        text.replace(start, from.size(), to);
        start += to.size();
    }
    return text;
}

} // namespace

ReportGenerator& ReportGenerator::instance() {
    static ReportGenerator generator;
    return generator;
}

void ReportGenerator::init(const JsonValue& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    outputDir_ = config.get("output_dir", config.get("outputDir", "reports")).asString();
    templateDir_ = config.get("template_dir", config.get("templateDir", "templates")).asString();
    dbPath_ = config.get("database", config.get("dbPath", "analytics_reports.json")).asString();

    std::filesystem::create_directories(outputDir_);
    std::filesystem::create_directories(templateDir_);
}

void ReportGenerator::shutdown() {
    stopScheduler();
}

bool ReportGenerator::registerTemplate(const ReportTemplate& tmpl) {
    if (tmpl.name.empty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    templates_[tmpl.name] = tmpl;
    return true;
}

ReportTemplate ReportGenerator::getTemplate(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = templates_.find(name);
    return it == templates_.end() ? ReportTemplate{} : it->second;
}

std::vector<std::string> ReportGenerator::listTemplates() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    for (const auto& [name, tmpl] : templates_) {
        (void)tmpl;
        names.push_back(name);
    }
    return names;
}

bool ReportGenerator::deleteTemplate(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    return templates_.erase(name) > 0;
}

ReportResult ReportGenerator::generate(const std::string& templateName,
                                       const std::map<std::string, std::string>& params,
                                       const std::string& format) {
    ReportResult result;
    result.name = templateName;
    result.format = format;
    result.generatedAt = nowString();

    const auto tmpl = getTemplate(templateName);
    if (tmpl.name.empty()) {
        result.errorMessage = "template not found";
        return result;
    }

    const auto data = previewData(templateName, params);
    const auto normalizedFormat = format.empty() ? std::string("html") : format;
    std::string content;

    if (normalizedFormat == "html") {
        content = renderHtml(tmpl, params, data);
    } else if (normalizedFormat == "csv") {
        content = renderCsv(tmpl, params, data);
    } else if (normalizedFormat == "json") {
        content = renderJson(tmpl, params, data);
    } else {
        result.errorMessage = "unsupported format: " + normalizedFormat;
        return result;
    }

    result.reportId = nextGeneratedId_.fetch_add(1);
    result.filePath = getReportPath(result.reportId) + "." + normalizedFormat;
    std::ofstream output(result.filePath, std::ios::trunc | std::ios::binary);
    output << content;
    if (!output.good()) {
        result.errorMessage = "failed to write report file";
        return result;
    }

    result.success = true;
    result.fileSize = static_cast<int64_t>(content.size());
    result.metadata["template"] = templateName;
    result.metadata["rowCount"] = std::to_string(data.get("sections", JsonValue(Json::arrayValue)).size());
    saveReport(result);
    return result;
}

ReportResult ReportGenerator::generate(int64_t reportId,
                                       const std::map<std::string, std::string>& params,
                                       const std::string& format) {
    auto report = DataWarehouse::instance().getReport(reportId);
    if (report.id == 0) {
        ReportResult result;
        result.reportId = reportId;
        result.format = format;
        result.generatedAt = nowString();
        result.errorMessage = "report not found";
        return result;
    }

    auto mergedParams = report.parameters;
    for (const auto& [key, value] : params) {
        mergedParams[key] = value;
    }

    auto result = generate(report.templateName, mergedParams, format);
    result.reportId = reportId;
    result.name = report.name.empty() ? result.name : report.name;
    return result;
}

JsonValue ReportGenerator::previewData(const std::string& templateName,
                                       const std::map<std::string, std::string>& params) {
    JsonValue data(Json::objectValue);
    const auto tmpl = getTemplate(templateName);
    if (tmpl.name.empty()) {
        return data;
    }

    JsonValue sections(Json::arrayValue);
    for (const auto& section : tmpl.sections) {
        JsonValue sectionJson(Json::objectValue);
        sectionJson["name"] = section.name;
        sectionJson["title"] = section.title;

        JsonValue metrics(Json::arrayValue);
        for (const auto& metric : section.metrics) {
            JsonValue metricJson(Json::objectValue);
            const auto expanded = expandQuery(metric.query, params);
            const auto queryResult = executeQuery(expanded);
            metricJson["name"] = metric.name;
            metricJson["label"] = metric.label;
            metricJson["format"] = metric.format;
            metricJson["unit"] = metric.unit;
            metricJson["query"] = expanded;
            metricJson["data"] = queryResult;
            metrics.append(metricJson);
        }
        sectionJson["metrics"] = metrics;

        JsonValue charts(Json::arrayValue);
        for (const auto& chart : section.charts) {
            JsonValue chartJson(Json::objectValue);
            const auto expanded = expandQuery(chart.query, params);
            chartJson["name"] = chart.name;
            chartJson["type"] = chart.type;
            chartJson["title"] = chart.title;
            chartJson["xAxis"] = chart.xAxis;
            JsonValue yAxes(Json::arrayValue);
            for (const auto& axis : chart.yAxes) {
                yAxes.append(axis);
            }
            chartJson["yAxes"] = yAxes;
            chartJson["query"] = expanded;
            chartJson["data"] = executeQuery(expanded);
            charts.append(chartJson);
        }
        sectionJson["charts"] = charts;
        sections.append(sectionJson);
    }

    data["template"] = templateName;
    JsonValue paramJson(Json::objectValue);
    for (const auto& [key, value] : params) {
        paramJson[key] = value;
    }
    data["params"] = paramJson;
    data["generatedAt"] = nowString();
    data["sections"] = sections;
    return data;
}

void ReportGenerator::startScheduler() {
    schedulerRunning_ = true;
}

void ReportGenerator::stopScheduler() {
    schedulerRunning_ = false;
}

bool ReportGenerator::updateSchedule(int64_t reportId, const ScheduleConfig& schedule) {
    std::lock_guard<std::mutex> lock(mutex_);
    Json::Value root(Json::objectValue);
    std::ifstream input(dbPath_);
    if (input.is_open()) {
        Json::CharReaderBuilder builder;
        std::string errors;
        Json::parseFromStream(builder, input, &root, &errors);
        if (!root.isObject()) {
            root = Json::Value(Json::objectValue);
        }
    }
    Json::Value node(Json::objectValue);
    node["enabled"] = schedule.enabled;
    node["cron"] = schedule.cron;
    node["format"] = schedule.format;
    node["timezone"] = schedule.timezone;
    Json::Value recipients(Json::arrayValue);
    for (const auto& item : schedule.recipients) {
        recipients.append(item);
    }
    node["recipients"] = recipients;
    root["schedules"][std::to_string(reportId)] = node;
    std::ofstream output(dbPath_, std::ios::trunc);
    output << root;
    return output.good();
}

std::string ReportGenerator::getReportPath(int64_t reportId) {
    std::filesystem::create_directories(outputDir_);
    return (std::filesystem::path(outputDir_) / ("report_" + std::to_string(reportId))).string();
}

std::vector<ReportResult> ReportGenerator::listGeneratedReports(int64_t reportId, int page, int pageSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ReportResult> results;
    std::ifstream input(dbPath_);
    if (!input.is_open()) {
        return results;
    }
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, input, &root, &errors) || !root["generatedReports"].isArray()) {
        return results;
    }
    for (const auto& node : root["generatedReports"]) {
        ReportResult result;
        result.reportId = node.get("reportId", 0).asInt64();
        if (reportId > 0 && result.reportId != reportId) {
            continue;
        }
        result.name = node.get("name", "").asString();
        result.generatedAt = node.get("generatedAt", "").asString();
        result.format = node.get("format", "").asString();
        result.filePath = node.get("filePath", "").asString();
        result.fileSize = node.get("fileSize", 0).asInt64();
        result.success = node.get("success", false).asBool();
        result.errorMessage = node.get("errorMessage", "").asString();
        if (node["metadata"].isObject()) {
            for (const auto& key : node["metadata"].getMemberNames()) {
                result.metadata[key] = node["metadata"][key].asString();
            }
        }
        results.push_back(std::move(result));
    }

    const auto start = static_cast<size_t>(std::max(0, (page - 1) * pageSize));
    if (start >= results.size()) {
        return {};
    }
    const auto end = std::min(results.size(), start + static_cast<size_t>(pageSize));
    return std::vector<ReportResult>(results.begin() + static_cast<std::ptrdiff_t>(start),
                                     results.begin() + static_cast<std::ptrdiff_t>(end));
}

bool ReportGenerator::deleteGeneratedReport(int64_t generatedId) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream input(dbPath_);
    if (!input.is_open()) {
        return false;
    }
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, input, &root, &errors) || !root["generatedReports"].isArray()) {
        return false;
    }

    Json::Value filtered(Json::arrayValue);
    bool removed = false;
    for (const auto& node : root["generatedReports"]) {
        if (node.get("generatedId", 0).asInt64() == generatedId) {
            removed = true;
            const auto filePath = node.get("filePath", "").asString();
            if (!filePath.empty()) {
                std::error_code ec;
                std::filesystem::remove(filePath, ec);
            }
            continue;
        }
        filtered.append(node);
    }
    if (!removed) {
        return false;
    }
    root["generatedReports"] = filtered;
    std::ofstream output(dbPath_, std::ios::trunc);
    output << root;
    return output.good();
}

std::string ReportGenerator::renderHtml(const ReportTemplate& tmpl,
                                        const std::map<std::string, std::string>& params,
                                        const JsonValue& data) {
    std::ostringstream html;
    html << "<html><head><meta charset=\"utf-8\"><title>" << tmpl.name << "</title></head><body>";
    html << "<h1>" << tmpl.description << "</h1>";
    html << "<p>Generated at: " << data.get("generatedAt", "").asString() << "</p>";

    html << "<h2>Parameters</h2><ul>";
    for (const auto& [key, value] : params) {
        html << "<li><strong>" << key << "</strong>: " << value << "</li>";
    }
    html << "</ul>";

    for (const auto& section : data["sections"]) {
        html << "<section><h2>" << section.get("title", "").asString() << "</h2>";
        html << "<h3>Metrics</h3><ul>";
        for (const auto& metric : section["metrics"]) {
            html << "<li>" << metric.get("label", metric.get("name", "")).asString()
                 << " - query: <code>" << metric.get("query", "").asString() << "</code></li>";
        }
        html << "</ul><h3>Charts</h3><ul>";
        for (const auto& chart : section["charts"]) {
            html << "<li>" << chart.get("title", chart.get("name", "")).asString()
                 << " (" << chart.get("type", "table").asString() << ")</li>";
        }
        html << "</ul></section>";
    }

    html << "</body></html>";
    return html.str();
}

std::string ReportGenerator::renderCsv(const ReportTemplate& tmpl,
                                       const std::map<std::string, std::string>& params,
                                       const JsonValue& data) {
    (void)tmpl;
    (void)params;
    std::ostringstream csv;
    csv << "section,kind,name,query\n";
    for (const auto& section : data["sections"]) {
        const auto sectionName = section.get("name", "").asString();
        for (const auto& metric : section["metrics"]) {
            csv << sectionName << ",metric," << metric.get("name", "").asString()
                << ",\"" << replaceAll(metric.get("query", "").asString(), "\"", "\"\"") << "\"\n";
        }
        for (const auto& chart : section["charts"]) {
            csv << sectionName << ",chart," << chart.get("name", "").asString()
                << ",\"" << replaceAll(chart.get("query", "").asString(), "\"", "\"\"") << "\"\n";
        }
    }
    return csv.str();
}

std::string ReportGenerator::renderJson(const ReportTemplate& tmpl,
                                        const std::map<std::string, std::string>& params,
                                        const JsonValue& data) {
    JsonValue output = data;
    output["templateDescription"] = tmpl.description;
    JsonValue paramJson(Json::objectValue);
    for (const auto& [key, value] : params) {
        paramJson[key] = value;
    }
    output["resolvedParams"] = paramJson;
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    return Json::writeString(builder, output);
}

bool ReportGenerator::saveReport(const ReportResult& result) {
    std::lock_guard<std::mutex> lock(mutex_);
    Json::Value root(Json::objectValue);
    std::ifstream input(dbPath_);
    if (input.is_open()) {
        Json::CharReaderBuilder builder;
        std::string errors;
        Json::parseFromStream(builder, input, &root, &errors);
        if (!root.isObject()) {
            root = Json::Value(Json::objectValue);
        }
    }

    Json::Value node(Json::objectValue);
    const auto generatedId = nextGeneratedId_.fetch_add(1);
    node["generatedId"] = static_cast<Json::Int64>(generatedId);
    node["reportId"] = static_cast<Json::Int64>(result.reportId);
    node["name"] = result.name;
    node["generatedAt"] = result.generatedAt;
    node["format"] = result.format;
    node["filePath"] = result.filePath;
    node["fileSize"] = static_cast<Json::Int64>(result.fileSize);
    node["success"] = result.success;
    node["errorMessage"] = result.errorMessage;
    Json::Value metadata(Json::objectValue);
    for (const auto& [key, value] : result.metadata) {
        metadata[key] = value;
    }
    node["metadata"] = metadata;
    root["generatedReports"].append(node);

    std::ofstream output(dbPath_, std::ios::trunc);
    output << root;
    return output.good();
}

std::string ReportGenerator::expandQuery(const std::string& query,
                                         const std::map<std::string, std::string>& params) {
    std::string expanded = query;
    for (const auto& [key, value] : params) {
        expanded = replaceAll(expanded, "{{" + key + "}}", value);
    }
    return expanded;
}

JsonValue ReportGenerator::executeQuery(const std::string& query) {
    JsonValue result(Json::arrayValue);
    const auto rows = DataWarehouse::instance().query(query, {}, {}, 0, 0);
    for (const auto& row : rows) {
        JsonValue entry(Json::objectValue);
        for (const auto& [key, value] : row) {
            entry[key] = value;
        }
        result.append(entry);
    }
    return result;
}

} // namespace Analytics
