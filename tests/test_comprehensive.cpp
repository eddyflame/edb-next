#include "core/Types.hpp"
#include "core/ExpressionEvaluator.hpp"
#include "core/BreakpointManager.hpp"
#include "core/MicroIR.hpp"
#include "core/SymbolicEngine.hpp"
#include "core/DapServer.hpp"
#include "core/BtfParser.hpp"
#include "core/ElfParser.hpp"
#include "core/DatabaseManager.hpp"
#include "core/MemoryScanner.hpp"
#include "core/PageGuardManager.hpp"
#include "tests/MockDebugBackend.hpp"

#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <cmath>

using namespace edb_next;

// ==============================================================================
// 1. ExpressionEvaluator Comprehensive Tests
// ==============================================================================
void test_expression_evaluator_comprehensive() {
    std::cout << "[TEST] Running test_expression_evaluator_comprehensive..." << std::endl;

    RegisterContext regs;
    regs.setRax(0x1234567890abcdefULL);
    regs.setRbx(0x100ULL);
    regs.setRcx(0x20ULL);
    regs.setRdx(0x5ULL);
    regs.setRsi(0x1000ULL);
    regs.setRdi(0x7ULL);
    regs.setRbp(Address(0x7fffffffdf00ULL));
    regs.setRsp(Address(0x7fffffffde00ULL));
    regs.setRip(Address(0x555555555000ULL));
    regs.setR8(0x88ULL);

    // 1. Basic Arithmetic & Precedence
    auto v1 = ExpressionEvaluator::evaluate("10 + 20 * 3", regs);
    assert(v1.has_value() && *v1 == 70);

    auto v2 = ExpressionEvaluator::evaluate("(10 + 20) * 3", regs);
    assert(v2.has_value() && *v2 == 90);

    auto v3 = ExpressionEvaluator::evaluate("((2 + 3) * (4 + 6)) / 10", regs);
    assert(v3.has_value() && *v3 == 5);

    // 2. Division & Modulo by Zero Protection (Edge Cases)
    auto divZero = ExpressionEvaluator::evaluate("100 / 0", regs);
    assert(!divZero.has_value() && "Division by zero must return nullopt");

    auto modZero = ExpressionEvaluator::evaluate("100 % 0", regs);
    assert(!modZero.has_value() && "Modulo by zero must return nullopt");

    auto divZeroExpr = ExpressionEvaluator::evaluate("50 / (10 - 10)", regs);
    assert(!divZeroExpr.has_value());

    // 3. Unary Operators
    auto u1 = ExpressionEvaluator::evaluate("-5 + 15", regs);
    assert(u1.has_value() && *u1 == 10);

    auto u2 = ExpressionEvaluator::evaluate("- -10", regs);
    assert(u2.has_value() && *u2 == 10);

    auto u3 = ExpressionEvaluator::evaluate("+ +25", regs);
    assert(u3.has_value() && *u3 == 25);

    auto u4 = ExpressionEvaluator::evaluate("~0", regs);
    assert(u4.has_value() && *u4 == 0xffffffffffffffffULL);

    auto u5 = ExpressionEvaluator::evaluate("!0", regs);
    assert(u5.has_value() && *u5 == 1);

    auto u6 = ExpressionEvaluator::evaluate("!42", regs);
    assert(u6.has_value() && *u6 == 0);

    // 4. Bitwise Shifts & Limits (>= 64 bits)
    auto sh1 = ExpressionEvaluator::evaluate("1 << 10", regs);
    assert(sh1.has_value() && *sh1 == 1024);

    auto sh2 = ExpressionEvaluator::evaluate("1024 >> 2", regs);
    assert(sh2.has_value() && *sh2 == 256);

    auto sh3 = ExpressionEvaluator::evaluate("1 << 64", regs);
    assert(sh3.has_value() && *sh3 == 0);

    auto sh4 = ExpressionEvaluator::evaluate("1 >> 65", regs);
    assert(sh4.has_value() && *sh4 == 0);

    // 5. Bitwise AND, OR, XOR
    auto b1 = ExpressionEvaluator::evaluate("0xff00 & 0x0ff0", regs);
    assert(b1.has_value() && *b1 == 0x0f00);

    auto b2 = ExpressionEvaluator::evaluate("0xaa | 0x55", regs);
    assert(b2.has_value() && *b2 == 0xff);

    auto b3 = ExpressionEvaluator::evaluate("0xaa ^ 0x55", regs);
    assert(b3.has_value() && *b3 == 0xff);

    // 6. Logical & Relational Operators
    auto r1 = ExpressionEvaluator::evaluate("10 < 20", regs);
    assert(r1.has_value() && *r1 == 1);

    auto r2 = ExpressionEvaluator::evaluate("20 <= 20", regs);
    assert(r2.has_value() && *r2 == 1);

    auto r3 = ExpressionEvaluator::evaluate("30 > 20", regs);
    assert(r3.has_value() && *r3 == 1);

    auto r4 = ExpressionEvaluator::evaluate("20 >= 30", regs);
    assert(r4.has_value() && *r4 == 0);

    auto r5 = ExpressionEvaluator::evaluate("10 == 10", regs);
    assert(r5.has_value() && *r5 == 1);

    auto r6 = ExpressionEvaluator::evaluate("10 != 10", regs);
    assert(r6.has_value() && *r6 == 0);

    auto r7 = ExpressionEvaluator::evaluate("(10 == 10) && (20 > 5)", regs);
    assert(r7.has_value() && *r7 == 1);

    auto r8 = ExpressionEvaluator::evaluate("(10 != 10) || (20 > 5)", regs);
    assert(r8.has_value() && *r8 == 1);

    // 7. Registers in all bit-widths & with $ prefix
    assert(ExpressionEvaluator::evaluate("rax", regs).value() == 0x1234567890abcdefULL);
    assert(ExpressionEvaluator::evaluate("$rax", regs).value() == 0x1234567890abcdefULL);
    assert(ExpressionEvaluator::evaluate("eax", regs).value() == 0x90abcdefULL);
    assert(ExpressionEvaluator::evaluate("ax", regs).value() == 0xcdefULL);
    assert(ExpressionEvaluator::evaluate("al", regs).value() == 0xefULL);
    assert(ExpressionEvaluator::evaluate("ah", regs).value() == 0xcdULL);
    assert(ExpressionEvaluator::evaluate("rbx", regs).value() == 0x100ULL);
    assert(ExpressionEvaluator::evaluate("rcx", regs).value() == 0x20ULL);
    assert(ExpressionEvaluator::evaluate("r8", regs).value() == 0x88ULL);
    assert(ExpressionEvaluator::evaluate("r8d", regs).value() == 0x88ULL);
    assert(ExpressionEvaluator::evaluate("rip", regs).value() == 0x555555555000ULL);

    // 8. Hex & Binary Notations with Upper/Lower Case
    assert(ExpressionEvaluator::evaluate("0x1a", regs).value() == 26);
    assert(ExpressionEvaluator::evaluate("0X1A", regs).value() == 26);
    assert(ExpressionEvaluator::evaluate("0b1011", regs).value() == 11);
    assert(ExpressionEvaluator::evaluate("0B1100", regs).value() == 12);

    // 9. Malformed Inputs (Should Return nullopt without crash)
    assert(!ExpressionEvaluator::evaluate("", regs).has_value());
    assert(!ExpressionEvaluator::evaluate("   ", regs).has_value());
    assert(!ExpressionEvaluator::evaluate("0x", regs).has_value());
    assert(!ExpressionEvaluator::evaluate("0b", regs).has_value());
    assert(!ExpressionEvaluator::evaluate("0x + 5", regs).has_value());
    assert(!ExpressionEvaluator::evaluate("rax + ", regs).has_value());
    assert(!ExpressionEvaluator::evaluate("(10 + 20", regs).has_value());
    assert(!ExpressionEvaluator::evaluate("10 + 20)", regs).has_value());
    assert(!ExpressionEvaluator::evaluate("unknown_reg", regs).has_value());

    // 10. evaluateCondition & formatLog
    assert(ExpressionEvaluator::evaluateCondition("rax != 0", regs));
    assert(!ExpressionEvaluator::evaluateCondition("rax == 0", regs));
    assert(ExpressionEvaluator::evaluateCondition("", regs) && "Empty condition evaluates to true");

    std::string logOut = ExpressionEvaluator::formatLog("Status: rax={rax}, rdx={rdx}", regs);
    assert(logOut.find("0x1234567890abcdef") != std::string::npos);
    assert(logOut.find("0x5 (5)") != std::string::npos);

    std::cout << "  -> test_expression_evaluator_comprehensive PASSED" << std::endl;
}

