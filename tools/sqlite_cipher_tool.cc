// sqlite_cipher_tool.cc
// CLI 工具：对 SQLite3MC 加密库进行 encrypt/decrypt/rekey/check/selftest 操作
// 用法：
//   sqlite_cipher_tool encrypt  <src.db> <dst.db> <key>
//   sqlite_cipher_tool decrypt  <src.db> <key>    <dst.db>
//   sqlite_cipher_tool rekey    <db>     <old_key> <new_key>
//   sqlite_cipher_tool check    <db>     [key]
//   sqlite_cipher_tool selftest
//
// 注意：
//   - encrypt/decrypt 使用 sqlite3mc 的 sqlcipher_export() SQL 函数跨 codec 拷贝
//     （sqlite3mc 2.x 默认 cipher 与 backup API 不兼容，必须用此方式）
//   - key 为任意字符串，会直接传入 sqlite3_key；外层若需 PBKDF2 派生，
//     请先用 SqliteCipher::deriveKey() 处理后再传入本工具

#include <sqlite3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

// ── 工具函数 ──────────────────────────────────────────────────────────────────

// 执行一条 SQL，失败时打印错误
static bool run(sqlite3* db, const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::fprintf(stderr, "[ERR] SQL 失败: %s\n  SQL: %s\n",
                     err ? err : "?", sql);
        if (err) sqlite3_free(err);
        return false;
    }
    return true;
}

// 打开数据库，可选要求文件已存在、可选应用 key 并验证
static sqlite3* openDb(const std::string& path, const std::string& key,
                        bool requireExisting) {
    if (requireExisting && !std::ifstream(path).good()) {
        std::fprintf(stderr, "[ERR] 文件不存在: %s\n", path.c_str());
        return nullptr;
    }
    sqlite3* db = nullptr;
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        std::fprintf(stderr, "[ERR] 无法打开 %s: %s\n", path.c_str(),
                     db ? sqlite3_errmsg(db) : "(null)");
        if (db) sqlite3_close(db);
        return nullptr;
    }
    if (!key.empty()) {
        if (sqlite3_key(db, key.data(), (int)key.size()) != SQLITE_OK) {
            std::fprintf(stderr, "[ERR] sqlite3_key 失败: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            return nullptr;
        }
        // 验证 key 正确（触发第一页解密，key 错会在这里失败）
        char* zerr = nullptr;
        if (sqlite3_exec(db, "SELECT count(*) FROM sqlite_master;",
                         nullptr, nullptr, &zerr) != SQLITE_OK) {
            std::string msg = zerr ? zerr : "key 验证失败";
            if (zerr) sqlite3_free(zerr);
            std::fprintf(stderr, "[ERR] 验证失败: %s\n", msg.c_str());
            sqlite3_close(db);
            return nullptr;
        }
    }
    return db;
}

// SQL 字符串字面量 escape：单引号 → 两个单引号
static std::string sqlEscape(const std::string& s) {
    std::string r;
    r.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '\'') r += "''";
        else r += c;
    }
    return r;
}

// 跨 codec 拷贝：用 VACUUM INTO 把已打开的 src 整库写到 dstPath（明文）
// 然后调用方按需对 dstPath 单独打开并 sqlite3_rekey 设置加密。
// 这避开 sqlite3mc backup API 在不同 cipher 配置下的不兼容问题。
static bool vacuumInto(sqlite3* src, const std::string& dstPath) {
    const std::string sql = "VACUUM INTO '" + sqlEscape(dstPath) + "'";
    return run(src, sql.c_str());
}

// 对一个已存在的明文 db 文件应用 key，让它变为加密文件
// sqlite3mc 文档：sqlite3_rekey 用于「将明文转加密 / 加密转明文 / 换密」
static bool rekeyPlainFile(const std::string& path, const std::string& key) {
    sqlite3* db = nullptr;
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        std::fprintf(stderr, "[ERR] rekeyPlainFile open: %s\n",
                     db ? sqlite3_errmsg(db) : "(null)");
        if (db) sqlite3_close(db);
        return false;
    }
    int rc = sqlite3_rekey(db, key.data(), (int)key.size());
    sqlite3_close(db);
    if (rc != SQLITE_OK) {
        std::fprintf(stderr, "[ERR] sqlite3_rekey 失败 rc=%d\n", rc);
        return false;
    }
    return true;
}

// 已加密文件 → 用 oldKey 打开后调 sqlite3_rekey("", 0) 移除加密（转明文）
static bool stripEncryption(const std::string& path, const std::string& oldKey) {
    sqlite3* db = openDb(path, oldKey, /*requireExisting*/true);
    if (!db) return false;
    int rc = sqlite3_rekey(db, "", 0);
    sqlite3_close(db);
    if (rc != SQLITE_OK) {
        std::fprintf(stderr, "[ERR] stripEncryption rekey('') 失败 rc=%d\n", rc);
        return false;
    }
    return true;
}

