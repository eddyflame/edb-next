#include "core/Types.hpp"
#include "core/EventLoopThread.hpp"
#include "core/LinuxDebugEngine.hpp"
#include "core/BreakpointManager.hpp"
#include "core/ClangAstParser.hpp"
#include "core/TypeManager.hpp"
#include "core/DatabaseManager.hpp"
#include "tests/MockDebugBackend.hpp"

#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

using namespace edb_next;

void test_cpp23_result_monads() {
    std::cout << "[TEST] Running test_cpp23_result_monads..." << std::endl;

    // 1. Result::Ok and transform
    auto r1 = Result<int>::Ok(42);
    assert(r1.success);
    assert(r1.value == 42);

    auto r2 = r1.transform([](int x) { return x * 2; });
    assert(r2.success);
    assert(r2.value == 84);

    // 2. and_then chaining
    auto r3 = r2.and_then([](int x) -> Result<std::string> {
        if (x > 50) return Result<std::string>::Ok("Large: " + std::to_string(x));
        return Result<std::string>::Err("Too small");
    });
    assert(r3.success);
    assert(r3.value == "Large: 84");

    // 3. Error propagation in chain
    auto r_err = Result<int>::Err("Initial failure");
    auto r4 = r_err.and_then([](int x) {
        return Result<std::string>::Ok(std::to_string(x));
    });
    assert(!r4.success);
    assert(r4.error == "Initial failure");

    // 4. or_else recovery
    auto r5 = r_err.or_else([](const std::string& err) {
        return Result<int>::Ok(999);
    });
    assert(r5.success);
    assert(r5.value == 999);

    // 5. C++23 std::expected interoperability
    std::expected<int, std::string> exp = r2.toExpected();
    assert(exp.has_value());
    assert(*exp == 84);

    Result<int> from_exp(exp);
    assert(from_exp.success);
    assert(from_exp.value == 84);

    // 6. Void result monads
    auto v_ok = Result<void>::Ok();
    assert(v_ok.success);
    auto v_chained = v_ok.and_then([]() {
        return Result<int>::Ok(77);
    });
    assert(v_chained.success);
    assert(v_chained.value == 77);

    std::cout << "  -> test_cpp23_result_monads PASSED" << std::endl;
}

void test_pidfd_event_loop() {
    std::cout << "[TEST] Running test_pidfd_event_loop..." << std::endl;

    LinuxDebugEngine engine;
    BreakpointManager bpMgr(
        [&engine](Address addr, void* buf, size_t sz) { return engine.readMemory(addr, buf, sz); },
        [&engine](Address addr, const void* buf, size_t sz) { return engine.writeMemory(addr, buf, sz); },
        [&engine](int slot, Address addr, HardwareBpType t, HardwareBpSize s) {
            return engine.setHardwareBreakpoint(engine.activeTid(), slot, addr, t, s);
        },
        [&engine](int slot) {
            return engine.clearHardwareBreakpoint(engine.activeTid(), slot);
        }
    );

    // Launch test target binary
    std::string targetPath = "./test_target";
    if (!std::filesystem::exists(targetPath)) {
        targetPath = "./build/test_target";
    }
    assert(std::filesystem::exists(targetPath) && "test_target binary must exist");

    auto launchRes = engine.launch(targetPath, {"worker"});
    assert(launchRes.success && "Target launch must succeed");
    Pid targetPid = launchRes.value;
    assert(targetPid > 0);

    EventLoopThread loop(engine, bpMgr);
    loop.startLoop();

    // Verify pidfd is active on modern Linux
    bool usingPidfd = loop.isUsingPidfd();
    std::cout << "  [Info] EventLoopThread using pidfd: " << (usingPidfd ? "YES (Linux pidfd+epoll active)" : "FALLBACK") << std::endl;
    assert(usingPidfd && "Linux kernel 5.3+ must support pidfd_open");

    // Test suspend/resume responsiveness
    auto t0 = std::chrono::steady_clock::now();
    loop.setSuspended(true);
    assert(loop.isSuspended());
    loop.setSuspended(false);
    assert(!loop.isSuspended());
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count();
    std::cout << "  [Info] EventLoop suspend/resume roundtrip: " << elapsed << " us" << std::endl;

    loop.stopLoop();
    engine.kill();
    std::cout << "  -> test_pidfd_event_loop PASSED" << std::endl;
}