// ==============================================================================
// 2. BreakpointManager Hardware Breakpoint & Watchpoint Comprehensive Tests
// ==============================================================================
void test_breakpoint_manager_hardware_comprehensive() {
    std::cout << "[TEST] Running test_breakpoint_manager_hardware_comprehensive..." << std::endl;

    std::unordered_map<uint64_t, uint8_t> mem;
    auto readMem = [&](Address a, void* buf, size_t sz) {
        auto* p = static_cast<uint8_t*>(buf);
        for (size_t i = 0; i < sz; ++i) {
            auto it = mem.find(a.value() + i);
            p[i] = (it != mem.end()) ? it->second : 0x90; // Default NOP
        }
        return true;
    };
    auto writeMem = [&](Address a, const void* buf, size_t sz) {
        const auto* p = static_cast<const uint8_t*>(buf);
        for (size_t i = 0; i < sz; ++i) {
            mem[a.value() + i] = p[i];
        }
        return true;
    };

    struct HwSlotState {
        bool active{false};
        Address addr{0};
        HardwareBpType type{HardwareBpType::Execute};
        HardwareBpSize size{HardwareBpSize::Byte1};
    };
    HwSlotState hwSlots[4]{};

    auto setHwBp = [&](int slot, Address addr, HardwareBpType t, HardwareBpSize s) {
        if (slot < 0 || slot >= 4) return false;
        hwSlots[slot] = {true, addr, t, s};
        return true;
    };
    auto clearHwBp = [&](int slot) {
        if (slot < 0 || slot >= 4) return false;
        hwSlots[slot].active = false;
        return true;
    };

    BreakpointManager mgr(readMem, writeMem, setHwBp, clearHwBp);

    // 1. Add 4 Hardware watchpoints with distinct sizes (Byte1, Byte2, Byte4, Byte8)
    assert(mgr.addHardwareBreakpoint(Address(0x1000), HardwareBpType::Write, HardwareBpSize::Byte1, "wp1"));
    assert(mgr.addHardwareBreakpoint(Address(0x2000), HardwareBpType::Write, HardwareBpSize::Byte2, "wp2"));
    assert(mgr.addHardwareBreakpoint(Address(0x3000), HardwareBpType::ReadWrite, HardwareBpSize::Byte4, "wp3"));
    assert(mgr.addHardwareBreakpoint(Address(0x4000), HardwareBpType::ReadWrite, HardwareBpSize::Byte8, "wp4"));

    // Verify all 4 slots are occupied with matching sizes
    assert(hwSlots[0].active && hwSlots[0].size == HardwareBpSize::Byte1);
    assert(hwSlots[1].active && hwSlots[1].size == HardwareBpSize::Byte2);
    assert(hwSlots[2].active && hwSlots[2].size == HardwareBpSize::Byte4);
    assert(hwSlots[3].active && hwSlots[3].size == HardwareBpSize::Byte8);

    // Verify breakpoint metadata retains hardwareSize
    const auto* bp3 = mgr.getBreakpoint(Address(0x3000));
    assert(bp3 != nullptr && bp3->hardwareSize == HardwareBpSize::Byte4);
    const auto* bp4 = mgr.getBreakpoint(Address(0x4000));
    assert(bp4 != nullptr && bp4->hardwareSize == HardwareBpSize::Byte8);

    // 2. Disable watchpoint 3 (slot 2)
    assert(mgr.disableBreakpoint(Address(0x3000)));
    assert(!mgr.getBreakpoint(Address(0x3000))->enabled);
    assert(!hwSlots[2].active && "Slot 2 hardware register must be cleared");

    // 3. Re-enable watchpoint 3: must restore size to Byte4, not drop to Byte1!
    assert(mgr.enableBreakpoint(Address(0x3000)));
    assert(mgr.getBreakpoint(Address(0x3000))->enabled);
    assert(hwSlots[2].active);
    assert(hwSlots[2].size == HardwareBpSize::Byte4 && "Re-enabled watchpoint must preserve Byte4 size!");

    // 4. Test slot reallocation when slot is taken by another breakpoint:
    // Disable watchpoint 2 (which had slot 1)
    assert(mgr.disableBreakpoint(Address(0x2000)));
    assert(!hwSlots[1].active);

    // Add a new watchpoint 5: should claim the newly freed slot 1
    assert(mgr.addHardwareBreakpoint(Address(0x5000), HardwareBpType::Execute, HardwareBpSize::Byte1, "wp5"));
    assert(hwSlots[1].active && hwSlots[1].addr == Address(0x5000));

    // Now all 4 slots (0, 1, 2, 3) are full.
    // Try to re-enable watchpoint 2 (without PageGuard fallback configured):
    // Should fail cleanly without corrupting slot 1!
    assert(!mgr.enableBreakpoint(Address(0x2000)) && "Enable should fail when all slots are occupied and no fallback");
    assert(hwSlots[1].addr == Address(0x5000) && "Slot 1 must not be stomped");

    // Remove watchpoint 5: slot 1 is free again
    assert(mgr.removeBreakpoint(Address(0x5000)));
    assert(!hwSlots[1].active);

    // Re-enable watchpoint 2 now: should dynamically claim slot 1 and restore Byte2 size
    assert(mgr.enableBreakpoint(Address(0x2000)));
    assert(hwSlots[1].active && hwSlots[1].size == HardwareBpSize::Byte2);

    // 5. Software breakpoint idempotence
    assert(mgr.addBreakpoint(Address(0x6000), false, "sw1"));
    assert(mgr.hasBreakpoint(Address(0x6000)));
    assert(!mgr.enableBreakpoint(Address(0x6000)) && "Enabling already enabled breakpoint must return false");
    assert(mgr.disableBreakpoint(Address(0x6000)));
    assert(!mgr.disableBreakpoint(Address(0x6000)) && "Disabling already disabled breakpoint must return false");
    assert(mgr.removeBreakpoint(Address(0x6000)));
    assert(!mgr.removeBreakpoint(Address(0x6000)) && "Removing non-existent breakpoint must return false");

    std::cout << "  -> test_breakpoint_manager_hardware_comprehensive PASSED" << std::endl;
}

