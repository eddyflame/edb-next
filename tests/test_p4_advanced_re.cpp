#include "core/Types.hpp"
#include "core/MicroIR.hpp"
#include "core/SymbolicEngine.hpp"
#include "core/DecompilerEngine.hpp"
#include "core/TimeTravelEngine.hpp"
#include "core/DapServer.hpp"
#include "core/EbpfHookEngine.hpp"

#include <iostream>
#include <cassert>
#include <vector>
#include <string>

using namespace edb_next;

void test_micro_ir_lifter_and_optimizations() {
    std::cout << "[TEST] Running test_micro_ir_lifter_and_optimizations..." << std::endl;

    std::vector<DisassembledInstruction> insns;

    auto addInsn = [&](Address addr, std::string mnem, std::string ops) {
        DisassembledInstruction insn;
        insn.address = addr;
        insn.mnemonic = std::move(mnem);
        insn.operands = std::move(ops);
        insns.push_back(std::move(insn));
    };

    // Construct a synthetic basic block with constant computations and dead variables
    addInsn(Address(0x401000), "mov", "rax, 10");
    addInsn(Address(0x401007), "add", "rax, 20");
    addInsn(Address(0x40100e), "mov", "rbx, 30");
    addInsn(Address(0x401015), "cmp", "rax, 30");
    addInsn(Address(0x40101c), "je", "0x401030");
    addInsn(Address(0x401022), "mov", "rcx, 0");
    addInsn(Address(0x401029), "jmp", "0x401035");
    addInsn(Address(0x401030), "mov", "rcx, 1");
    addInsn(Address(0x401035), "ret", "");

    // 1. Lifting to Micro-IR
    IRFunction fn = IRLifter::lift(insns);
    assert(!fn.blocks.empty());
    assert(fn.blocks.size() >= 3);
    std::cout << "  [Info] Lifted " << fn.blocks.size() << " basic blocks into SSA Micro-IR." << std::endl;

    // 2. Constant Folding Pass
    size_t folded = IRLifter::foldConstants(fn);
    assert(folded >= 1);
    std::cout << "  [Info] Folded " << folded << " constant operation(s)." << std::endl;

    // 3. Opaque Predicates Simplification Pass
    size_t simplified = IRLifter::simplifyOpaquePredicates(fn);
    std::cout << "  [Info] Simplified " << simplified << " branch predicate(s)." << std::endl;

    // 4. Dead Code Elimination Pass
    size_t eliminated = IRLifter::eliminateDeadCode(fn);
    std::cout << "  [Info] Eliminated " << eliminated << " dead instruction(s)." << std::endl;

    std::cout << "  -> test_micro_ir_lifter_and_optimizations PASSED" << std::endl;
}

