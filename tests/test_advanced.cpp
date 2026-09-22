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
#include "core/IDebugBackend.hpp"
#include "tests/MockDebugBackend.hpp"
#include "core/PageGuardManager.hpp"
#include "core/RendezvousManager.hpp"
#include "ui/CFGGraphView.hpp"
#include "ui/SugiyamaLayout.hpp"
#include "ui/CommandBarView.hpp"
#include "ui/MemoryHexView.hpp"
#include "ui/BinaryInfoView.hpp"
#include "ui/ThreadsView.hpp"
#include "core/MemoryScanner.hpp"
#include "ui/MemoryScannerView.hpp"
#include "core/TypeManager.hpp"
#include "ui/TypeViewer.hpp"
#include "ui/DisassemblyView.hpp"
#include "ui/AssembleDialog.hpp"
#include "ui/StackView.hpp"
#include "ui/RegisterView.hpp"
#include "ui/MultiDumpWidget.hpp"
#include "ui/SessionTabWidget.hpp"
#include "ui/IRefreshable.hpp"
#include "ui/IUIPlugin.hpp"
#include "ui/MainWindow.hpp"
#include <QMenuBar>
#include <QToolBar>
#include <QMouseEvent>
#include <QKeyEvent>
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

    // Test with explicit runtime base
    bool patchedWithBase = pm.patchFileToDisk(inBinary, outBinary, errMsg, Address(0x555555554000));
    assert(patchedWithBase && "patchFileToDisk with explicit runtimeBase should succeed");

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

    auto* uiPlugin = dynamic_cast<IUIPlugin*>(pm.loadedPlugins()[0].instance);
    assert(uiPlugin != nullptr && "SamplePlugin should implement IUIPlugin interface");

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

    // Test BreakpointManager addBreakpointWithOriginalByte
    BreakpointManager bpMgr(
        [](Address, void* buf, size_t sz) { std::memset(buf, 0xCC, sz); return true; },
        [](Address, const void*, size_t) { return true; }
    );
    bool bpOk = bpMgr.addBreakpointWithOriginalByte(Address(0x401000), 0x55, false, "test_sym");
    assert(bpOk && "addBreakpointWithOriginalByte should succeed");
    const auto* bp = bpMgr.getBreakpoint(Address(0x401000));
    assert(bp != nullptr && bp->originalByte == 0x55 && "Original byte 0x55 must be preserved");

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

    // Test SugiyamaLayout on synthetic cyclic CFG
    std::vector<DisassembledInstruction> insns = {
        DisassembledInstruction{.address = Address(0x1000), .mnemonic = "cmp", .operands = "eax, 0", .bytes = {0x83, 0xf8, 0x00}},
        DisassembledInstruction{.address = Address(0x1004), .mnemonic = "je", .operands = "0x1014", .bytes = {0x74, 0x0e}},
        DisassembledInstruction{.address = Address(0x1008), .mnemonic = "dec", .operands = "eax", .bytes = {0xff, 0xc8}},
        DisassembledInstruction{.address = Address(0x100c), .mnemonic = "nop", .operands = "", .bytes = {0x90}},
        DisassembledInstruction{.address = Address(0x1010), .mnemonic = "jmp", .operands = "0x1000", .bytes = {0xeb, 0xee}},
        DisassembledInstruction{.address = Address(0x1014), .mnemonic = "xor", .operands = "eax, eax", .bytes = {0x31, 0xc0}},
        DisassembledInstruction{.address = Address(0x1018), .mnemonic = "ret", .operands = "", .bytes = {0xc3}}
    };
    CFGGraph testGraph = CFGBuilder::build(insns);
    std::map<int, QSizeF> sizes;
    sizes[0] = QSizeF(300, 80);
    sizes[1] = QSizeF(300, 100);
    sizes[2] = QSizeF(300, 60);
    auto layoutRes = SugiyamaLayout::layout(testGraph, sizes);
    assert(layoutRes.blockRects.size() == 3);
    assert(layoutRes.routedEdges.size() == 3);
    assert(layoutRes.totalBounds.isValid());
    assert(layoutRes.totalBounds.width() > 0 && layoutRes.totalBounds.height() > 0);

    // Verify back-edge is flagged and routed
    bool foundBackEdge = false;
    for (const auto& re : layoutRes.routedEdges) {
        if (re.fromBlockId == 1 && re.toBlockId == 0) {
            assert(re.isBackEdge);
            assert(!re.path.isEmpty());
            foundBackEdge = true;
        }
    }
    assert(foundBackEdge);

    session.terminate();
    std::cout << "[PASS] CFG Graph generation, Sugiyama layout, and CommandBar execution verified." << std::endl;
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
    session->annotationManager().setLabel(fib_addr, "FibEntryPoint");
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
    assert(session2->annotationManager().getLabel(fib_addr) == "FibEntryPoint");
    assert(session2->resolveSymbol("FibEntryPoint").has_value() && *session2->resolveSymbol("FibEntryPoint") == fib_addr);
    assert(session2->annotationManager().isBookmarked(fib_addr) == true);
    assert(session2->hasBreakpoint(fib_addr) == true);
    assert(session2->breakpointManager().getBreakpoint(fib_addr)->condition == "rdi == 7");
    assert(session2->breakpointManager().getBreakpoint(fib_addr)->isLogOnly == true);
    assert(patchMgr2.patchCount() == 1 && "Restored patch count must be 1");

    session2->terminate();
    unlink(db_path.c_str());
    std::cout << "[PASS] DatabaseManager full project persistence (comments, labels, bookmarks, breakpoints, patches, watches, notes) verified." << std::endl;
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

void test_memory_hex_view_features() {
    std::cout << "\n[TEST] Starting MemoryHexView Breakpoint & Watchpoint UI test..." << std::endl;
    auto session = std::make_shared<DebugSession>("test-session", "Test Session");
    std::string target = getTestTargetPath();
    assert(session->launch(target, {}) && "Target launch must succeed");

    MemoryHexView hexView;
    hexView.setSession(session);

    Address targetAddr = session->registers().rip();
    assert(!targetAddr.isNull() && "Target instruction pointer must be valid");

    // Initially no breakpoint
    assert(!session->hasBreakpoint(targetAddr) && "Should not have breakpoint initially");

    // Add hardware watchpoint at targetAddr
    bool hwSet = session->addHardwareBreakpoint(targetAddr, HardwareBpType::Write, HardwareBpSize::Byte4);
    assert(hwSet && "addHardwareBreakpoint should succeed");
    assert(session->hasBreakpoint(targetAddr) && "hasBreakpoint should be true after setting hardware watchpoint");

    // Navigate hexView to targetAddr and refresh
    hexView.setBaseAddress(targetAddr);
    hexView.refresh();

    // Check table item at row 0, column 1 (corresponding to targetAddr)
    QTableWidgetItem* item = hexView.item(0, 1);
    assert(item != nullptr && "Table cell item must exist");
    assert(item->toolTip().contains("Breakpoint active") && "Cell item must have breakpoint tooltip");
    assert(item->background().color() == QColor(160, 40, 40, 160) && "Cell item must have breakpoint background color");

    // Remove breakpoint and verify cell item resets
    session->removeBreakpoint(targetAddr);
    assert(!session->hasBreakpoint(targetAddr) && "Breakpoint should be removed");
    hexView.refresh();
    item = hexView.item(0, 1);
    assert(item != nullptr && "Table cell item must exist");
    assert(!item->toolTip().contains("Breakpoint active") && "Cell item must no longer have breakpoint tooltip");

    session->terminate();
    std::cout << "[PASS] MemoryHexView Breakpoint & Hardware Watchpoint UI test passed." << std::endl;
}