// ==============================================================================
// 3. MicroIR & IRLifter Edge Cases Tests
// ==============================================================================
void test_micro_ir_edge_cases() {
    std::cout << "[TEST] Running test_micro_ir_edge_cases..." << std::endl;

    std::vector<DisassembledInstruction> insns;
    auto addInsn = [&](Address addr, std::string mnem, std::string ops) {
        DisassembledInstruction in;
        in.address = addr;
        in.mnemonic = std::move(mnem);
        in.operands = std::move(ops);
        insns.push_back(std::move(in));
    };

    // Test negative decimal and hex numbers in operands
    addInsn(Address(0x401000), "mov", "rax, -10");
    addInsn(Address(0x401007), "add", "rax, -0x20");
    addInsn(Address(0x40100e), "mov", "rbx, 0x50");
    addInsn(Address(0x401015), "sub", "rbx, 20");
    addInsn(Address(0x40101c), "mov", "rcx, [rbp-0x18]");
    addInsn(Address(0x401023), "mov", "dword ptr [rsp+8], 42");
    addInsn(Address(0x40102b), "ret", "");

    IRFunction fn = IRLifter::lift(insns);
    assert(!fn.blocks.empty());

    // Verify negative decimal parsed as -10 (0xfffffffffffffff6) not -0x10 (-16)
    const auto& block0 = fn.blocks[0];
    assert(block0.instructions.size() >= 5);

    const auto& insnMov = block0.instructions[0];
    assert(insnMov.op == IROp::Mov);
    assert(insnMov.src1.isImm());
    assert(insnMov.src1.immValue == static_cast<uint64_t>(-10));

    const auto& insnAdd = block0.instructions[1];
    assert(insnAdd.op == IROp::Add);
    assert(insnAdd.src2.isImm());
    assert(insnAdd.src2.immValue == static_cast<uint64_t>(-0x20));

    // Verify memory operands with displacement & size specifiers
    bool foundMem1 = false;
    bool foundMem2 = false;
    for (const auto& in : block0.instructions) {
        if (in.src1.isMem() && in.src1.name.find("rbp-0x18") != std::string::npos) {
            foundMem1 = true;
        }
        if (in.dst.isMem() && in.dst.name.find("rsp+8") != std::string::npos && in.dst.size == 4) {
            foundMem2 = true;
        }
    }
    assert(foundMem1 && "Memory operand with displacement must be parsed");
    assert(foundMem2 && "Memory operand with dword ptr must have size 4");

    // Test constant folding pass
    size_t folded = IRLifter::foldConstants(fn);
    assert(folded >= 1);

    std::cout << "  -> test_micro_ir_edge_cases PASSED" << std::endl;
}