void test_z3_symbolic_execution_and_reachability() {
    std::cout << "[TEST] Running test_z3_symbolic_execution_and_reachability..." << std::endl;

    SymbolicEngine engine;
    assert(SymbolicEngine::isZ3Available());

    // 1. Dynamic Taint Tracking
    engine.markTainted("rdi");
    assert(engine.isTainted("rdi"));
    assert(!engine.isTainted("rax"));

    // Propagate taint: rax = rdi + 5
    IRInstruction insn1;
    insn1.op = IROp::Add;
    insn1.dst = IROperand::Reg("rax");
    insn1.src1 = IROperand::Reg("rdi");
    insn1.src2 = IROperand::Imm(5);
    engine.stepIR(insn1);

    assert(engine.isTainted("rax"));

    // Untaint: rbx = 42
    IRInstruction insn2;
    insn2.op = IROp::Mov;
    insn2.dst = IROperand::Reg("rbx");
    insn2.src1 = IROperand::Imm(42);
    engine.stepIR(insn2);

    assert(!engine.isTainted("rbx"));

    // 2. Symbolic Path Reachability Solving via Z3 SMT
    // Create a target function with condition: (rdi + 0x10 == 0x1337) -> target block
    IRFunction fn;
    fn.entryAddr = Address(0x401000);

    IRBlock blk0;
    blk0.id = 0;
    blk0.startAddr = Address(0x401000);
    blk0.endAddr = Address(0x401015);
    blk0.successors = {1, 2};

    // add rdi, 0x10
    IRInstruction irAdd;
    irAdd.originAddr = Address(0x401000);
    irAdd.op = IROp::Add;
    irAdd.dst = IROperand::Reg("rdi");
    irAdd.src1 = IROperand::Reg("rdi");
    irAdd.src2 = IROperand::Imm(0x10);
    blk0.instructions.push_back(irAdd);

    // cmp rdi, 0x1337
    IRInstruction irCmp;
    irCmp.originAddr = Address(0x401007);
    irCmp.op = IROp::Cmp;
    irCmp.src1 = IROperand::Reg("rdi");
    irCmp.src2 = IROperand::Imm(0x1337);
    blk0.instructions.push_back(irCmp);

    // cjmp.e target: 0x401030 (Block 1)
    IRInstruction irCJmp;
    irCJmp.originAddr = Address(0x40100e);
    irCJmp.op = IROp::CJmp;
    irCJmp.cond = "e";
    irCJmp.dst = IROperand::Imm(0x401030);
    blk0.instructions.push_back(irCJmp);

    fn.blocks.push_back(blk0);

    // Block 1: Target Reachable Block
    IRBlock blk1;
    blk1.id = 1;
    blk1.startAddr = Address(0x401030);
    blk1.endAddr = Address(0x401035);
    fn.blocks.push_back(blk1);

    // Block 2: Fallthrough
    IRBlock blk2;
    blk2.id = 2;
    blk2.startAddr = Address(0x401016);
    blk2.endAddr = Address(0x401020);
    fn.blocks.push_back(blk2);

    auto result = engine.solveReachability(fn, Address(0x401030));
    assert(result.satisfiable);
    assert(result.registerValues.contains("rdi"));
    uint64_t solvedRdi = result.registerValues["rdi"];
    assert(solvedRdi == (0x1337 - 0x10)); // 0x1327

    std::cout << "  [Info] Z3 SMT Solver solved input constraint: rdi = 0x" << std::hex << solvedRdi
              << " (" << result.summary << ")" << std::endl;

    std::cout << "  -> test_z3_symbolic_execution_and_reachability PASSED" << std::endl;
}

void test_decompiler_pseudo_code_generation() {
    std::cout << "[TEST] Running test_decompiler_pseudo_code_generation..." << std::endl;

    std::vector<DisassembledInstruction> insns;
    auto addInsn = [&](Address addr, std::string mnem, std::string ops) {
        DisassembledInstruction insn;
        insn.address = addr;
        insn.mnemonic = std::move(mnem);
        insn.operands = std::move(ops);
        insns.push_back(std::move(insn));
    };

    addInsn(Address(0x401000), "mov", "rax, 100");
    addInsn(Address(0x401007), "add", "rax, rdi");
    addInsn(Address(0x40100e), "cmp", "rax, 500");
    addInsn(Address(0x401015), "jl", "0x401025");
    addInsn(Address(0x40101b), "mov", "rax, 0");
    addInsn(Address(0x401022), "ret", "");
    addInsn(Address(0x401025), "mov", "rax, 1");
    addInsn(Address(0x40102c), "ret", "");

    DecompiledFunction decomp = DecompilerEngine::decompile(insns, "verify_key");

    assert(!decomp.pseudoCode.empty());
    assert(decomp.pseudoCode.find("int64_t verify_key()") != std::string::npos);
    assert(decomp.pseudoCode.find("return rax;") != std::string::npos);
    assert(decomp.pseudoCode.find("loc_") != std::string::npos);
    assert(!decomp.sourceMap.empty());

    // Verify SourceMap bidirectional lookup
    auto lineOpt = decomp.lineForAddress(Address(0x401000));
    assert(lineOpt.has_value());
    auto addrOpt = decomp.addressForLine(*lineOpt);
    assert(addrOpt.has_value());
    assert(addrOpt->value() == 0x401000);

    std::cout << "  [Info] Generated C Pseudo-Code:\n" << decomp.pseudoCode << std::endl;
    std::cout << "  -> test_decompiler_pseudo_code_generation PASSED" << std::endl;
}

