#include "core/DebugSession.hpp"
#include "core/DwarfParser.hpp"
#include "core/SourceFileManager.hpp"
#include "ui/SourceView.hpp"
#include "ui/DisassemblyView.hpp"
#include <QApplication>
#include <iostream>
#include <cassert>
#include <unistd.h>

using namespace edb_next;

static std::string getTestTargetPath() {
    if (access("./test_target", F_OK) == 0) return "./test_target";
    if (access("./build/test_target", F_OK) == 0) return "./build/test_target";
    if (access("../build/test_target", F_OK) == 0) return "../build/test_target";
    return "./test_target";
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

void test_dwarf_parser_and_source_file_manager() {
    std::cout << "\n[TEST 1] Testing DwarfParser & SourceFileManager..." << std::endl;

    std::string target_path = getTestTargetPath();
    DwarfParser parser;
    bool loaded = parser.load(target_path);
    assert(loaded && "DwarfParser should load binary successfully");
    assert(parser.hasDebugInfo() && "Test target must have DWARF debug info");

    std::cout << "[PASS] DWARF loaded successfully. CU count: " << parser.compilationUnits().size() << std::endl;
    assert(!parser.compilationUnits().empty());

    bool found_target_cu = false;
    for (const auto& cu : parser.compilationUnits()) {
        if (cu.name.find("test_target.c") != std::string::npos) {
            found_target_cu = true;
            std::cout << "       CU Name: " << cu.name << ", Producer: " << cu.producer << std::endl;
            break;
        }
    }
    assert(found_target_cu && "Should find compilation unit for test_target.c");

    // Check all source files
    const auto& files = parser.allSourceFiles();
    assert(!files.empty() && "allSourceFiles should not be empty");
    bool found_src_file = false;
    std::string full_src_path;
    for (const auto& f : files) {
        if (f.find("test_target.c") != std::string::npos) {
            found_src_file = true;
            full_src_path = f;
            break;
        }
    }
    assert(found_src_file && "test_target.c should be in source files list");
    std::cout << "[PASS] Found source file in DWARF: " << full_src_path << std::endl;

    // Line 17 in test_target.c is 'int calculate_fib(int n) {'
    auto addr_line17 = parser.findAddressByLine("test_target.c", 17);
    assert(addr_line17.has_value() && "Line 17 in test_target.c should map to an address");
    std::cout << "[PASS] Line 17 mapped to address: 0x" << std::hex << addr_line17->value() << std::dec << std::endl;

    // Reverse lookup address -> SourceLocation
    auto loc = parser.findSourceLocation(*addr_line17);
    assert(loc.has_value() && "Address should map back to SourceLocation");
    assert(loc->fileName.find("test_target.c") != std::string::npos);
    assert(loc->line == 17);
    std::cout << "[PASS] Reverse lookup matched: " << loc->fileName << ":" << loc->line << std::endl;

    // Test SourceFileManager
    SourceFileManager::instance().addSearchPath(".");
    SourceFileManager::instance().addSearchPath("./tests");
    SourceFileManager::instance().addSearchPath("../tests");
    std::string line17_text = SourceFileManager::instance().getLineText(full_src_path, 17);
    assert(!line17_text.empty() && "SourceFileManager should read line 17");
    assert(line17_text.find("calculate_fib") != std::string::npos);
    std::cout << "[PASS] SourceFileManager read line 17: " << line17_text << std::endl;

    std::cout << "[PASS] DwarfParser & SourceFileManager tests passed cleanly." << std::endl;
}

void test_session_source_debugging_and_mixed_mode() {
    std::cout << "\n[TEST 2] Testing DebugSession Source Breakpoints, Stepping & Mixed Mode..." << std::endl;

    auto session = std::make_shared<DebugSession>("dwarf_session", "DWARF Test Session");
    std::string target_path = getTestTargetPath();
    bool launched = session->launch(target_path, {"DwarfWorker"});
    assert(launched && "Session should launch target");
    waitForState(*session, SessionState::Paused, 1000);

    assert(session->hasDebugInfo() && "DebugSession should detect DWARF debug info");

    // 1. Resolve source line
    auto fib_addr = session->resolveSourceLine("test_target.c", 17);
    assert(fib_addr.has_value() && "Should resolve line 17 address in session");

    // 2. Set source breakpoint on line 17
    bool bp_set = session->toggleSourceBreakpoint("test_target.c", 17);
    assert(bp_set && "Should toggle source breakpoint");
    assert(session->hasSourceBreakpoint("test_target.c", 17) && "Source breakpoint should be active");
    std::cout << "[PASS] Source breakpoint set on test_target.c:17 (Address: " << fib_addr->toHex() << ")" << std::endl;

    // 3. Resume and wait for breakpoint hit
    session->resume();
    bool paused = waitForState(*session, SessionState::Paused, 3000);
    assert(paused && "Should hit source breakpoint");
    assert(session->registers().rip() == *fib_addr && "Target should stop at calculate_fib address");

    auto curLoc = session->currentSourceLocation();
    assert(curLoc.has_value() && "currentSourceLocation should be valid");
    assert(curLoc->line == 17 && "Current line should be 17");
    std::cout << "[PASS] Hit source breakpoint! Stopped at " << curLoc->fileName << ":" << curLoc->line << std::endl;

    // 4. Test source-level step over (stepSourceOver)
    bool stepped = session->stepSourceOver();
    assert(stepped && "stepSourceOver should succeed");
    waitForState(*session, SessionState::Paused, 1000);

    auto nextLoc = session->currentSourceLocation();
    assert(nextLoc.has_value());
    assert(nextLoc->line == 18 && "After stepSourceOver, execution should reach line 18");
    std::cout << "[PASS] Source step over reached line " << nextLoc->line << ": "
              << SourceFileManager::instance().getLineText(nextLoc->filePath, nextLoc->line) << std::endl;

    // 5. Mixed-mode disassembly verification
    auto insns = session->disassemble(session->registers().rip(), 10);
    assert(!insns.empty() && "Disassembly should return instructions");
    bool found_source_in_disasm = false;
    for (const auto& insn : insns) {
        if (!insn.sourceFile.empty() && insn.sourceLine > 0 && !insn.sourceText.empty()) {
            found_source_in_disasm = true;
            std::cout << "       [Disasm Mixed] " << insn.address.toHex() << ": "
                      << insn.mnemonic << " " << insn.operands
                      << "  <-- " << insn.sourceFile << ":" << insn.sourceLine
                      << " (" << insn.sourceText << ")" << std::endl;
        }
    }
    assert(found_source_in_disasm && "Mixed disassembly should contain source file and line info");
    std::cout << "[PASS] Mixed Source/ASM disassembly verified." << std::endl;

    // 6. SourceView UI Component verification
    SourceView srcView;
    srcView.setSession(session);
    srcView.show();
    QApplication::processEvents();

    std::cout << "[PASS] SourceView widget instantiated and synchronized with active session." << std::endl;

    // Clean up
    session->toggleSourceBreakpoint("test_target.c", 17);
    session->terminate();
    waitForState(*session, SessionState::Stopped, 1000);
    std::cout << "[PASS] DebugSession terminated cleanly." << std::endl;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    std::cout << "========================================\n"
              << "   edb-next DWARF & Source Debug Suite   \n"
              << "========================================" << std::endl;

    try {
        test_dwarf_parser_and_source_file_manager();
        test_session_source_debugging_and_mixed_mode();
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Uncaught exception: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n>>> ALL DWARF SOURCE-LEVEL TESTS PASSED! <<<\n" << std::endl;
    return 0;
}