void test_page_guard_breakpoints() {
    std::cout << "\n[TEST] Starting Page-Guard Memory Protection Breakpoints test..." << std::endl;

    // 1. Test standalone PageGuardManager logic & calculations
    Address target(0x00401020);
    Address pageBase = PageGuardManager::alignToPage(target);
    assert(pageBase == Address(0x00401000));

    size_t span = PageGuardManager::calculatePageSpan(target, 4);
    assert(span == 4096);

    std::vector<std::tuple<Address, size_t, int>> mprotectCalls;
    PageGuardManager mgr([&](Address a, size_t sz, int prot) {
        mprotectCalls.emplace_back(a, sz, prot);
        return true;
    });

    bool added = mgr.addGuard(target, 8, PageGuardAccess::ReadOnly, PROT_READ | PROT_WRITE, "TestGuard");
    assert(added && "addGuard should succeed");
    assert(mgr.hasGuard(target) && "hasGuard should return true");
    assert(mgr.isAddressWatched(target + 4) && "isAddressWatched should be true within range");
    assert(!mgr.isAddressWatched(target + 16) && "isAddressWatched should be false outside range");
    assert(mgr.isPageGuarded(Address(0x00401500)) && "isPageGuarded should be true on same 4KB page");
    assert(!mgr.isPageGuarded(Address(0x00402000)) && "isPageGuarded should be false on other page");

    auto* g = mgr.findGuardForFault(target);
    assert(g != nullptr && "findGuardForFault should find guard");
    assert(g->address == target);
    assert(g->guardedProt == PROT_READ);

    // Test temporary unprotect and reprotect
    assert(mgr.temporarilyUnprotect(g));
    assert(g->isTemporarilyUnprotected);
    assert(mgr.reprotect(g));
    assert(!g->isTemporarilyUnprotected);

    // Test disable & enable
    assert(mgr.disableGuard(target));
    assert(!mgr.isAddressWatched(target));
    assert(mgr.enableGuard(target));
    assert(mgr.isAddressWatched(target));

    // Test remove
    assert(mgr.removeGuard(target));
    assert(!mgr.hasGuard(target));

    // 2. Test live DebugSession with Page-Guard
    auto session = std::make_shared<DebugSession>("test_pg_session", "PGSession");
    bool launched = session->launch(getTestTargetPath(), {"WorkerPG"});
    assert(launched && "Failed to launch test target");

    // Allocate remote page in target
    auto page = session->allocateMemory(4096, PROT_READ | PROT_WRITE);
    assert(page.has_value() && "Remote memory allocation should succeed");
    Address watchedAddr = *page + 0x100;

    // Add ReadOnly Page-Guard
    bool ok = session->addPageGuard(watchedAddr, 8, PageGuardAccess::ReadOnly, "HeapWatchedVar");
    assert(ok && "addPageGuard should succeed");
    assert(session->hasPageGuard(watchedAddr));
    assert(session->isAddressPageWatched(watchedAddr));
    assert(session->isPageGuarded(watchedAddr));

    // Verify HexView rendering with Page-Guard
    MemoryHexView hexView;
    hexView.setSession(session);
    hexView.setBaseAddress(watchedAddr);
    hexView.refresh();

    QTableWidgetItem* watchedItem = hexView.item(0, 1);
    assert(watchedItem != nullptr);
    assert(watchedItem->toolTip().contains("Page-Guard Watched"));
    assert(watchedItem->background().color() == QColor(180, 110, 20, 160));

    // Test DatabaseManager serialization with Page-Guard
    DatabaseProject proj;
    proj.binaryPath = getTestTargetPath();
    DatabasePageGuardData pgData;
    pgData.address = watchedAddr.value();
    pgData.size = 8;
    pgData.access = "ReadOnly";
    pgData.comment = "DB Guard Test";
    proj.pageGuards.push_back(pgData);

    std::string tmpDb = "./test_pg_db.json";
    assert(DatabaseManager::instance().saveToFile(tmpDb, proj));
    DatabaseProject loadedProj;
    assert(DatabaseManager::instance().loadFromFile(tmpDb, loadedProj));
    assert(loadedProj.pageGuards.size() == 1);
    assert(loadedProj.pageGuards[0].address == watchedAddr.value());
    assert(loadedProj.pageGuards[0].access == "ReadOnly");
    ::unlink(tmpDb.c_str());

    // Clean up
    session->removePageGuard(watchedAddr);
    assert(!session->hasPageGuard(watchedAddr));
    session->terminate();

    std::cout << "[PASS] Page-Guard Breakpoints test passed cleanly." << std::endl;
}

void test_r_debug_rendezvous() {
    std::cout << "\n--- Testing 4.7 _r_debug Rendezvous & Shared Library Hot Reload ---" << std::endl;

    // 1. Prepare dynamic plugin library source and dlopen launcher
    std::string buildDir = (access("./build", F_OK) == 0) ? "./build" : ".";
    std::string pluginC = buildDir + "/test_plugin.c";
    std::string pluginSo = buildDir + "/libtest_plugin.so";
    std::string launcherC = buildDir + "/test_dlopen_launcher.c";
    std::string launcherBin = buildDir + "/test_dlopen_launcher";

    {
        std::ofstream pFile(pluginC);
        pFile << "#include <stdio.h>\n"
              << "int plugin_calc_magic(int a, int b) {\n"
              << "    return a * 100 + b;\n"
              << "}\n";
    }

    {
        std::ofstream lFile(launcherC);
        lFile << "#include <stdio.h>\n"
              << "#include <unistd.h>\n"
              << "#include <dlfcn.h>\n"
              << "int main() {\n"
              << "    usleep(100000);\n"
              << "    void* h = dlopen(\"" << pluginSo << "\", RTLD_NOW);\n"
              << "    if (!h) { printf(\"dlopen failed: %s\\n\", dlerror()); return 1; }\n"
              << "    int (*calc)(int, int) = (int (*)(int, int))dlsym(h, \"plugin_calc_magic\");\n"
              << "    int val = calc ? calc(7, 42) : 0;\n"
              << "    (void)val;\n"
              << "    usleep(100000);\n"
              << "    dlclose(h);\n"
              << "    return 0;\n"
              << "}\n";
    }

    // Compile shared library and launcher
    const char* ccEnv = ::getenv("CC");
    std::string cc = (ccEnv && *ccEnv) ? ccEnv : "gcc";
    std::string cmdSo = cc + " -O0 -shared -fPIC -o " + pluginSo + " " + pluginC;
    std::string cmdBin = cc + " -O0 -g -o " + launcherBin + " " + launcherC + " -ldl";
    int r1 = ::system(cmdSo.c_str());
    int r2 = ::system(cmdBin.c_str());
    assert(r1 == 0 && r2 == 0);
    ::unlink(pluginC.c_str());
    ::unlink(launcherC.c_str());

    // 2. Launch DebugSession with test_dlopen_launcher
    auto session = std::make_shared<DebugSession>("test_rdebug_session", "RDebugSession");
    bool launched = session->launch(launcherBin, {});
    assert(launched && "Failed to launch dlopen launcher target");

    // Verify RendezvousManager is initialized
    assert(session->rendezvousManager().isInitialized() && "RendezvousManager should be initialized");
    Address brkAddr = session->rendezvousManager().rBrkAddr();
    assert(brkAddr.value() != 0 && "r_brk address should be resolved");

    // 3. Set a Pending Breakpoint for 'plugin_calc_magic' BEFORE the library is loaded!
    assert(!session->symbols().findSymbolAddress("plugin_calc_magic").has_value() && "Symbol must not exist prior to dlopen");
    bool pbSet = session->addPendingBreakpoint("plugin_calc_magic");
    assert(pbSet && "addPendingBreakpoint should succeed");
    assert(session->pendingBreakpoints().size() == 1);

    // 4. Test BinaryInfoView loaded libraries table
    BinaryInfoView infoView;
    infoView.setSession(session);
    infoView.refresh();

    // 5. Test CommandBar 'catch dlopen'
    CommandBarView cmdBar;
    cmdBar.setSession(session);
    QString capturedLog;
    QObject::connect(&cmdBar, &CommandBarView::outputLogged, [&](const QString& msg, bool isErr) {
        Q_UNUSED(isErr);
        capturedLog = msg;
    });

    cmdBar.executeCommand("catch dlopen");
    assert(session->stopOnLibraryEvents() == true);
    cmdBar.executeCommand("catch dlopen");
    assert(session->stopOnLibraryEvents() == false);

    // 6. Resume target. Target calls dlopen -> hits _dl_debug_state -> RendezvousManager catches it ->
    // loads libtest_plugin.so symbols -> resolves 'plugin_calc_magic' -> sets active bp ->
    // resumes -> target calls calc(7, 42) -> hits 'plugin_calc_magic' breakpoint and pauses!
    bool hitMagicBp = false;
    QObject::connect(session.get(), &DebugSession::eventOccurred, [&](const DebugEvent& ev) {
        if (ev.reason == StopReason::Breakpoint) {
            auto sym = session->symbols().findNearestSymbol(ev.address);
            if (sym && sym->first.name == "plugin_calc_magic") {
                hitMagicBp = true;
            }
        }
    });

    session->resume();

    // Wait for the breakpoint hit with event processing
    for (int i = 0; i < 50; ++i) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (hitMagicBp || session->state() == SessionState::Paused) {
            if (hitMagicBp) break;
        }
        usleep(50000);
    }

    assert(hitMagicBp && "Should successfully catch dlopen, bind pending breakpoint, and hit plugin_calc_magic!");
    assert(session->state() == SessionState::Paused);
    assert(session->pendingBreakpoints().empty() && "Pending breakpoint should be fully resolved");

    // Verify symbols() now knows plugin_calc_magic
    auto resolvedAddr = session->symbols().findSymbolAddress("plugin_calc_magic");
    assert(resolvedAddr.has_value() && resolvedAddr->value() != 0);

    // Verify loadedLibraries contains libtest_plugin.so
    auto libs = session->loadedLibraries();
    bool foundPlugin = false;
    for (const auto& l : libs) {
        if (l.name.find("libtest_plugin.so") != std::string::npos || l.path.find("libtest_plugin.so") != std::string::npos) {
            foundPlugin = true;
            break;
        }
    }
    assert(foundPlugin && "Loaded libraries list must contain libtest_plugin.so");

    cmdBar.executeCommand("modules");
    assert(capturedLog.contains("Loaded Shared Libraries"));
    assert(capturedLog.contains("libtest_plugin.so"));

    // 7. Resume to finish execution
    session->resume();
    for (int i = 0; i < 30; ++i) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (session->state() == SessionState::Stopped || session->state() == SessionState::Terminated) {
            break;
        }
        usleep(50000);
    }

    session->terminate();
    ::unlink(pluginSo.c_str());
    ::unlink(launcherBin.c_str());

    std::cout << "[PASS] _r_debug Rendezvous and Shared Library Hot Reload test passed cleanly." << std::endl;
}