void test_time_travel_debugging_step_back() {
    std::cout << "[TEST] Running test_time_travel_debugging_step_back..." << std::endl;

    TimeTravelEngine ttd(500);

    // Check hardware branch tracing detection (graceful probe)
    bool hwSupported = TimeTravelEngine::isHardwareBranchTracingSupported();
    std::cout << "  [Info] Intel PT / Hardware branch recording available: "
              << (hwSupported ? "YES" : "NO (Using deterministic snapshot diff engine)") << std::endl;

    // Record sequential frames
    RegisterContext r1, r2, r3;
    r1.setRip(Address(0x400100));
    r1.setRax(10);
    ttd.recordFrame(r1.rip(), r1, "mov rax, 10");

    r2.setRip(Address(0x400105));
    r2.setRax(20);
    // Record memory change delta
    uint8_t oldB = 0xAA;
    uint8_t newB = 0xBB;
    ttd.recordMemoryChange(Address(0x600000), std::span(&oldB, 1), std::span(&newB, 1));
    ttd.recordFrame(r2.rip(), r2, "add rax, 10");

    r3.setRip(Address(0x40010a));
    r3.setRax(30);
    ttd.recordFrame(r3.rip(), r3, "add rax, 10");

    assert(ttd.frameCount() == 3);
    assert(!ttd.isReplaying());
    assert(ttd.canStepBack());

    // 1. Step Back 1 instruction
    auto f2 = ttd.stepBack();
    assert(f2.has_value());
    assert(f2->rip.value() == 0x400105);
    assert(f2->registers.rax() == 20);
    assert(ttd.isReplaying());

    // 2. Step Back another instruction
    auto f1 = ttd.stepBack();
    assert(f1.has_value());
    assert(f1->rip.value() == 0x400100);
    assert(f1->registers.rax() == 10);
    assert(!ttd.canStepBack());

    // 3. Step Forward
    auto f_fwd = ttd.stepForward();
    assert(f_fwd.has_value());
    assert(f_fwd->rip.value() == 0x400105);

    // 4. Reverse Continue to breakpoint
    std::unordered_set<uint64_t> bps = {0x400100};
    auto f_rev_bp = ttd.reverseContinue(bps);
    assert(f_rev_bp.has_value());
    assert(f_rev_bp->rip.value() == 0x400100);

    std::cout << "  -> test_time_travel_debugging_step_back PASSED" << std::endl;
}

