#include "InstructionInspector.hpp"
#include "LinuxDebugEngine.hpp"
#include "CapstoneContext.hpp"
#include <capstone/capstone.h>
#include <sstream>
#include <iomanip>

namespace edb_next {

namespace {

uint64_t getCapstoneRegValue(x86_reg reg, const RegisterContext& regs) {
    switch (reg) {
        // RAX / EAX / AX / AH / AL
        case X86_REG_RAX: return regs.rax();
        case X86_REG_EAX: return static_cast<uint32_t>(regs.rax());
        case X86_REG_AX:  return static_cast<uint16_t>(regs.rax());
        case X86_REG_AH:  return static_cast<uint8_t>((regs.rax() >> 8) & 0xFF);
        case X86_REG_AL:  return static_cast<uint8_t>(regs.rax() & 0xFF);

        // RBX / EBX / BX / BH / BL
        case X86_REG_RBX: return regs.rbx();
        case X86_REG_EBX: return static_cast<uint32_t>(regs.rbx());
        case X86_REG_BX:  return static_cast<uint16_t>(regs.rbx());
        case X86_REG_BH:  return static_cast<uint8_t>((regs.rbx() >> 8) & 0xFF);
        case X86_REG_BL:  return static_cast<uint8_t>(regs.rbx() & 0xFF);

        // RCX / ECX / CX / CH / CL
        case X86_REG_RCX: return regs.rcx();
        case X86_REG_ECX: return static_cast<uint32_t>(regs.rcx());
        case X86_REG_CX:  return static_cast<uint16_t>(regs.rcx());
        case X86_REG_CH:  return static_cast<uint8_t>((regs.rcx() >> 8) & 0xFF);
        case X86_REG_CL:  return static_cast<uint8_t>(regs.rcx() & 0xFF);

        // RDX / EDX / DX / DH / DL
        case X86_REG_RDX: return regs.rdx();
        case X86_REG_EDX: return static_cast<uint32_t>(regs.rdx());
        case X86_REG_DX:  return static_cast<uint16_t>(regs.rdx());
        case X86_REG_DH:  return static_cast<uint8_t>((regs.rdx() >> 8) & 0xFF);
        case X86_REG_DL:  return static_cast<uint8_t>(regs.rdx() & 0xFF);

        // RSI / ESI / SI / SIL
        case X86_REG_RSI: return regs.rsi();
        case X86_REG_ESI: return static_cast<uint32_t>(regs.rsi());
        case X86_REG_SI:  return static_cast<uint16_t>(regs.rsi());
        case X86_REG_SIL: return static_cast<uint8_t>(regs.rsi() & 0xFF);

        // RDI / EDI / DI / DIL
        case X86_REG_RDI: return regs.rdi();
        case X86_REG_EDI: return static_cast<uint32_t>(regs.rdi());
        case X86_REG_DI:  return static_cast<uint16_t>(regs.rdi());
        case X86_REG_DIL: return static_cast<uint8_t>(regs.rdi() & 0xFF);

        // RBP / EBP / BP / BPL
        case X86_REG_RBP: return regs.rbp().value();
        case X86_REG_EBP: return static_cast<uint32_t>(regs.rbp().value());
        case X86_REG_BP:  return static_cast<uint16_t>(regs.rbp().value());
        case X86_REG_BPL: return static_cast<uint8_t>(regs.rbp().value() & 0xFF);

        // RSP / ESP / SP / SPL
        case X86_REG_RSP: return regs.rsp().value();
        case X86_REG_ESP: return static_cast<uint32_t>(regs.rsp().value());
        case X86_REG_SP:  return static_cast<uint16_t>(regs.rsp().value());
        case X86_REG_SPL: return static_cast<uint8_t>(regs.rsp().value() & 0xFF);

        // R8
        case X86_REG_R8:  return regs.r8();
        case X86_REG_R8D: return static_cast<uint32_t>(regs.r8());
        case X86_REG_R8W: return static_cast<uint16_t>(regs.r8());
        case X86_REG_R8B: return static_cast<uint8_t>(regs.r8() & 0xFF);

        // R9
        case X86_REG_R9:  return regs.r9();
        case X86_REG_R9D: return static_cast<uint32_t>(regs.r9());
        case X86_REG_R9W: return static_cast<uint16_t>(regs.r9());
        case X86_REG_R9B: return static_cast<uint8_t>(regs.r9() & 0xFF);

        // R10
        case X86_REG_R10:  return regs.r10();
        case X86_REG_R10D: return static_cast<uint32_t>(regs.r10());
        case X86_REG_R10W: return static_cast<uint16_t>(regs.r10());
        case X86_REG_R10B: return static_cast<uint8_t>(regs.r10() & 0xFF);

        // R11
        case X86_REG_R11:  return regs.r11();
        case X86_REG_R11D: return static_cast<uint32_t>(regs.r11());
        case X86_REG_R11W: return static_cast<uint16_t>(regs.r11());
        case X86_REG_R11B: return static_cast<uint8_t>(regs.r11() & 0xFF);

        // R12
        case X86_REG_R12:  return regs.r12();
        case X86_REG_R12D: return static_cast<uint32_t>(regs.r12());
        case X86_REG_R12W: return static_cast<uint16_t>(regs.r12());
        case X86_REG_R12B: return static_cast<uint8_t>(regs.r12() & 0xFF);

        // R13
        case X86_REG_R13:  return regs.r13();
        case X86_REG_R13D: return static_cast<uint32_t>(regs.r13());
        case X86_REG_R13W: return static_cast<uint16_t>(regs.r13());
        case X86_REG_R13B: return static_cast<uint8_t>(regs.r13() & 0xFF);

        // R14
        case X86_REG_R14:  return regs.r14();
        case X86_REG_R14D: return static_cast<uint32_t>(regs.r14());
        case X86_REG_R14W: return static_cast<uint16_t>(regs.r14());
        case X86_REG_R14B: return static_cast<uint8_t>(regs.r14() & 0xFF);

        // R15
        case X86_REG_R15:  return regs.r15();
        case X86_REG_R15D: return static_cast<uint32_t>(regs.r15());
        case X86_REG_R15W: return static_cast<uint16_t>(regs.r15());
        case X86_REG_R15B: return static_cast<uint8_t>(regs.r15() & 0xFF);

        // RIP
        case X86_REG_RIP:
            return regs.rip().value();
        default:
            return 0;
    }
}

} // namespace

InstructionDetails InstructionInspector::inspect(
    Address addr,
    const RegisterContext& regs,
    LinuxDebugEngine& engine) {

    InstructionDetails details;
    details.address = addr;

    if (!engine.isAttached() || addr.isNull()) return details;

    uint8_t code[16] = {0};
    if (!engine.readMemory(addr, code, sizeof(code))) {
        return details;
    }

    auto cs = CapstoneContext::acquire(true);
    if (!cs.isValid()) {
        return details;
    }
    csh handle = cs.get();

    cs_insn* insn = nullptr;
    size_t count = cs_disasm(handle, code, sizeof(code), addr.value(), 1, &insn);
    if (count == 0 || !insn) {
        return details;
    }

    details.mnemonic = insn[0].mnemonic;
    details.operands = insn[0].op_str;

    const auto& detail = insn[0].detail->x86;
    uint64_t eflags = regs.eflags();
    bool cf = (eflags & 0x0001) != 0;
    bool pf = (eflags & 0x0004) != 0;
    bool zf = (eflags & 0x0040) != 0;
    bool sf = (eflags & 0x0080) != 0;
    bool of = (eflags & 0x0800) != 0;

    int insn_id = insn[0].id;

    // 1. Check branch condition
    switch (insn_id) {
        case X86_INS_JMP:
            details.isBranch = true;
            details.isConditional = false;
            details.branchTaken = true;
            break;
        case X86_INS_CALL:
            details.isBranch = true;
            details.isConditional = false;
            details.branchTaken = true;
            break;
        case X86_INS_JE:
            details.isBranch = true;
            details.isConditional = true;
            details.branchTaken = zf;
            break;
        case X86_INS_JNE:
            details.isBranch = true;
            details.isConditional = true;
            details.branchTaken = !zf;
            break;
        case X86_INS_JA:
            details.isBranch = true;
            details.isConditional = true;
            details.branchTaken = (!cf && !zf);
            break;
        case X86_INS_JAE:
            details.isBranch = true;
            details.isConditional = true;
            details.branchTaken = !cf;
            break;
        case X86_INS_JB:
            details.isBranch = true;
            details.isConditional = true;
            details.branchTaken = cf;
            break;
        case X86_INS_JBE:
            details.isBranch = true;
            details.isConditional = true;
            details.branchTaken = (cf || zf);
            break;
        case X86_INS_JG:
            details.isBranch = true;
            details.isConditional = true;
            details.branchTaken = (!zf && (sf == of));
            break;
        case X86_INS_JGE:
            details.isBranch = true;
            details.isConditional = true;
            details.branchTaken = (sf == of);
            break;
        case X86_INS_JL:
            details.isBranch = true;
            details.isConditional = true;
            details.branchTaken = (sf != of);
            break;
        case X86_INS_JLE:
            details.isBranch = true;
            details.isConditional = true;
            details.branchTaken = (zf || (sf != of));
            break;
        case X86_INS_JS:
            details.isBranch = true;
            details.isConditional = true;
            details.branchTaken = sf;
            break;
        case X86_INS_JNS:
            details.isBranch = true;
            details.isConditional = true;
            details.branchTaken = !sf;
            break;
        case X86_INS_JO:
            details.isBranch = true;
            details.isConditional = true;
            details.branchTaken = of;
            break;
        case X86_INS_JNO:
            details.isBranch = true;
            details.isConditional = true;
            details.branchTaken = !of;
            break;
        case X86_INS_JP:
            details.isBranch = true;
            details.isConditional = true;
            details.branchTaken = pf;
            break;
        case X86_INS_JNP:
            details.isBranch = true;
            details.isConditional = true;
            details.branchTaken = !pf;
            break;
        default:
            break;
    }

    // Determine branch target address
    if (details.isBranch && detail.op_count > 0) {
        const auto& op = detail.operands[0];
        if (op.type == X86_OP_IMM) {
            details.branchTarget = Address(static_cast<uint64_t>(op.imm));
        } else if (op.type == X86_OP_REG) {
            details.branchTarget = Address(getCapstoneRegValue(op.reg, regs));
        }
    }

    // 2. Check memory operands and calculate Effective Address
    for (int i = 0; i < detail.op_count; ++i) {
        const auto& op = detail.operands[i];
        if (op.type == X86_OP_MEM) {
            details.hasMemoryOperand = true;
            uint64_t effAddr = 0;
            if (op.mem.base == X86_REG_RIP) {
                effAddr = addr.value() + insn[0].size + op.mem.disp;
            } else {
                if (op.mem.base != X86_REG_INVALID) {
                    effAddr += getCapstoneRegValue(op.mem.base, regs);
                }
                if (op.mem.index != X86_REG_INVALID) {
                    effAddr += getCapstoneRegValue(op.mem.index, regs) * op.mem.scale;
                }
                effAddr += op.mem.disp;
            }

            details.effectiveAddress = Address(effAddr);
            uint64_t memVal = 0;
            if (engine.readMemory(details.effectiveAddress, &memVal, sizeof(memVal))) {
                details.memoryValue = memVal;
                details.memoryReadSuccess = true;
            }
            if (details.isBranch && details.branchTarget.isNull()) {
                if (details.memoryReadSuccess) {
                    details.branchTarget = Address(details.memoryValue);
                } else if (!details.effectiveAddress.isNull()) {
                    details.branchTarget = details.effectiveAddress;
                }
            }
            break; // take first memory operand
        }
    }

    // 3. Build human readable summary
    std::ostringstream oss;
    std::ostringstream rich;
    if (details.isBranch) {
        if (details.isConditional) {
            if (details.branchTaken) {
                oss << "✔ JUMP IS TAKEN ➔ " << details.branchTarget.toHex();
                rich << "<span style='color:#4caf50; font-weight:bold;'>✔ JUMP IS TAKEN</span> ➔ <span style='color:#4fc3f7; font-weight:bold;'>" << details.branchTarget.toHex() << "</span>";
            } else {
                oss << "✘ JUMP NOT TAKEN (Fall through)";
                rich << "<span style='color:#ef5350; font-weight:bold;'>✘ JUMP NOT TAKEN</span> (Fall through)";
            }
        } else {
            oss << "➔ UNCONDITIONAL JUMP ➔ " << details.branchTarget.toHex();
            rich << "<span style='color:#ffb74d; font-weight:bold;'>➔ UNCONDITIONAL JUMP</span> ➔ <span style='color:#4fc3f7; font-weight:bold;'>" << details.branchTarget.toHex() << "</span>";
        }
    }

    if (details.hasMemoryOperand) {
        if (!oss.str().empty()) oss << " | ";
        if (!rich.str().empty()) rich << " <span style='color:#757575;'>|</span> ";
        oss << "Mem: " << details.effectiveAddress.toHex();
        rich << "<span style='color:#bb86fc; font-weight:bold;'>Mem:</span> " << details.effectiveAddress.toHex();
        if (details.memoryReadSuccess) {
            oss << " => 0x" << std::hex << details.memoryValue;
            rich << " =&gt; <span style='color:#ffb74d; font-family:monospace;'>0x" << std::hex << details.memoryValue << "</span>";
        }
    }

    details.summary = oss.str();
    details.richSummary = rich.str();

    cs_free(insn, count);
    return details;
}

} // namespace edb_next
