#include "core/DebugSession.hpp"
#include "core/SessionManager.hpp"
#include "core/ExpressionEvaluator.hpp"
#include "core/CapstoneContext.hpp"
#include <QCoreApplication>
#include <iostream>
#include <thread>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <unistd.h>

using namespace edb_next;

bool waitForState(DebugSession& session, SessionState target, int timeout_ms = 2000) {
    int elapsed = 0;
    while (session.state() != target && elapsed < timeout_ms) {
        QCoreApplication::processEvents();
        usleep(10000); // 10ms
        elapsed += 10;
    }
    QCoreApplication::processEvents();
    return session.state() == target;
}

std::string getTestTargetPath() {
    if (access("./test_target", F_OK) == 0) return "./test_target";
    if (access("./build/test_target", F_OK) == 0) return "./build/test_target";
    if (access("../build/test_target", F_OK) == 0) return "../build/test_target";
    return "./test_target";
}

void test_breakpoint_and_stepping() {
    std::cout << "[TEST] Starting DebugSession test..." << std::endl;

    DebugSession session("test_session", "Unit Test");

    std::string target_path = getTestTargetPath();
    bool launched = session.launch(target_path, {"TestWorker"});
    assert(launched && "Failed to launch test_target");
    std::cout << "[PASS] Target launched with PID: " << session.pid() << std::endl;

    // Check registers
    Address entry_rip = session.registers().rip();
    Address entry_rsp = session.registers().rsp();
    assert(!entry_rip.isNull() && "RIP should not be null");
    assert(!entry_rsp.isNull() && "RSP should not be null");
    std::cout << "[PASS] Registers read successfully. RIP: " << entry_rip.toHex()
              << ", RSP: " << entry_rsp.toHex() << std::endl;

    // Disassemble from entry point
    auto instructions = session.disassemble(entry_rip, 10);
    assert(!instructions.empty() && "Disassembly should return instructions");
    std::cout << "[PASS] Disassembled " << instructions.size() << " instructions:" << std::endl;
    for (size_t i = 0; i < std::min<size_t>(5, instructions.size()); ++i) {
        std::cout << "       " << instructions[i].address.toHex() << ": "
                  << instructions[i].mnemonic << " " << instructions[i].operands << std::endl;
    }

    // Set a breakpoint on the second instruction
    Address bp_addr = instructions[1].address;
    bool bp_set = session.toggleBreakpoint(bp_addr);
    assert(bp_set && "Breakpoint should be set");
    assert(session.hasBreakpoint(bp_addr) && "Breakpoint should be registered");
    std::cout << "[PASS] Breakpoint set at: " << bp_addr.toHex() << std::endl;

    // Resume execution to hit the breakpoint
    std::cout << "[INFO] Resuming execution..." << std::endl;
    session.resume();

    // Wait for event loop to catch breakpoint and transition state to Paused
    bool paused = waitForState(session, SessionState::Paused, 2000);
    assert(paused && "Session should pause on breakpoint");

    Address current_rip = session.registers().rip();
    std::cout << "[DEBUG] current_rip: " << current_rip.toHex() << ", bp_addr: " << bp_addr.toHex() << std::endl;
    assert(current_rip == bp_addr && "RIP must be adjusted to breakpoint address");
    std::cout << "[PASS] Breakpoint hit cleanly! Current RIP: " << current_rip.toHex() << std::endl;

    // Single step
    std::cout << "[INFO] Stepping single instruction..." << std::endl;
    session.stepInto();

    bool stepped = waitForState(session, SessionState::Paused, 1000);
    assert(stepped && "Session should be paused after single-step");

    Address stepped_rip = session.registers().rip();
    assert(stepped_rip != current_rip && "RIP must advance after single-step");
    std::cout << "[PASS] Single-step succeeded! New RIP: " << stepped_rip.toHex() << std::endl;

    // Test Memory Read
    auto mem = session.readMemory(entry_rsp, 16);
    assert(mem.size() == 16 && "Should read 16 bytes of stack");
    std::cout << "[PASS] Memory read test passed (16 bytes from stack)." << std::endl;

    // Terminate
    session.terminate();
    assert(session.state() == SessionState::Stopped && "Session should be stopped");
    std::cout << "[PASS] Session terminated cleanly." << std::endl;
}