// ==============================================================================
// 4. SymbolicEngine Mixed Widths & Edge Cases Tests
// ==============================================================================
void test_symbolic_engine_mixed_widths_and_edge_cases() {
    std::cout << "[TEST] Running test_symbolic_engine_mixed_widths_and_edge_cases..." << std::endl;

    SymbolicEngine engine;

    // 1. Mixed bit-width arithmetic (64-bit symbolic register with 32-bit immediate / sub-register)
    // Previously this caused uncaught Z3 sort mismatch exceptions
    engine.makeRegisterSymbolic("rdi", 8); // 64-bit

    // Add 32-bit immediate to 64-bit register: rdi + 0x100
    IRInstruction addInsn;
    addInsn.op = IROp::Add;
    addInsn.dst = IROperand::Reg("rax", 8);
    addInsn.src1 = IROperand::Reg("rdi", 8);
    addInsn.src2 = IROperand::Imm(0x100, 4); // 32-bit immediate!
    engine.stepIR(addInsn);

    // Subtract 8-bit immediate from 64-bit register
    IRInstruction subInsn;
    subInsn.op = IROp::Sub;
    subInsn.dst = IROperand::Reg("rbx", 8);
    subInsn.src1 = IROperand::Reg("rax", 8);
    subInsn.src2 = IROperand::Imm(0x5, 1); // 8-bit immediate!
    engine.stepIR(subInsn);

    // Comparison between mixed bit-widths (e.g. 64-bit register vs 32-bit operand)
    IRInstruction cmpInsn;
    cmpInsn.op = IROp::Cmp;
    cmpInsn.dst = IROperand::None();
    cmpInsn.src1 = IROperand::Reg("rbx", 8);
    cmpInsn.src2 = IROperand::Imm(0x1337, 4); // 32-bit
    engine.stepIR(cmpInsn);

    // 2. Concrete Register Evaluation
    engine.setRegisterConcrete("rcx", 40, 8);
    IRInstruction concreteAdd;
    concreteAdd.op = IROp::Add;
    concreteAdd.dst = IROperand::Reg("rdx", 8);
    concreteAdd.src1 = IROperand::Reg("rcx", 8);
    concreteAdd.src2 = IROperand::Imm(2, 8);
    engine.stepIR(concreteAdd);

    auto rdxVal = engine.evaluateRegister("rdx");
    assert(rdxVal.has_value());
    assert(*rdxVal == 42);

    // 3. Taint Tracking across mixed widths
    engine.markTainted("rdi");
    assert(engine.isTainted("rdi"));

    // Taint propagates to rsi
    IRInstruction taintProp;
    taintProp.op = IROp::Mov;
    taintProp.dst = IROperand::Reg("rsi", 4); // 32-bit dest
    taintProp.src1 = IROperand::Reg("rdi", 8); // 64-bit src
    engine.stepIR(taintProp);
    assert(engine.isTainted("rsi"));

    // Overwrite with constant immediate: must clear taint
    IRInstruction untaint;
    untaint.op = IROp::Mov;
    untaint.dst = IROperand::Reg("rsi", 4);
    untaint.src1 = IROperand::Imm(0, 4);
    engine.stepIR(untaint);
    assert(!engine.isTainted("rsi"));

    std::cout << "  -> test_symbolic_engine_mixed_widths_and_edge_cases PASSED" << std::endl;
}

