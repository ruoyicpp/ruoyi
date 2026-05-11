#include "doctest.h"
#include "../src/common/SqliteFileCipher.h"
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstring>
#include <cstdint>

namespace fs = std::filesystem;

// ── 辅助 ─────────────────────────────────────────────────────────────────
static const std::string kTmpRoot = "test_sqlite_file_cipher_tmp";

static void mkTmp()  { fs::create_directories(kTmpRoot); }
static void rmTmp()  { std::error_code ec; fs::remove_all(kTmpRoot, ec); }

static std::string tmpPath(const std::string& name) { return kTmpRoot + "/" + name; }

// 构造看起来像 SQLite 的明文文件（前 16 字节是 SQLite magic）
static void writeFakeSqlite(const std::string& path, size_t size = 4096) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    REQUIRE(f.is_open());
    const char m[] = "SQLite format 3";  // 15 bytes + '\0'
    f.write(m, 16);
    for (size_t i = 16; i < size; ++i) f.put((char)(i & 0xFF));
}

static std::vector<uint8_t> readAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    REQUIRE(f.is_open());
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

// ── 测试用例 ─────────────────────────────────────────────────────────────

TEST_CASE("RYENC1: encrypt -> decrypt roundtrip preserves bytes") {
    mkTmp();
    writeFakeSqlite(tmpPath("orig.db"), 4096);
    auto orig = readAll(tmpPath("orig.db"));

    CHECK(SqliteFileCipher::encryptFile(tmpPath("orig.db"),
                                        tmpPath("orig.db.enc"),
                                        "TestKey@2026!"));
    CHECK(fs::exists(tmpPath("orig.db.enc")));

    CHECK(SqliteFileCipher::decryptFile(tmpPath("orig.db.enc"),
                                        tmpPath("back.db"),
                                        "TestKey@2026!"));
    CHECK(orig == readAll(tmpPath("back.db")));
    rmTmp();
}

TEST_CASE("RYENC1: magic header 'RYENC1' present") {
    mkTmp();
    writeFakeSqlite(tmpPath("a.db"), 512);
    CHECK(SqliteFileCipher::encryptFile(tmpPath("a.db"),
                                        tmpPath("a.enc"),
                                        "k"));

    auto enc = readAll(tmpPath("a.enc"));
    REQUIRE(enc.size() >= 92);    // header(44) + tag(16) + hmac(32)
    CHECK(enc[0] == 'R');
    CHECK(enc[1] == 'Y');
    CHECK(enc[2] == 'E');
    CHECK(enc[3] == 'N');
    CHECK(enc[4] == 'C');
    CHECK(enc[5] == '1');
    CHECK(enc[6] == '\0');
    CHECK(enc[7] == '\0');
    rmTmp();
}

TEST_CASE("RYENC1: isEncryptedFile distinguishes plain SQLite vs encrypted") {
    mkTmp();
    writeFakeSqlite(tmpPath("plain.db"), 256);
    CHECK(SqliteFileCipher::encryptFile(tmpPath("plain.db"),
                                        tmpPath("plain.enc"),
                                        "k"));
    CHECK_FALSE(SqliteFileCipher::isEncryptedFile(tmpPath("plain.db")));
    CHECK(SqliteFileCipher::isEncryptedFile(tmpPath("plain.enc")));
    rmTmp();
}

TEST_CASE("RYENC1: wrong passphrase fails (HMAC catches before GCM)") {
    mkTmp();
    writeFakeSqlite(tmpPath("orig.db"), 1024);
    CHECK(SqliteFileCipher::encryptFile(tmpPath("orig.db"),
                                        tmpPath("orig.enc"),
                                        "correct"));
    CHECK_FALSE(SqliteFileCipher::decryptFile(tmpPath("orig.enc"),
                                              tmpPath("x.db"),
                                              "wrong"));
    rmTmp();
}

TEST_CASE("RYENC1: HMAC detects single-bit ciphertext tampering") {
    mkTmp();
    writeFakeSqlite(tmpPath("orig.db"), 2048);
    CHECK(SqliteFileCipher::encryptFile(tmpPath("orig.db"),
                                        tmpPath("orig.enc"),
                                        "k"));

    fs::copy_file(tmpPath("orig.enc"), tmpPath("tampered.enc"),
                  fs::copy_options::overwrite_existing);
    // 翻 ciphertext 区任意一 bit（跳过 44 字节头）
    {
        std::fstream f(tmpPath("tampered.enc"),
                       std::ios::binary | std::ios::in | std::ios::out);
        f.seekg(60);
        char b; f.read(&b, 1);
        f.seekp(60);
        b ^= 0x01;
        f.write(&b, 1);
    }
    CHECK_FALSE(SqliteFileCipher::decryptFile(tmpPath("tampered.enc"),
                                              tmpPath("x.db"),
                                              "k"));
    rmTmp();
}