void test_multi_session() {
    std::cout << "\n[TEST] Starting SessionManager multi-session test..." << std::endl;
    SessionManager mgr;

    auto sess1 = mgr.createSession("Target A");
    auto sess2 = mgr.createSession("Target B");

    assert(mgr.sessionCount() == 2 && "Should have 2 sessions");
    assert(sess1->id() != sess2->id() && "Session IDs must be distinct");

    bool ok1 = sess1->launch(getTestTargetPath(), {"Worker1"});
    bool ok2 = sess2->launch(getTestTargetPath(), {"Worker2"});

    assert(ok1 && ok2 && "Both sessions should launch independently");
    assert(sess1->pid() != sess2->pid() && "PIDs of both sessions must be different");

    std::cout << "[PASS] Multi-session parallel launch passed! Session1 PID: "
              << sess1->pid() << ", Session2 PID: " << sess2->pid() << std::endl;

    sess1->terminate();
    sess2->terminate();
    std::cout << "[PASS] Both sessions terminated cleanly." << std::endl;
}

void test_symbols_and_memory_search() {
    std::cout << "\n[TEST] Starting ELF symbol resolution & memory search test..." << std::endl;

    DebugSession session("test_symbol_session", "Symbol Test");
    bool launched = session.launch(getTestTargetPath(), {"WorkerSearchTest"});
    assert(launched && "Failed to launch test_target");

    // 1. Verify symbol resolution for functions in test_target
    auto main_sym = session.resolveSymbol("main");
    assert(main_sym.has_value() && "Symbol 'main' should be resolved");
    std::cout << "[PASS] Resolved 'main' at address: " << main_sym->toHex() << std::endl;

    auto fib_sym = session.resolveSymbol("calculate_fib");
    assert(fib_sym.has_value() && "Symbol 'calculate_fib' should be resolved");
    std::cout << "[PASS] Resolved 'calculate_fib' at address: " << fib_sym->toHex() << std::endl;

    auto print_sym = session.resolveSymbol("print_secret_message");
    assert(print_sym.has_value() && "Symbol 'print_secret_message' should be resolved");
    std::cout << "[PASS] Resolved 'print_secret_message' at address: " << print_sym->toHex() << std::endl;

    // 2. Disassemble at calculate_fib and verify symbol annotation
    auto fib_insns = session.disassemble(*fib_sym, 5);
    assert(!fib_insns.empty() && "Should disassemble calculate_fib");
    std::cout << "[PASS] Disassembly at calculate_fib[0]: " << fib_insns[0].mnemonic << " "
              << fib_insns[0].operands << " " << fib_insns[0].symbol << std::endl;
    assert(fib_insns[0].symbol.find("calculate_fib") != std::string::npos && "Symbol annotation should match");

    // 3. Test Register Modification
    auto regs = session.registers();
    uint64_t test_val = 0x1234567890ABCDEFULL;
    regs.raw().rbx = test_val;
    bool reg_ok = session.setRegisters(regs);
    assert(reg_ok && "setRegisters should succeed");
    assert(session.registers().rbx() == test_val && "RBX register must be updated to test value");
    std::cout << "[PASS] Register RBX modified and verified successfully: 0x"
              << std::hex << session.registers().rbx() << std::dec << std::endl;

    // 4. Test Memory Search
    // Search for string "WorkerSearchTest" in memory regions
    std::string needle = "WorkerSearchTest";
    std::vector<uint8_t> pattern(needle.begin(), needle.end());

    Address found_addr(0);
    for (const auto& reg : session.memoryRegions()) {
        if (reg.isReadable() && (reg.pathname.find("stack") != std::string::npos || reg.pathname.find("test_target") != std::string::npos)) {
            size_t size = reg.end.value() - reg.start.value();
            auto res = session.searchMemory(reg.start, size, pattern);
            if (res) {
                found_addr = *res;
                break;
            }
        }
    }
    if (found_addr.isNull()) {
        for (const auto& reg : session.memoryRegions()) {
            if (reg.isReadable()) {
                size_t size = reg.end.value() - reg.start.value();
                auto res = session.searchMemory(reg.start, size, pattern);
                if (res) {
                    found_addr = *res;
                    break;
                }
            }
        }
    }
    assert(!found_addr.isNull() && "Should find pattern string in memory");
    std::cout << "[PASS] Memory search found pattern '" << needle << "' at address: "
              << found_addr.toHex() << std::endl;

    session.terminate();
    std::cout << "[PASS] Symbol & memory search test completed cleanly." << std::endl;
}

