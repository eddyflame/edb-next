#include "core/ConfigurationManager.hpp"
#include "core/PluginManager.hpp"
#include "core/PatchManager.hpp"
#include "core/TraceEngine.hpp"
#include "core/DebugSession.hpp"
#include "core/LogManager.hpp"
#include "core/DatabaseManager.hpp"
#include "core/IntermodularCallsFinder.hpp"
#include "core/OpcodeSearcher.hpp"
#include "core/StateDumper.hpp"
#include "ui/CFGGraphView.hpp"
#include "ui/CommandBarView.hpp"
#include <sys/mman.h>
#include <QApplication>
#include <QFileInfo>
#include <iostream>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <unistd.h>
#include <fstream>
#include <csignal>

using namespace edb_next;

class DummyPluginContext : public IPluginContext {
public:
    explicit DummyPluginContext(SessionManager& mgr) : mgr_(mgr) {}

    SessionManager& sessionManager() override { return mgr_; }
    std::shared_ptr<DebugSession> activeSession() override { return mgr_.activeSession(); }
    void addDockWidget(QDockWidget*, Qt::DockWidgetArea) override {}
    void logMessage(const QString& msg) override {
        std::cout << "[PluginLog] " << msg.toStdString() << std::endl;
    }
    void registerDebugEventListener(std::function<void(const DebugEvent&)> cb) override {
        listeners_.push_back(cb);
    }
    void registerSessionStateListener(std::function<void(SessionState)>) override {}
    void registerCommand(const std::string& cmd,
                         std::function<void(const std::vector<std::string>&)> handler,
                         const std::string& help) override {
        std::cout << "[RegisterCommand] " << cmd << " (" << help << ")" << std::endl;
        commands_[cmd] = handler;
    }

    std::vector<std::function<void(const DebugEvent&)>> listeners_;
    std::map<std::string, std::function<void(const std::vector<std::string>&)>> commands_;

private:
    SessionManager& mgr_;
};

std::string getTestTargetPath() {
    if (access("./test_target", F_OK) == 0) return "./test_target";
    if (access("./build/test_target", F_OK) == 0) return "./build/test_target";
    if (access("../build/test_target", F_OK) == 0) return "../build/test_target";
    return "./test_target";
}

void test_configuration() {
    std::cout << "\n[TEST] Starting ConfigurationManager test..." << std::endl;
    auto& cfg = ConfigurationManager::instance();

    // 1. Check default values
    assert(cfg.engine().disableASLR == true && "Default ASLR should be disabled");
    assert(cfg.disasm().syntax == DisassemblySyntax::Intel && "Default syntax should be Intel");

    // 2. Modify and verify persistence
    cfg.disasm().syntax = DisassemblySyntax::ATT;
    cfg.engine().disableASLR = false;
    cfg.save();

    cfg.load();
    assert(cfg.disasm().syntax == DisassemblySyntax::ATT && "Syntax should persist");
    assert(cfg.engine().disableASLR == false && "ASLR should persist");

    // Revert back to defaults
    cfg.disasm().syntax = DisassemblySyntax::Intel;
    cfg.engine().disableASLR = true;
    cfg.save();

    // 3. Test Signal Policy lookup
    auto sigsegv_pol = cfg.signalPolicy(SIGSEGV);
    assert(sigsegv_pol.stopDebugger == true && "SIGSEGV should stop debugger by default");
    assert(sigsegv_pol.passToApp == false && "SIGSEGV should not pass by default");

    auto sigchld_pol = cfg.signalPolicy(SIGCHLD);
    assert(sigchld_pol.stopDebugger == false && "SIGCHLD should not stop debugger by default");
    assert(sigchld_pol.passToApp == true && "SIGCHLD should pass to app by default");

    std::cout << "[PASS] ConfigurationManager loading, saving, and signal policies verified." << std::endl;
}

