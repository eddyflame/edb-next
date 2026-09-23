#include "DatabaseManager.hpp"
#include "DebugSession.hpp"
#include "AnnotationManager.hpp"
#include "BreakpointManager.hpp"
#include "PatchManager.hpp"

#include <zstd.h>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <iostream>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>

namespace edb_next {

namespace {

// SQLite3 C ABI Types & Function Signatures
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;

constexpr int SQLITE_OK = 0;
constexpr int SQLITE_ROW = 100;
constexpr int SQLITE_DONE = 101;
constexpr int SQLITE_OPEN_READONLY = 0x00000001;
constexpr int SQLITE_OPEN_READWRITE = 0x00000002;
constexpr int SQLITE_OPEN_CREATE = 0x00000004;

struct SqliteLib {
    void* handle{nullptr};
    int (*open_v2)(const char*, sqlite3**, int, const char*){nullptr};
    int (*close)(sqlite3*){nullptr};
    int (*exec)(sqlite3*, const char*, int (*)(void*, int, char**, char**), void*, char**){nullptr};
    int (*prepare_v2)(sqlite3*, const char*, int, sqlite3_stmt**, const char**){nullptr};
    int (*step)(sqlite3_stmt*){nullptr};
    int (*finalize)(sqlite3_stmt*){nullptr};
    int64_t (*column_int64)(sqlite3_stmt*, int){nullptr};
    int (*column_int)(sqlite3_stmt*, int){nullptr};
    const unsigned char* (*column_text)(sqlite3_stmt*, int){nullptr};
    const void* (*column_blob)(sqlite3_stmt*, int){nullptr};
    int (*column_bytes)(sqlite3_stmt*, int){nullptr};
    int (*bind_int64)(sqlite3_stmt*, int, int64_t){nullptr};
    int (*bind_int)(sqlite3_stmt*, int, int){nullptr};
    int (*bind_text)(sqlite3_stmt*, int, const char*, int, void (*)(void*)){nullptr};
    int (*bind_blob)(sqlite3_stmt*, int, const void*, int, void (*)(void*)){nullptr};
    const char* (*errmsg)(sqlite3*){nullptr};
    int (*changes)(sqlite3*){nullptr};

    static SqliteLib& instance() {
        static SqliteLib inst;
        return inst;
    }

    bool load() {
        if (handle) return true;

        const char* candidates[] = {
            "/usr/lib/x86_64-linux-gnu/libsqlite3.so.0",
            "/usr/lib/libsqlite3.so.0",
            "/usr/lib/x86_64-linux-gnu/libsqlite3.so",
            "libsqlite3.so.0",
            "libsqlite3.so"
        };

        for (const char* path : candidates) {
            handle = ::dlopen(path, RTLD_NOW | RTLD_LOCAL);
            if (handle) break;
        }

        if (!handle) return false;

        #define BIND_SYM(name) name = reinterpret_cast<decltype(name)>(::dlsym(handle, "sqlite3_" #name)); \
            if (!name) { ::dlclose(handle); handle = nullptr; return false; }

        BIND_SYM(open_v2);
        BIND_SYM(close);
        BIND_SYM(exec);
        BIND_SYM(prepare_v2);
        BIND_SYM(step);
        BIND_SYM(finalize);
        BIND_SYM(column_int64);
        BIND_SYM(column_int);
        BIND_SYM(column_text);
        BIND_SYM(column_blob);
        BIND_SYM(column_bytes);
        BIND_SYM(bind_int64);
        BIND_SYM(bind_int);
        BIND_SYM(bind_text);
        BIND_SYM(bind_blob);
        BIND_SYM(errmsg);
        BIND_SYM(changes);

        #undef BIND_SYM
        return true;
    }
};

std::vector<uint8_t> compressBytes(const void* src, size_t srcSize) {
    if (!src || srcSize == 0) return {};
    size_t bound = ZSTD_compressBound(srcSize);
    std::vector<uint8_t> comp(bound);
    size_t cSize = ZSTD_compress(comp.data(), comp.size(), src, srcSize, 3);
    if (ZSTD_isError(cSize)) return {};
    comp.resize(cSize);
    return comp;
}

std::vector<uint8_t> decompressBytes(const void* src, size_t srcSize, size_t origSize) {
    if (!src || srcSize == 0 || origSize == 0) return {};
    std::vector<uint8_t> decomp(origSize);
    size_t dSize = ZSTD_decompress(decomp.data(), decomp.size(), src, srcSize);
    if (ZSTD_isError(dSize)) return {};
    decomp.resize(dSize);
    return decomp;
}

} // anonymous namespace

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager inst;
    return inst;
}

bool DatabaseManager::isSqliteAvailable() const noexcept {
    return SqliteLib::instance().load();
}