void test_target_fd_introspection() {
    std::cout << "[TEST] Running test_target_fd_introspection..." << std::endl;

    LinuxDebugEngine engine;
    std::string targetPath = "./test_target";
    if (!std::filesystem::exists(targetPath)) {
        targetPath = "./build/test_target";
    }

    auto launchRes = engine.launch(targetPath, {"worker"});
    assert(launchRes.success);

    // 1. Enumerate target file descriptors
    auto fds = engine.enumerateTargetFds();
    assert(!fds.empty() && "Target must have open file descriptors (stdin, stdout, stderr)");

    std::cout << "  [Info] Discovered " << fds.size() << " open FD(s) in target process:" << std::endl;
    bool foundStdout = false;
    for (const auto& fdInfo : fds) {
        std::cout << "    FD " << fdInfo.targetFd << ": " << fdInfo.type << " -> " << fdInfo.path;
        if (!fdInfo.extraInfo.empty()) {
            std::cout << " (" << fdInfo.extraInfo << ")";
        }
        std::cout << std::endl;
        if (fdInfo.targetFd == 1) foundStdout = true;
    }
    assert(foundStdout && "Target must have stdout (FD 1)");

    // 2. Test pidfd_getfd duplicating target FD 1 into debugger space
    auto dupRes = engine.getTargetFd(1);
    assert(dupRes.success && "pidfd_getfd on target stdout must succeed");
    int localFd = dupRes.value;
    assert(localFd >= 0);

    struct stat st{};
    int rc = ::fstat(localFd, &st);
    assert(rc == 0 && "fstat on duplicated target fd must succeed");
    ::close(localFd);

    engine.kill();
    std::cout << "  -> test_target_fd_introspection PASSED" << std::endl;
}

void test_clang_struct_parser() {
    std::cout << "[TEST] Running test_clang_struct_parser..." << std::endl;

    assert(ClangAstParser::isAvailable() && "libclang must be dynamically available on system");

    // 1. Test parsing complex C struct with bitfields and nested members
    std::string ipHeaderCode = R"(
        struct IpHeader {
            unsigned char version:4;
            unsigned char ihl:4;
            unsigned char tos;
            unsigned short tot_len;
            unsigned short id;
            unsigned short frag_off:13;
            unsigned short flags:3;
            unsigned char ttl;
            unsigned char protocol;
            unsigned short check;
            unsigned int saddr;
            unsigned int daddr;
        };
    )";

    std::string err;
    auto defOpt = ClangAstParser::parseCStruct(ipHeaderCode, &err);
    assert(defOpt.has_value() && "ClangAstParser must parse IpHeader");
    const auto& def = *defOpt;

    assert(def.name == "IpHeader");
    assert(def.fields.size() == 12);

    // Verify bitfield properties
    // version: 4 bits @ bit 0
    assert(def.fields[0].name == "version");
    assert(def.fields[0].isBitfield);
    assert(def.fields[0].bitOffset == 0);
    assert(def.fields[0].bitWidth == 4);
    assert(def.fields[0].offset == 0);

    // ihl: 4 bits @ bit 4
    assert(def.fields[1].name == "ihl");
    assert(def.fields[1].isBitfield);
    assert(def.fields[1].bitOffset == 4);
    assert(def.fields[1].bitWidth == 4);
    assert(def.fields[1].offset == 0);

    // tos: standard byte @ offset 1
    assert(def.fields[2].name == "tos");
    assert(!def.fields[2].isBitfield);
    assert(def.fields[2].offset == 1);

    // 2. Test bitfield extraction and formatting
    // Construct IPv4 packet first byte: version=4 (0x04), ihl=5 (0x05) -> byte = (5 << 4) | 4 = 0x54
    uint8_t dummyPacket[20] = {0};
    dummyPacket[0] = 0x54; // version 4, ihl 5
    dummyPacket[1] = 0x00; // tos
    dummyPacket[2] = 0x00; dummyPacket[3] = 0x3c; // tot_len 60

    std::string vVal = def.formatFieldValue(def.fields[0], dummyPacket, sizeof(dummyPacket));
    std::string ihlVal = def.formatFieldValue(def.fields[1], dummyPacket, sizeof(dummyPacket));

    assert(vVal.find("4") != std::string::npos && "version field formatted value must contain 4");
    assert(ihlVal.find("5") != std::string::npos && "ihl field formatted value must contain 5");

    // 3. Test packed struct with #pragma pack
    std::string packedCode = R"(
        #pragma pack(push, 1)
        struct PackedRecord {
            char tag;
            int code;
            char flag;
        };
        #pragma pack(pop)
    )";

    auto packedOpt = ClangAstParser::parseCStruct(packedCode, &err);
    assert(packedOpt.has_value());
    assert(packedOpt->totalSize == 6); // 1 + 4 + 1 without padding!
    assert(packedOpt->fields[1].offset == 1);
    assert(packedOpt->fields[2].offset == 5);

    // 4. Test TypeManager delegation
    TypeManager typeMgr;
    bool regOk = typeMgr.parseAndRegister(ipHeaderCode);
    assert(regOk);
    const auto* found = typeMgr.findStruct("IpHeader");
    assert(found != nullptr);
    assert(found->fields[0].isBitfield);

    std::cout << "  -> test_clang_struct_parser PASSED" << std::endl;
}