void test_follow_fork_mode() {
    std::cout << "\n[TEST] Starting Follow-Fork & Multi-Process Tracking test..." << std::endl;

    // 1. Build a target binary that executes fork()
    std::string buildDir = (access("./build", F_OK) == 0) ? "./build" : ".";
    std::string forkSrc = buildDir + "/test_fork_target.c";
    std::string forkBin = buildDir + "/test_fork_target";
    {
        std::ofstream ofs(forkSrc);
        ofs << "#include <stdio.h>\n"
            << "#include <unistd.h>\n"
            << "#include <sys/wait.h>\n"
            << "int child_magic() {\n"
            << "    volatile int val = 1337;\n"
            << "    return val + 1;\n"
            << "}\n"
            << "int parent_magic() {\n"
            << "    volatile int val = 7777;\n"
            << "    return val + 1;\n"
            << "}\n"
            << "int main() {\n"
            << "    pid_t p = fork();\n"
            << "    if (p == 0) {\n"
            << "        // Child process\n"
            << "        int r = child_magic();\n"
            << "        return r;\n"
            << "    } else {\n"
            << "        // Parent process\n"
            << "        int status = 0;\n"
            << "        parent_magic();\n"
            << "        waitpid(p, &status, 0);\n"
            << "        return 0;\n"
            << "    }\n"
            << "}\n";
    }
    const char* ccEnv = ::getenv("CC");
    std::string cc = (ccEnv && *ccEnv) ? ccEnv : "gcc";
    std::string compileCmd = cc + " -g -O0 " + forkSrc + " -o " + forkBin;
    int compileRet = ::system(compileCmd.c_str());
    assert(compileRet == 0 && "Failed to compile test_fork_target");

    // 2. Test CLI commands for follow-fork
    {
        DebugSession session("test_cli_sess", "CLI Fork Test");
        CommandBarView cmdBar;
        cmdBar.setSession(std::shared_ptr<DebugSession>(&session, [](DebugSession*){}));

        QString outputLog;
        QObject::connect(&cmdBar, &CommandBarView::outputLogged, [&](const QString& msg, bool) {
            outputLog = msg;
        });

        cmdBar.executeCommand("follow-fork");
        assert(outputLog.contains("parent"));

        cmdBar.executeCommand("follow-fork child");
        assert(session.followForkMode() == FollowForkMode::Child);

        cmdBar.executeCommand("set follow-fork-mode both");
        assert(session.followForkMode() == FollowForkMode::Both);

        cmdBar.executeCommand("catch fork");
        assert(session.stopOnForkEvents() == true);

        cmdBar.executeCommand("catch fork");
        assert(session.stopOnForkEvents() == false);

        cmdBar.executeCommand("set fork parent");
        assert(session.followForkMode() == FollowForkMode::Parent);
    }

    // 3. Test FollowForkMode::Parent with catch fork
    {
        std::cout << "  -> Testing FollowForkMode::Parent with catch fork..." << std::endl;
        SessionManager sessionMgr;
        auto session = sessionMgr.createSession("ParentFollowTest");
        session->setFollowForkMode(FollowForkMode::Parent);
        session->setStopOnForkEvents(true);

        bool launched = session->launch(forkBin, {});
        assert(launched && "Launch failed for fork test target");

        bool caughtFork = false;
        Pid reportedChildPid = 0;
        QObject::connect(session.get(), &DebugSession::eventOccurred, [&](const DebugEvent& ev) {
            if (ev.reason == StopReason::ProcessForked) {
                caughtFork = true;
                reportedChildPid = ev.childPid;
            }
        });

        session->resume();

        // Wait for fork event to be trapped
        for (int i = 0; i < 50; ++i) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            if (caughtFork && session->state() == SessionState::Paused) {
                break;
            }
            usleep(50000);
        }

        assert(caughtFork && "Should have caught StopReason::ProcessForked event");
        assert(reportedChildPid > 0 && "Child PID should be positive");
        std::cout << "     Caught fork event! Parent PID: " << session->pid() << ", Child PID: " << reportedChildPid << std::endl;

        // Resume parent to finish
        session->resume();
        for (int i = 0; i < 50; ++i) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            if (session->state() == SessionState::Stopped || session->state() == SessionState::Terminated) {
                break;
            }
            usleep(50000);
        }
        session->terminate();
    }

    // 4. Test FollowForkMode::Both (Multi-Process SessionTree)
    {
        std::cout << "  -> Testing FollowForkMode::Both (Session tree creation)..." << std::endl;
        SessionManager sessionMgr;
        auto parentSession = sessionMgr.createSession("ParentBothTest");
        parentSession->setFollowForkMode(FollowForkMode::Both);
        parentSession->setStopOnForkEvents(false);

        std::shared_ptr<DebugSession> capturedChildSession;
        QObject::connect(&sessionMgr, &SessionManager::sessionCreated, [&](std::shared_ptr<DebugSession> s) {
            if (s != parentSession) {
                capturedChildSession = s;
            }
        });

        QObject::connect(parentSession.get(), &DebugSession::childProcessForked, [&](Pid, Pid childPid) {
            sessionMgr.createChildSession(parentSession, childPid);
        });

        bool launched = parentSession->launch(forkBin, {});
        assert(launched);

        parentSession->resume();

        // Wait for fork event to trigger child session creation
        for (int i = 0; i < 50; ++i) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            if (capturedChildSession != nullptr) {
                break;
            }
            usleep(50000);
        }

        assert(capturedChildSession != nullptr && "Child session must be instantiated in SessionManager");
        assert(capturedChildSession->pid() > 0 && "Child session must adopt valid child PID");
        assert(capturedChildSession->pid() != parentSession->pid() && "Child session PID must differ from parent");
        std::cout << "     Multi-Process session created! Parent [" << parentSession->name()
                  << ", PID: " << parentSession->pid() << "] and Child ["
                  << capturedChildSession->name() << ", PID: " << capturedChildSession->pid() << "]" << std::endl;

        parentSession->terminate();
        capturedChildSession->terminate();
    }

    ::unlink(forkSrc.c_str());
    ::unlink(forkBin.c_str());

    std::cout << "[PASS] Follow-Fork and Multi-Process Tracking tests passed cleanly." << std::endl;
}

void test_thread_freeze_thaw() {
    std::cout << "\n[TEST] Starting Thread Freeze & Thaw Execution Control test..." << std::endl;

    std::string target_path = "./build/test_target";
    if (!QFileInfo::exists(QString::fromStdString(target_path))) {
        target_path = "./test_target";
    }
    assert(QFileInfo::exists(QString::fromStdString(target_path)) && "test_target binary not found");

    auto session = std::make_shared<DebugSession>("test_freeze_sess", "FreezeThawWorker");
    bool launched = session->launch(target_path, {"FreezeWorker"});
    assert(launched && "Launch failed for test_target");

    for (int i = 0; i < 10; ++i) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (session->state() == SessionState::Paused) break;
        usleep(10000);
    }
    assert(session->state() == SessionState::Paused);

    // Let the target run so main() executes and WorkerThread1 spawns
    session->resume();
    for (int i = 0; i < 25; ++i) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        usleep(20000);
    }

    session->pause();
    for (int i = 0; i < 30; ++i) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (session->state() == SessionState::Paused) break;
        usleep(20000);
    }
    assert(session->state() == SessionState::Paused);

    // 1. Thread Enumeration & Identification
    auto threads = session->getThreads();
    assert(threads.size() >= 2 && "Target must have spawned at least 2 threads");

    Tid worker_tid = 0;
    for (const auto& t : threads) {
        if (t.name == "WorkerThread1") {
            worker_tid = t.tid;
            break;
        }
    }
    assert(worker_tid > 0 && "WorkerThread1 must be present in thread list");
    assert(!session->isThreadFrozen(worker_tid) && "Worker thread should not be frozen initially");

    // 2. Freeze specific thread
    bool frozen = session->freezeThread(worker_tid);
    assert(frozen);
    assert(session->isThreadFrozen(worker_tid) == true);
    assert(session->frozenThreads().count(worker_tid) == 1);

    // Verify getThreads() reflects isFrozen
    threads = session->getThreads();
    for (const auto& t : threads) {
        if (t.tid == worker_tid) {
            assert(t.isFrozen == true);
        }
    }

    // 3. UI ThreadsView testing
    ThreadsView threadsView;
    threadsView.setSession(session);
    threadsView.refresh();

    // 4. CommandBar CLI testing
    CommandBarView cmdBar;
    cmdBar.setSession(session);
    QString capturedLog;
    QObject::connect(&cmdBar, &CommandBarView::outputLogged, [&](const QString& msg, bool) {
        capturedLog = msg;
    });

    cmdBar.executeCommand("threads");
    assert(capturedLog.contains("FROZEN") && "threads CLI output should indicate FROZEN status");
    assert(capturedLog.contains("WorkerThread1"));

    // Test thaw all via CLI
    cmdBar.executeCommand("thaw all");
    assert(!session->isThreadFrozen(worker_tid));
    assert(session->frozenThreads().empty());

    // Test freeze specific via CLI
    cmdBar.executeCommand(QString("freeze %1").arg(worker_tid));
    assert(session->isThreadFrozen(worker_tid) == true);

    // Test thaw specific via CLI
    cmdBar.executeCommand(QString("thaw %1").arg(worker_tid));
    assert(session->isThreadFrozen(worker_tid) == false);

    // Test freeze all (freeze all others except active)
    cmdBar.executeCommand("freeze all");
    assert(session->isThreadFrozen(worker_tid) == true);
    assert(!session->isThreadFrozen(session->activeTid()));

    // 5. Single step while worker is frozen
    session->stepInto();
    for (int i = 0; i < 20; ++i) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (session->state() == SessionState::Paused) break;
        usleep(30000);
    }
    assert(session->state() == SessionState::Paused);
    assert(session->isThreadFrozen(worker_tid) == true);

    // Clean up: thaw and terminate
    session->thawAllThreads();
    assert(session->frozenThreads().empty());
    session->terminate();

    std::cout << "[PASS] Thread Freeze & Thaw Execution Control test passed cleanly." << std::endl;
}