bool DatabaseManager::isSqliteDatabase(const std::string& filepath) const {
    std::ifstream f(filepath, std::ios::binary);
    if (!f.is_open()) return false;
    char magic[16] = {0};
    f.read(magic, 16);
    return (std::memcmp(magic, "SQLite format 3\000", 16) == 0);
}

std::string DatabaseManager::defaultDatabasePath(const std::string& binaryPath) const {
    if (binaryPath.empty()) return "";
    std::filesystem::path p(binaryPath);
    std::string dbName = p.stem().string() + ".edb_db";
    return (p.parent_path() / dbName).string();
}

bool DatabaseManager::saveToFile(const std::string& filepath, const DatabaseProject& project) {
    auto& sql = SqliteLib::instance();
    if (!sql.load()) {
        // Fallback to JSON if SQLite3 library is completely unavailable
        QJsonObject root;
        root["version"] = 1;
        root["binary_path"] = QString::fromStdString(project.binaryPath);
        root["notes"] = QString::fromStdString(project.notes);
        root["base_address"] = QString("0x%1").arg(project.baseAddress, 0, 16);

        QJsonArray comments_arr;
        for (const auto& [addr, text] : project.comments) {
            QJsonObject c_obj;
            std::ostringstream ss;
            ss << "0x" << std::hex << addr;
            c_obj["address"] = QString::fromStdString(ss.str());
            c_obj["comment"] = QString::fromStdString(text);
            comments_arr.append(c_obj);
        }
        root["comments"] = comments_arr;

        QJsonArray labels_arr;
        for (const auto& [addr, text] : project.labels) {
            QJsonObject l_obj;
            std::ostringstream ss;
            ss << "0x" << std::hex << addr;
            l_obj["address"] = QString::fromStdString(ss.str());
            l_obj["label"] = QString::fromStdString(text);
            labels_arr.append(l_obj);
        }
        root["labels"] = labels_arr;

        QJsonArray bm_arr;
        for (uint64_t addr : project.bookmarks) {
            std::ostringstream ss;
            ss << "0x" << std::hex << addr;
            bm_arr.append(QString::fromStdString(ss.str()));
        }
        root["bookmarks"] = bm_arr;

        QJsonArray bp_arr;
        for (const auto& bp : project.breakpoints) {
            QJsonObject bp_obj;
            std::ostringstream ss;
            ss << "0x" << std::hex << bp.address;
            bp_obj["address"] = QString::fromStdString(ss.str());
            bp_obj["type"] = QString::fromStdString(bp.type);
            bp_obj["condition"] = QString::fromStdString(bp.condition);
            bp_obj["log_format"] = QString::fromStdString(bp.logFormat);
            bp_obj["ignore_count"] = static_cast<int>(bp.ignoreCount);
            bp_obj["script_code"] = QString::fromStdString(bp.scriptCode);
            bp_obj["script_lang"] = QString::fromStdString(bp.scriptLanguage);
            bp_arr.append(bp_obj);
        }
        root["breakpoints"] = bp_arr;

        QJsonArray pg_arr;
        for (const auto& pg : project.pageGuards) {
            QJsonObject pg_obj;
            std::ostringstream ss;
            ss << "0x" << std::hex << pg.address;
            pg_obj["address"] = QString::fromStdString(ss.str());
            pg_obj["size"] = static_cast<int>(pg.size);
            pg_obj["access"] = QString::fromStdString(pg.access);
            pg_obj["comment"] = QString::fromStdString(pg.comment);
            pg_obj["condition"] = QString::fromStdString(pg.condition);
            pg_obj["script_code"] = QString::fromStdString(pg.scriptCode);
            pg_obj["script_lang"] = QString::fromStdString(pg.scriptLanguage);
            pg_arr.append(pg_obj);
        }
        root["page_guards"] = pg_arr;

        QJsonArray w_arr;
        for (const auto& w : project.watches) {
            w_arr.append(QString::fromStdString(w));
        }
        root["watches"] = w_arr;

        QJsonArray p_arr;
        for (const auto& p : project.patches) {
            QJsonObject p_obj;
            std::ostringstream ss;
            ss << "0x" << std::hex << p.address;
            p_obj["address"] = QString::fromStdString(ss.str());
            p_obj["original_hex"] = QString::fromStdString(p.originalHex);
            p_obj["patched_hex"] = QString::fromStdString(p.patchedHex);
            p_arr.append(p_obj);
        }
        root["patches"] = p_arr;

        QFile file(QString::fromStdString(filepath));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        QJsonDocument doc(root);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        return true;
    }

    return saveProjectIncremental(filepath, project);
}