void test_dap_server_json_rpc_handshake() {
    std::cout << "[TEST] Running test_dap_server_json_rpc_handshake..." << std::endl;

    DapServer server;

    // 1. Initialize Request
    std::string initReq = "{\"seq\":1,\"type\":\"request\",\"command\":\"initialize\",\"arguments\":{\"adapterID\":\"edb-next\"}}";
    std::string initResp = server.handleMessage(initReq);

    assert(initResp.find("\"command\":\"initialize\"") != std::string::npos);
    assert(initResp.find("\"success\":true") != std::string::npos);
    assert(initResp.find("\"supportsStepBack\":true") != std::string::npos);
    assert(initResp.find("\"supportsDisassembleRequest\":true") != std::string::npos);

    // 2. Threads Request
    std::string threadsReq = "{\"seq\":2,\"type\":\"request\",\"command\":\"threads\"}";
    std::string threadsResp = server.handleMessage(threadsReq);
    assert(threadsResp.find("\"threads\":") != std::string::npos);

    // 3. Stack Trace Request
    std::string stackReq = "{\"seq\":3,\"type\":\"request\",\"command\":\"stackTrace\",\"arguments\":{\"threadId\":1}}";
    std::string stackResp = server.handleMessage(stackReq);
    assert(stackResp.find("\"stackFrames\":") != std::string::npos);

    // 4. Scopes Request
    std::string scopesReq = "{\"seq\":4,\"type\":\"request\",\"command\":\"scopes\",\"arguments\":{\"frameId\":1}}";
    std::string scopesResp = server.handleMessage(scopesReq);
    assert(scopesResp.find("\"Registers\"") != std::string::npos);

    // 5. Variables Request (Registers Scope: variablesReference = 1)
    std::string varsReq = "{\"seq\":5,\"type\":\"request\",\"command\":\"variables\",\"arguments\":{\"variablesReference\":1}}";
    std::string varsResp = server.handleMessage(varsReq);
    assert(varsResp.find("\"rax\"") != std::string::npos);
    assert(varsResp.find("\"rip\"") != std::string::npos);

    // 6. Disassemble Request
    std::string disasmReq = "{\"seq\":6,\"type\":\"request\",\"command\":\"disassemble\",\"arguments\":{\"memoryReference\":\"0x401000\",\"instructionCount\":3}}";
    std::string disasmResp = server.handleMessage(disasmReq);
    assert(disasmResp.find("\"instructions\":") != std::string::npos);

    std::cout << "  [Info] DAP JSON-RPC Protocol handshake completed verified." << std::endl;
    std::cout << "  -> test_dap_server_json_rpc_handshake PASSED" << std::endl;
}

void test_ebpf_hook_engine_probes() {
    std::cout << "[TEST] Running test_ebpf_hook_engine_probes..." << std::endl;

    EbpfHookEngine ebpf;
    bool kernelSupported = EbpfHookEngine::isKernelTracingSupported();
    std::cout << "  [Info] Linux Kernel eBPF / uprobe tracing accessible: "
              << (kernelSupported ? "YES (Root / CAP_BPF active)" : "NO (Non-privileged mode active)") << std::endl;

    // Attach Uprobe
    auto res = ebpf.attachUprobe("/bin/ls", 0x1000, "probe_test_main");
    assert(res.success);
    assert(ebpf.attachedProbes().size() == 1);
    assert(ebpf.attachedProbes().front().name == "probe_test_main");

    // Duplicate rejection
    auto resDup = ebpf.attachUprobe("/bin/ls", 0x1000, "probe_test_main");
    assert(!resDup.success);

    // Simulated event ingestion & polling
    UprobeEvent ev;
    ev.probeName = "probe_test_main";
    ev.pid = 4242;
    ev.tid = 4242;
    ev.ip = 0x555555555000;
    ev.args = {1, 2, 3};
    ebpf.recordSimulatedEvent(ev);

    assert(ebpf.eventCount() == 1);
    auto polled = ebpf.pollEvents();
    assert(polled.size() == 1);
    assert(polled[0].probeName == "probe_test_main");
    assert(polled[0].pid == 4242);
    assert(ebpf.eventCount() == 0);

    // Detach
    auto detRes = ebpf.detachUprobe("probe_test_main");
    assert(detRes.success);
    assert(ebpf.attachedProbes().empty());

    std::cout << "  -> test_ebpf_hook_engine_probes PASSED" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   edb-next Advanced RE (P4) Test Suite " << std::endl;
    std::cout << "========================================" << std::endl;

    test_micro_ir_lifter_and_optimizations();
    test_z3_symbolic_execution_and_reachability();
    test_decompiler_pseudo_code_generation();
    test_time_travel_debugging_step_back();
    test_dap_server_json_rpc_handshake();
    test_ebpf_hook_engine_probes();

    std::cout << "========================================" << std::endl;
    std::cout << "  ALL ADVANCED RE (P4) TESTS PASSED!    " << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
