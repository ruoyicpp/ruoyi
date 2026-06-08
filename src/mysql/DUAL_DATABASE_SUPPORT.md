# MySQL 和 PostgreSQL 双数据库支持

## 📋 概述

项目现在支持 **MySQL 和 PostgreSQL 同时使用**，通过运行时动态选择数据库驱动。

### 🎯 设计目标

1. ✅ **编译时**：包含所有数据库驱动（MySQL + PostgreSQL）
2. ✅ **运行时**：根据 `config.json` 的 `database.type` 选择驱动
3. ✅ **业务代码**：统一使用 PostgreSQL 语法，自动转换为对应数据库

## 🏗️ 架构设计

```
┌─────────────────────────────────────────────┐
│         业务代码（PostgreSQL 语法）         │
├─────────────────────────────────────────────┤
│    DatabaseAdapter::instance().convertSql() │
├─────────────────────────────────────────────┤
│  根据 config.json 的 database.type 选择：   │
│  ├─ MySQL → 转换为 MySQL 语法              │
│  └─ PostgreSQL → 保持原样                  │
├─────────────────────────────────────────────┤
│  Drogon ORM（原生支持 MySQL + PostgreSQL）  │
├─────────────────────────────────────────────┤
│         MySQL / PostgreSQL 数据库           │
└─────────────────────────────────────────────┘
```

## 📁 核心文件

| 文件 | 说明 |
|------|------|
| `db_sql_map.h` | SQL 宏定义（编译时转换） |
| `DatabaseAdapter.h` | 运行时适配层接口 |
| `DatabaseAdapter.cc` | 运行时适配层实现 |
| `AppIncludes.h` | 主项目包含文件 |

## 🔧 配置方式

### config.json

```json
{
  "database": {
    "type": "postgresql",  // 或 "mysql"
    "host": "localhost",
    "port": 5432,          // PostgreSQL: 5432, MySQL: 3306
    "dbname": "ruoyi",
    "user": "postgres",    // PostgreSQL: postgres, MySQL: root
    "passwd": "password",
    "charset": "utf8mb4"
  }
}
```

### 支持的数据库类型

| 类型 | 别名 | 说明 |
|------|------|------|
| `postgresql` | `postgres`, `pg` | PostgreSQL（推荐） |
| `mysql` | `mariadb` | MySQL 5.7+ / 8.0+ / MariaDB |
| `sqlite3` | `sqlite` | SQLite3（可选） |

## 💻 使用示例

### 初始化

```cpp
#include "AppIncludes.h"

int main() {
    // 读取配置
    auto config = loadConfig("config.json");
    
    // 初始化数据库适配器
    auto& adapter = ruoyi::DatabaseAdapter::instance();
    adapter.init(config["database"]);
    
    // 输出当前数据库类型
    LOG_INFO << "Database: " << adapter.getTypeName();
    
    // 初始化 Drogon
    drogon::app().run();
    
    return 0;
}
```

### 查询示例

```cpp
#include "AppIncludes.h"

// 业务代码统一使用 PostgreSQL 语法
std::string pgSql = R"(
    SELECT * FROM users 
    WHERE id = $1 AND is_active = $2
    LIMIT 10 OFFSET 0
)";

// 自动转换为对应数据库的语法
auto& adapter = ruoyi::DatabaseAdapter::instance();
std::string sql = adapter.convertSql(pgSql);

// 执行查询
auto result = drogon::app().getDbClient()->execSqlAsync(sql, userId, true);
```

### UPSERT 示例

```cpp
// PostgreSQL 语法
std::string pgSql = R"(
    INSERT INTO users (id, name, age) VALUES ($1, $2, $3)
    ON CONFLICT (id) DO UPDATE SET 
        name = EXCLUDED.name,
        age = EXCLUDED.age
)";

// 自动转换
auto& adapter = ruoyi::DatabaseAdapter::instance();
std::string sql = adapter.convertSql(pgSql);

// MySQL 会转换为：
// INSERT INTO users (id, name, age) VALUES (?, ?, ?)
// ON DUPLICATE KEY UPDATE 
//     name = VALUES(name),
//     age = VALUES(age)
```

### 布尔值转换

```cpp
auto& adapter = ruoyi::DatabaseAdapter::instance();

// 转换布尔值
std::string value = adapter.convertBoolean(true);
// MySQL: "1"
// PostgreSQL: "true"

// 转换布尔字符串
std::string value = adapter.convertBooleanString("true");
// MySQL: "1"
// PostgreSQL: "true"
```

### 分页查询

```cpp
auto& adapter = ruoyi::DatabaseAdapter::instance();

int pageSize = 10;
int pageNum = 2;
int offset = (pageNum - 1) * pageSize;

std::string clause = adapter.getLimitOffsetClause(pageSize, offset);
// MySQL: "LIMIT 10, 10"
// PostgreSQL: "LIMIT 10 OFFSET 10"

std::string sql = "SELECT * FROM users " + clause;
```

## 🔄 SQL 转换规则

### 参数占位符

| PostgreSQL | MySQL |
|------------|-------|
| `$1, $2, $3` | `?, ?, ?` |

### UPSERT 语句