TEST_CASE("RYENC1: HMAC detects header tampering (kdf_iter forge)") {
    mkTmp();
    writeFakeSqlite(tmpPath("orig.db"), 1024);
    CHECK(SqliteFileCipher::encryptFile(tmpPath("orig.db"),
                                        tmpPath("orig.enc"),
                                        "k"));

    fs::copy_file(tmpPath("orig.enc"), tmpPath("hacked.enc"),
                  fs::copy_options::overwrite_existing);
    // 改 kdf_iter 字段（offset 12，4 字节）→ HMAC 应失败
    {
        std::fstream f(tmpPath("hacked.enc"),
                       std::ios::binary | std::ios::in | std::ios::out);
        f.seekp(12);
        uint32_t fake = 1;   // 试图把 100000 改成 1（暴破友好）
        f.write((const char*)&fake, 4);
    }
    CHECK_FALSE(SqliteFileCipher::decryptFile(tmpPath("hacked.enc"),
                                              tmpPath("x.db"),
                                              "k"));
    rmTmp();
}

TEST_CASE("RYENC1: random salt+nonce → ciphertext differs each time") {
    mkTmp();
    writeFakeSqlite(tmpPath("orig.db"), 512);
    CHECK(SqliteFileCipher::encryptFile(tmpPath("orig.db"),
                                        tmpPath("a.enc"), "k"));
    CHECK(SqliteFileCipher::encryptFile(tmpPath("orig.db"),
                                        tmpPath("b.enc"), "k"));

    auto a = readAll(tmpPath("a.enc"));
    auto b = readAll(tmpPath("b.enc"));
    CHECK(a.size() == b.size());
    CHECK(a != b);

    // 但都能用同一密钥解回
    CHECK(SqliteFileCipher::decryptFile(tmpPath("a.enc"),
                                        tmpPath("a-back.db"), "k"));
    CHECK(SqliteFileCipher::decryptFile(tmpPath("b.enc"),
                                        tmpPath("b-back.db"), "k"));
    CHECK(readAll(tmpPath("a-back.db")) == readAll(tmpPath("b-back.db")));
    rmTmp();
}

TEST_CASE("RYENC1: large file (256 KB) roundtrip") {
    mkTmp();
    writeFakeSqlite(tmpPath("big.db"), 256 * 1024);
    auto big = readAll(tmpPath("big.db"));

    CHECK(SqliteFileCipher::encryptFile(tmpPath("big.db"),
                                        tmpPath("big.enc"),
                                        "BigFileKey"));
    CHECK(SqliteFileCipher::decryptFile(tmpPath("big.enc"),
                                        tmpPath("big-back.db"),
                                        "BigFileKey"));
    CHECK(big == readAll(tmpPath("big-back.db")));
    rmTmp();
}

TEST_CASE("RYENC1: empty file edge case") {
    mkTmp();
    { std::ofstream f(tmpPath("empty.db"), std::ios::binary); }    // 0 字节
    CHECK(SqliteFileCipher::encryptFile(tmpPath("empty.db"),
                                        tmpPath("empty.enc"),
                                        "k"));
    CHECK(SqliteFileCipher::decryptFile(tmpPath("empty.enc"),
                                        tmpPath("empty-back.db"),
                                        "k"));
    CHECK(fs::file_size(tmpPath("empty.db")) == 0);
    CHECK(fs::file_size(tmpPath("empty-back.db")) == 0);
    rmTmp();
}

TEST_CASE("RYENC1: truncated encrypted file fails gracefully") {
    mkTmp();
    writeFakeSqlite(tmpPath("orig.db"), 1024);
    CHECK(SqliteFileCipher::encryptFile(tmpPath("orig.db"),
                                        tmpPath("orig.enc"), "k"));
    // 截断到只剩 50 字节（< 最小合法 92 字节）
    fs::resize_file(tmpPath("orig.enc"), 50);
    CHECK_FALSE(SqliteFileCipher::decryptFile(tmpPath("orig.enc"),
                                              tmpPath("x.db"), "k"));
    rmTmp();
}