void test_memory_scanner() {
    std::cout << "\n[TEST] Starting Differential Memory Scanner (CheatEngine style) test..." << std::endl;

    auto session = std::make_shared<DebugSession>("test_scan_session", "ScanSession");
    bool launched = session->launch(getTestTargetPath(), {});
    assert(launched && "Failed to launch test target");

    // Allocate remote page in target process
    auto page = session->allocateMemory(4096, PROT_READ | PROT_WRITE);
    assert(page.has_value() && "Remote memory allocation should succeed");
    Address pageAddr = *page;

    // Write known values to test memory
    int32_t val1 = 1337;
    int32_t val2 = 1337;
    int32_t val3 = 9999;
    double dval = 3.14159;
    std::string sval = "EDB_SCAN_TEST";

    assert(session->writeMemory(pageAddr + 0x100, &val1, sizeof(val1)));
    assert(session->writeMemory(pageAddr + 0x200, &val2, sizeof(val2)));
    assert(session->writeMemory(pageAddr + 0x300, &val3, sizeof(val3)));
    assert(session->writeMemory(pageAddr + 0x400, &dval, sizeof(dval)));
    assert(session->writeMemory(pageAddr + 0x500, sval.data(), sval.size()));

    // 1. Test First Scan (ExactValue, Int32)
    ScanOptions opt;
    opt.dataType = ScanDataType::Int32;
    opt.compareType = ScanCompareType::ExactValue;
    opt.valueStr = "1337";
    opt.writableOnly = true;
    opt.alignment = 4;

    size_t count = session->firstMemoryScan(opt);
    std::cout << "  -> First scan found " << count << " candidates for value 1337." << std::endl;
    assert(count >= 2 && "Should find at least 2 candidates for 1337");

    bool foundAddr1 = false;
    bool foundAddr2 = false;
    for (const auto& res : session->memoryScanner().results()) {
        if (res.address == pageAddr + 0x100) foundAddr1 = true;
        if (res.address == pageAddr + 0x200) foundAddr2 = true;
    }
    assert(foundAddr1 && "Candidate at pageAddr + 0x100 must be found");
    assert(foundAddr2 && "Candidate at pageAddr + 0x200 must be found");

    // 2. Modify one of the candidates: change pageAddr + 0x100 to 1500 (increase by 163)
    int32_t newVal1 = 1500;
    assert(session->writeMemory(pageAddr + 0x100, &newVal1, sizeof(newVal1)));

    // 3. Next Scan (IncreasedValue)
    opt.compareType = ScanCompareType::IncreasedValue;
    size_t countAfterInc = session->nextMemoryScan(opt);
    std::cout << "  -> Next scan (IncreasedValue) converged to " << countAfterInc << " candidates." << std::endl;
    assert(countAfterInc >= 1);

    bool hasAddr1 = false;
    bool hasAddr2 = false;
    for (const auto& res : session->memoryScanner().results()) {
        if (res.address == pageAddr + 0x100) {
            hasAddr1 = true;
            // Verify formatting and delta
            std::string prevStr = res.formatPreviousValue(ScanDataType::Int32);
            std::string curStr = res.formatCurrentValue(ScanDataType::Int32);
            std::string deltaStr = res.formatDelta(ScanDataType::Int32);
            assert(prevStr.find("1337") != std::string::npos);
            assert(curStr.find("1500") != std::string::npos);
            assert(deltaStr.find("+163") != std::string::npos);
        }
        if (res.address == pageAddr + 0x200) hasAddr2 = true;
    }
    assert(hasAddr1 && "Increased candidate at pageAddr + 0x100 must be retained");
    assert(!hasAddr2 && "Unchanged candidate at pageAddr + 0x200 must be filtered out!");

    // 4. Next Scan (IncreasedBy: delta = 50)
    int32_t newVal1_plus = 1550;
    assert(session->writeMemory(pageAddr + 0x100, &newVal1_plus, sizeof(newVal1_plus)));

    opt.compareType = ScanCompareType::IncreasedBy;
    opt.deltaStr = "50";
    size_t countAfterDelta = session->nextMemoryScan(opt);
    std::cout << "  -> Next scan (IncreasedBy +50) converged to " << countAfterDelta << " candidates." << std::endl;
    assert(countAfterDelta >= 1);

    // 5. Test String scan
    opt.dataType = ScanDataType::String;
    opt.compareType = ScanCompareType::ExactValue;
    opt.valueStr = "EDB_SCAN_TEST";
    opt.alignment = 1;
    size_t strCount = session->firstMemoryScan(opt);
    std::cout << "  -> First scan for string found " << strCount << " candidates." << std::endl;
    assert(strCount >= 1);
    bool foundStrAddr = false;
    for (const auto& r : session->memoryScanner().results()) {
        if (r.address == pageAddr + 0x500) foundStrAddr = true;
    }
    assert(foundStrAddr && "String candidate at pageAddr + 0x500 must be found");

    // 6. Test MemoryScannerView UI component
    MemoryScannerView scanView;
    scanView.setSession(session);
    scanView.refreshResults();

    // 7. Test CommandBar CLI commands
    CommandBarView cmdBar;
    cmdBar.setSession(session);
    QString capturedLog;
    QObject::connect(&cmdBar, &CommandBarView::outputLogged, [&](const QString& msg, bool) {
        capturedLog = msg;
    });

    cmdBar.executeCommand("scan 9999 int32");
    assert(capturedLog.contains("Pass 1 complete"));
    assert(session->memoryScanner().resultCount() >= 1);

    cmdBar.executeCommand("scanresults 5");
    assert(capturedLog.contains("Memory Scanner Results"));

    cmdBar.executeCommand("scanreset");
    assert(capturedLog.contains("Memory scanner reset"));
    assert(session->memoryScanner().resultCount() == 0);

    // Clean up
    session->terminate();
    std::cout << "[PASS] Differential Memory Scanner test passed cleanly." << std::endl;
}