// ── 子命令：encrypt ──────────────────────────────────────────────────────────
// 打开明文源 → VACUUM INTO 明文中间文件 → sqlite3_rekey 转为加密
static int cmd_encrypt(const std::string& src, const std::string& dst,
                        const std::string& key) {
    if (std::ifstream(dst).good()) {
        std::fprintf(stderr, "[ERR] 目标文件已存在: %s（请先删除或选择新路径）\n",
                     dst.c_str());
        return 2;
    }
    if (key.empty()) {
        std::fprintf(stderr, "[ERR] encrypt 的 key 不能为空\n");
        return 2;
    }
    sqlite3* srcDb = openDb(src, "", /*requireExisting*/true);
    if (!srcDb) return 3;

    bool ok = vacuumInto(srcDb, dst);
    sqlite3_close(srcDb);
    if (!ok) {
        std::remove(dst.c_str());
        return 6;
    }
    if (!rekeyPlainFile(dst, key)) {
        std::remove(dst.c_str());
        return 7;
    }
    std::printf("[OK] 加密完成：%s -> %s\n", src.c_str(), dst.c_str());
    return 0;
}

// ── 子命令：decrypt ──────────────────────────────────────────────────────────
// 打开加密源（应用 key）→ VACUUM INTO 明文目标（由于源已解密，直接得明文）
static int cmd_decrypt(const std::string& src, const std::string& key,
                        const std::string& dst) {
    if (std::ifstream(dst).good()) {
        std::fprintf(stderr, "[ERR] 目标文件已存在: %s（请先删除或选择新路径）\n",
                     dst.c_str());
        return 2;
    }
    if (key.empty()) {
        std::fprintf(stderr, "[ERR] decrypt 的 key 不能为空\n");
        return 2;
    }
    sqlite3* srcDb = openDb(src, key, /*requireExisting*/true);
    if (!srcDb) return 3;

    bool ok = vacuumInto(srcDb, dst);
    sqlite3_close(srcDb);
    if (!ok) {
        std::remove(dst.c_str());
        return 6;
    }
    // VACUUM INTO 让 dst 继承源 codec → 仍是加密格式，需再用同 key 打开后 rekey("") 转明文
    if (!stripEncryption(dst, key)) {
        std::remove(dst.c_str());
        return 7;
    }
    std::printf("[OK] 解密完成：%s -> %s\n", src.c_str(), dst.c_str());
    return 0;
}