void test_patch_manager() {
    std::cout << "\n[TEST] Starting PatchManager & ELF File Patching test..." << std::endl;
    PatchManager pm;

    Address targetAddr(0x555555555297);
    std::vector<uint8_t> origBytes = {0xf3, 0x0f, 0x1e, 0xfa}; // endbr64
    std::vector<uint8_t> patchBytes = {0x90, 0x90, 0x90, 0x90}; // 4 x NOP

    pm.addPatch(targetAddr, origBytes, patchBytes, "NOP out endbr64");
    assert(pm.patchCount() == 1 && "Patch count should be 1");
    assert(pm.patches()[0].isApplied == true && "Patch should be active");

    std::string inBinary = getTestTargetPath();
    std::string outBinary = "./test_target_patched";
    std::string errMsg;

    bool patched = pm.patchFileToDisk(inBinary, outBinary, errMsg);
    if (!patched) {
        std::cerr << "Patch file error: " << errMsg << std::endl;
    }
    assert(patched && "patchFileToDisk should succeed");
    assert(access(outBinary.c_str(), X_OK) == 0 && "Patched binary must exist and have executable permissions");

    // Verify file size is identical
    std::ifstream origFile(inBinary, std::ios::binary | std::ios::ate);
    std::ifstream patchFile(outBinary, std::ios::binary | std::ios::ate);
    assert(origFile.tellg() == patchFile.tellg() && "Patched binary size should match original");

    std::cout << "[PASS] PatchManager recorded patch and successfully exported patched ELF to disk: " << outBinary << std::endl;
}

void test_plugin_manager() {
    std::cout << "\n[TEST] Starting PluginManager & SamplePlugin test..." << std::endl;
    SessionManager sm;
    DummyPluginContext ctx(sm);
    PluginManager pm(&ctx);

    std::string pluginPath = "./plugins/sample_plugin.so";
    if (access(pluginPath.c_str(), F_OK) != 0) {
        pluginPath = "./build/plugins/sample_plugin.so";
    }
    QString absPath = QFileInfo(QString::fromStdString(pluginPath)).absoluteFilePath();

    bool loaded = pm.loadPlugin(absPath);
    assert(loaded && "sample_plugin.so should load successfully");
    assert(pm.loadedPlugins().size() == 1 && "Loaded plugin count should be 1");

    const auto& meta = pm.loadedPlugins()[0].metadata;
    assert(meta.id == "sample_logger" && "Plugin ID mismatch");
    assert(meta.name == "Activity Logger & Tools Plugin" && "Plugin Name mismatch");
    assert(meta.version == "1.0.0" && "Plugin Version mismatch");

    // Verify registered command
    assert(ctx.commands_.count("sample_ping") == 1 && "sample_ping command should be registered");
    ctx.commands_["sample_ping"]({"arg1", "arg2"});

    // Verify debug event hooking
    assert(!ctx.listeners_.empty() && "Plugin should have registered event listener");
    DebugEvent ev{
        .pid = 1234,
        .tid = 1234,
        .reason = StopReason::Breakpoint,
        .address = Address(0x401000),
        .message = "Test hit"
    };
    for (auto& l : ctx.listeners_) l(ev);

    ctx.listeners_.clear();
    ctx.commands_.clear();
    pm.unloadAll();
    assert(pm.loadedPlugins().empty() && "All plugins should be unloaded");
    std::cout << "[PASS] PluginManager loaded, executed, and unloaded plugin cleanly." << std::endl;
}