void test_call_stack_and_hardware_breakpoints() {
    std::cout << "\n[TEST] Starting Call Stack unwinding & Hardware Breakpoint test..." << std::endl;

    DebugSession session("test_callstack_session", "CallStack Test");
    bool launched = session.launch(getTestTargetPath(), {"CallStackWorker"});
    assert(launched && "Failed to launch test_target");

    auto fib_sym = session.resolveSymbol("calculate_fib");
    assert(fib_sym.has_value() && "calculate_fib must be found");
    std::cout << "[INFO] calculate_fib resolved to: " << fib_sym->toHex() << std::endl;

    // 1. Set a breakpoint on calculate_fib
    bool bp_ok = session.addBreakpoint(*fib_sym, "calculate_fib");
    assert(bp_ok && "addBreakpoint on calculate_fib should succeed");

    std::cout << "[INFO] Resuming until calculate_fib is reached..." << std::endl;
    session.resume();

    bool paused = waitForState(session, SessionState::Paused, 3000);
    assert(paused && "Should pause when calculate_fib is called");
    std::cout << "[PASS] Target paused at: " << session.registers().rip().toHex() << std::endl;

    // 2. Unwind call stack
    auto frames = session.callStack();
    std::cout << "[PASS] Unwound " << frames.size() << " stack frames:" << std::endl;
    for (const auto& f : frames) {
        std::cout << "       #" << f.frameIndex << " IP: " << f.ip.toHex()
                  << " " << f.functionSymbol << " in " << f.moduleName
                  << " (RBP: " << f.frameBase.toHex() << ")" << std::endl;
    }

    assert(frames.size() >= 2 && "Should have at least 2 stack frames (calculate_fib and caller)");
    assert(frames[0].functionSymbol.find("calculate_fib") != std::string::npos && "Frame 0 should be calculate_fib");

    // 3. Test Hardware Breakpoint
    session.removeBreakpoint(*fib_sym);

    bool hw_ok = session.addHardwareBreakpoint(*fib_sym, HardwareBpType::Execute, HardwareBpSize::Byte1, "calculate_fib_hw");
    assert(hw_ok && "addHardwareBreakpoint should succeed");
    std::cout << "[PASS] Hardware breakpoint set on calculate_fib." << std::endl;

    auto bps = session.breakpoints();
    bool found_hw = false;
    for (const auto& b : bps) {
        if (b.type == BreakpointType::HardwareExecute && b.address == *fib_sym) {
            found_hw = true;
            std::cout << "[PASS] Hardware breakpoint registered in slot DR" << b.hardwareSlot << std::endl;
            break;
        }
    }
    assert(found_hw && "Hardware breakpoint must be registered in session");

    session.removeBreakpoint(*fib_sym);
    session.terminate();
    std::cout << "[PASS] Call Stack & Hardware Breakpoints test passed cleanly." << std::endl;
}

#include "core/StringScanner.hpp"
#include "core/FunctionFinder.hpp"
#include "core/HeapAnalyzer.hpp"