// ==============================================================================
// 5. DapServer Robustness & Bounds Checking Tests
// ==============================================================================
void test_dap_server_robustness() {
    std::cout << "[TEST] Running test_dap_server_robustness..." << std::endl;

    DapServer server;

    // 1. Negative count in readMemory (must return error, NOT crash with length_error!)
    std::string negativeCountReq = R"({"seq":10,"type":"request","command":"readMemory","arguments":{"memoryReference":"0x401000","count":-1}})";
    std::string res1 = server.handleMessage(negativeCountReq);
    assert(res1.find("\"success\":false") != std::string::npos);

    // 2. Oversized count in readMemory (> 64MB)
    std::string oversizedReq = R"({"seq":11,"type":"request","command":"readMemory","arguments":{"memoryReference":"0x401000","count":100000000}})";
    std::string res2 = server.handleMessage(oversizedReq);
    assert(res2.find("\"success\":false") != std::string::npos);

    // 3. Negative instruction count in disassemble
    std::string negDisasmReq = R"({"seq":12,"type":"request","command":"disassemble","arguments":{"memoryReference":"0x401000","instructionCount":-5}})";
    std::string res3 = server.handleMessage(negDisasmReq);
    assert(res3.find("\"success\":false") != std::string::npos);

    // 4. Unknown command (should respond gracefully without crashing)
    std::string unknownCmdReq = R"({"seq":13,"type":"request","command":"unknownFuzzCommand","arguments":{}})";
    std::string res4 = server.handleMessage(unknownCmdReq);
    assert(res4.find("\"request_seq\":13") != std::string::npos);

    // 5. Valid initialize & scopes & threads
    std::string initReq = R"({"seq":1,"type":"request","command":"initialize","arguments":{}})";
    std::string initRes = server.handleMessage(initReq);
    assert(initRes.find("\"supportsConditionalBreakpoints\":true") != std::string::npos);

    std::string threadsReq = R"({"seq":2,"type":"request","command":"threads","arguments":{}})";
    std::string threadsRes = server.handleMessage(threadsReq);
    assert(threadsRes.find("\"threads\":") != std::string::npos);

    std::cout << "  -> test_dap_server_robustness PASSED" << std::endl;
}