void test_trace_engine() {
    std::cout << "\n[TEST] Starting TraceEngine (Hit Trace & Run Trace) test..." << std::endl;
    TraceEngine te;

    // 1. Hit Trace (Code Coverage)
    assert(te.hitCount() == 0);
    te.recordHit(Address(0x401000));
    te.recordHit(Address(0x401005));
    te.recordHit(Address(0x401000)); // Duplicate hit
    assert(te.hitCount() == 2 && "Unique hit count should be 2");
    assert(te.isHit(Address(0x401000)) && "0x401000 must be marked hit");
    assert(te.isHit(Address(0x401005)) && "0x401005 must be marked hit");
    assert(!te.isHit(Address(0x401010)) && "0x401010 was not hit");

    // 2. Run Trace with register diffs
    RegisterContext r1;
    r1.setRax(0x10);
    r1.setRbx(0x20);
    te.recordFrame(Address(0x401000), "mov", "rax, 0x10", r1);

    RegisterContext r2 = r1;
    r2.setRax(0x50); // Changed!
    te.recordFrame(Address(0x401005), "add", "rax, 0x40", r2);

    RegisterContext r3 = r2;
    r3.setRbx(0x99);
    te.recordFrame(Address(0x401009), "mov", "rbx, 0x99", r3);

    assert(te.frameCount() == 3 && "Should have 3 trace frames");
    assert(te.traceFrames()[1].changedRegs.size() == 1 && "Only RAX should have changed in frame 1");
    assert(te.traceFrames()[1].changedRegs[0] == "RAX" && "Changed register must be RAX");

    // 3. Test Time-Travel History Navigation
    assert(te.currentFrameIndex() == 2 && "Should point to latest frame");
    assert(te.currentFrame() && te.currentFrame()->address == Address(0x401009));
    assert(te.stepBack() == true && "Should step back to frame 1");
    assert(te.currentFrameIndex() == 1);
    assert(te.currentFrame() && te.currentFrame()->address == Address(0x401005));
    assert(te.stepForward() == true && "Should step forward to frame 2");
    assert(te.currentFrameIndex() == 2);
    assert(te.stepForward() == false && "Cannot step forward past latest frame");

    te.clearHitTrace();
    te.clearRunTrace();
    assert(te.hitCount() == 0 && te.frameCount() == 0 && "Clear must reset counters");
    std::cout << "[PASS] TraceEngine hit coverage, run trace delta tracking, and time-travel navigation passed." << std::endl;
}

void test_cfg_and_command_bar() {
    std::cout << "\n[TEST] Starting CFG & CommandBar test..." << std::endl;
    DebugSession session("test_cfg_sess", "CFG Test");
    bool launched = session.launch(getTestTargetPath(), {"WorkerCFG"});
    assert(launched && "Failed to launch target for CFG test");

    // Test CommandBar command execution
    CommandBarView cmdBar;
    cmdBar.setSession(std::shared_ptr<DebugSession>(&session, [](DebugSession*){}));

    bool logReceived = false;
    QObject::connect(&cmdBar, &CommandBarView::outputLogged, [&](const QString& msg, bool isErr) {
        logReceived = true;
    });

    cmdBar.executeCommand("eval 0x10 + 0x20");
    assert(logReceived && "CommandBar should output eval result");

    cmdBar.executeCommand("bp main");
    assert(session.hasBreakpoint(*session.resolveSymbol("main")) && "CommandBar 'bp main' must set breakpoint");

    // Test CFG Basic Block generation
    CFGGraphView cfgView;
    cfgView.setSession(std::shared_ptr<DebugSession>(&session, [](DebugSession*){}));
    cfgView.buildGraphForFunction(*session.resolveSymbol("calculate_fib"));

    session.terminate();
    std::cout << "[PASS] CFG Graph generation and CommandBar execution verified." << std::endl;
}

void test_log_manager() {
    std::cout << "\n[TEST] Starting LogManager test..." << std::endl;
    auto& lm = LogManager::instance();
    lm.clear();

    bool cb_hit = false;
    lm.setCallback([&](const LogEntry& e) {
        cb_hit = true;
        assert(e.category == "UnitTest");
    });

    lm.info("UnitTest", "Test information message");
    assert(cb_hit && "Callback must be invoked on log");
    assert(lm.entries().size() == 1 && "Entries count must be 1");
    assert(lm.entries()[0].level == LogLevel::Info);

    lm.bp("UnitTest", "Breakpoint triggered");
    lm.event("UnitTest", "Process stopped");
    lm.cmd("UnitTest", "bp 0x1234");
    assert(lm.entries().size() == 4);

    lm.setCallback(nullptr);
    lm.clear();
    assert(lm.entries().empty());
    std::cout << "[PASS] LogManager entries, categories, and callbacks verified." << std::endl;
}