void test_sqlite3_zstd_database() {
    std::cout << "[TEST] Running test_sqlite3_zstd_database..." << std::endl;

    auto& dbMgr = DatabaseManager::instance();
    assert(dbMgr.isSqliteAvailable() && "SQLite3 runtime library must be available");

    std::string testDbPath = "/tmp/test_edb_next_gen.edb_db";
    std::filesystem::remove(testDbPath);

    DatabaseProject origProj;
    origProj.binaryPath = "/bin/ls";
    origProj.notes = "Project notes with special chars & symbols: <>\"'";
    origProj.baseAddress = 0x555555554000ULL;

    for (uint64_t i = 0; i < 200; ++i) {
        origProj.comments.emplace_back(0x401000 + i * 4, "Comment for instruction #" + std::to_string(i));
        if (i % 5 == 0) origProj.labels.emplace_back(0x401000 + i * 4, "loc_label_" + std::to_string(i));
        if (i % 10 == 0) origProj.bookmarks.push_back(0x401000 + i * 4);
    }

    origProj.breakpoints.push_back(DatabaseBreakpointData{
        .address = 0x401050,
        .type = "Software",
        .condition = "rax == 1",
        .logFormat = "RAX={rax}",
        .ignoreCount = 2,
        .scriptCode = "print('Hit BP')",
        .scriptLanguage = "python"
    });

    origProj.pageGuards.push_back(DatabasePageGuardData{
        .address = 0x602000,
        .size = 4096,
        .access = "WriteOnly",
        .comment = "Heap Guard",
        .condition = "",
        .scriptCode = "",
        .scriptLanguage = "python"
    });

    origProj.watches.push_back("rsp + 8");
    origProj.watches.push_back("rax * 2");

    origProj.patches.push_back(DatabasePatchData{
        .address = 0x401000,
        .originalHex = "31c0",
        .patchedHex = "9090"
    });

    // Synthetic large blob for Zstandard compression
    std::vector<uint8_t> largeBlob(65536);
    for (size_t i = 0; i < largeBlob.size(); ++i) {
        largeBlob[i] = static_cast<uint8_t>((i % 251) ^ 0x5a);
    }
    origProj.blobs.emplace_back("run_trace_frame_0", largeBlob);

    // 1. Save to SQLite3 + Zstandard
    bool saveOk = dbMgr.saveToFile(testDbPath, origProj);
    assert(saveOk && "Saving project to SQLite3 must succeed");

    // 2. Verify magic header
    assert(dbMgr.isSqliteDatabase(testDbPath) && "Database must have SQLite format 3 header");

    // 3. Load back and verify integrity
    DatabaseProject loadedProj;
    bool loadOk = dbMgr.loadFromFile(testDbPath, loadedProj);
    assert(loadOk && "Loading project from SQLite3 must succeed");

    assert(loadedProj.binaryPath == origProj.binaryPath);
    assert(loadedProj.notes == origProj.notes);
    assert(loadedProj.baseAddress == origProj.baseAddress);
    assert(loadedProj.comments.size() == origProj.comments.size());
    assert(loadedProj.labels.size() == origProj.labels.size());
    assert(loadedProj.bookmarks.size() == origProj.bookmarks.size());
    assert(loadedProj.breakpoints.size() == origProj.breakpoints.size());
    assert(loadedProj.pageGuards.size() == origProj.pageGuards.size());
    assert(loadedProj.watches.size() == origProj.watches.size());
    assert(loadedProj.patches.size() == origProj.patches.size());
    assert(loadedProj.blobs.size() == 1);
    assert(loadedProj.blobs[0].first == "run_trace_frame_0");
    assert(loadedProj.blobs[0].second == largeBlob && "Zstandard compressed blob must decompress identically");

    // 4. Test incremental update
    loadedProj.comments.emplace_back(0x409999, "Incremental Comment");
    bool incOk = dbMgr.saveProjectIncremental(testDbPath, loadedProj);
    assert(incOk);

    DatabaseProject verifyProj;
    assert(dbMgr.loadFromFile(testDbPath, verifyProj));
    assert(verifyProj.comments.size() == origProj.comments.size() + 1);

    // 5. Test legacy JSON fallback import
    std::string jsonPath = "/tmp/legacy_project.edb_db";
    std::filesystem::remove(jsonPath);
    {
        std::ofstream jf(jsonPath);
        jf << R"({
            "version": 1,
            "binary_path": "/bin/echo",
            "notes": "Legacy JSON test",
            "base_address": "0x400000",
            "comments": [
                {"address": "0x401000", "comment": "Legacy Comment"}
            ],
            "labels": [
                {"address": "0x401000", "label": "legacy_start"}
            ],
            "bookmarks": ["0x401000"],
            "breakpoints": [],
            "page_guards": [],
            "watches": ["rax"],
            "patches": []
        })";
    }

    assert(!dbMgr.isSqliteDatabase(jsonPath) && "Legacy file should not be identified as SQLite");
    DatabaseProject legacyLoaded;
    assert(dbMgr.loadFromFile(jsonPath, legacyLoaded) && "Legacy JSON file must be seamlessly loaded");
    assert(legacyLoaded.binaryPath == "/bin/echo");
    assert(legacyLoaded.notes == "Legacy JSON test");
    assert(legacyLoaded.comments.size() == 1);
    assert(legacyLoaded.comments[0].second == "Legacy Comment");
    assert(legacyLoaded.labels.size() == 1);
    assert(legacyLoaded.labels[0].second == "legacy_start");

    std::filesystem::remove(testDbPath);
    std::filesystem::remove(jsonPath);
    std::cout << "  -> test_sqlite3_zstd_database PASSED" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "   edb-next Next-Gen (P3) Test Suite   " << std::endl;
    std::cout << "========================================" << std::endl;

    test_cpp23_result_monads();
    test_clang_struct_parser();
    test_sqlite3_zstd_database();
    test_pidfd_event_loop();
    test_target_fd_introspection();

    std::cout << "========================================" << std::endl;
    std::cout << "  ALL NEXT-GEN (P3) TESTS PASSED (100%) " << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