void test_phase3_1_execution_controls_eflags_and_strings() {
    std::cout << "\n[TEST] Starting Phase 3.1 Advanced Controls, EFLAGS, and String Scanner test..." << std::endl;

    DebugSession session("phase3_1", "Phase 3.1 Test");
    bool launched = session.launch(getTestTargetPath(), {"Phase31Worker"});
    assert(launched && "Launch failed");
    waitForState(session, SessionState::Paused, 1000);

    // 1. EFLAGS interactive bit decomposition
    auto regs = session.registers();
    bool orig_zf = regs.flagZF();
    regs.toggleFlag(6); // bit 6 = ZF
    assert(regs.flagZF() == !orig_zf && "toggleFlag(6) must flip ZF");
    session.setRegisters(regs);
    assert(session.registers().flagZF() == !orig_zf && "Registers must reflect updated ZF");

    // Restore ZF
    regs.toggleFlag(6);
    session.setRegisters(regs);
    assert(session.registers().flagZF() == orig_zf);
    std::cout << "[PASS] EFLAGS interactive bit toggle verified." << std::endl;

    // 2. Run to Cursor (F4)
    auto fib_sym = session.resolveSymbol("calculate_fib");
    assert(fib_sym.has_value());
    session.runTo(*fib_sym);
    bool paused = waitForState(session, SessionState::Paused, 2000);
    assert(paused && "runTo should pause at target");
    assert(session.registers().rip() == *fib_sym && "RIP should equal calculate_fib");
    std::cout << "[PASS] Run to Selection (runTo) reached calculate_fib cleanly: " << session.registers().rip().toHex() << std::endl;

    // 3. Step Out (Shift+F11)
    session.stepOut();
    bool stepped_out = waitForState(session, SessionState::Paused, 2000);
    assert(stepped_out && "stepOut should pause in caller");
    assert(session.registers().rip() != *fib_sym && "RIP should be in caller");
    std::cout << "[PASS] Step Out reached caller frame: " << session.registers().rip().toHex() << std::endl;

    // 4. String Scanner
    auto strings = StringScanner::scan(session, 4, 1000);
    assert(!strings.empty() && "StringScanner should find strings in process");
    std::cout << "[PASS] StringScanner found " << strings.size() << " strings in process memory." << std::endl;

    // 5. Symbol Viewer listing
    const auto& all_syms = session.symbols().allSymbols();
    assert(!all_syms.empty() && "ElfParser should contain symbols");
    std::cout << "[PASS] ElfParser verified " << all_syms.size() << " symbols loaded." << std::endl;

    session.terminate();
    std::cout << "[PASS] Phase 3.1 tests completed cleanly." << std::endl;
}

void test_phase3_2_patching_comments_and_proc_info() {
    std::cout << "\n[TEST] Starting Phase 3.2 Byte Patching, Annotations, and Process Info test..." << std::endl;

    DebugSession session("phase3_2", "Phase 3.2 Test");
    bool launched = session.launch(getTestTargetPath(), {"Phase32Worker"});
    assert(launched && "Launch failed");
    waitForState(session, SessionState::Paused, 1000);

    auto fib_sym = session.resolveSymbol("calculate_fib");
    assert(fib_sym.has_value());

    // 1. Instruction & Byte Patching
    auto orig_bytes = session.readMemory(*fib_sym, 4);
    assert(orig_bytes.size() == 4);

    std::vector<uint8_t> nop_patch = {0x90, 0x90, 0x90, 0x90};
    bool patched = session.writeMemory(*fib_sym, nop_patch.data(), 4);
    assert(patched && "writeMemory should succeed");

    auto read_back = session.readMemory(*fib_sym, 4);
    assert(read_back == nop_patch && "Memory should contain 4 NOP bytes");

    auto disasm = session.disassemble(*fib_sym, 2);
    assert(!disasm.empty() && disasm[0].mnemonic == "nop" && "Disassembly should reflect patched NOP");
    std::cout << "[PASS] Byte patching and live disassembly update verified." << std::endl;

    // Restore original bytes
    session.writeMemory(*fib_sym, orig_bytes.data(), 4);
    auto disasm_restored = session.disassemble(*fib_sym, 2);
    assert(disasm_restored[0].mnemonic != "nop");
    std::cout << "[PASS] Original opcodes restored successfully." << std::endl;

    // 2. AnnotationManager (Comments, Labels & Bookmarks)
    session.annotations().setComment(*fib_sym, "Recursive Fibonacci calculation start");
    assert(session.annotations().getComment(*fib_sym) == "Recursive Fibonacci calculation start");
    assert(session.annotations().hasComment(*fib_sym));

    // Labels & Symbol resolution
    session.annotations().setLabel(*fib_sym, "fib_entry_custom");
    assert(session.annotations().hasLabel(*fib_sym));
    assert(session.annotations().getLabel(*fib_sym) == "fib_entry_custom");
    auto found_addr = session.annotations().findAddressByLabel("fib_entry_custom");
    assert(found_addr.has_value() && *found_addr == *fib_sym);
    auto resolved_sym = session.resolveSymbol("fib_entry_custom");
    assert(resolved_sym.has_value() && *resolved_sym == *fib_sym);

    Address bmk1 = *fib_sym;
    Address bmk2 = *fib_sym + 0x20;
    session.annotations().setBookmark(bmk1, true);
    session.annotations().setBookmark(bmk2, true);
    assert(session.annotations().isBookmarked(bmk1));
    assert(session.annotations().isBookmarked(bmk2));

    auto next_bmk = session.annotations().nextBookmark(bmk1);
    assert(next_bmk.has_value() && *next_bmk == bmk2);
    std::cout << "[PASS] AnnotationManager comments, labels and bookmark navigation verified." << std::endl;

    // 3. Process Introspection
    assert(session.pid() > 0);
    std::string cmdPath = "/proc/" + std::to_string(session.pid()) + "/cmdline";
    assert(access(cmdPath.c_str(), R_OK) == 0 && "Target /proc/cmdline should be accessible");
    std::string statPath = "/proc/" + std::to_string(session.pid()) + "/status";
    assert(access(statPath.c_str(), R_OK) == 0 && "Target /proc/status should be accessible");
    std::cout << "[PASS] Process properties and /proc introspection verified." << std::endl;

    session.terminate();
    std::cout << "[PASS] Phase 3.2 tests completed cleanly." << std::endl;
}