void test_extended_elf_and_intermodular() {
    std::cout << "\n[TEST] Starting Extended ElfParser & Intermodular Calls test..." << std::endl;
    ElfParser parser;
    bool loaded = parser.loadBinary(getTestTargetPath());
    assert(loaded && "Failed to load test target in ElfParser");

    const auto& hdr = parser.headerInfo();
    assert(hdr.elfClass == 2 && "Must be 64-bit ELF");
    assert(hdr.machine == 62 && "Must be x86-64 machine");
    assert(!hdr.entryPoint.isNull() && "Entry point must be non-zero");

    // Test sections
    const auto& sections = parser.sections();
    assert(!sections.empty() && "Sections must not be empty");
    bool found_text = false;
    bool found_rodata = false;
    for (const auto& s : sections) {
        if (s.name == ".text") {
            found_text = true;
            assert(s.isExecutable() && ".text section must be executable");
        }
        if (s.name == ".rodata") {
            found_rodata = true;
            assert(!s.isExecutable() && ".rodata must not be executable");
        }
    }
    assert(found_text && "Must find .text section");
    assert(found_rodata && "Must find .rodata section");

    // Test program headers (segments)
    const auto& segs = parser.programHeaders();
    assert(!segs.empty() && "Program headers must not be empty");

    // Test dynamic dependencies
    const auto& deps = parser.dynamicDependencies();
    bool found_libc = false;
    for (const auto& d : deps) {
        if (d.find("libc.so") != std::string::npos) {
            found_libc = true;
            break;
        }
    }
    assert(found_libc && "Dynamic dependencies must include libc.so");

    // Test Intermodular Calls
    DebugSession session("test_intermod_sess", "Intermod Test");
    bool launched = session.launch(getTestTargetPath(), {"WorkerIntermod"});
    assert(launched && "Failed to launch target for Intermod test");
    auto calls = IntermodularCallsFinder::findCalls(std::shared_ptr<DebugSession>(&session, [](DebugSession*){}));
    session.terminate();

    assert(!calls.empty() && "Must find intermodular library calls in test_target");
    std::cout << "[PASS] Found " << calls.size() << " intermodular call(s) in test target." << std::endl;
    if (!calls.empty()) {
        std::cout << "       Sample Call: " << calls[0].callerFunction << " -> " << calls[0].calleeApi << " (" << calls[0].instruction << ")" << std::endl;
    }
    std::cout << "[PASS] Extended ElfParser and IntermodularCallsFinder verified." << std::endl;
}

