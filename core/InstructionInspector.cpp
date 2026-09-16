#include "InstructionInspector.hpp"
#include "LinuxDebugEngine.hpp"
#include <capstone/capstone.h>
#include <sstream>
#include <iomanip>

namespace edb_next {

namespace {

uint64_t getCapstoneRegValue(x86_reg reg, const RegisterContext& regs) {
    switch (reg) {
        case X86_REG_RAX: case X86_REG_EAX: case X86_REG_AX: case X86_REG_AL: case X86_REG_AH:
            return regs.rax();
        case X86_REG_RBX: case X86_REG_EBX: case X86_REG_BX: case X86_REG_BL: case X86_REG_BH:
            return regs.rbx();
        case X86_REG_RCX: case X86_REG_ECX: case X86_REG_CX: case X86_REG_CL: case X86_REG_CH:
            return regs.rcx();
        case X86_REG_RDX: case X86_REG_EDX: case X86_REG_DX: case X86_REG_DL: case X86_REG_DH:
            return regs.rdx();
        case X86_REG_RSI: case X86_REG_ESI: case X86_REG_SI: case X86_REG_SIL:
            return regs.rsi();
        case X86_REG_RDI: case X86_REG_EDI: case X86_REG_DI: case X86_REG_DIL:
            return regs.rdi();
        case X86_REG_RBP: case X86_REG_EBP: case X86_REG_BP: case X86_REG_BPL:
            return regs.rbp().value();
        case X86_REG_RSP: case X86_REG_ESP: case X86_REG_SP: case X86_REG_SPL:
            return regs.rsp().value();
        case X86_REG_R8: case X86_REG_R8D: case X86_REG_R8W: case X86_REG_R8B:
            return regs.r8();
        case X86_REG_R9: case X86_REG_R9D: case X86_REG_R9W: case X86_REG_R9B:
            return regs.r9();
        case X86_REG_R10: case X86_REG_R10D: case X86_REG_R10W: case X86_REG_R10B:
            return regs.r10();
        case X86_REG_R11: case X86_REG_R11D: case X86_REG_R11W: case X86_REG_R11B:
            return regs.r11();
        case X86_REG_R12: case X86_REG_R12D: case X86_REG_R12W: case X86_REG_R12B:
            return regs.r12();
        case X86_REG_R13: case X86_REG_R13D: case X86_REG_R13W: case X86_REG_R13B:
            return regs.r13();
        case X86_REG_R14: case X86_REG_R14D: case X86_REG_R14W: case X86_REG_R14B:
            return regs.r14();
        case X86_REG_R15: case X86_REG_R15D: case X86_REG_R15W: case X86_REG_R15B:
            return regs.r15();
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

    csh handle;
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK) {
        return details;
    }
    cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);

    cs_insn* insn = nullptr;
    size_t count = cs_disasm(handle, code, sizeof(code), addr.value(), 1, &insn);
    if (count == 0 || !insn) {
        cs_close(&handle);
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
            break; // take first memory operand
        }
    }

    // 3. Build human readable summary
    std::ostringstream oss;
    if (details.isBranch) {
        if (details.isConditional) {
            if (details.branchTaken) {
                oss << "✔ JUMP IS TAKEN ➔ " << details.branchTarget.toHex();
            } else {
                oss << "✘ JUMP NOT TAKEN (Fall through)";
            }
        } else {
            oss << "➔ UNCONDITIONAL JUMP ➔ " << details.branchTarget.toHex();
        }
    }

    if (details.hasMemoryOperand) {
        if (!oss.str().empty()) oss << " | ";
        oss << "Mem: " << details.effectiveAddress.toHex();
        if (details.memoryReadSuccess) {
            oss << " => 0x" << std::hex << details.memoryValue;
        }
    }

    details.summary = oss.str();

    cs_free(insn, count);
    cs_close(&handle);
    return details;
}

} // namespace edb_next
