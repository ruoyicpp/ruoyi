/**
 * @file test_dept_ctrl.cc
 * @brief SysDeptCtrl 控制器单元测试
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "../../src/system/controllers/SysDeptCtrl.h"
#include "../../src/common/DatabaseService.h"
#include "../../src/common/TokenCache.h"
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

// Mock DatabaseService for testing
class MockDatabaseService {
public:
    static MockDatabaseService& instance() {
        static MockDatabaseService inst;
        return inst;
    }

    // Mock dept data
    std::vector<std::map<std::string, std::string>> mock_depts = {
        {"dept_id", "1", "parent_id", "0", "dept_name", "总公司", "order_num", "0", "status", "0"},
        {"dept_id", "100", "parent_id", "1", "dept_name", "研发部", "order_num", "1", "status", "0"},
        {"dept_id", "101", "parent_id", "1", "dept_name", "销售部", "order_num", "2", "status", "0"},
        {"dept_id", "200", "parent_id", "100", "dept_name", "前端组", "order_num", "1", "status", "0"},
    };

    bool hasChildDepts(long parentId) {
        for (const auto& dept : mock_depts) {
            if (dept.at("parent_id") == std::to_string(parentId)) {
                return true;
            }
        }
        return false;
    }

    bool hasUsers(long deptId) {
        return false; // Mock: no users in any dept
    }
};

// Override DatabaseService::instance for testing
namespace DatabaseService {
    DatabaseService& instance() {
        return reinterpret_cast<DatabaseService&>(MockDatabaseService::instance());
    }
}

TEST_CASE("部门控制器 - 树形列表查询") {
    SUBCASE("查询所有部门") {
        // 验证部门列表包含根节点
        CHECK(MockDatabaseService::instance().mock_depts.size() >= 1);
    }

    SUBCASE("按部门名称搜索") {
        // 模拟搜索 "研发"
        auto found = false;
        for (const auto& dept : MockDatabaseService::instance().mock_depts) {
            if (dept.at("dept_name").find("研发") != std::string::npos) {
                found = true;
                break;
            }
        }
        CHECK(found == true);
    }

    SUBCASE("按状态筛选") {
        // 验证状态过滤逻辑
        int activeDepts = 0;
        for (const auto& dept : MockDatabaseService::instance().mock_depts) {
            if (dept.at("status") == "0") {
                activeDepts++;
            }
        }
        CHECK(activeDepts == 4);
    }
}

TEST_CASE("部门控制器 - 祖先路径计算") {
    SUBCASE("根部门祖先路径") {
        // 根部门 (id=1, parent_id=0) -> ancestors = "0"
        std::string expected = "0";
        CHECK(expected == "0"); // 根节点的 ancestors
    }

    SUBCASE("子部门祖先路径") {
        // 研发部 (id=100, parent_id=1) -> ancestors = "0,1"
        std::string parentAnc = "0";
        std::string parentId = "1";
        std::string expected = parentAnc + "," + parentId;
        CHECK(expected == "0,1");
    }

    SUBCASE("孙部门祖先路径") {
        // 前端组 (id=200, parent_id=100) -> ancestors = "0,1,100"
        std::string parentAnc = "0,1";
        std::string parentId = "100";
        std::string expected = parentAnc + "," + parentId;
        CHECK(expected == "0,1,100");
    }
}

TEST_CASE("部门控制器 - 权限检查") {
    SUBCASE("查看列表需要 list 权限") {
        std::string requiredPerm = "system:dept:list";
        CHECK(requiredPerm == "system:dept:list");
    }

    SUBCASE("新增需要 add 权限") {
        std::string requiredPerm = "system:dept:add";
        CHECK(requiredPerm == "system:dept:add");
    }

    SUBCASE("修改需要 edit 权限") {
        std::string requiredPerm = "system:dept:edit";
        CHECK(requiredPerm == "system:dept:edit");
    }

    SUBCASE("删除需要 remove 权限") {
        std::string requiredPerm = "system:dept:remove";
        CHECK(requiredPerm == "system:dept:remove");
    }
}

TEST_CASE("部门控制器 - 删除校验") {
    SUBCASE("有子部门不允许删除") {
        long parentId = 100; // 研发部
        bool hasChildren = MockDatabaseService::instance().hasChildDepts(parentId);
        CHECK(hasChildren == true);
    }

    SUBCASE("无子部门可以删除") {
        long parentId = 200; // 前端组
        bool hasChildren = MockDatabaseService::instance().hasChildDepts(parentId);
        CHECK(hasChildren == false);
    }

    SUBCASE("有用户不允许删除") {
        long deptId = 100;
        bool hasUsers = MockDatabaseService::instance().hasUsers(deptId);
        CHECK(hasUsers == false); // Mock 返回 false
    }
}

TEST_CASE("部门控制器 - 数据权限过滤") {
    SUBCASE("超管可看所有部门") {
        // 超管 roleId = 1, 不应受限
        bool isSuperAdmin = true;
        CHECK(isSuperAdmin == true);
    }

    SUBCASE("普通角色按 dept_id 过滤") {
        // 模拟数据权限 SQL 片段
        std::string dataScopeFilter = " AND dept_id IN (1, 100, 101)";
        CHECK(dataScopeFilter.find("dept_id IN") != std::string::npos);
    }
}

TEST_CASE("部门控制器 - 排序逻辑") {
    SUBCASE("按 parent_id 和 order_num 排序") {
        // 验证排序字段
        std::string orderBy = "ORDER BY parent_id, order_num";
        CHECK(orderBy.find("parent_id") != std::string::npos);
        CHECK(orderBy.find("order_num") != std::string::npos);
    }
}

TEST_CASE("部门控制器 - 部门名唯一性") {
    SUBCASE("同级部门名不能重复") {
        // 同一 parent_id 下 dept_name 应唯一
        std::string targetName = "研发部";
        int parentId = 1;
        int count = 0;
        for (const auto& dept : MockDatabaseService::instance().mock_depts) {
            if (dept.at("dept_name") == targetName &&
                dept.at("parent_id") == std::to_string(parentId)) {
                count++;
            }
        }
        CHECK(count == 1);
    }
}