// ==============================================================================
// 6. BtfParser Edge Cases & Boundary Tests
// ==============================================================================
void test_btf_parser_edge_cases() {
    std::cout << "[TEST] Running test_btf_parser_edge_cases..." << std::endl;

    BtfParser parser;

    // 1. Empty buffer
    assert(!parser.parseBuffer({}) && "Empty buffer must return false");

    // 2. Buffer smaller than btf_header
    uint8_t tinyBuf[8] = {0x9f, 0xeb};
    assert(!parser.parseBuffer(tinyBuf));

    // 3. Invalid magic
    uint8_t invalidMagicBuf[32] = {0};
    assert(!parser.parseBuffer(invalidMagicBuf));

    // 4. Header with corrupt lengths extending beyond buffer
    struct FakeBtfHdr {
        uint16_t magic{0xeb9f};
        uint8_t version{1};
        uint8_t flags{0};
        uint32_t hdr_len{24};
        uint32_t type_off{0};
        uint32_t type_len{1000}; // claims 1000 bytes
        uint32_t str_off{1000};
        uint32_t str_len{500};
    } fakeHdr;
    std::vector<uint8_t> corruptBuf(sizeof(fakeHdr) + 16, 0);
    std::memcpy(corruptBuf.data(), &fakeHdr, sizeof(fakeHdr));
    assert(!parser.parseBuffer(corruptBuf) && "Buffer smaller than header claims must return false");

    // 5. Synthetic BTF generation and type verification
    std::vector<std::pair<std::string, uint32_t>> fields = {
        {"magic_id", 4},
        {"timestamp", 8},
        {"flags", 2}
    };
    auto btfData = BtfParser::createSyntheticBtf("TelemetryData", fields);
    assert(!btfData.empty());

    bool parseOk = parser.parseBuffer(btfData);
    assert(parseOk);
    const auto* entry = parser.findType("TelemetryData");
    assert(entry != nullptr);
    assert(entry->name == "TelemetryData");
    assert(entry->fields.size() == 3);
    assert(entry->fields[0].name == "magic_id");
    assert(entry->fields[1].name == "timestamp");
    assert(entry->fields[2].name == "flags");

    std::cout << "  -> test_btf_parser_edge_cases PASSED" << std::endl;
}

// ==============================================================================
// 7. ElfParser Memory Safety & Robustness Tests
// ==============================================================================
void test_elf_parser_memory_safety() {
    std::cout << "[TEST] Running test_elf_parser_memory_safety..." << std::endl;

    ElfParser parser;

    // 1. Non-existent file
    assert(!parser.loadBinary("/non/existent/path/binary.elf"));

    // 2. Corrupt / truncated file
    std::string tmpCorrupt = "/tmp/test_corrupt_elf.bin";
    {
        std::ofstream ofs(tmpCorrupt, std::ios::binary);
        uint8_t garbage[16] = {0x7f, 'E', 'L', 'F', 1, 1, 1, 0};
        ofs.write(reinterpret_cast<const char*>(garbage), sizeof(garbage));
    }
    assert(!parser.loadBinary(tmpCorrupt));
    std::filesystem::remove(tmpCorrupt);

    // 3. Demangling C++ symbols
    assert(ElfParser::demangle("_Z3fooi") == "foo(int)");
    assert(ElfParser::demangle("_ZN3std6vectorIiSaIiEED1Ev").find("vector") != std::string::npos);
    assert(ElfParser::demangle("plain_c_function") == "plain_c_function");
    assert(ElfParser::demangle("") == "");

    std::cout << "  -> test_elf_parser_memory_safety PASSED" << std::endl;
}