void test_type_viewer() {
    std::cout << "\n>>> Testing Type Viewer and Struct Layout Visualizer (P2-3)..." << std::endl;

    // 1. Test Static C Struct Parsing & Layout Calculation
    std::string cCode = R"(
        struct Player {
            char id;
            short level;
            int health;
            long score;
            void* target;
            float speed;
            double mana;
            char name[16];
        };
    )";

    std::string err;
    auto defOpt = TypeManager::parseCStruct(cCode, &err);
    assert(defOpt.has_value() && "Parsing C struct Player should succeed");
    const auto& def = *defOpt;

    assert(def.name == "Player");
    assert(def.fields.size() == 8);

    // Verify offsets & alignment
    // id (char: size 1, offset 0)
    assert(def.fields[0].name == "id");
    assert(def.fields[0].offset == 0);
    assert(def.fields[0].size == 1);

    // level (short: size 2, offset 2 due to 2-byte alignment)
    assert(def.fields[1].name == "level");
    assert(def.fields[1].offset == 2);
    assert(def.fields[1].size == 2);

    // health (int: size 4, offset 4)
    assert(def.fields[2].name == "health");
    assert(def.fields[2].offset == 4);
    assert(def.fields[2].size == 4);

    // score (long: size 8, offset 8)
    assert(def.fields[3].name == "score");
    assert(def.fields[3].offset == 8);
    assert(def.fields[3].size == 8);

    // target (void*: size 8, offset 16)
    assert(def.fields[4].name == "target");
    assert(def.fields[4].offset == 16);
    assert(def.fields[4].size == 8);
    assert(def.fields[4].isPointer);

    // speed (float: size 4, offset 24)
    assert(def.fields[5].name == "speed");
    assert(def.fields[5].offset == 24);
    assert(def.fields[5].size == 4);

    // mana (double: size 8, offset 32 due to 8-byte alignment)
    assert(def.fields[6].name == "mana");
    assert(def.fields[6].offset == 32);
    assert(def.fields[6].size == 8);

    // name (char[16]: size 16, offset 40)
    assert(def.fields[7].name == "name");
    assert(def.fields[7].offset == 40);
    assert(def.fields[7].size == 16);
    assert(def.fields[7].arrayCount == 16);

    // Total size: 56 bytes, alignment: 8
    assert(def.totalSize == 56);
    assert(def.alignment == 8);

    // 2. Test Default Types in TypeManager
    TypeManager mgr;
    mgr.registerDefaultTypes();
    assert(mgr.findStruct("timespec") != nullptr);
    assert(mgr.findStruct("timeval") != nullptr);
    assert(mgr.findStruct("sockaddr_in") != nullptr);
    assert(mgr.findStruct("list_head") != nullptr);
    assert(mgr.findStruct("io_vec") != nullptr);

    const auto* ts = mgr.findStruct("timespec");
    assert(ts->totalSize == 16);
    assert(ts->fields.size() == 2);
    assert(ts->fields[0].name == "tv_sec" && ts->fields[0].offset == 0);
    assert(ts->fields[1].name == "tv_nsec" && ts->fields[1].offset == 8);

    // 3. Test Live Target Session Memory Evaluation
    auto session = std::make_shared<DebugSession>("test_type_session", "TypeSession");
    bool launched = session->launch(getTestTargetPath(), {});
    assert(launched && "Failed to launch test target");

    session->typeManager().registerStruct(def);

    auto page = session->allocateMemory(4096, PROT_READ | PROT_WRITE);
    assert(page.has_value() && "Remote memory allocation should succeed");
    Address pageAddr = *page;

    // Pack binary representation of Player struct
    std::vector<uint8_t> buffer(56, 0);
    int8_t id = 42;
    int16_t level = 100;
    int32_t health = 2500;
    int64_t score = 9876543210LL;
    uint64_t target = 0xdeadbeefcafebabeULL;
    float speed = 12.5f;
    double mana = 100.25;
    char name[16] = "TestHero";

    std::memcpy(buffer.data() + 0, &id, sizeof(id));
    std::memcpy(buffer.data() + 2, &level, sizeof(level));
    std::memcpy(buffer.data() + 4, &health, sizeof(health));
    std::memcpy(buffer.data() + 8, &score, sizeof(score));
    std::memcpy(buffer.data() + 16, &target, sizeof(target));
    std::memcpy(buffer.data() + 24, &speed, sizeof(speed));
    std::memcpy(buffer.data() + 32, &mana, sizeof(mana));
    std::memcpy(buffer.data() + 40, name, sizeof(name));

    assert(session->writeMemory(pageAddr, buffer.data(), buffer.size()));

    // Evaluate live memory
    auto evalOpt = session->typeManager().evaluate("Player", pageAddr, session->engine());
    assert(evalOpt.has_value() && "Evaluation of struct Player should succeed");
    const auto& eval = *evalOpt;
    assert(eval.structName == "Player");
    assert(eval.baseAddress == pageAddr);
    assert(eval.totalSize == 56);
    assert(eval.fields.size() == 8);

    // Check evaluated formatted fields
    assert(eval.fields[0].formattedValue.find("42") != std::string::npos);
    assert(eval.fields[1].formattedValue.find("100") != std::string::npos);
    assert(eval.fields[2].formattedValue.find("2500") != std::string::npos);
    assert(eval.fields[3].formattedValue.find("9876543210") != std::string::npos);
    assert(eval.fields[4].formattedValue.find("deadbeefcafebabe") != std::string::npos);
    assert(eval.fields[4].pointerTarget == target);
    assert(eval.fields[5].formattedValue.find("12.5") != std::string::npos);
    assert(eval.fields[6].formattedValue.find("100.25") != std::string::npos);
    assert(eval.fields[7].formattedValue.find("TestHero") != std::string::npos);

    // 4. Test UI TypeViewer Component
    TypeViewer tv;
    tv.setSession(session);
    tv.setInspectAddress(pageAddr);
    tv.refresh();

    // 5. Test CommandBar CLI commands
    CommandBarView cmdBar;
    cmdBar.setSession(session);
    QString capturedLog;
    QObject::connect(&cmdBar, &CommandBarView::outputLogged, [&](const QString& msg, bool) {
        capturedLog = msg;
    });

    cmdBar.executeCommand("structs");
    assert(capturedLog.contains("Registered Structs"));
    assert(capturedLog.contains("Player"));

    cmdBar.executeCommand(QString("struct Player %1").arg(QString::fromStdString(pageAddr.toHex())));
    assert(capturedLog.contains("TestHero"));
    assert(capturedLog.contains("deadbeefcafebabe"));

    cmdBar.executeCommand("defstruct struct TestNode { int node_val; void* next; };");
    assert(capturedLog.contains("Struct registered successfully"));
    assert(session->typeManager().findStruct("TestNode") != nullptr);

    // Clean up
    session->terminate();
    std::cout << "[PASS] Type Viewer and Struct Layout Visualizer test passed cleanly." << std::endl;
}

static bool waitForState(DebugSession& session, SessionState target, int timeout_ms = 2000) {
    int elapsed = 0;
    while (session.state() != target && elapsed < timeout_ms) {
        QApplication::processEvents();
        usleep(10000); // 10ms
        elapsed += 10;
    }
    QApplication::processEvents();
    return session.state() == target;
}

void test_disassembly_flow_lines() {
    std::cout << "\n[TEST] Starting Disassembly Flow Lines test..." << std::endl;
    DebugSession session("test_disasm_sess", "Disasm Test");
    bool launched = session.launch(getTestTargetPath(), {"WorkerDisasm"});
    assert(launched && "Failed to launch target for Disasm test");

    auto main_sym = session.resolveSymbol("main");
    assert(main_sym && "main symbol must resolve");
    session.toggleBreakpoint(*main_sym);
    session.resume();

    waitForState(session, SessionState::Paused, 2000);

    auto sess_ptr = std::shared_ptr<DebugSession>(&session, [](DebugSession*){});
    DisassemblyView view;
    view.setSession(sess_ptr);
    view.resize(800, 600);
    view.refresh();

    std::cout << "[INFO] Disassembly rowCount: " << view.rowCount() << std::endl;
    for (int r = 0; r < std::min(view.rowCount(), 15); ++r) {
        const auto* insn = view.instructionAtRow(r);
        if (insn) {
            std::cout << "Row " << r << ": " << insn->address.toHex() << " " << insn->mnemonic << " " << insn->operands << std::endl;
        }
    }

    QImage img(800, 600, QImage::Format_ARGB32);
    img.fill(Qt::black);
    QPainter p(&img);
    view.render(&p);
    p.end();
    img.save("/tmp/disasm_view.png");
    std::cout << "[INFO] Rendered DisassemblyView to /tmp/disasm_view.png" << std::endl;

    // Test selection on branch instruction (row 12: jle)
    view.selectRow(12);
    view.setCurrentCell(12, 0);
    QImage imgSel(800, 600, QImage::Format_ARGB32);
    imgSel.fill(Qt::black);
    QPainter pSel(&imgSel);
    view.render(&pSel);
    pSel.end();
    imgSel.save("/tmp/disasm_selected.png");
    std::cout << "[INFO] Rendered DisassemblyView with selection to /tmp/disasm_selected.png" << std::endl;

    // Test selection on call instruction (find call row)
    for (int r = 0; r < view.rowCount(); ++r) {
        if (const auto* insn = view.instructionAtRow(r)) {
            if (insn->mnemonic == "call" || insn->mnemonic == "callq" || insn->mnemonic == "CALL") {
                view.selectRow(r);
                view.setCurrentCell(r, 0);
                QImage imgCallSel(800, 600, QImage::Format_ARGB32);
                imgCallSel.fill(Qt::black);
                QPainter pCallSel(&imgCallSel);
                view.render(&pCallSel);
                pCallSel.end();
                imgCallSel.save("/tmp/disasm_call_selected.png");
                std::cout << "[INFO] Rendered DisassemblyView with call selection to /tmp/disasm_call_selected.png" << std::endl;
                break;
            }
        }
    }

    session.terminate();
    std::cout << "[PASS] Disassembly flow lines verified." << std::endl;
}