void test_database_persistence_and_memory_dump() {
    std::cout << "\n[TEST] Starting DatabaseManager persistence & Memory Dump test..." << std::endl;

    std::string db_path = "./test_project.edb_db";
    std::string dump_path = "./test_memory_dump.bin";

    // 1. Setup session and populate reversing metadata
    auto session = std::make_shared<DebugSession>("test_db_sess", "DB Test");
    bool launched = session->launch(getTestTargetPath(), {"WorkerDB"});
    assert(launched && "Failed to launch target for DB test");

    Address fib_addr = *session->resolveSymbol("calculate_fib");
    session->annotationManager().setComment(fib_addr, "Calculates fibonacci number");
    session->annotationManager().setBookmark(fib_addr, true);
    session->addBreakpoint(fib_addr);
    session->breakpointManager().setBreakpointCondition(fib_addr, "rdi == 7");
    session->breakpointManager().setBreakpointLogOnly(fib_addr, true, "Hit calculate_fib(7)");

    PatchManager patchMgr;
    std::vector<uint8_t> orig_bytes = {0x90, 0x90};
    std::vector<uint8_t> new_bytes = {0x31, 0xc0};
    patchMgr.addPatch(fib_addr, orig_bytes, new_bytes, "Test patch");

    std::string notes = "# Reverse Engineering Notes\nTarget: test_target\nFunction calculate_fib analyzed.";
    std::vector<std::string> watches = {"rax", "rdi", "[rbp - 8]"};

    // 2. Test memory dump
    bool dumped = session->dumpMemoryToFile(fib_addr, 32, dump_path);
    assert(dumped && "Failed to dump memory to file");
    QFileInfo dump_fi(QString::fromStdString(dump_path));
    assert(dump_fi.exists() && dump_fi.size() == 32 && "Dump file must exist and be 32 bytes");
    unlink(dump_path.c_str());
    std::cout << "[PASS] Memory region dump to file verified." << std::endl;

    // 3. Export database
    bool saved = DatabaseManager::instance().exportSession(session, &patchMgr, notes, watches, db_path);
    assert(saved && "Failed to export session to database");
    session->terminate();

    // 4. Create fresh session and import database
    auto session2 = std::make_shared<DebugSession>("test_db_sess2", "DB Test 2");
    bool launched2 = session2->launch(getTestTargetPath(), {"WorkerDB2"});
    assert(launched2 && "Failed to launch target for DB2 test");

    PatchManager patchMgr2;
    std::string restored_notes;
    std::vector<std::string> restored_watches;
    bool loaded = DatabaseManager::instance().importSession(session2, &patchMgr2, restored_notes, restored_watches, db_path);
    assert(loaded && "Failed to import database file");

    assert(restored_notes == notes && "Restored notes must match exactly");
    assert(restored_watches.size() == 3 && "Restored watches count must be 3");
    assert(restored_watches[0] == "rax" && restored_watches[1] == "rdi");
    assert(session2->annotationManager().getComment(fib_addr) == "Calculates fibonacci number");
    assert(session2->annotationManager().isBookmarked(fib_addr) == true);
    assert(session2->hasBreakpoint(fib_addr) == true);
    assert(session2->breakpointManager().getBreakpoint(fib_addr)->condition == "rdi == 7");
    assert(session2->breakpointManager().getBreakpoint(fib_addr)->isLogOnly == true);
    assert(patchMgr2.patchCount() == 1 && "Restored patch count must be 1");

    session2->terminate();
    unlink(db_path.c_str());
    std::cout << "[PASS] DatabaseManager full project persistence (comments, bookmarks, breakpoints, patches, watches, notes) verified." << std::endl;
}

void test_opcode_searcher_and_state_dumper() {
    std::cout << "\n[TEST] Starting OpcodeSearcher & StateDumper test..." << std::endl;
    DebugSession session("test_opcode_sess", "Opcode Test");
    bool launched = session.launch(getTestTargetPath(), {"WorkerOpcode"});
    assert(launched && "Failed to launch target for Opcode test");

    // 1. Test Opcode Searcher with CustomInstruction query ("push")
    auto customResults = OpcodeSearcher::search(session, OpcodeSearchType::CustomInstruction, "push");
    std::cout << "Found " << customResults.size() << " 'push' instructions in target." << std::endl;
    assert(!customResults.empty() && "Should find push instructions in test_target executable segments");
    assert(!customResults[0].mnemonic.empty());
    assert(customResults[0].address > Address(0));

    // 2. Test Syscall / Interrupt search
    auto syscallResults = OpcodeSearcher::search(session, OpcodeSearchType::InterruptOrSyscall);
    std::cout << "Found " << syscallResults.size() << " syscall/interrupt instructions." << std::endl;

    // 3. Test StateDumper snapshot formatting
    std::string dump = StateDumper::dumpState(session);
    assert(dump.find("CPU STATE DUMP") != std::string::npos);
    assert(dump.find("RAX:") != std::string::npos);
    assert(dump.find("RIP:") != std::string::npos);
    assert(dump.find("EFLAGS:") != std::string::npos);
    assert(dump.find("DISASSEMBLY") != std::string::npos);
    assert(dump.find("STACK") != std::string::npos);
    std::cout << "[PASS] StateDumper formatted CPU snapshot verified." << std::endl;

    session.terminate();
    std::cout << "[PASS] OpcodeSearcher and StateDumper tests passed." << std::endl;
}

