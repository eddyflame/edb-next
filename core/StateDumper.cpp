#include "StateDumper.hpp"
#include "DebugSession.hpp"
#include <sstream>
#include <iomanip>

namespace edb_next {

std::string StateDumper::dumpState(DebugSession& session) {
    std::ostringstream ss;
    const auto& regs = session.registers();

    ss << "============================== CPU STATE DUMP ==============================\n";
    ss << "PID: " << session.pid() << "  State: " << (session.state() == SessionState::Paused ? "PAUSED" : "RUNNING") << "\n";
    ss << "-------------------------------- REGISTERS ---------------------------------\n";

    auto fmtReg = [](const char* name, uint64_t val) {
        std::ostringstream r;
        std::string label = std::string(name) + ":";
        r << std::left << std::setw(5) << label << " 0x" << std::hex << std::setw(16) << std::setfill('0') << val;
        return r.str();
    };

    ss << fmtReg("RAX", regs.rax()) << "   " << fmtReg("RBX", regs.rbx()) << "   " << fmtReg("RCX", regs.rcx()) << "\n";
    ss << fmtReg("RDX", regs.rdx()) << "   " << fmtReg("RSI", regs.rsi()) << "   " << fmtReg("RDI", regs.rdi()) << "\n";
    ss << fmtReg("RBP", regs.rbp().value()) << "   " << fmtReg("RSP", regs.rsp().value()) << "   " << fmtReg("RIP", regs.rip().value()) << "\n";
    ss << fmtReg("R8",  regs.r8())  << "   " << fmtReg("R9",  regs.r9())  << "   " << fmtReg("R10", regs.r10()) << "\n";
    ss << fmtReg("R11", regs.r11()) << "   " << fmtReg("R12", regs.r12()) << "   " << fmtReg("R13", regs.r13()) << "\n";
    ss << fmtReg("R14", regs.r14()) << "   " << fmtReg("R15", regs.r15()) << "\n";

    ss << "---------------------------------- FLAGS -----------------------------------\n";
    uint64_t eflags = regs.raw().eflags;
    ss << "EFLAGS: 0x" << std::hex << std::setw(8) << std::setfill('0') << eflags << " [ ";
    if (eflags & (1 << 0)) ss << "CF ";
    if (eflags & (1 << 2)) ss << "PF ";
    if (eflags & (1 << 4)) ss << "AF ";
    if (eflags & (1 << 6)) ss << "ZF ";
    if (eflags & (1 << 7)) ss << "SF ";
    if (eflags & (1 << 8)) ss << "TF ";
    if (eflags & (1 << 9)) ss << "IF ";
    if (eflags & (1 << 10)) ss << "DF ";
    if (eflags & (1 << 11)) ss << "OF ";
    ss << "]\n";

    ss << "------------------------------ DISASSEMBLY ---------------------------------\n";
    // Disassemble 12 instructions around RIP
    Address ripAddr = regs.rip();
    Address disasmStart = ripAddr.value() >= 16 ? ripAddr - 16 : ripAddr;
    auto insns = session.disassemble(disasmStart, 10);
    for (const auto& insn : insns) {
        if (insn.address == ripAddr) {
            ss << "=> " << std::left << std::setw(18) << insn.address.toHex() << ": "
               << std::setw(8) << insn.mnemonic << " " << insn.operands;
        } else {
            ss << "   " << std::left << std::setw(18) << insn.address.toHex() << ": "
               << std::setw(8) << insn.mnemonic << " " << insn.operands;
        }
        if (!insn.symbol.empty()) {
            ss << "  " << insn.symbol;
        }
        ss << "\n";
    }

    ss << "---------------------------------- STACK -----------------------------------\n";
    Address rspAddr = regs.rsp();
    for (int i = 0; i < 8; ++i) {
        Address curStack = rspAddr + (i * 8);
        auto valBytes = session.readMemory(curStack, 8);
        if (valBytes.size() == 8) {
            uint64_t val = *reinterpret_cast<const uint64_t*>(valBytes.data());
            ss << "[RSP+" << std::hex << std::setw(2) << std::setfill('0') << (i * 8) << "] "
               << curStack.toHex() << ": 0x" << std::setw(16) << std::setfill('0') << val;

            if (auto sym = session.symbols().findNearestSymbol(Address(val))) {
                ss << " (<" << sym->first.name;
                if (sym->second > 0) ss << "+0x" << std::hex << sym->second;
                ss << ">)";
            }
            ss << "\n";
        }
    }
    ss << "============================================================================\n";

    return ss.str();
}

} // namespace edb_next