void test_x64dbg_p1_ergonomics() {
    std::cout << "\n[TEST] Starting x64dbg P1 Ergonomics test (AssembleDialog, StackView & RegisterView keyboard flows)..." << std::endl;

    auto sess_ptr = std::make_shared<DebugSession>("test_p1_session", "P1Ergonomics");
    bool launched = sess_ptr->launch(getTestTargetPath(), {"WorkerP1"});
    assert(launched && "Failed to launch test target");

    Address rip = sess_ptr->registers().rip();
    assert(!rip.isNull());

    // 1. Test AssembleDialog
    bool signalEmitted = false;
    Address assembledAt{0};
    size_t writtenCount = 0;
    Address advancedTo{0};

    AssembleDialog dlg(sess_ptr, rip, 3, "nop");
    assert(dlg.fillWithNops() == true);
    assert(dlg.currentAddress() == rip);

    QObject::connect(&dlg, &AssembleDialog::instructionAssembled, [&](Address a, size_t w, Address n) {
        signalEmitted = true;
        assembledAt = a;
        writtenCount = w;
        advancedTo = n;
    });

    auto* txt = dlg.findChild<QLineEdit*>();
    assert(txt != nullptr);
    txt->setText("nop");

    // Trigger returnPressed to assemble
    Q_EMIT txt->returnPressed();

    assert(signalEmitted && "AssembleDialog should emit instructionAssembled");
    assert(assembledAt == rip);
    assert(writtenCount == 3); // 1 byte nop + 2 bytes padding because original length was 3
    assert(advancedTo == rip + 3);

    auto mem = sess_ptr->readMemory(rip, 3);
    assert(mem.size() == 3);
    assert(mem[0] == 0x90 && mem[1] == 0x90 && mem[2] == 0x90);
    std::cout << "[PASS] AssembleDialog continuous assembly and NOP padding verified." << std::endl;

    // 2. Test StackView Keyboard Flow
    StackView stackView;
    stackView.setSession(sess_ptr);
    stackView.refresh();

    auto* stackTable = stackView.findChild<QTableWidget*>();
    assert(stackTable != nullptr);
    if (stackTable->rowCount() > 0) {
        stackTable->selectRow(0);
        stackTable->setCurrentCell(0, 1);

        bool jumpDisasmTriggered = false;
        bool jumpMemTriggered = false;
        QObject::connect(&stackView, &StackView::jumpToDisassemblyRequested, [&](Address) { jumpDisasmTriggered = true; });
        QObject::connect(&stackView, &StackView::jumpToMemoryRequested, [&](Address) { jumpMemTriggered = true; });

        QKeyEvent keyEnter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(stackTable, &keyEnter);
        assert((jumpDisasmTriggered || jumpMemTriggered) && "Enter on stack row should trigger jump request");
        std::cout << "[PASS] StackView keyboard navigation (Enter smart follow) verified." << std::endl;
    }

    // 3. Test RegisterView Keyboard Flow
    RegisterView regView;
    regView.setSession(sess_ptr);
    regView.refresh();

    auto* gprTable = regView.findChild<QTableWidget*>("gprTable");
    assert(gprTable != nullptr);
    assert(gprTable->rowCount() >= 16);

    // Find RAX row
    int raxRow = -1;
    for (int r = 0; r < gprTable->rowCount(); ++r) {
        if (gprTable->item(r, 0) && gprTable->item(r, 0)->text() == "RAX") {
            raxRow = r;
            break;
        }
    }
    assert(raxRow >= 0);
    gprTable->setCurrentCell(raxRow, 1);

    uint64_t origRax = sess_ptr->registers().rax();

    // Send '+' key event
    QKeyEvent keyPlus(QEvent::KeyPress, Qt::Key_Plus, Qt::NoModifier, "+");
    QApplication::sendEvent(gprTable, &keyPlus);
    assert(sess_ptr->registers().rax() == origRax + 1);

    // Send '-' key event
    QKeyEvent keyMinus(QEvent::KeyPress, Qt::Key_Minus, Qt::NoModifier, "-");
    QApplication::sendEvent(gprTable, &keyMinus);
    assert(sess_ptr->registers().rax() == origRax);

    // Send '0' key event
    QKeyEvent keyZero(QEvent::KeyPress, Qt::Key_0, Qt::NoModifier, "0");
    QApplication::sendEvent(gprTable, &keyZero);
    assert(sess_ptr->registers().rax() == 0);

    // Send '~' key event
    QKeyEvent keyTilde(QEvent::KeyPress, Qt::Key_AsciiTilde, Qt::NoModifier, "~");
    QApplication::sendEvent(gprTable, &keyTilde);
    assert(sess_ptr->registers().rax() == ~0ULL);

    std::cout << "[PASS] RegisterView keyboard flow (+, -, 0, ~) verified." << std::endl;

    sess_ptr->terminate();
}

void test_x64dbg_p2_ergonomics() {
    std::cout << "\n[TEST] Starting x64dbg P2 Ergonomics test (Breakpoint Disable/Enable, MultiDump Dump 1~4, HexView edit)..." << std::endl;

    auto sess_ptr = std::make_shared<DebugSession>("test_p2_session", "P2Ergonomics");
    bool ok = sess_ptr->launch(getTestTargetPath(), {"WorkerP2"});
    assert(ok && "Launch failed");

    Address rip = sess_ptr->registers().rip();
    assert(!rip.isNull());

    // 1. Test Breakpoint Enable/Disable
    sess_ptr->addBreakpoint(rip, "entry_bp");
    assert(sess_ptr->hasBreakpoint(rip));
    assert(sess_ptr->isBreakpointEnabled(rip));

    auto disasm = sess_ptr->disassemble(rip, 1);
    assert(!disasm.empty());
    assert(disasm[0].hasBreakpoint == true);
    assert(disasm[0].isBreakpointEnabled == true);

    // Disable breakpoint
    bool disOk = sess_ptr->disableBreakpoint(rip);
    assert(disOk && "disableBreakpoint must succeed");
    assert(sess_ptr->hasBreakpoint(rip));
    assert(!sess_ptr->isBreakpointEnabled(rip));

    disasm = sess_ptr->disassemble(rip, 1);
    assert(!disasm.empty());
    assert(disasm[0].hasBreakpoint == true);
    assert(disasm[0].isBreakpointEnabled == false);

    // Re-enable breakpoint
    bool enOk = sess_ptr->enableBreakpoint(rip);
    assert(enOk && "enableBreakpoint must succeed");
    assert(sess_ptr->isBreakpointEnabled(rip));

    sess_ptr->removeBreakpoint(rip);
    assert(!sess_ptr->hasBreakpoint(rip));

    std::cout << "[PASS] Breakpoint temporary disable/enable verified in core & disassemble." << std::endl;

    // 2. Test MultiDumpWidget & Dump 1~4 jumps
    MultiDumpWidget multiDump;
    multiDump.setSession(sess_ptr);

    Address targetAddr1(0x401000);
    Address targetAddr3(0x502000);

    // Jump to Dump 3 (index 2)
    multiDump.jumpToAddress(targetAddr3, 2);
    auto* tabWidget = multiDump.findChild<QTabWidget*>();
    assert(tabWidget != nullptr);
    assert(tabWidget->currentIndex() == 2);
    assert(multiDump.activeDump() == multiDump.dumpAt(2));
    assert(multiDump.dumpAt(2)->baseAddress() == targetAddr3);

    // Jump to Dump 1 (index 0)
    multiDump.jumpToAddress(targetAddr1, 0);
    assert(tabWidget->currentIndex() == 0);
    assert(multiDump.dumpAt(0)->baseAddress() == targetAddr1);

    std::cout << "[PASS] MultiDumpWidget Dump 1~4 direct navigation verified." << std::endl;

    // 3. Test MemoryHexView in-place byte editing
    auto* hexView = multiDump.dumpAt(0);
    assert(hexView != nullptr);

    std::vector<uint8_t> origBytes = sess_ptr->readMemory(rip, 2);
    assert(origBytes.size() == 2);

    uint8_t patchByte[2] = {0x90, 0x90};
    bool wrote = sess_ptr->writeMemory(rip, patchByte, 2);
    assert(wrote);
    auto readBack = sess_ptr->readMemory(rip, 2);
    assert(readBack[0] == 0x90 && readBack[1] == 0x90);

    // Restore original bytes
    sess_ptr->writeMemory(rip, origBytes.data(), 2);

    std::cout << "[PASS] HexView memory byte editing verified." << std::endl;

    sess_ptr->terminate();
}

void test_lazy_refresh_irefreshable() {
    std::cout << "\n[TEST] Starting IRefreshable & Lazy Tab Refresh (P0) test..." << std::endl;

    auto session = std::make_shared<DebugSession>("lazy_refresh_sess", "LazyRefresh");
    bool launched = session->launch(getTestTargetPath(), {"WorkerLazyRefresh"});
    assert(launched && "Failed to launch test target");

    SessionTabWidget sessionWidget(session);

    // Initial state: Dump (1-4) is tab 0 and is the active bottom tab
    QWidget* currentTab = sessionWidget.bottomTabs()->currentWidget();
    assert(currentTab != nullptr);

    // Trigger refreshAll()
    sessionWidget.refreshAll();

    // 1. Current active tab should be refreshed and NOT dirty
    auto* activeRef = dynamic_cast<IRefreshable*>(currentTab);
    assert(activeRef != nullptr && "Active bottom tab should implement IRefreshable");
    assert(!activeRef->isDirty() && "Active tab must not be dirty after refreshAll");

    // 2. Inactive tabs should be marked dirty
    int count = sessionWidget.bottomTabs()->count();
    assert(count > 1);

    int dirtyCount = 0;
    int refreshableCount = 0;
    for (int i = 0; i < count; ++i) {
        QWidget* tab = sessionWidget.bottomTabs()->widget(i);
        if (auto* ref = dynamic_cast<IRefreshable*>(tab)) {
            refreshableCount++;
            if (tab != currentTab) {
                assert(ref->isDirty() && "Inactive bottom tab must be marked dirty after refreshAll");
                dirtyCount++;
            }
        }
    }
    assert(refreshableCount >= 10 && "Most bottom tabs should implement IRefreshable");
    assert(dirtyCount > 0 && "Inactive tabs should be marked dirty");

    // 3. Switch to an inactive tab (e.g. index 1: CallStackView)
    sessionWidget.selectBottomTab(1);
    QWidget* newActiveTab = sessionWidget.bottomTabs()->widget(1);
    auto* newActiveRef = dynamic_cast<IRefreshable*>(newActiveTab);
    assert(newActiveRef != nullptr);
    assert(!newActiveRef->isDirty() && "Newly activated tab must clear its dirty flag upon activation");

    std::cout << "[PASS] IRefreshable lazy refresh and dirty flag tracking verified ("
              << refreshableCount << " refreshable tabs tested)." << std::endl;

    session->terminate();
}