// ── 子命令：rekey ────────────────────────────────────────────────────────────
// 用旧 key 打开，调用 sqlite3_rekey 更换为新 key
static int cmd_rekey(const std::string& path, const std::string& oldKey,
                      const std::string& newKey) {
    sqlite3* db = openDb(path, oldKey, /*requireExisting*/true);
    if (!db) return 1;
    int rc = sqlite3_rekey(db, newKey.data(), (int)newKey.size());
    if (rc != SQLITE_OK) {
        std::fprintf(stderr, "[ERR] rekey 失败: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 2;
    }
    sqlite3_close(db);
    std::printf("[OK] 重新加密完成: %s\n", path.c_str());
    return 0;
}

// ── 子命令：check ────────────────────────────────────────────────────────────
// 验证数据库可以打开并读取，打印表数量
static int cmd_check(const std::string& path, const std::string& key) {
    sqlite3* db = openDb(path, key, /*requireExisting*/true);
    if (!db) return 1;
    int count = 0;
    sqlite3_exec(db, "SELECT count(*) FROM sqlite_master WHERE type='table';",
                 [](void* p, int, char** data, char**) -> int {
                     *static_cast<int*>(p) = data[0] ? std::atoi(data[0]) : 0;
                     return 0;
                 }, &count, nullptr);
    sqlite3_close(db);
    std::printf("[OK] 数据库正常，共 %d 张表: %s\n", count, path.c_str());
    return 0;
}

// ── 子命令：selftest ─────────────────────────────────────────────────────────
// 5 项测试，验证 sqlite3mc 加密栈正常工作
static int cmd_selftest() {
    const char*        PLAIN = "_selftest_plain.db";
    const char*        ENC   = "_selftest_enc.db";
    const std::string  KEY   = "selftest_key_ruoyi_12345";

    // 清理上次残留
    std::remove(PLAIN);
    std::remove(ENC);

    // ── [1/5] 建立明文库（3 行测试数据）──────────────────────────────────
    {
        sqlite3* db = nullptr;
        if (sqlite3_open(PLAIN, &db) != SQLITE_OK) {
            std::fprintf(stderr, "[FAIL] 无法创建明文库\n");
            return 1;
        }
        run(db, "CREATE TABLE test (id INTEGER PRIMARY KEY, val TEXT);");
        run(db, "INSERT INTO test VALUES (1, 'foo');");
        run(db, "INSERT INTO test VALUES (2, 'bar');");
        run(db, "INSERT INTO test VALUES (3, 'baz');");
        sqlite3_close(db);
        std::printf("  [1/5] 明文库已建：%s\n", PLAIN);
    }

    // ── [2/5] encrypt ────────────────────────────────────────────────────
    if (cmd_encrypt(PLAIN, ENC, KEY) != 0) {
        std::fprintf(stderr, "[FAIL] encrypt 失败\n");
        return 1;
    }
    std::printf("  [2/5] encrypt OK\n");

    // ── [3/5] 无 key 打开加密库应被拒绝 ──────────────────────────────────
    {
        sqlite3* db = nullptr;
        sqlite3_open(ENC, &db);
        char* zerr = nullptr;
        bool rejected = (sqlite3_exec(db, "SELECT * FROM test;",
                                      nullptr, nullptr, &zerr) != SQLITE_OK);
        if (zerr) sqlite3_free(zerr);
        sqlite3_close(db);
        if (!rejected) {
            std::fprintf(stderr, "[FAIL] 无 key 应被拒绝但未拒绝\n");
            return 1;
        }
        std::printf("  [3/5] 无 key 拒绝 OK\n");
    }

    // ── [4/5] 错 key 打开加密库应被拒绝 ──────────────────────────────────
    {
        sqlite3* db = nullptr;
        sqlite3_open(ENC, &db);
        sqlite3_key(db, "wrong_key", 9);
        char* zerr = nullptr;
        bool rejected = (sqlite3_exec(db, "SELECT * FROM test;",
                                      nullptr, nullptr, &zerr) != SQLITE_OK);
        if (zerr) sqlite3_free(zerr);
        sqlite3_close(db);
        if (!rejected) {
            std::fprintf(stderr, "[FAIL] 错 key 应被拒绝但未拒绝\n");
            return 1;
        }
        std::printf("  [4/5] 错 key 拒绝 OK\n");
    }

    // ── [5/5] 正确 key 读取数据 ───────────────────────────────────────────
    {
        sqlite3* db = openDb(ENC, KEY, true);
        if (!db) return 1;
        int rows = 0;
        sqlite3_exec(db, "SELECT count(*) FROM test;",
                     [](void* p, int, char** data, char**) -> int {
                         *static_cast<int*>(p) = data[0] ? std::atoi(data[0]) : 0;
                         return 0;
                     }, &rows, nullptr);
        sqlite3_close(db);
        if (rows != 3) {
            std::fprintf(stderr, "[FAIL] 应有 3 行，实际 %d 行\n", rows);
            return 1;
        }
        std::printf("  [5/5] 正确 key 读到 %d 行 OK\n", rows);
    }

    // 清理测试文件
    std::remove(PLAIN);
    std::remove(ENC);

    std::printf("\n[selftest PASS] SQLite3MC 加密栈工作正常\n");
    return 0;
}

// ── main ─────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr,
            "用法:\n"
            "  sqlite_cipher_tool encrypt  <src.db> <dst.db> <key>\n"
            "  sqlite_cipher_tool decrypt  <src.db> <key>    <dst.db>\n"
            "  sqlite_cipher_tool rekey    <db>     <old>    <new>\n"
            "  sqlite_cipher_tool check    <db>     [key]\n"
            "  sqlite_cipher_tool selftest\n");
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "encrypt") {
        if (argc < 5) {
            std::fprintf(stderr, "[ERR] encrypt: 需要参数 <src.db> <dst.db> <key>\n");
            return 1;
        }
        return cmd_encrypt(argv[2], argv[3], argv[4]);
    }
    if (cmd == "decrypt") {
        if (argc < 5) {
            std::fprintf(stderr, "[ERR] decrypt: 需要参数 <src.db> <key> <dst.db>\n");
            return 1;
        }
        return cmd_decrypt(argv[2], argv[3], argv[4]);
    }
    if (cmd == "rekey") {
        if (argc < 5) {
            std::fprintf(stderr, "[ERR] rekey: 需要参数 <db> <old_key> <new_key>\n");
            return 1;
        }
        return cmd_rekey(argv[2], argv[3], argv[4]);
    }
    if (cmd == "check") {
        std::string key = (argc >= 4) ? argv[3] : "";
        return cmd_check(argv[2], key);
    }
    if (cmd == "selftest") {
        return cmd_selftest();
    }

    std::fprintf(stderr, "[ERR] 未知命令: %s\n", cmd.c_str());
    return 1;
}