void test_remote_syscalls_and_memory_mgmt() {
    std::cout << "\n[TEST] Starting Remote Syscalls (mprotect / mmap / munmap) & Set RIP test..." << std::endl;
    DebugSession session("test_remote_sess", "Remote Syscall Test");
    bool launched = session.launch(getTestTargetPath(), {"WorkerRemote"});
    assert(launched && "Failed to launch target for Remote test");

    Address origRip = session.registers().rip();
    assert(origRip > Address(0));

    // 1. Test setInstructionPointer (Set New Origin Here)
    Address targetAddr = origRip + 1;
    session.setInstructionPointer(targetAddr);
    assert(session.registers().rip() == targetAddr && "RIP must be updated to targetAddr");
    // Restore RIP
    session.setInstructionPointer(origRip);
    assert(session.registers().rip() == origRip);

    // 2. Test allocateMemory (mmap)
    size_t allocSize = 4096;
    auto newBlockOpt = session.allocateMemory(allocSize, PROT_READ | PROT_WRITE);
    assert(newBlockOpt.has_value() && "allocateMemory should return valid non-null page address");
    Address newBlock = *newBlockOpt;
    std::cout << "Allocated remote page at: " << newBlock.toHex() << std::endl;

    // Verify we can read/write memory in newly allocated block
    std::vector<uint8_t> testData = {0xde, 0xad, 0xbe, 0xef, 0x12, 0x34, 0x56, 0x78};
    bool written = session.writeMemory(newBlock, testData.data(), testData.size());
    assert(written && "writeMemory into allocated page must succeed");

    std::vector<uint8_t> readData = session.readMemory(newBlock, testData.size());
    assert(readData == testData && "readMemory must match written test data");

    // 3. Test changeMemoryProtection (mprotect)
    bool protChanged = session.changeMemoryProtection(newBlock, allocSize, PROT_READ | PROT_WRITE | PROT_EXEC);
    assert(protChanged && "changeMemoryProtection to RWX should succeed");

    // 4. Test freeMemory (munmap)
    bool freed = session.freeMemory(newBlock, allocSize);
    assert(freed && "freeMemory should unmap the allocated page");

    session.terminate();
    std::cout << "[PASS] Remote syscalls, mmap, mprotect, munmap and setInstructionPointer passed." << std::endl;
}

void test_cxx_demangling() {
    std::cout << "\n[TEST] Starting C++ Demangling test..." << std::endl;
    std::string mangled1 = "_Z13calculate_fibi";
    std::string demangled1 = ElfParser::demangle(mangled1);
    assert(demangled1 == "calculate_fib(int)" && "Demangle of _Z13calculate_fibi failed");

    std::string mangled2 = "_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEED1Ev";
    std::string demangled2 = ElfParser::demangle(mangled2);
    assert(demangled2.find("basic_string") != std::string::npos && "Demangle of std::string failed");

    std::string non_mangled = "printf";
    assert(ElfParser::demangle(non_mangled) == "printf" && "Non-mangled symbol should be unchanged");
    assert(ElfParser::demangle("").empty() && "Empty symbol should remain empty");

    std::cout << "[PASS] C++ Demangler verified: '" << mangled1 << "' -> '" << demangled1 << "'" << std::endl;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    std::cout << "========================================" << std::endl;
    std::cout << "   edb-next Advanced Features Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    test_configuration();
    test_patch_manager();
    test_plugin_manager();
    test_trace_engine();
    test_cfg_and_command_bar();
    test_log_manager();
    test_extended_elf_and_intermodular();
    test_database_persistence_and_memory_dump();
    test_opcode_searcher_and_state_dumper();
    test_remote_syscalls_and_memory_mgmt();
    test_cxx_demangling();

    std::cout << "\n>>> ALL ADVANCED TESTS PASSED CLEANLY! <<<" << std::endl;
    return 0;
}