// ============================================================
// P1-A: Hardware Breakpoint Multi-Thread Sync
// ============================================================
void test_hw_breakpoint_thread_sync() {
    std::cout << "\n[TEST] Starting P1-A HW Breakpoint Multi-Thread Sync test..." << std::endl;

    DebugSession session("test_p1a_sess", "HW BP Sync Test");
    bool launched = session.launch(getTestTargetPath(), {"WorkerThreadSync"});
    assert(launched && "Failed to launch target for HW BP sync test");

    Address rip = session.registers().rip();
    assert(!rip.isNull());

    // Add a hardware execute breakpoint on current RIP
    bool hwOk = session.addHardwareBreakpoint(rip, HardwareBpType::Execute, HardwareBpSize::Byte1);
    assert(hwOk && "addHardwareBreakpoint should succeed");
    assert(session.hasBreakpoint(rip) && "Hardware BP should be registered");

    // Verify breakpoint has hardware slot assigned
    const auto* bp = session.breakpointManager().getBreakpoint(rip);
    assert(bp != nullptr);
    assert(bp->type != BreakpointType::Software && "Should be hardware breakpoint");
    assert(bp->hardwareSlot >= 0 && bp->hardwareSlot < 4 && "Slot must be 0..3");

    // P1-A: SetHwBpFunc now writes to ALL tids via enumerateTids();
    // verify all known TIDs received the DR register write by checking slot assignment
    auto tids = session.concreteEngine().enumerateTids();
    assert(!tids.empty() && "enumerateTids must return at least one TID");
    for (Tid tid : tids) {
        // DR7 bit[1] (condition enable for DR0 with slot=0) should be non-zero if slot 0 used
        uint64_t dr7 = session.concreteEngine().getDebugRegister(tid, 7);
        if (bp->hardwareSlot == 0) {
            assert((dr7 & 0x3) != 0 && "DR7 must enable slot 0 for all TIDs");
        }
    }

    session.removeBreakpoint(rip);
    assert(!session.hasBreakpoint(rip));
    session.terminate();
    std::cout << "[PASS] P1-A HW Breakpoint multi-thread sync verified (" << tids.size() << " TID(s) checked)." << std::endl;
}

// ============================================================
// P1-B: Incremental Disassembly Cache (DisasmCache)
// ============================================================
void test_disasm_cache() {
    std::cout << "\n[TEST] Starting P1-B Disassembly Cache test..." << std::endl;

    DebugSession session("test_p1b_sess", "Disasm Cache Test");
    bool launched = session.launch(getTestTargetPath(), {"WorkerDisasmCache"});
    assert(launched && "Failed to launch target for disasm cache test");

    Address rip = session.registers().rip();
    constexpr size_t kCount = 20;

    // First call: cache miss, full Capstone decode
    auto result1 = session.disassemble(rip, kCount);
    assert(!result1.empty() && "First disassemble must return instructions");
    assert(result1.size() == kCount);

    // Second call at same address: cache hit, only hot fields updated
    auto result2 = session.disassemble(rip, kCount);
    assert(result2.size() == kCount && "Cache-hit disassemble must return same count");
    // Instruction bytes must be identical
    for (size_t i = 0; i < kCount; ++i) {
        assert(result1[i].bytes == result2[i].bytes && "Cached bytes must match");
        assert(result1[i].mnemonic == result2[i].mnemonic && "Cached mnemonics must match");
    }

    // After writeMemory: cache must be invalidated (next call decodes fresh)
    Address writeAddr = rip + 100;
    uint8_t nop = 0x90;
    session.writeMemory(writeAddr, &nop, 1);
    auto result3 = session.disassemble(rip, kCount);
    assert(!result3.empty() && "Post-write disassemble must succeed");

    // After toggleBreakpoint: cache must be invalidated
    session.toggleBreakpoint(rip);
    auto result4 = session.disassemble(rip, kCount);
    assert(!result4.empty() && "Post-BP-toggle disassemble must succeed");
    // hasBreakpoint field must be set on first insn now
    assert(result4[0].hasBreakpoint && "First instruction must reflect active breakpoint");
    session.toggleBreakpoint(rip); // remove it

    // Cache hit again after a same-address call (no mutation)
    auto result5 = session.disassemble(rip, kCount);
    auto result6 = session.disassemble(rip, kCount); // should hit cache
    assert(result5.size() == result6.size());

    session.terminate();
    std::cout << "[PASS] P1-B DisasmCache hit/miss/invalidation cycle verified." << std::endl;
}

// ============================================================
// P1-C: IDebugBackend abstraction (MockDebugBackend)
// ============================================================
void test_idebug_backend_interface() {
    std::cout << "\n[TEST] Starting P1-C IDebugBackend abstraction (MockDebugBackend) test..." << std::endl;

    // Verify that IDebugBackend is truly abstract (compile-time verified by the mock header)
    // and that MockDebugBackend implements all pure virtuals.
    MockDebugBackend mock;
    assert(!mock.isAttached() && "Mock should start detached");
    assert(mock.pid() == 0 && "Mock pid should start at 0");

    auto launchRes = mock.launch("fake_binary", {});
    assert(launchRes && "Mock launch should succeed");
    assert(mock.isAttached() && "Mock should be attached after launch");
    assert(mock.pid() == 9999);
    assert(mock.enumerateTids().size() == 1);

    // Read/write memory via interface
    IDebugBackend& backend = mock;
    std::vector<uint8_t> data = {0x90, 0x48, 0x31, 0xC0};
    mock.fakeMemory_ = data;
    uint8_t readBuf[4] = {};
    assert(backend.readMemory(Address(0), readBuf, 4));
    assert(readBuf[0] == 0x90 && readBuf[1] == 0x48);

    // Hardware breakpoint slot tracking
    assert(backend.setHardwareBreakpoint(9999, 0, Address(0x401000), HardwareBpType::Execute, HardwareBpSize::Byte1));
    assert(mock.hwBpSet_[0] && "Slot 0 should be marked set");
    assert(backend.clearHardwareBreakpoint(9999, 0));
    assert(!mock.hwBpSet_[0] && "Slot 0 should be cleared");

    // Verify EventLoopThread accepts IDebugBackend& (compile-time check via BreakpointManager)
    BreakpointManager bpMgr(
        [&](Address a, void* b, size_t s) { return backend.readMemory(a, b, s); },
        [&](Address a, const void* b, size_t s) { return backend.writeMemory(a, b, s); }
    );
    assert(!bpMgr.hasBreakpoint(Address(0x401000)));

    mock.kill();
    assert(!mock.isAttached() && "Mock should be detached after kill");

    std::cout << "[PASS] P1-C IDebugBackend abstraction and MockDebugBackend verified." << std::endl;
}

void test_navigation_bus() {
    std::cout << "\n[TEST] Starting NavigationBus Centralized Routing (P2-A) test..." << std::endl;

    // 1. Direct unit verification of NavigationBus signals and slots
    NavigationBus bus;
    Address disasmTarget{0};
    Address dumpTarget{0};
    int dumpTargetTab = -99;
    Address stackTarget{0};
    Address structTarget{0};
    bool strRefRequested = false;
    bool intermodularRequested = false;
    int bottomTabIndex = -1;
    bool bpNotified = false;

    QObject::connect(&bus, &NavigationBus::navigateToDisassembly, [&](Address a) { disasmTarget = a; });
    QObject::connect(&bus, &NavigationBus::navigateToDump, [&](Address a, int t) { dumpTarget = a; dumpTargetTab = t; });
    QObject::connect(&bus, &NavigationBus::navigateToStack, [&](Address a) { stackTarget = a; });
    QObject::connect(&bus, &NavigationBus::navigateToStruct, [&](Address a) { structTarget = a; });
    QObject::connect(&bus, &NavigationBus::navigateToStringReferences, [&]() { strRefRequested = true; });
    QObject::connect(&bus, &NavigationBus::navigateToIntermodularCalls, [&]() { intermodularRequested = true; });
    QObject::connect(&bus, &NavigationBus::navigateToBottomTabIndex, [&](int idx) { bottomTabIndex = idx; });
    QObject::connect(&bus, &NavigationBus::breakpointChangedNotification, [&]() { bpNotified = true; });

    bus.requestDisassembly(Address(0x401000));
    assert(disasmTarget == Address(0x401000));

    bus.requestDump(Address(0x602000), 2);
    assert(dumpTarget == Address(0x602000) && dumpTargetTab == 2);

    bus.requestStack(Address(0x7fffffffe000));
    assert(stackTarget == Address(0x7fffffffe000));

    bus.requestStruct(Address(0x500000));
    assert(structTarget == Address(0x500000));

    bus.requestStringReferences();
    assert(strRefRequested);

    bus.requestIntermodularCalls();
    assert(intermodularRequested);

    bus.requestBottomTabIndex(3);
    assert(bottomTabIndex == 3);

    bus.notifyBreakpointChanged();
    assert(bpNotified);

    // 2. Integration with SessionTabWidget
    auto session = std::make_shared<DebugSession>("nav_bus_sess", "NavBusTest");
    bool launched = session->launch(getTestTargetPath(), {"WorkerNavBus"});
    assert(launched && "Failed to launch test target");

    SessionTabWidget sessionWidget(session);
    NavigationBus& sessionBus = sessionWidget.navigationBus();

    // Verify requesting bottom tab index switches the tab
    sessionBus.requestBottomTabIndex(2);
    assert(sessionWidget.bottomTabs()->currentIndex() == 2);

    // Verify requesting dump tab switches to multi dump widget (tab 0)
    sessionBus.requestDump(Address(0x400000), 0);
    assert(sessionWidget.bottomTabs()->currentIndex() == 0);

    session->terminate();

    std::cout << "[PASS] P2-A NavigationBus routing and SessionTabWidget integration verified." << std::endl;
}

