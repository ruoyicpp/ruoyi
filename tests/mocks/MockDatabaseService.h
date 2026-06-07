/**
 * @file MockDatabaseService.h
 * @brief 数据库服务 Mock 对象，用于单元测试
 */

#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>

/**
 * @class MockDbRow
 * @brief Mock 数据库行
 */
class MockDbRow {
public:
    template<typename T>
    T get(const std::string& col) const {
        if (auto it = data_.find(col); it != data_.end()) {
            if constexpr (std::is_same_v<T, std::string>) {
                return it->second;
            } else if constexpr (std::is_same_v<T, int>) {
                return std::stoi(it->second);
            } else if constexpr (std::is_same_v<T, long>) {
                return std::stol(it->second);
            }
        }
        return T{};
    }

    void set(const std::string& col, const std::string& val) {
        data_[col] = val;
    }

private:
    std::map<std::string, std::string> data_;
};

/**
 * @class MockQueryResult
 * @brief Mock 查询结果
 */
class MockQueryResult {
public:
    MockQueryResult() = default;

    void addRow(const std::vector<std::pair<std::string, std::string>>& row) {
        rows_.push_back(row);
    }

    int rows() const { return static_cast<int>(rows_.size()); }
    int cols() const { return rows_.empty() ? 0 : static_cast<int>(rows_[0].size()); }

    std::string str(int row, int col) const {
        if (row >= 0 && row < rows() && col >= 0 && col < cols()) {
            return rows_[row][col].second;
        }
        return "";
    }

    int intVal(int row, int col) const {
        return std::stoi(str(row, col));
    }

    long long longVal(int row, int col) const {
        return std::stoll(str(row, col));
    }

    bool ok() const { return !error_; }
    std::string errorMsg() const { return errorMsg_; }

private:
    std::vector<std::vector<std::pair<std::string, std::string>>> rows_;
    bool error_ = false;
    std::string errorMsg_;
};

/**
 * @class MockDatabaseService
 * @brief Mock 数据库服务
 */
class MockDatabaseService {
public:
    static MockDatabaseService& instance() {
        static MockDatabaseService inst;
        return inst;
    }

    // 配置 Mock 数据
    void setMockData(const std::vector<MockDbRow>& data) {
        mockData_ = data;
    }

    void clearMockData() {
        mockData_.clear();
    }

    // Mock 查询方法
    MockQueryResult query(const std::string& sql) {
        MockQueryResult result;
        for (const auto& row : mockData_) {
            std::vector<std::pair<std::string, std::string>> rowData;
            for (const auto& [col, val] : row.getData()) {
                rowData.emplace_back(col, val);
            }
            result.addRow(rowData);
        }
        return result;
    }

    MockQueryResult queryParams(const std::string& sql, const std::vector<std::string>& params) {
        // 支持参数替换的 Mock
        return query(sql);
    }

    bool exec(const std::string& sql) {
        lastExecSql_ = sql;
        return true;
    }

    bool execParams(const std::string& sql, const std::vector<std::string>& params) {
        lastExecSql_ = sql;
        lastExecParams_ = params;
        return true;
    }

    // 获取最后执行的 SQL
    std::string getLastExecSql() const { return lastExecSql_; }
    std::vector<std::string> getLastExecParams() const { return lastExecParams_; }

    // Mock 辅助方法
    void addDept(long id, long parentId, const std::string& name, int orderNum = 0) {
        MockDbRow row;
        row.set("dept_id", std::to_string(id));
        row.set("parent_id", std::to_string(parentId));
        row.set("dept_name", name);
        row.set("order_num", std::to_string(orderNum));
        row.set("status", "0");
        mockData_.push_back(row);
    }

    void addUser(long id, const std::string& username, long deptId = 0) {
        MockDbRow row;
        row.set("user_id", std::to_string(id));
        row.set("user_name", username);
        row.set("dept_id", std::to_string(deptId));
        mockData_.push_back(row);
    }

private:
    std::vector<MockDbRow> mockData_;
    std::string lastExecSql_;
    std::vector<std::string> lastExecParams_;
};