void test_phase3_3_fpu_functions_and_heap() {
    std::cout << "\n[TEST] Starting Phase 3.3 SSE/FPU, Function Boundary, and Heap Analyzer test..." << std::endl;

    DebugSession session("phase3_3", "Phase 3.3 Test");
    bool launched = session.launch(getTestTargetPath(), {"Phase33Worker"});
    assert(launched && "Launch failed");
    waitForState(session, SessionState::Paused, 1000);

    // 1. SSE / AVX Floating-Point Registers
    auto fpregs = session.fpRegisters();
    fpregs.xmm_space[0] = 0xdeadbeef;
    fpregs.xmm_space[1] = 0xcafebabe;
    bool fp_set = session.setFpRegisters(fpregs);
    assert(fp_set && "setFpRegisters should succeed");

    auto fpregs_read = session.fpRegisters();
    assert(fpregs_read.xmm_space[0] == 0xdeadbeef);
    assert(fpregs_read.xmm_space[1] == 0xcafebabe);
    std::cout << "[PASS] SSE XMM0 register modification and readback verified." << std::endl;

    // 2. Function Boundary Finder
    auto fib_sym = session.resolveSymbol("calculate_fib");
    assert(fib_sym.has_value());

    auto enclosing = FunctionFinder::findEnclosingFunction(session, *fib_sym + 4);
    assert(enclosing.has_value());
    assert(enclosing->startAddress == *fib_sym);
    std::cout << "[PASS] FunctionFinder detected function: " << enclosing->name
              << " from " << enclosing->startAddress.toHex()
              << " to " << enclosing->endAddress.toHex()
              << " (" << enclosing->size << " bytes)" << std::endl;

    // 3. Glibc Heap Chunk Analyzer
    // Resume briefly to allow target to execute loop iterations and call printf / glibc malloc
    session.resume();
    usleep(50000); // 50ms
    session.pause();
    waitForState(session, SessionState::Paused, 1000);

    auto chunks = HeapAnalyzer::analyze(session);
    std::cout << "[PASS] HeapAnalyzer executed successfully. Analyzed " << chunks.size() << " glibc heap chunks." << std::endl;
    for (size_t i = 0; i < std::min<size_t>(3, chunks.size()); ++i) {
        std::cout << "       Chunk #" << i << " at " << chunks[i].chunkAddress.toHex()
                  << " User: " << chunks[i].userAddress.toHex()
                  << " Size: " << chunks[i].actualSize << " bytes"
                  << " Status: " << chunks[i].status << std::endl;
    }

    session.terminate();
    std::cout << "[PASS] Phase 3.3 tests completed cleanly." << std::endl;
}