bool DatabaseManager::saveProjectIncremental(const std::string& filepath, const DatabaseProject& project) {
    auto& sql = SqliteLib::instance();
    if (!sql.load()) return false;

    sqlite3* db = nullptr;
    int rc = sql.open_v2(filepath.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc != SQLITE_OK || !db) return false;

    const char* schema = R"(
        PRAGMA journal_mode = WAL;
        PRAGMA synchronous = NORMAL;

        CREATE TABLE IF NOT EXISTS metadata (
            key TEXT PRIMARY KEY,
            value TEXT
        );
        CREATE TABLE IF NOT EXISTS comments (
            address INTEGER PRIMARY KEY,
            text TEXT
        );
        CREATE TABLE IF NOT EXISTS labels (
            address INTEGER PRIMARY KEY,
            text TEXT
        );
        CREATE TABLE IF NOT EXISTS bookmarks (
            address INTEGER PRIMARY KEY
        );
        CREATE TABLE IF NOT EXISTS breakpoints (
            address INTEGER PRIMARY KEY,
            type TEXT,
            condition TEXT,
            log_format TEXT,
            ignore_count INTEGER,
            script_code TEXT,
            script_lang TEXT
        );
        CREATE TABLE IF NOT EXISTS page_guards (
            address INTEGER,
            size INTEGER,
            access TEXT,
            comment TEXT,
            condition TEXT,
            script_code TEXT,
            script_lang TEXT,
            PRIMARY KEY (address, size)
        );
        CREATE TABLE IF NOT EXISTS patches (
            address INTEGER PRIMARY KEY,
            original_hex TEXT,
            patched_hex TEXT
        );
        CREATE TABLE IF NOT EXISTS watches (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            expression TEXT
        );
        CREATE TABLE IF NOT EXISTS blobs (
            name TEXT PRIMARY KEY,
            orig_size INTEGER,
            compressed_data BLOB
        );
    )";

    char* errMsg = nullptr;
    sql.exec(db, schema, nullptr, nullptr, &errMsg);

    sql.exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    // Save metadata
    sqlite3_stmt* stmt = nullptr;
    const char* metaSql = "INSERT OR REPLACE INTO metadata (key, value) VALUES (?, ?);";
    if (sql.prepare_v2(db, metaSql, -1, &stmt, nullptr) == SQLITE_OK) {
        auto insertMeta = [&](const char* k, const std::string& v) {
            sql.bind_text(stmt, 1, k, -1, nullptr);
            sql.bind_text(stmt, 2, v.c_str(), -1, nullptr);
            sql.step(stmt);
            // Reset for next binding
            sqlite3_stmt* nextStmt = nullptr;
            sql.prepare_v2(db, metaSql, -1, &nextStmt, nullptr);
            sql.finalize(stmt);
            stmt = nextStmt;
        };
        insertMeta("version", "2");
        insertMeta("binary_path", project.binaryPath);
        insertMeta("notes", project.notes);
        insertMeta("base_address", std::to_string(project.baseAddress));
        if (stmt) sql.finalize(stmt);
    }

    // Save comments
    sql.exec(db, "DELETE FROM comments;", nullptr, nullptr, nullptr);
    const char* commSql = "INSERT OR REPLACE INTO comments (address, text) VALUES (?, ?);";
    if (sql.prepare_v2(db, commSql, -1, &stmt, nullptr) == SQLITE_OK) {
        for (const auto& [addr, text] : project.comments) {
            sql.bind_int64(stmt, 1, static_cast<int64_t>(addr));
            sql.bind_text(stmt, 2, text.c_str(), -1, nullptr);
            sql.step(stmt);
            sqlite3_stmt* nextStmt = nullptr;
            sql.prepare_v2(db, commSql, -1, &nextStmt, nullptr);
            sql.finalize(stmt);
            stmt = nextStmt;
        }
        if (stmt) sql.finalize(stmt);
    }

    // Save labels
    sql.exec(db, "DELETE FROM labels;", nullptr, nullptr, nullptr);
    const char* lblSql = "INSERT OR REPLACE INTO labels (address, text) VALUES (?, ?);";
    if (sql.prepare_v2(db, lblSql, -1, &stmt, nullptr) == SQLITE_OK) {
        for (const auto& [addr, text] : project.labels) {
            sql.bind_int64(stmt, 1, static_cast<int64_t>(addr));
            sql.bind_text(stmt, 2, text.c_str(), -1, nullptr);
            sql.step(stmt);
            sqlite3_stmt* nextStmt = nullptr;
            sql.prepare_v2(db, lblSql, -1, &nextStmt, nullptr);
            sql.finalize(stmt);
            stmt = nextStmt;
        }
        if (stmt) sql.finalize(stmt);
    }

    // Save bookmarks
    sql.exec(db, "DELETE FROM bookmarks;", nullptr, nullptr, nullptr);
    const char* bmSql = "INSERT OR REPLACE INTO bookmarks (address) VALUES (?);";
    if (sql.prepare_v2(db, bmSql, -1, &stmt, nullptr) == SQLITE_OK) {
        for (uint64_t addr : project.bookmarks) {
            sql.bind_int64(stmt, 1, static_cast<int64_t>(addr));
            sql.step(stmt);
            sqlite3_stmt* nextStmt = nullptr;
            sql.prepare_v2(db, bmSql, -1, &nextStmt, nullptr);
            sql.finalize(stmt);
            stmt = nextStmt;
        }
        if (stmt) sql.finalize(stmt);
    }

    // Save breakpoints
    sql.exec(db, "DELETE FROM breakpoints;", nullptr, nullptr, nullptr);
    const char* bpSql = "INSERT OR REPLACE INTO breakpoints (address, type, condition, log_format, ignore_count, script_code, script_lang) VALUES (?, ?, ?, ?, ?, ?, ?);";
    if (sql.prepare_v2(db, bpSql, -1, &stmt, nullptr) == SQLITE_OK) {
        for (const auto& bp : project.breakpoints) {
            sql.bind_int64(stmt, 1, static_cast<int64_t>(bp.address));
            sql.bind_text(stmt, 2, bp.type.c_str(), -1, nullptr);
            sql.bind_text(stmt, 3, bp.condition.c_str(), -1, nullptr);
            sql.bind_text(stmt, 4, bp.logFormat.c_str(), -1, nullptr);
            sql.bind_int(stmt, 5, static_cast<int>(bp.ignoreCount));
            sql.bind_text(stmt, 6, bp.scriptCode.c_str(), -1, nullptr);
            sql.bind_text(stmt, 7, bp.scriptLanguage.c_str(), -1, nullptr);
            sql.step(stmt);
            sqlite3_stmt* nextStmt = nullptr;
            sql.prepare_v2(db, bpSql, -1, &nextStmt, nullptr);
            sql.finalize(stmt);
            stmt = nextStmt;
        }
        if (stmt) sql.finalize(stmt);
    }

    // Save page guards
    sql.exec(db, "DELETE FROM page_guards;", nullptr, nullptr, nullptr);
    const char* pgSql = "INSERT OR REPLACE INTO page_guards (address, size, access, comment, condition, script_code, script_lang) VALUES (?, ?, ?, ?, ?, ?, ?);";
    if (sql.prepare_v2(db, pgSql, -1, &stmt, nullptr) == SQLITE_OK) {
        for (const auto& pg : project.pageGuards) {
            sql.bind_int64(stmt, 1, static_cast<int64_t>(pg.address));
            sql.bind_int(stmt, 2, static_cast<int>(pg.size));
            sql.bind_text(stmt, 3, pg.access.c_str(), -1, nullptr);
            sql.bind_text(stmt, 4, pg.comment.c_str(), -1, nullptr);
            sql.bind_text(stmt, 5, pg.condition.c_str(), -1, nullptr);
            sql.bind_text(stmt, 6, pg.scriptCode.c_str(), -1, nullptr);
            sql.bind_text(stmt, 7, pg.scriptLanguage.c_str(), -1, nullptr);
            sql.step(stmt);
            sqlite3_stmt* nextStmt = nullptr;
            sql.prepare_v2(db, pgSql, -1, &nextStmt, nullptr);
            sql.finalize(stmt);
            stmt = nextStmt;
        }
        if (stmt) sql.finalize(stmt);
    }

    // Save watches
    sql.exec(db, "DELETE FROM watches;", nullptr, nullptr, nullptr);
    const char* wSql = "INSERT INTO watches (expression) VALUES (?);";
    if (sql.prepare_v2(db, wSql, -1, &stmt, nullptr) == SQLITE_OK) {
        for (const auto& w : project.watches) {
            sql.bind_text(stmt, 1, w.c_str(), -1, nullptr);
            sql.step(stmt);
            sqlite3_stmt* nextStmt = nullptr;
            sql.prepare_v2(db, wSql, -1, &nextStmt, nullptr);
            sql.finalize(stmt);
            stmt = nextStmt;
        }
        if (stmt) sql.finalize(stmt);
    }

    // Save patches
    sql.exec(db, "DELETE FROM patches;", nullptr, nullptr, nullptr);
    const char* pSql = "INSERT OR REPLACE INTO patches (address, original_hex, patched_hex) VALUES (?, ?, ?);";
    if (sql.prepare_v2(db, pSql, -1, &stmt, nullptr) == SQLITE_OK) {
        for (const auto& p : project.patches) {
            sql.bind_int64(stmt, 1, static_cast<int64_t>(p.address));
            sql.bind_text(stmt, 2, p.originalHex.c_str(), -1, nullptr);
            sql.bind_text(stmt, 3, p.patchedHex.c_str(), -1, nullptr);
            sql.step(stmt);
            sqlite3_stmt* nextStmt = nullptr;
            sql.prepare_v2(db, pSql, -1, &nextStmt, nullptr);
            sql.finalize(stmt);
            stmt = nextStmt;
        }
        if (stmt) sql.finalize(stmt);
    }

    // Save blobs with Zstandard compression
    sql.exec(db, "DELETE FROM blobs;", nullptr, nullptr, nullptr);
    const char* blobSql = "INSERT OR REPLACE INTO blobs (name, orig_size, compressed_data) VALUES (?, ?, ?);";
    if (sql.prepare_v2(db, blobSql, -1, &stmt, nullptr) == SQLITE_OK) {
        for (const auto& [name, rawBytes] : project.blobs) {
            auto comp = compressBytes(rawBytes.data(), rawBytes.size());
            sql.bind_text(stmt, 1, name.c_str(), -1, nullptr);
            sql.bind_int64(stmt, 2, static_cast<int64_t>(rawBytes.size()));
            sql.bind_blob(stmt, 3, comp.data(), static_cast<int>(comp.size()), nullptr);
            sql.step(stmt);
            sqlite3_stmt* nextStmt = nullptr;
            sql.prepare_v2(db, blobSql, -1, &nextStmt, nullptr);
            sql.finalize(stmt);
            stmt = nextStmt;
        }
        if (stmt) sql.finalize(stmt);
    }

    sql.exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    sql.close(db);
    return true;
}