// ==============================================================================
// 8. DatabaseManager JSON & SQLite Dual Mode Tests
// ==============================================================================
void test_database_manager_comprehensive() {
    std::cout << "[TEST] Running test_database_manager_comprehensive..." << std::endl;

    auto& dbMgr = DatabaseManager::instance();

    // 1. Explicit .json file saving and loading
    std::string jsonPath = "/tmp/test_project_format.json";
    std::filesystem::remove(jsonPath);

    DatabaseProject proj;
    proj.binaryPath = "/bin/echo";
    proj.notes = "Testing notes with \"quotes\" & 'apostrophes' <xml>";
    proj.baseAddress = 0x400000;
    proj.comments.emplace_back(0x401000, "Comment with \"quoted string\"");
    proj.labels.emplace_back(0x401000, "entry_point");
    proj.bookmarks.push_back(0x401000);
    proj.watches.push_back("rax == 42");

    bool saveJsonOk = dbMgr.saveToFile(jsonPath, proj);
    assert(saveJsonOk);
    assert(!dbMgr.isSqliteDatabase(jsonPath) && "Explicit .json path must save JSON document, not SQLite");

    DatabaseProject loadedJson;
    bool loadJsonOk = dbMgr.loadFromFile(jsonPath, loadedJson);
    assert(loadJsonOk);
    assert(loadedJson.binaryPath == "/bin/echo");
    assert(loadedJson.notes == proj.notes);
    assert(loadedJson.comments.size() == 1);
    assert(loadedJson.comments[0].second == "Comment with \"quoted string\"");
    assert(loadedJson.watches.size() == 1);
    std::filesystem::remove(jsonPath);

    // 2. Explicit SQLite database saving and loading
    std::string sqlitePath = "/tmp/test_project_format.edb_db";
    std::filesystem::remove(sqlitePath);

    proj.blobs.emplace_back("snapshot_0", std::vector<uint8_t>{0x11, 0x22, 0x33, 0x44, 0x55});
    bool saveSqlOk = dbMgr.saveToFile(sqlitePath, proj);
    assert(saveSqlOk);
    assert(dbMgr.isSqliteDatabase(sqlitePath));

    DatabaseProject loadedSql;
    bool loadSqlOk = dbMgr.loadFromFile(sqlitePath, loadedSql);
    assert(loadSqlOk);
    assert(loadedSql.binaryPath == "/bin/echo");
    assert(loadedSql.comments.size() == 1);
    assert(loadedSql.blobs.size() == 1);
    assert(loadedSql.blobs[0].second == proj.blobs[0].second);
    std::filesystem::remove(sqlitePath);

    // 3. Corrupt file loading
    std::string corruptPath = "/tmp/corrupt_db.bin";
    {
        std::ofstream ofs(corruptPath, std::ios::binary);
        ofs << "not a valid json or sqlite file!!!";
    }
    DatabaseProject corruptProj;
    assert(!dbMgr.loadFromFile(corruptPath, corruptProj));
    std::filesystem::remove(corruptPath);

    std::cout << "  -> test_database_manager_comprehensive PASSED" << std::endl;
}