void test_phase4_threads_assembler_and_conditional_bp() {
    std::cout << "\n[TEST] Starting Phase 4 Threads, Assembler, and Conditional Breakpoints test..." << std::endl;

    // 1. Assembler test
    auto asm_res1 = Assembler::assemble("xor eax, eax");
    assert(asm_res1 && "Assembler should succeed for 'xor eax, eax'");
    assert(asm_res1.value.size() == 2 && asm_res1.value[0] == 0x31 && asm_res1.value[1] == 0xc0);
    std::cout << "[PASS] Assembler verified: 'xor eax, eax' -> 31 c0" << std::endl;

    auto asm_res2 = Assembler::assemble("nop");
    assert(asm_res2 && asm_res2.value.size() == 1 && asm_res2.value[0] == 0x90);
    std::cout << "[PASS] Assembler verified: 'nop' -> 90" << std::endl;

    auto asm_mem = Assembler::assemble("mov rdi, [rbp - 8]");
    assert(asm_mem && asm_mem.value.size() == 4);
    assert(asm_mem.value[0] == 0x48 && asm_mem.value[1] == 0x8b && asm_mem.value[2] == 0x7d && asm_mem.value[3] == 0xf8);
    std::cout << "[PASS] Assembler verified: 'mov rdi, [rbp - 8]' -> 48 8b 7d f8" << std::endl;

    // Relative branch resolution test with origin
    auto asm_jmp = Assembler::assemble("jmp 0x401050", Address(0x401000));
    assert(asm_jmp && asm_jmp.value.size() == 2);
    assert(asm_jmp.value[0] == 0xeb && asm_jmp.value[1] == 0x4e);
    std::cout << "[PASS] Assembler verified relative branch: 'jmp 0x401050' from 0x401000 -> eb 4e" << std::endl;

    auto asm_bad = Assembler::assemble("invalid_opcode_xyz 123");
    assert(!asm_bad && "Assembler should fail on invalid instruction");
    std::cout << "[PASS] Assembler error handling verified: " << asm_bad.error << std::endl;

    // High-frequency benchmark: 1000 in-memory assemblies
    auto start_time = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        auto bench_res = Assembler::assemble("mov eax, 1");
        assert(bench_res && bench_res.value.size() == 5);
    }
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();
    std::cout << "[PASS] In-memory assembler benchmark: 1000 instructions assembled in " << elapsed_ms << "ms." << std::endl;

    // 2. ExpressionEvaluator test
    RegisterContext dummy_regs;
    dummy_regs.raw().rax = 0x100;
    dummy_regs.raw().rdi = 42;

    auto val_eval = ExpressionEvaluator::evaluateValue("rax + 0x20", dummy_regs);
    assert(val_eval.has_value() && *val_eval == 0x120);
    std::cout << "[PASS] ExpressionEvaluator value evaluation verified: rax + 0x20 = 0x120" << std::endl;

    bool cond_true = ExpressionEvaluator::evaluateCondition("rdi == 42", dummy_regs);
    bool cond_false = ExpressionEvaluator::evaluateCondition("rdi == 99", dummy_regs);
    assert(cond_true && !cond_false);
    std::cout << "[PASS] ExpressionEvaluator condition evaluation verified." << std::endl;

    // 3. Threads and Active TID test
    DebugSession session("phase4_test", "Phase 4 Test");
    bool launched = session.launch(getTestTargetPath(), {"Phase4Worker"});
    assert(launched && "Launch failed");
    waitForState(session, SessionState::Paused, 1000);

    // Let target run for 100ms so the worker thread starts
    session.resume();
    usleep(100000);
    session.pause();
    waitForState(session, SessionState::Paused, 1000);

    auto threads = session.getThreads();
    std::cout << "[PASS] Target thread enumeration detected " << threads.size() << " threads." << std::endl;
    assert(threads.size() >= 1 && "Should detect at least the main thread");
    for (const auto& t : threads) {
        std::cout << "       TID: " << t.tid << " Name: " << t.name << " State: " << t.state
                  << " RIP: " << t.rip.toHex() << " Symbol: " << t.symbol << std::endl;
    }

    if (threads.size() > 1) {
        Tid worker_tid = threads[1].tid;
        bool switched = session.switchThread(worker_tid);
        assert(switched && session.activeTid() == worker_tid);
        std::cout << "[PASS] Switched active thread to TID " << worker_tid << " successfully." << std::endl;
        session.switchThread(threads[0].tid);
    }

    // 4. Conditional Breakpoint Test
    auto fib_sym = session.resolveSymbol("calculate_fib");
    assert(fib_sym.has_value());
    session.addBreakpoint(*fib_sym, "calculate_fib");
    session.setBreakpointCondition(*fib_sym, "rdi == 7");

    // Resume execution until calculate_fib is reached with rdi == 7
    session.resume();
    waitForState(session, SessionState::Paused, 2000);
    assert(session.state() == SessionState::Paused);
    assert(session.registers().rdi() == 7 && "Conditional breakpoint should only stop when rdi == 7");
    std::cout << "[PASS] Conditional breakpoint triggered cleanly with rdi == 7." << std::endl;

    session.terminate();
    std::cout << "[PASS] Phase 4 tests completed cleanly." << std::endl;
}