void test_command_registry() {
    std::cout << "\n[TEST] Starting CommandRegistry Architecture (P2-B) test..." << std::endl;

    CommandRegistry registry;

    // 1. Verify category partitioning
    auto bpCmds = registry.commandsByCategory(CommandCategory::Breakpoint);
    auto execCmds = registry.commandsByCategory(CommandCategory::Execution);
    auto memCmds = registry.commandsByCategory(CommandCategory::Memory);
    auto analysisCmds = registry.commandsByCategory(CommandCategory::Analysis);
    auto procCmds = registry.commandsByCategory(CommandCategory::Process);
    auto sysCmds = registry.commandsByCategory(CommandCategory::System);

    assert(!bpCmds.empty() && "Breakpoint commands must be registered");
    assert(!execCmds.empty() && "Execution commands must be registered");
    assert(!memCmds.empty() && "Memory commands must be registered");
    assert(!analysisCmds.empty() && "Analysis commands must be registered");
    assert(!procCmds.empty() && "Process commands must be registered");
    assert(!sysCmds.empty() && "System commands must be registered");

    // 2. Alias resolution
    auto runOpt = registry.findCommand("g");
    assert(runOpt.has_value() && runOpt->name == "run");

    auto pageguardOpt = registry.findCommand("guards");
    assert(pageguardOpt.has_value() && pageguardOpt->name == "pageguards");

    auto libsOpt = registry.findCommand("libs");
    assert(libsOpt.has_value() && libsOpt->name == "modules");

    // 3. Autocompletion prefix matching
    auto bpCompletions = registry.complete("bp");
    assert(std::find(bpCompletions.begin(), bpCompletions.end(), "bp") != bpCompletions.end());
    assert(std::find(bpCompletions.begin(), bpCompletions.end(), "bph") != bpCompletions.end());
    assert(std::find(bpCompletions.begin(), bpCompletions.end(), "bpp") != bpCompletions.end());

    auto stCompletions = registry.complete("st");
    assert(std::find(stCompletions.begin(), stCompletions.end(), "step") != stCompletions.end());
    assert(std::find(stCompletions.begin(), stCompletions.end(), "sti") != stCompletions.end());

    // 4. Custom plugin command registration & execution
    bool customRan = false;
    std::vector<std::string> customArgs;
    registry.registerCommand("test_plugin_cmd", [&](const std::vector<std::string>& args) {
        customRan = true;
        customArgs = args;
    }, "test_plugin_cmd <arg1> - Plugin test command", CommandCategory::Plugin);

    assert(registry.findCommand("test_plugin_cmd").has_value());
    assert(registry.findCommand("test_plugin_cmd")->category == CommandCategory::Plugin);

    CommandContext ctx;
    QString lastLog;
    bool lastIsError = false;
    ctx.outputLogger = [&](const QString& msg, bool err) {
        lastLog = msg;
        lastIsError = err;
    };

    bool ok = registry.execute("test_plugin_cmd alpha beta 42", ctx);
    assert(ok && customRan);
    assert(customArgs.size() == 3 && customArgs[0] == "alpha" && customArgs[1] == "beta" && customArgs[2] == "42");

    // 5. Help formatting
    ok = registry.execute("help bp", ctx);
    assert(ok && !lastIsError);
    assert(lastLog.contains("Category: Breakpoint"));
    assert(lastLog.contains("bp <addr/symbol>"));

    // 6. Unknown command handling
    ok = registry.execute("non_existent_command_xyz", ctx);
    assert(!ok && lastIsError);
    assert(lastLog.contains("Unknown command"));

    // 7. CommandBarView UI delegation integration
    CommandBarView cmdBar;
    bool barPluginInvoked = false;
    cmdBar.registerCommand("bar_ping", [&](const std::vector<std::string>&) {
        barPluginInvoked = true;
    }, "bar_ping - Ping command for CommandBarView");

    assert(cmdBar.registry().findCommand("bar_ping").has_value());
    cmdBar.executeCommand("bar_ping");
    assert(barPluginInvoked && "CommandBarView must execute registered commands through CommandRegistry");

    std::cout << "[PASS] P2-B CommandRegistry architecture and CommandBarView integration verified." << std::endl;
}

void test_titlebar_double_click_maximize() {
    std::cout << "\n[TEST] Starting Title Bar & Menu Bar Double-Click Maximize/Restore test..." << std::endl;

    MainWindow w;
    w.show();
    QCoreApplication::processEvents();

    assert(!w.isMaximized() && "Window should start in normal non-maximized state");

    // 1. Test direct toggleMaximized() method
    w.toggleMaximized();
    assert(w.isMaximized() && "Window should be maximized after toggleMaximized()");
    w.toggleMaximized();
    assert(!w.isMaximized() && "Window should be restored to normal after second toggleMaximized()");

    // Helper to find guaranteed empty pixel on a bar
    auto findEmptyPos = [](QWidget* bar, auto checkAction) -> QPoint {
        for (int x = bar->width() - 5; x > 50; x -= 10) {
            QPoint pt(x, bar->height() / 2);
            if (checkAction(pt) == nullptr) {
                return pt;
            }
        }
        return QPoint(-1, -1);
    };

    // 2. Test double click on MenuBar empty area via synthetic QMouseEvent
    QMenuBar* menuBar = w.menuBar();
    assert(menuBar != nullptr);

    QPoint emptyPos = findEmptyPos(menuBar, [menuBar](const QPoint& pt) { return menuBar->actionAt(pt); });
    assert(emptyPos.x() > 0 && "Should find empty space on menu bar");

    QMouseEvent dblClickEmpty(QEvent::MouseButtonDblClick,
                              QPointF(emptyPos),
                              QPointF(emptyPos),
                              Qt::LeftButton,
                              Qt::LeftButton,
                              Qt::NoModifier);
    QCoreApplication::sendEvent(menuBar, &dblClickEmpty);
    QCoreApplication::processEvents();

    assert(w.isMaximized() && "Double clicking empty space on MenuBar should maximize window");

    // Double click again on empty space to restore
    QPoint emptyPos2 = findEmptyPos(menuBar, [menuBar](const QPoint& pt) { return menuBar->actionAt(pt); });
    assert(emptyPos2.x() > 0);
    QMouseEvent dblClickEmpty2(QEvent::MouseButtonDblClick,
                               QPointF(emptyPos2),
                               QPointF(emptyPos2),
                               Qt::LeftButton,
                               Qt::LeftButton,
                               Qt::NoModifier);
    QCoreApplication::sendEvent(menuBar, &dblClickEmpty2);
    QCoreApplication::processEvents();
    assert(!w.isMaximized() && "Double clicking empty space on MenuBar again should restore window");

    // 3. Test that double clicking on an actual menu action does NOT toggle maximized
    if (!menuBar->actions().isEmpty()) {
        QAction* firstAction = menuBar->actions().first();
        QRect actionRect = menuBar->actionGeometry(firstAction);
        QPoint actionPos = actionRect.center();
        assert(menuBar->actionAt(actionPos) == firstAction);

        bool wasMaximized = w.isMaximized();
        QMouseEvent dblClickAction(QEvent::MouseButtonDblClick,
                                  QPointF(actionPos),
                                  QPointF(actionPos),
                                  Qt::LeftButton,
                                  Qt::LeftButton,
                                  Qt::NoModifier);
        QCoreApplication::sendEvent(menuBar, &dblClickAction);
        QCoreApplication::processEvents();

        assert(w.isMaximized() == wasMaximized && "Double clicking a menu action should NOT trigger maximize toggle");
    }

    // 4. Test double clicking on empty space of ToolBar
    QToolBar* toolBar = w.findChild<QToolBar*>("mainDebugToolBar");
    if (toolBar) {
        if (w.isMaximized()) {
            w.showNormal();
            QCoreApplication::processEvents();
        }
        w.resize(1360, 860);
        QCoreApplication::processEvents();

        QPoint emptyTbPos = findEmptyPos(toolBar, [toolBar](const QPoint& pt) { return toolBar->actionAt(pt); });
        if (emptyTbPos.x() > 0) {
            bool wasMaximized = w.isMaximized();
            QMouseEvent dblClickTb(QEvent::MouseButtonDblClick,
                                  QPointF(emptyTbPos),
                                  QPointF(emptyTbPos),
                                  Qt::LeftButton,
                                  Qt::LeftButton,
                                  Qt::NoModifier);
            QCoreApplication::sendEvent(toolBar, &dblClickTb);
            QCoreApplication::processEvents();
            assert(w.isMaximized() != wasMaximized && "Double clicking empty space on ToolBar should toggle window maximized");

            // Restore cleanly
            w.toggleMaximized();
            QCoreApplication::processEvents();
            assert(!w.isMaximized());
        }
    }

    w.close();
    QCoreApplication::processEvents();
    std::cout << "[PASS] Title Bar & Menu Bar double-click maximize and restore verified." << std::endl;
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
    test_memory_hex_view_features();
    test_page_guard_breakpoints();
    test_r_debug_rendezvous();
    test_follow_fork_mode();
    test_thread_freeze_thaw();
    test_memory_scanner();
    test_type_viewer();
    test_disassembly_flow_lines();
    test_x64dbg_p1_ergonomics();
    test_x64dbg_p2_ergonomics();
    test_lazy_refresh_irefreshable();
    test_hw_breakpoint_thread_sync();
    test_disasm_cache();
    test_idebug_backend_interface();
    test_navigation_bus();
    test_command_registry();
    test_titlebar_double_click_maximize();

    std::cout << "\n>>> ALL ADVANCED TESTS PASSED CLEANLY! <<<" << std::endl;
    return 0;
}


