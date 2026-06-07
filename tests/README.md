# =============================================================================
# 测试组织结构
# =============================================================================
#
# tests/
# ├── unit/                    # 单元测试
# │   ├── common/             # 公共工具测试
# │   │   ├── test_string_utils.cc
# │   │   ├── test_security.cc
# │   │   ├── test_json_utils.cc
# │   │   └── test_crypto.cc
# │   ├── services/           # 服务层测试
# │   │   ├── test_database_service.cc
# │   │   └── test_token_service.cc
# │   └── controllers/        # 控制器测试
# │       └── test_dept_ctrl.cc
# ├── integration/             # 集成测试
# │   ├── api/               # API 测试
# │   │   ├── test_dept_api.cc
# │   │   ├── test_user_api.cc
# │   │   └── test_auth_api.cc
# │   └── database/          # 数据库集成测试
# │       └── test_db_migration.cc
# ├── e2e/                    # 端到端测试
# │   └── test_full_workflow.cc
# ├── mocks/                   # Mock 对象
# │   ├── MockDatabaseService.h
# │   ├── MockHttpClient.h
# │   └── MockCache.h
# ├── fixtures/                # 测试数据
# │   ├── test_data.json
# │   └── test_db.sql
# ├── coverage/                # 覆盖率报告输出
# ├── CMakeLists.txt
# ├── doctest.h               # doctest 框架
# └── test_main.cc
#
# 运行方式:
#   cmake -B build -DRUOYI_BUILD_TESTS=ON -DRUOYI_COVERAGE=ON
#   cmake --build build --target ruoyi-tests
#   ctest --test-dir build --output-on-failure
#   # 生成覆盖率报告
#   cmake --build build --target coverage
#
# =============================================================================

# 测试覆盖范围
# 目标: 核心业务代码覆盖率 > 70%
# - 公共工具类: 90%+
# - Service 层: 80%+
# - Controller 层: 60%+