// ==============================================================================
// 9. MemoryScanner Comprehensive & Boundary Tests
// ==============================================================================
void test_memory_scanner_comprehensive() {
    std::cout << "[TEST] Running test_memory_scanner_comprehensive..." << std::endl;

    MockDebugBackend mockBackend;
    mockBackend.attached_ = true;
    mockBackend.pid_ = 1234;

    // Create a 16KB mock memory buffer with known patterns
    mockBackend.fakeMemory_.resize(16384, 0);

    // Write values at specific offsets
    int32_t val32 = 133742;
    std::memcpy(mockBackend.fakeMemory_.data() + 100, &val32, sizeof(val32));
    std::memcpy(mockBackend.fakeMemory_.data() + 5000, &val32, sizeof(val32));

    float valFloat = 3.14159f;
    std::memcpy(mockBackend.fakeMemory_.data() + 200, &valFloat, sizeof(valFloat));

    std::string secretStr = "EdbNextSecretFlag";
    std::memcpy(mockBackend.fakeMemory_.data() + 300, secretStr.data(), secretStr.size());

    uint8_t bytePattern[] = {0xaa, 0xbb, 0xcc, 0xdd};
    std::memcpy(mockBackend.fakeMemory_.data() + 400, bytePattern, sizeof(bytePattern));

    MemoryRegion reg;
    reg.start = Address(0x10000);
    reg.end = Address(0x14000);
    reg.permissions = "rw-p"; // Read | Write
    mockBackend.fakeRegions_.push_back(reg);

    MemoryScanner scanner;

    // 1. Exact Int32 scan
    ScanOptions opt32;
    opt32.dataType = ScanDataType::Int32;
    opt32.compareType = ScanCompareType::ExactValue;
    opt32.valueStr = "133742";
    opt32.alignment = 4;
    opt32.maxResults = 100;

    size_t count32 = scanner.firstScan(mockBackend, opt32);
    assert(count32 >= 2);

    // 2. Exact Float scan
    ScanOptions optFloat;
    optFloat.dataType = ScanDataType::Float;
    optFloat.compareType = ScanCompareType::ExactValue;
    optFloat.valueStr = "3.14159";
    optFloat.alignment = 4;
    optFloat.maxResults = 100;

    size_t countFloat = scanner.firstScan(mockBackend, optFloat);
    assert(countFloat >= 1);

    // 3. Exact String scan
    ScanOptions optStr;
    optStr.dataType = ScanDataType::String;
    optStr.compareType = ScanCompareType::ExactValue;
    optStr.valueStr = "EdbNextSecretFlag";
    optStr.alignment = 1;
    optStr.maxResults = 100;

    size_t countStr = scanner.firstScan(mockBackend, optStr);
    assert(countStr == 1);
    assert(scanner.results()[0].address == Address(0x10000 + 300));

    // 4. Hex Byte Array with Wildcard scan
    ScanOptions optHex;
    optHex.dataType = ScanDataType::ByteArray;
    optHex.compareType = ScanCompareType::ExactValue;
    optHex.valueStr = "aa ?? cc dd";
    optHex.alignment = 1;
    optHex.maxResults = 100;

    size_t countHex = scanner.firstScan(mockBackend, optHex);
    assert(countHex == 1);
    assert(scanner.results()[0].address == Address(0x10000 + 400));

    // 5. Differential Scan cycle
    // Step A: Unknown initial value scan
    ScanOptions optDiff;
    optDiff.dataType = ScanDataType::Int32;
    optDiff.compareType = ScanCompareType::UnknownInitialValue;
    optDiff.alignment = 4;
    optDiff.maxResults = 1000;

    size_t initialCount = scanner.firstScan(mockBackend, optDiff);
    assert(initialCount > 0);

    // Mutate value at offset 100: from 133742 to 133792 (+50)
    int32_t newVal = 133792;
    std::memcpy(mockBackend.fakeMemory_.data() + 100, &newVal, sizeof(newVal));

    // Step B: Increased by 50 scan
    ScanOptions optIncBy;
    optIncBy.dataType = ScanDataType::Int32;
    optIncBy.compareType = ScanCompareType::IncreasedBy;
    optIncBy.deltaStr = "50";
    optIncBy.alignment = 4;
    optIncBy.maxResults = 1000;

    size_t nextCount = scanner.nextScan(mockBackend, optIncBy);
    assert(nextCount >= 1);
    bool foundTarget = false;
    for (const auto& r : scanner.results()) {
        if (r.address == Address(0x10000 + 100)) {
            foundTarget = true;
            assert(r.formatDelta(ScanDataType::Int32) == "+50");
        }
    }
    assert(foundTarget);

    std::cout << "  -> test_memory_scanner_comprehensive PASSED" << std::endl;
}

// ==============================================================================
// Main Entry Point
// ==============================================================================
int main() {
    std::cout << "========================================\n"
              << "   edb-next Comprehensive Test Suite    \n"
              << "========================================" << std::endl;

    try {
        test_expression_evaluator_comprehensive();
        test_breakpoint_manager_hardware_comprehensive();
        test_micro_ir_edge_cases();
        test_symbolic_engine_mixed_widths_and_edge_cases();
        test_dap_server_robustness();
        test_btf_parser_edge_cases();
        test_elf_parser_memory_safety();
        test_database_manager_comprehensive();
        test_memory_scanner_comprehensive();
    } catch (const std::exception& e) {
        std::cerr << "\n[FATAL EXCEPTION]: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n[FATAL UNKNOWN EXCEPTION]" << std::endl;
        return 1;
    }

    std::cout << "\n========================================\n"
              << "   ALL COMPREHENSIVE TESTS PASSED!     \n"
              << "========================================\n" << std::endl;
    return 0;
}