| PostgreSQL | MySQL |
|------------|-------|
| `ON CONFLICT (id) DO UPDATE SET col = EXCLUDED.col` | `ON DUPLICATE KEY UPDATE col = VALUES(col)` |

### RETURNING 子句

| PostgreSQL | MySQL |
|------------|-------|
| `RETURNING id` | （删除，使用 LAST_INSERT_ID()） |

### 布尔值

| PostgreSQL | MySQL |
|------------|-------|
| `true`, `false` | `1`, `0` |

### 分页

| PostgreSQL | MySQL |
|------------|-------|
| `LIMIT 10 OFFSET 20` | `LIMIT 20, 10` |

### 字符串拼接

| PostgreSQL | MySQL |
|------------|-------|
| `a \|\| b` | `CONCAT(a, b)` |

### 自增 ID

| PostgreSQL | MySQL |
|------------|-------|
| `LASTVAL()` | `LAST_INSERT_ID()` |

## 🧪 测试

### 编译

```bash
# Windows
.\build_test.bat

# Linux
chmod +x build_test.sh
./build_test.sh
```

### 运行测试

```cpp
#include "AppIncludes.h"

void testDatabaseAdapter() {
    auto& adapter = ruoyi::DatabaseAdapter::instance();
    
    // 测试 PostgreSQL
    Json::Value pgConfig;
    pgConfig["type"] = "postgresql";
    adapter.init(pgConfig);
    assert(adapter.isPostgresql());
    
    // 测试 MySQL
    Json::Value mysqlConfig;
    mysqlConfig["type"] = "mysql";
    adapter.init(mysqlConfig);
    assert(adapter.isMysql());
    
    // 测试 SQL 转换
    std::string pgSql = "SELECT * FROM users WHERE id = $1";
    std::string convertedSql = adapter.convertSql(pgSql);
    assert(convertedSql == "SELECT * FROM users WHERE id = ?");
    
    LOG_INFO << "All tests passed!";
}
```

## 📊 性能考虑

### 编译时开销

✅ **零开销** - 宏定义在编译时展开，无运行时开销

### 运行时开销

⚠️ **最小化** - SQL 转换使用正则表达式，仅在初始化时执行一次

### 优化建议

1. **缓存转换结果** - 对频繁使用的 SQL 进行缓存
2. **使用参数化查询** - 避免字符串拼接
3. **批量操作** - 减少数据库往返次数

## 🎯 最佳实践

### 1. 统一使用 PostgreSQL 语法

```cpp
// ✅ 推荐
std::string sql = "SELECT * FROM users WHERE id = $1";

// ❌ 避免
std::string sql = "SELECT * FROM users WHERE id = ?";
```

### 2. 使用 DatabaseAdapter 进行转换

```cpp
// ✅ 推荐
auto& adapter = ruoyi::DatabaseAdapter::instance();
std::string sql = adapter.convertSql(pgSql);

// ❌ 避免
std::string sql = pgSql;  // 直接使用，可能不兼容
```

### 3. 在初始化时配置数据库

```cpp
// ✅ 推荐
int main() {
    auto& adapter = ruoyi::DatabaseAdapter::instance();
    adapter.init(config["database"]);
    // ...
}

// ❌ 避免
// 在业务代码中多次初始化
```

### 4. 检查数据库类型

```cpp
// ✅ 推荐
auto& adapter = ruoyi::DatabaseAdapter::instance();
if (adapter.isMysql()) {
    // MySQL 特定逻辑
} else if (adapter.isPostgresql()) {
    // PostgreSQL 特定逻辑
}

// ❌ 避免
// 硬编码数据库特定的 SQL
```

## 🐛 常见问题

### Q: 如何同时支持 MySQL 和 PostgreSQL？

**A:** 使用 `DatabaseAdapter` 进行运行时转换：

```cpp
auto& adapter = ruoyi::DatabaseAdapter::instance();
adapter.init(config["database"]);
std::string sql = adapter.convertSql(pgSql);
```

### Q: 是否需要修改业务代码？

**A:** 不需要。业务代码统一使用 PostgreSQL 语法，`DatabaseAdapter` 自动转换。

### Q: 性能如何？

**A:** 
- 编译时：零开销（宏定义）
- 运行时：最小化（正则表达式转换）
- 建议：缓存转换结果

### Q: 如何切换数据库？

**A:** 修改 `config.json` 中的 `database.type`，无需重新编译。

### Q: 支持哪些数据库？

**A:** 
- ✅ PostgreSQL 9.4+
- ✅ MySQL 5.7+ / 8.0+
- ✅ MariaDB
- ⚠️ SQLite3（可选）

## 📚 相关文件

- `src/mysql/db_sql_map.h` - SQL 宏定义
- `src/mysql/DatabaseAdapter.h` - 适配器接口
- `src/mysql/DatabaseAdapter.cc` - 适配器实现
- `src/AppIncludes.h` - 主项目包含
- `src/mysql/mysql.sql` - MySQL 初始化脚本
- `src/mysql/postgresql.sql` - PostgreSQL 初始化脚本

## 🚀 下一步

1. ✅ 修改 `config.json` 配置数据库
2. ✅ 编译项目
3. ✅ 初始化数据库
4. ✅ 启动服务
5. ✅ 访问 http://localhost:18080

---

**双数据库支持完成！** 🎉