void test_phase5_rop_branch_prediction_xrefs_and_pattern() {
    std::cout << "\n[TEST] Starting Phase 5 ROP Scanner, Branch Prediction, XREFS, and Pattern Search test..." << std::endl;

    DebugSession session("phase5_test", "Phase 5 Test");
    bool launched = session.launch(getTestTargetPath(), {"Phase5Worker"});
    assert(launched && "Launch failed");
    waitForState(session, SessionState::Paused, 1000);

    // 1. ROP Scanner test
    auto gadgets = session.scanROP(4, 20);
    assert(!gadgets.empty() && "ROP scanner should discover gadgets in executable memory");
    std::cout << "[PASS] ROP Scanner found " << gadgets.size() << " gadgets in target process." << std::endl;
    std::cout << "       Gadget #0: " << gadgets[0].address.toHex() << " : "
              << gadgets[0].disassembly << " [" << gadgets[0].category << "]" << std::endl;

    // 2. Instruction Inspector and Dynamic Branch Prediction
    auto fib_sym = session.resolveSymbol("calculate_fib");
    assert(fib_sym.has_value());
    session.runTo(*fib_sym);
    waitForState(session, SessionState::Paused, 1000);

    // Inspect instruction at calculate_fib
    auto details = session.inspectInstruction(*fib_sym);
    std::cout << "[PASS] InstructionInspector inspected " << fib_sym->toHex() << ": "
              << details.mnemonic << " " << details.operands << std::endl;

    // Step to the cmp / jle in calculate_fib
    session.stepInto();
    waitForState(session, SessionState::Paused, 1000);
    auto step_details = session.inspectInstruction(session.registers().rip());
    std::cout << "[PASS] Dynamic branch prediction result: " << (step_details.isBranch ? step_details.summary : "Non-branch instruction") << std::endl;

    // 3. Code-to-Code Cross References (XREFS)
    auto secret_sym = session.resolveSymbol("print_secret_message");
    assert(secret_sym.has_value());
    auto xrefs = session.findCodeXRefs(*secret_sym);
    std::cout << "[PASS] CodeXRefFinder discovered " << xrefs.size() << " references to print_secret_message." << std::endl;
    assert(!xrefs.empty() && "Should find call to print_secret_message in main loop");
    for (const auto& ref : xrefs) {
        std::cout << "       XREF from: " << ref.sourceAddress.toHex()
                  << " (" << ref.sourceFunction << ") [" << ref.type << "] "
                  << ref.mnemonic << " " << ref.operands << std::endl;
    }

    // 4. Pattern Searcher with Wildcards
    // Search for endbr64 (f3 0f 1e fa) with wildcard: "f3 0f ?? fa"
    auto matches = session.searchPattern("f3 0f ?? fa", true);
    std::cout << "[PASS] PatternSearcher found " << matches.size() << " matches for 'f3 0f ?? fa'." << std::endl;
    assert(!matches.empty() && "Should find endbr64 instructions in target");

    session.terminate();
    std::cout << "[PASS] Phase 5 tests completed cleanly." << std::endl;
}