bool DatabaseManager::loadFromFile(const std::string& filepath, DatabaseProject& project) {
    if (isSqliteDatabase(filepath)) {
        auto& sql = SqliteLib::instance();
        if (!sql.load()) return false;

        sqlite3* db = nullptr;
        int rc = sql.open_v2(filepath.c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
        if (rc != SQLITE_OK || !db) return false;

        // Clear existing
        project = DatabaseProject{};

        // 1. Metadata
        sqlite3_stmt* stmt = nullptr;
        if (sql.prepare_v2(db, "SELECT key, value FROM metadata;", -1, &stmt, nullptr) == SQLITE_OK) {
            while (sql.step(stmt) == SQLITE_ROW) {
                const char* k = reinterpret_cast<const char*>(sql.column_text(stmt, 0));
                const char* v = reinterpret_cast<const char*>(sql.column_text(stmt, 1));
                if (k && v) {
                    std::string key = k;
                    std::string val = v;
                    if (key == "binary_path") project.binaryPath = val;
                    else if (key == "notes") project.notes = val;
                    else if (key == "base_address") {
                        try { project.baseAddress = std::stoull(val); } catch (...) {}
                    }
                }
            }
            sql.finalize(stmt);
        }

        // 2. Comments
        if (sql.prepare_v2(db, "SELECT address, text FROM comments ORDER BY address;", -1, &stmt, nullptr) == SQLITE_OK) {
            while (sql.step(stmt) == SQLITE_ROW) {
                uint64_t addr = static_cast<uint64_t>(sql.column_int64(stmt, 0));
                const char* t = reinterpret_cast<const char*>(sql.column_text(stmt, 1));
                project.comments.emplace_back(addr, t ? t : "");
            }
            sql.finalize(stmt);
        }

        // 3. Labels
        if (sql.prepare_v2(db, "SELECT address, text FROM labels ORDER BY address;", -1, &stmt, nullptr) == SQLITE_OK) {
            while (sql.step(stmt) == SQLITE_ROW) {
                uint64_t addr = static_cast<uint64_t>(sql.column_int64(stmt, 0));
                const char* t = reinterpret_cast<const char*>(sql.column_text(stmt, 1));
                project.labels.emplace_back(addr, t ? t : "");
            }
            sql.finalize(stmt);
        }

        // 4. Bookmarks
        if (sql.prepare_v2(db, "SELECT address FROM bookmarks ORDER BY address;", -1, &stmt, nullptr) == SQLITE_OK) {
            while (sql.step(stmt) == SQLITE_ROW) {
                uint64_t addr = static_cast<uint64_t>(sql.column_int64(stmt, 0));
                project.bookmarks.push_back(addr);
            }
            sql.finalize(stmt);
        }

        // 5. Breakpoints
        if (sql.prepare_v2(db, "SELECT address, type, condition, log_format, ignore_count, script_code, script_lang FROM breakpoints ORDER BY address;", -1, &stmt, nullptr) == SQLITE_OK) {
            while (sql.step(stmt) == SQLITE_ROW) {
                DatabaseBreakpointData bp;
                bp.address = static_cast<uint64_t>(sql.column_int64(stmt, 0));
                const char* type = reinterpret_cast<const char*>(sql.column_text(stmt, 1));
                const char* cond = reinterpret_cast<const char*>(sql.column_text(stmt, 2));
                const char* logf = reinterpret_cast<const char*>(sql.column_text(stmt, 3));
                bp.ignoreCount = static_cast<uint32_t>(sql.column_int(stmt, 4));
                const char* scode = reinterpret_cast<const char*>(sql.column_text(stmt, 5));
                const char* slang = reinterpret_cast<const char*>(sql.column_text(stmt, 6));

                bp.type = type ? type : "Software";
                bp.condition = cond ? cond : "";
                bp.logFormat = logf ? logf : "";
                bp.scriptCode = scode ? scode : "";
                bp.scriptLanguage = slang ? slang : "python";
                project.breakpoints.push_back(std::move(bp));
            }
            sql.finalize(stmt);
        }

        // 6. Page Guards
        if (sql.prepare_v2(db, "SELECT address, size, access, comment, condition, script_code, script_lang FROM page_guards ORDER BY address;", -1, &stmt, nullptr) == SQLITE_OK) {
            while (sql.step(stmt) == SQLITE_ROW) {
                DatabasePageGuardData pg;
                pg.address = static_cast<uint64_t>(sql.column_int64(stmt, 0));
                pg.size = static_cast<size_t>(sql.column_int(stmt, 1));
                const char* acc = reinterpret_cast<const char*>(sql.column_text(stmt, 2));
                const char* cmt = reinterpret_cast<const char*>(sql.column_text(stmt, 3));
                const char* cnd = reinterpret_cast<const char*>(sql.column_text(stmt, 4));
                const char* scode = reinterpret_cast<const char*>(sql.column_text(stmt, 5));
                const char* slang = reinterpret_cast<const char*>(sql.column_text(stmt, 6));

                pg.access = acc ? acc : "NoAccess";
                pg.comment = cmt ? cmt : "";
                pg.condition = cnd ? cnd : "";
                pg.scriptCode = scode ? scode : "";
                pg.scriptLanguage = slang ? slang : "python";
                project.pageGuards.push_back(std::move(pg));
            }
            sql.finalize(stmt);
        }

        // 7. Watches
        if (sql.prepare_v2(db, "SELECT expression FROM watches ORDER BY id;", -1, &stmt, nullptr) == SQLITE_OK) {
            while (sql.step(stmt) == SQLITE_ROW) {
                const char* expr = reinterpret_cast<const char*>(sql.column_text(stmt, 0));
                if (expr) project.watches.push_back(expr);
            }
            sql.finalize(stmt);
        }

        // 8. Patches
        if (sql.prepare_v2(db, "SELECT address, original_hex, patched_hex FROM patches ORDER BY address;", -1, &stmt, nullptr) == SQLITE_OK) {
            while (sql.step(stmt) == SQLITE_ROW) {
                DatabasePatchData p;
                p.address = static_cast<uint64_t>(sql.column_int64(stmt, 0));
                const char* orig = reinterpret_cast<const char*>(sql.column_text(stmt, 1));
                const char* patch = reinterpret_cast<const char*>(sql.column_text(stmt, 2));
                p.originalHex = orig ? orig : "";
                p.patchedHex = patch ? patch : "";
                project.patches.push_back(std::move(p));
            }
            sql.finalize(stmt);
        }

        // 9. Blobs (with Zstandard decompression)
        if (sql.prepare_v2(db, "SELECT name, orig_size, compressed_data FROM blobs ORDER BY name;", -1, &stmt, nullptr) == SQLITE_OK) {
            while (sql.step(stmt) == SQLITE_ROW) {
                const char* name = reinterpret_cast<const char*>(sql.column_text(stmt, 0));
                size_t origSize = static_cast<size_t>(sql.column_int64(stmt, 1));
                const void* blobData = sql.column_blob(stmt, 2);
                int blobBytes = sql.column_bytes(stmt, 2);

                if (name && blobData && blobBytes > 0 && origSize > 0) {
                    auto raw = decompressBytes(blobData, static_cast<size_t>(blobBytes), origSize);
                    project.blobs.emplace_back(name, std::move(raw));
                }
            }
            sql.finalize(stmt);
        }

        sql.close(db);
        return true;
    }

    // Backward compatibility fallback: Load legacy JSON format
    QFile file(QString::fromStdString(filepath));
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (doc.isNull() || !doc.isObject()) {
        return false;
    }

    QJsonObject root = doc.object();
    project.binaryPath = root["binary_path"].toString().toStdString();
    project.notes = root["notes"].toString().toStdString();
    project.baseAddress = root["base_address"].toString().toULongLong(nullptr, 16);

    // Comments
    project.comments.clear();
    QJsonArray comments_arr = root["comments"].toArray();
    for (const auto& val : comments_arr) {
        QJsonObject obj = val.toObject();
        uint64_t addr = obj["address"].toString().toULongLong(nullptr, 16);
        std::string comment = obj["comment"].toString().toStdString();
        project.comments.emplace_back(addr, comment);
    }

    // Labels
    project.labels.clear();
    QJsonArray labels_arr = root["labels"].toArray();
    for (const auto& val : labels_arr) {
        QJsonObject obj = val.toObject();
        uint64_t addr = obj["address"].toString().toULongLong(nullptr, 16);
        std::string label = obj["label"].toString().toStdString();
        project.labels.emplace_back(addr, label);
    }

    // Bookmarks
    project.bookmarks.clear();
    QJsonArray bm_arr = root["bookmarks"].toArray();
    for (const auto& val : bm_arr) {
        uint64_t addr = val.toString().toULongLong(nullptr, 16);
        project.bookmarks.push_back(addr);
    }

    // Breakpoints
    project.breakpoints.clear();
    QJsonArray bp_arr = root["breakpoints"].toArray();
    for (const auto& val : bp_arr) {
        QJsonObject obj = val.toObject();
        DatabaseBreakpointData bp;
        bp.address = obj["address"].toString().toULongLong(nullptr, 16);
        bp.type = obj["type"].toString().toStdString();
        bp.condition = obj["condition"].toString().toStdString();
        bp.logFormat = obj["log_format"].toString().toStdString();
        bp.ignoreCount = static_cast<uint32_t>(obj["ignore_count"].toInt());
        bp.scriptCode = obj["script_code"].toString().toStdString();
        bp.scriptLanguage = obj["script_lang"].toString().toStdString();
        if (bp.scriptLanguage.empty()) bp.scriptLanguage = "python";
        project.breakpoints.push_back(bp);
    }

    // Page Guards
    project.pageGuards.clear();
    QJsonArray pg_arr = root["page_guards"].toArray();
    for (const auto& val : pg_arr) {
        QJsonObject obj = val.toObject();
        DatabasePageGuardData pg;
        pg.address = obj["address"].toString().toULongLong(nullptr, 16);
        pg.size = static_cast<size_t>(obj["size"].toInt(1));
        pg.access = obj["access"].toString().toStdString();
        pg.comment = obj["comment"].toString().toStdString();
        pg.condition = obj["condition"].toString().toStdString();
        pg.scriptCode = obj["script_code"].toString().toStdString();
        pg.scriptLanguage = obj["script_lang"].toString().toStdString();
        if (pg.scriptLanguage.empty()) pg.scriptLanguage = "python";
        project.pageGuards.push_back(pg);
    }

    // Watches
    project.watches.clear();
    QJsonArray w_arr = root["watches"].toArray();
    for (const auto& val : w_arr) {
        project.watches.push_back(val.toString().toStdString());
    }

    // Patches
    project.patches.clear();
    QJsonArray p_arr = root["patches"].toArray();
    for (const auto& val : p_arr) {
        QJsonObject obj = val.toObject();
        DatabasePatchData p;
        p.address = obj["address"].toString().toULongLong(nullptr, 16);
        p.originalHex = obj["original_hex"].toString().toStdString();
        p.patchedHex = obj["patched_hex"].toString().toStdString();
        project.patches.push_back(p);
    }

    return true;
}

bool DatabaseManager::exportSession(std::shared_ptr<DebugSession> session,
                                    const PatchManager* patchMgr,
                                    const std::string& notes,
                                    const std::vector<std::string>& watches,
                                    const std::string& filepath) {
    if (!session) return false;

    DatabaseProject proj;
    proj.binaryPath = session->targetPath();
    proj.notes = notes;
    proj.baseAddress = session->baseAddress().value();
    proj.watches = watches;

    // Comments, Labels & Bookmarks
    for (const auto& [addr, comment] : session->annotationManager().allComments()) {
        proj.comments.emplace_back(addr, comment);
    }
    for (const auto& [addr, label] : session->annotationManager().allLabels()) {
        proj.labels.emplace_back(addr, label);
    }
    for (const auto& addr : session->annotationManager().bookmarks()) {
        proj.bookmarks.push_back(addr.value());
    }

    // Breakpoints
    for (const auto& bp : session->breakpointManager().allBreakpoints()) {
        if (bp.isInternal) continue;
        std::string type_str = "Software";
        if (bp.type == BreakpointType::HardwareExecute) {
            type_str = "HwExecute";
        } else if (bp.type == BreakpointType::HardwareWrite) {
            type_str = "HwWrite";
        } else if (bp.type == BreakpointType::HardwareReadWrite) {
            type_str = "HwReadWrite";
        }

        proj.breakpoints.push_back(DatabaseBreakpointData{
            .address = bp.address.value(),
            .type = type_str,
            .condition = bp.condition,
            .logFormat = bp.logFormat,
            .ignoreCount = bp.ignoreCount,
            .scriptCode = bp.scriptCode,
            .scriptLanguage = bp.scriptLanguage
        });
    }

    // Page Guards
    for (const auto& pg : session->pageGuardManager().allGuards()) {
        proj.pageGuards.push_back(DatabasePageGuardData{
            .address = pg.address.value(),
            .size = pg.size,
            .access = pageGuardAccessToString(pg.access),
            .comment = pg.comment,
            .condition = pg.condition,
            .scriptCode = pg.scriptCode,
            .scriptLanguage = pg.scriptLanguage
        });
    }

    // Patches
    if (patchMgr) {
        for (const auto& patch : patchMgr->patches()) {
            QByteArray orig(reinterpret_cast<const char*>(patch.originalBytes.data()), patch.originalBytes.size());
            QByteArray patched(reinterpret_cast<const char*>(patch.patchedBytes.data()), patch.patchedBytes.size());
            proj.patches.push_back(DatabasePatchData{
                .address = patch.address.value(),
                .originalHex = orig.toHex().toStdString(),
                .patchedHex = patched.toHex().toStdString()
            });
        }
    }

    return saveToFile(filepath, proj);
}

bool DatabaseManager::importSession(std::shared_ptr<DebugSession> session,
                                    PatchManager* patchMgr,
                                    std::string& outNotes,
                                    std::vector<std::string>& outWatches,
                                    const std::string& filepath) {
    if (!session) return false;

    DatabaseProject proj;
    if (!loadFromFile(filepath, proj)) {
        return false;
    }

    outNotes = proj.notes;
    outWatches = proj.watches;

    uint64_t savedBase = proj.baseAddress;
    uint64_t currentBase = session->baseAddress().value();

    auto remapAddr = [&](uint64_t raw) -> Address {
        // Only remap addresses belonging to the main module (within 1GB of base)
        if (savedBase != 0 && currentBase != 0 && raw >= savedBase && (raw - savedBase) < 0x40000000ULL) {
            return Address(currentBase + (raw - savedBase));
        }
        return Address(raw);
    };

    // Restore comments, labels & bookmarks
    for (const auto& [addr, text] : proj.comments) {
        session->annotationManager().setComment(remapAddr(addr), text);
    }
    for (const auto& [addr, text] : proj.labels) {
        session->annotationManager().setLabel(remapAddr(addr), text);
    }
    for (uint64_t addr : proj.bookmarks) {
        session->annotationManager().setBookmark(remapAddr(addr), true);
    }

    // Restore breakpoints
    for (const auto& bp : proj.breakpoints) {
        Address addr = remapAddr(bp.address);
        if (bp.type == "HwExecute") {
            session->addHardwareBreakpoint(addr, HardwareBpType::Execute);
        } else if (bp.type == "HwWrite") {
            session->addHardwareBreakpoint(addr, HardwareBpType::Write);
        } else if (bp.type == "HwReadWrite") {
            session->addHardwareBreakpoint(addr, HardwareBpType::ReadWrite);
        } else {
            session->addBreakpoint(addr);
        }

        if (!bp.condition.empty()) {
            session->breakpointManager().setBreakpointCondition(addr, bp.condition);
        }
        if (bp.ignoreCount > 0) {
            session->breakpointManager().setBreakpointIgnoreCount(addr, bp.ignoreCount);
        }
        if (!bp.logFormat.empty()) {
            session->breakpointManager().setBreakpointLogOnly(addr, true, bp.logFormat);
        }
        if (!bp.scriptCode.empty()) {
            session->breakpointManager().setBreakpointScript(addr, bp.scriptCode, bp.scriptLanguage);
        }
    }

    // Restore page guards
    for (const auto& pg : proj.pageGuards) {
        Address addr = remapAddr(pg.address);
        session->addPageGuard(addr, pg.size, pageGuardAccessFromString(pg.access), pg.comment);
        auto* g = session->pageGuardManager().getGuardMutable(addr);
        if (g) {
            g->condition = pg.condition;
            g->scriptCode = pg.scriptCode;
            g->scriptLanguage = pg.scriptLanguage;
        }
    }

    // Restore patches
    if (patchMgr) {
        for (const auto& p : proj.patches) {
            Address patch_addr = remapAddr(p.address);
            QByteArray orig = QByteArray::fromHex(QByteArray::fromStdString(p.originalHex));
            QByteArray patched = QByteArray::fromHex(QByteArray::fromStdString(p.patchedHex));
            std::vector<uint8_t> obytes(orig.begin(), orig.end());
            std::vector<uint8_t> pbytes(patched.begin(), patched.end());
            patchMgr->addPatch(patch_addr, obytes, pbytes, "Restored Patch");
        }
    }
    return true;
}

} // namespace edb_next