void test_capstone_context_and_lru_cache() {
    std::cout << "[TEST] Starting CapstoneContext and DisasmCache LRU tests..." << std::endl;

    // 1. Basic handle acquisition and reuse
    {
        auto cs1 = CapstoneContext::acquire(false);
        assert(cs1.isValid());
        const uint8_t code[] = {0x90}; // nop
        cs_insn* insn = nullptr;
        size_t count = cs_disasm(cs1.get(), code, sizeof(code), 0x1000, 1, &insn);
        assert(count == 1 && insn != nullptr);
        assert(std::string(insn[0].mnemonic) == "nop");
        cs_free(insn, count);
    }
    // After lease destruction, second acquire in same thread reuses the pooled handle
    {
        auto cs2 = CapstoneContext::acquire(false);
        assert(cs2.isValid());
    }

    // 2. Detail mode handle
    {
        auto cs_det = CapstoneContext::acquire(true);
        assert(cs_det.isValid());
        const uint8_t code[] = {0x48, 0x89, 0xd8}; // mov rax, rbx
        cs_insn* insn = nullptr;
        size_t count = cs_disasm(cs_det.get(), code, sizeof(code), 0x2000, 1, &insn);
        assert(count == 1 && insn != nullptr);
        assert(insn[0].detail != nullptr && "Detail mode must populate insn detail");
        assert(insn[0].detail->x86.op_count == 2);
        cs_free(insn, count);
    }

    // 3. Re-entrancy on same thread
    {
        auto lease1 = CapstoneContext::acquire(false);
        assert(lease1.isValid());
        // Nested acquire while lease1 is still alive
        auto lease2 = CapstoneContext::acquire(false);
        assert(lease2.isValid());
        assert(lease1.get() != lease2.get() && "Reentrant acquire should return distinct standalone handle");
    }

    // 4. Multithreading concurrency test
    {
        std::atomic<bool> threadsOk{true};
        std::vector<std::thread> workers;
        for (int t = 0; t < 4; ++t) {
            workers.emplace_back([&threadsOk, t]() {
                for (int i = 0; i < 50; ++i) {
                    auto cs = CapstoneContext::acquire(t % 2 == 0);
                    if (!cs.isValid()) {
                        threadsOk = false;
                        return;
                    }
                    const uint8_t code[] = {0x55, 0x48, 0x89, 0xe5}; // push rbp; mov rbp, rsp
                    cs_insn* insn = nullptr;
                    size_t count = cs_disasm(cs.get(), code, sizeof(code), 0x400000 + i * 16, 2, &insn);
                    if (count != 2) {
                        threadsOk = false;
                    }
                    if (insn) {
                        cs_free(insn, count);
                    }
                }
            });
        }
        for (auto& w : workers) {
            w.join();
        }
        assert(threadsOk && "Multithreaded CapstoneContext acquisition failed");
        std::cout << "[PASS] CapstoneContext concurrent thread-local acquisition verified." << std::endl;
    }

    // 5. DisasmCache Multi-entry LRU unit test
    {
        DebugSession::DisasmCache cache;
        assert(cache.find(Address(0x1000), 10) == nullptr);

        // Populate entries up to limit (8 entries)
        for (uint64_t i = 1; i <= 8; ++i) {
            std::vector<DisassembledInstruction> insns;
            DisassembledInstruction insn;
            insn.address = Address(0x1000 * i);
            insn.mnemonic = "nop";
            insns.push_back(std::move(insn));
            cache.put(Address(0x1000 * i), 10, std::move(insns));
        }

        // All 8 should be present
        for (uint64_t i = 1; i <= 8; ++i) {
            auto* e = cache.find(Address(0x1000 * i), 10);
            assert(e != nullptr);
            assert(e->insns.size() == 1);
        }

        // Access 0x1000 so it becomes recently used
        assert(cache.find(Address(0x1000), 10) != nullptr);

        // Put a 9th entry (Address 0x9000); 0x2000 was least recently accessed, so it should be evicted!
        {
            std::vector<DisassembledInstruction> insns;
            DisassembledInstruction insn;
            insn.address = Address(0x9000);
            insn.mnemonic = "ret";
            insns.push_back(std::move(insn));
            cache.put(Address(0x9000), 10, std::move(insns));
        }

        assert(cache.find(Address(0x1000), 10) != nullptr && "0x1000 should remain in cache (recently accessed)");
        assert(cache.find(Address(0x9000), 10) != nullptr && "0x9000 should be in cache");
        assert(cache.find(Address(0x2000), 10) == nullptr && "0x2000 should have been evicted by LRU");

        // Invalidate test
        cache.invalidate();
        assert(cache.find(Address(0x1000), 10) == nullptr);
        assert(cache.find(Address(0x9000), 10) == nullptr);
        std::cout << "[PASS] DisasmCache multi-entry LRU eviction and invalidation verified." << std::endl;
    }

    std::cout << "[PASS] CapstoneContext and LRU DisasmCache tests passed." << std::endl;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "========================================" << std::endl;
    std::cout << "   edb-next Automated Test Suite        " << std::endl;
    std::cout << "========================================" << std::endl;

    test_breakpoint_and_stepping();
    test_multi_session();
    test_symbols_and_memory_search();
    test_call_stack_and_hardware_breakpoints();
    test_phase3_1_execution_controls_eflags_and_strings();
    test_phase3_2_patching_comments_and_proc_info();
    test_phase3_3_fpu_functions_and_heap();
    test_phase4_threads_assembler_and_conditional_bp();
    test_phase5_rop_branch_prediction_xrefs_and_pattern();
    test_capstone_context_and_lru_cache();

    std::cout << "\n>>> ALL UNIT TESTS PASSED SUCCESSFULLY! <<<" << std::endl;
    return 0;
}
