#include "MicroIR.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace edb_next {

namespace {

std::string trim(std::string_view str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return std::string(str.substr(first, (last - first + 1)));
}

std::vector<std::string> splitOperands(const std::string& operands) {
    std::vector<std::string> result;
    std::string current;
    bool inBracket = false;

    for (char c : operands) {
        if (c == '[') inBracket = true;
        else if (c == ']') inBracket = false;

        if (c == ',' && !inBracket) {
            result.push_back(trim(current));
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        result.push_back(trim(current));
    }
    return result;
}

uint64_t parseHexOrDec(const std::string& str) {
    if (str.empty()) return 0;
    try {
        if (str.starts_with("0x") || str.starts_with("0X")) {
            return std::stoull(str, nullptr, 16);
        }
        if (std::all_of(str.begin(), str.end(), ::isdigit)) {
            return std::stoull(str, nullptr, 10);
        }
        return std::stoull(str, nullptr, 16);
    } catch (...) {
        return 0;
    }
}

IROperand parseOperandString(const std::string& text, uint8_t defaultSz = 8) {
    std::string s = trim(text);
    if (s.empty()) return IROperand::None();

    // Check if memory operand e.g. [rax+0x10] or qword ptr [rax]
    size_t openBracket = s.find('[');
    size_t closeBracket = s.rfind(']');
    if (openBracket != std::string::npos && closeBracket != std::string::npos && closeBracket > openBracket) {
        uint8_t sz = defaultSz;
        if (s.find("byte ptr") != std::string::npos) sz = 1;
        else if (s.find("word ptr") != std::string::npos) sz = 2;
        else if (s.find("dword ptr") != std::string::npos) sz = 4;
        else if (s.find("qword ptr") != std::string::npos) sz = 8;

        std::string inner = s.substr(openBracket + 1, closeBracket - openBracket - 1);
        return IROperand::Mem(trim(inner), sz);
    }

    // Check if immediate number
    bool isNumber = false;
    if (s.starts_with("0x") || s.starts_with("0X") || s.starts_with("-0x") || s.starts_with("-0X")) {
        isNumber = true;
    } else if (std::all_of(s.begin() + (s.starts_with('-') ? 1 : 0), s.end(), ::isdigit)) {
        isNumber = true;
    }

    if (isNumber) {
        return IROperand::Imm(parseHexOrDec(s), defaultSz);
    }

    // Register or named variable
    return IROperand::Reg(s, defaultSz);
}

} // anonymous namespace

std::string IROperand::toString() const {
    switch (kind) {
        case IROperandKind::None: return "";
        case IROperandKind::Register: return name;
        case IROperandKind::Variable: return name;
        case IROperandKind::Memory: return "[" + name + "]";
        case IROperandKind::Immediate: {
            std::ostringstream oss;
            oss << "0x" << std::hex << immValue;
            return oss.str();
        }
    }
    return "";
}

bool IROperand::operator==(const IROperand& other) const {
    if (kind != other.kind || size != other.size) return false;
    if (kind == IROperandKind::Immediate) return immValue == other.immValue;
    return name == other.name;
}

std::string IRInstruction::toString() const {
    if (isDead) return "; (dead) " + originAddr.toHex();

    std::ostringstream oss;
    oss << "0x" << std::hex << originAddr.value() << ": ";

    auto opToStr = [](IROp op) -> std::string {
        switch (op) {
            case IROp::Nop: return "nop";
            case IROp::Mov: return "mov";
            case IROp::Add: return "add";
            case IROp::Sub: return "sub";
            case IROp::Mul: return "mul";
            case IROp::Div: return "div";
            case IROp::Mod: return "mod";
            case IROp::And: return "and";
            case IROp::Or: return "or";
            case IROp::Xor: return "xor";
            case IROp::Not: return "not";
            case IROp::Neg: return "neg";
            case IROp::Shl: return "shl";
            case IROp::Shr: return "shr";
            case IROp::Sar: return "sar";
            case IROp::Load: return "load";
            case IROp::Store: return "store";
            case IROp::Cmp: return "cmp";
            case IROp::CJmp: return "cjmp";
            case IROp::Jmp: return "jmp";
            case IROp::Call: return "call";
            case IROp::Ret: return "ret";
            case IROp::Syscall: return "syscall";
            case IROp::Phi: return "phi";
        }
        return "unknown";
    };

    if (op == IROp::CJmp) {
        oss << "cjmp." << cond << " " << src1.toString() << ", target: " << dst.toString();
        return oss.str();
    }
    if (op == IROp::Jmp || op == IROp::Call) {
        oss << opToStr(op) << " " << dst.toString();
        return oss.str();
    }
    if (op == IROp::Store) {
        oss << "store " << dst.toString() << ", " << src1.toString();
        return oss.str();
    }

    if (!dst.isNone()) {
        oss << dst.toString() << " = ";
    }
    oss << opToStr(op);
    if (!src1.isNone()) {
        oss << " " << src1.toString();
    }
    if (!src2.isNone()) {
        oss << ", " << src2.toString();
    }
    return oss.str();
}

std::string IRBlock::dump() const {
    std::ostringstream oss;
    oss << "Block " << id << " [0x" << std::hex << startAddr.value() << " - 0x" << endAddr.value() << "]:\n";
    for (const auto& insn : instructions) {
        oss << "  " << insn.toString() << "\n";
    }
    oss << "  -> Succs: [";
    for (size_t i = 0; i < successors.size(); ++i) {
        oss << successors[i] << (i + 1 < successors.size() ? ", " : "");
    }
    oss << "]\n";
    return oss.str();
}

const IRBlock* IRFunction::findBlock(int id) const noexcept {
    for (const auto& b : blocks) {
        if (b.id == id) return &b;
    }
    return nullptr;
}

IRBlock* IRFunction::findBlock(int id) noexcept {
    for (auto& b : blocks) {
        if (b.id == id) return &b;
    }
    return nullptr;
}

const IRBlock* IRFunction::findBlockByAddress(Address addr) const noexcept {
    for (const auto& b : blocks) {
        if (addr >= b.startAddr && addr <= b.endAddr) return &b;
    }
    return nullptr;
}

std::string IRFunction::dump() const {
    std::ostringstream oss;
    oss << "Function " << name << " @ 0x" << std::hex << entryAddr.value() << " (" << blocks.size() << " blocks):\n";
    for (const auto& b : blocks) {
        oss << b.dump();
    }
    return oss.str();
}

std::vector<IRInstruction> IRLifter::liftInstruction(Address addr, const std::string& mnemonic, const std::string& operands, int& nextVarId) {
    std::vector<IRInstruction> result;
    std::string m = mnemonic;
    std::transform(m.begin(), m.end(), m.begin(), ::tolower);
    auto ops = splitOperands(operands);

    if (m == "nop") {
        IRInstruction ir;
        ir.originAddr = addr;
        ir.op = IROp::Nop;
        result.push_back(ir);
        return result;
    }

    if (m == "ret") {
        IRInstruction ir;
        ir.originAddr = addr;
        ir.op = IROp::Ret;
        result.push_back(ir);
        return result;
    }

    if (m == "syscall") {
        IRInstruction ir;
        ir.originAddr = addr;
        ir.op = IROp::Syscall;
        result.push_back(ir);
        return result;
    }

    if (m == "jmp") {
        IRInstruction ir;
        ir.originAddr = addr;
        ir.op = IROp::Jmp;
        if (!ops.empty()) {
            ir.dst = parseOperandString(ops[0]);
        }
        result.push_back(ir);
        return result;
    }

    if (m.starts_with('j')) {
        // Conditional jump: je, jz, jne, jnz, jg, jge, jl, jle, ja, jae, jb, jbe
        std::string cond = m.substr(1);
        if (cond == "z") cond = "e";
        if (cond == "nz") cond = "ne";

        IRInstruction ir;
        ir.originAddr = addr;
        ir.op = IROp::CJmp;
        ir.cond = cond;
        if (!ops.empty()) {
            ir.dst = parseOperandString(ops[0]);
        }
        result.push_back(ir);
        return result;
    }

    if (m == "call") {
        IRInstruction ir;
        ir.originAddr = addr;
        ir.op = IROp::Call;
        if (!ops.empty()) {
            ir.dst = parseOperandString(ops[0]);
        }
        result.push_back(ir);
        return result;
    }

    if (m == "push") {
        if (!ops.empty()) {
            auto src = parseOperandString(ops[0]);
            // sub rsp, 8
            IRInstruction subRsp;
            subRsp.originAddr = addr;
            subRsp.op = IROp::Sub;
            subRsp.dst = IROperand::Reg("rsp");
            subRsp.src1 = IROperand::Reg("rsp");
            subRsp.src2 = IROperand::Imm(8);
            result.push_back(subRsp);

            // store [rsp], src
            IRInstruction store;
            store.originAddr = addr;
            store.op = IROp::Store;
            store.dst = IROperand::Mem("rsp");
            store.src1 = src;
            result.push_back(store);
        }
        return result;
    }

    if (m == "pop") {
        if (!ops.empty()) {
            auto dst = parseOperandString(ops[0]);
            // load dst, [rsp]
            IRInstruction load;
            load.originAddr = addr;
            load.op = IROp::Load;
            load.dst = dst;
            load.src1 = IROperand::Mem("rsp");
            result.push_back(load);

            // add rsp, 8
            IRInstruction addRsp;
            addRsp.originAddr = addr;
            addRsp.op = IROp::Add;
            addRsp.dst = IROperand::Reg("rsp");
            addRsp.src1 = IROperand::Reg("rsp");
            addRsp.src2 = IROperand::Imm(8);
            result.push_back(addRsp);
        }
        return result;
    }

    if (ops.size() >= 2) {
        auto dst = parseOperandString(ops[0]);
        auto src = parseOperandString(ops[1]);

        if (m == "mov") {
            if (dst.isMem()) {
                IRInstruction ir;
                ir.originAddr = addr;
                ir.op = IROp::Store;
                ir.dst = dst;
                ir.src1 = src;
                result.push_back(ir);
            } else if (src.isMem()) {
                IRInstruction ir;
                ir.originAddr = addr;
                ir.op = IROp::Load;
                ir.dst = dst;
                ir.src1 = src;
                result.push_back(ir);
            } else {
                IRInstruction ir;
                ir.originAddr = addr;
                ir.op = IROp::Mov;
                ir.dst = dst;
                ir.src1 = src;
                result.push_back(ir);
            }
            return result;
        }

        if (m == "lea") {
            IRInstruction ir;
            ir.originAddr = addr;
            ir.op = IROp::Mov;
            ir.dst = dst;
            ir.src1 = IROperand::VarNamed(src.name); // Pure address expression without memory dereference
            result.push_back(ir);
            return result;
        }

        if (m == "xor" && dst == src) {
            // Optimization canonicalization: xor reg, reg -> mov reg, 0
            IRInstruction ir;
            ir.originAddr = addr;
            ir.op = IROp::Mov;
            ir.dst = dst;
            ir.src1 = IROperand::Imm(0, dst.size);
            result.push_back(ir);
            return result;
        }

        if (m == "cmp") {
            IRInstruction ir;
            ir.originAddr = addr;
            ir.op = IROp::Cmp;
            ir.src1 = dst;
            ir.src2 = src;
            result.push_back(ir);
            return result;
        }

        if (m == "test") {
            if (dst == src) {
                IRInstruction ir;
                ir.originAddr = addr;
                ir.op = IROp::Cmp;
                ir.src1 = dst;
                ir.src2 = IROperand::Imm(0, dst.size);
                result.push_back(ir);
            } else {
                auto temp = IROperand::Var(nextVarId++);
                IRInstruction andInsn;
                andInsn.originAddr = addr;
                andInsn.op = IROp::And;
                andInsn.dst = temp;
                andInsn.src1 = dst;
                andInsn.src2 = src;
                result.push_back(andInsn);

                IRInstruction cmpInsn;
                cmpInsn.originAddr = addr;
                cmpInsn.op = IROp::Cmp;
                cmpInsn.src1 = temp;
                cmpInsn.src2 = IROperand::Imm(0, dst.size);
                result.push_back(cmpInsn);
            }
            return result;
        }

        IROp op = IROp::Nop;
        if (m == "add") op = IROp::Add;
        else if (m == "sub") op = IROp::Sub;
        else if (m == "imul" || m == "mul") op = IROp::Mul;
        else if (m == "and") op = IROp::And;
        else if (m == "or") op = IROp::Or;
        else if (m == "xor") op = IROp::Xor;
        else if (m == "shl") op = IROp::Shl;
        else if (m == "shr") op = IROp::Shr;
        else if (m == "sar") op = IROp::Sar;

        if (op != IROp::Nop) {
            IRInstruction ir;
            ir.originAddr = addr;
            ir.op = op;
            ir.dst = dst;
            ir.src1 = dst;
            ir.src2 = src;
            result.push_back(ir);
            return result;
        }
    } else if (ops.size() == 1) {
        auto dst = parseOperandString(ops[0]);
        if (m == "not") {
            IRInstruction ir;
            ir.originAddr = addr;
            ir.op = IROp::Not;
            ir.dst = dst;
            ir.src1 = dst;
            result.push_back(ir);
            return result;
        }
        if (m == "neg") {
            IRInstruction ir;
            ir.originAddr = addr;
            ir.op = IROp::Neg;
            ir.dst = dst;
            ir.src1 = dst;
            result.push_back(ir);
            return result;
        }
        if (m == "inc") {
            IRInstruction ir;
            ir.originAddr = addr;
            ir.op = IROp::Add;
            ir.dst = dst;
            ir.src1 = dst;
            ir.src2 = IROperand::Imm(1, dst.size);
            result.push_back(ir);
            return result;
        }
        if (m == "dec") {
            IRInstruction ir;
            ir.originAddr = addr;
            ir.op = IROp::Sub;
            ir.dst = dst;
            ir.src1 = dst;
            ir.src2 = IROperand::Imm(1, dst.size);
            result.push_back(ir);
            return result;
        }
    }

    // Fallback: Generic instruction placeholder
    IRInstruction fallback;
    fallback.originAddr = addr;
    fallback.op = IROp::Nop;
    result.push_back(fallback);
    return result;
}

std::vector<IRInstruction> IRLifter::liftInstruction(const DisassembledInstruction& insn, int& nextVarId) {
    return liftInstruction(insn.address, insn.mnemonic, insn.operands, nextVarId);
}

IRFunction IRLifter::lift(std::span<const DisassembledInstruction> instructions) {
    if (instructions.empty()) return IRFunction{};
    CFGGraph cfg = CFGBuilder::build(instructions);
    return lift(cfg);
}

IRFunction IRLifter::lift(const CFGGraph& cfg) {
    IRFunction fn;
    if (cfg.blocks.empty()) return fn;

    fn.entryAddr = cfg.blocks.front().startAddr;
    int varCounter = 0;

    for (const auto& blk : cfg.blocks) {
        IRBlock irBlk;
        irBlk.id = blk.id;
        irBlk.startAddr = blk.startAddr;
        irBlk.endAddr = blk.endAddr;

        for (const auto& insn : blk.instructions) {
            auto lifted = liftInstruction(insn.address, insn.mnemonic, insn.operands, varCounter);
            for (auto& irInsn : lifted) {
                irBlk.instructions.push_back(std::move(irInsn));
            }
        }

        irBlk.successors = cfg.getSuccessors(blk.id);
        irBlk.predecessors = cfg.getPredecessors(blk.id);
        fn.blocks.push_back(std::move(irBlk));
    }

    return fn;
}

size_t IRLifter::eliminateDeadCode(IRFunction& fn) {
    size_t eliminated = 0;

    // Count usages of variables
    std::unordered_map<std::string, int> useCounts;
    for (const auto& blk : fn.blocks) {
        for (const auto& insn : blk.instructions) {
            if (insn.isDead) continue;
            if (insn.src1.isVar()) useCounts[insn.src1.name]++;
            if (insn.src2.isVar()) useCounts[insn.src2.name]++;
        }
    }

    for (auto& blk : fn.blocks) {
        for (auto& insn : blk.instructions) {
            if (insn.isDead) continue;
            // Only eliminate pure SSA variable assignments with zero uses and no side effects
            if (insn.dst.isVar() && useCounts[insn.dst.name] == 0) {
                if (insn.op != IROp::Store && insn.op != IROp::Call && insn.op != IROp::Syscall &&
                    insn.op != IROp::CJmp && insn.op != IROp::Jmp && insn.op != IROp::Ret) {
                    insn.isDead = true;
                    eliminated++;
                }
            }
        }
    }

    return eliminated;
}

size_t IRLifter::foldConstants(IRFunction& fn) {
    size_t folded = 0;

    for (auto& blk : fn.blocks) {
        std::unordered_map<std::string, uint64_t> constEnv;

        for (auto& insn : blk.instructions) {
            if (insn.isDead) continue;

            // Substitute known constants in src1 and src2
            if (insn.src1.isVar() || insn.src1.isReg()) {
                auto it = constEnv.find(insn.src1.name);
                if (it != constEnv.end()) {
                    insn.src1 = IROperand::Imm(it->second, insn.src1.size);
                }
            }
            if (insn.src2.isVar() || insn.src2.isReg()) {
                auto it = constEnv.find(insn.src2.name);
                if (it != constEnv.end()) {
                    insn.src2 = IROperand::Imm(it->second, insn.src2.size);
                }
            }

            // Fold constant operations
            if (insn.src1.isImm() && insn.src2.isImm()) {
                uint64_t v1 = insn.src1.immValue;
                uint64_t v2 = insn.src2.immValue;
                uint64_t res = 0;
                bool canFold = true;

                switch (insn.op) {
                    case IROp::Add: res = v1 + v2; break;
                    case IROp::Sub: res = v1 - v2; break;
                    case IROp::Mul: res = v1 * v2; break;
                    case IROp::Div: res = (v2 != 0) ? (v1 / v2) : 0; break;
                    case IROp::And: res = v1 & v2; break;
                    case IROp::Or:  res = v1 | v2; break;
                    case IROp::Xor: res = v1 ^ v2; break;
                    case IROp::Shl: res = v1 << (v2 & 63); break;
                    case IROp::Shr: res = v1 >> (v2 & 63); break;
                    default: canFold = false; break;
                }

                if (canFold && !insn.dst.isNone()) {
                    insn.op = IROp::Mov;
                    insn.src1 = IROperand::Imm(res, insn.dst.size);
                    insn.src2 = IROperand::None();
                    constEnv[insn.dst.name] = res;
                    folded++;
                    continue;
                }
            } else if (insn.op == IROp::Mov && insn.src1.isImm() && !insn.dst.isNone()) {
                constEnv[insn.dst.name] = insn.src1.immValue;
            } else if (!insn.dst.isNone()) {
                // Invalidate constant if modified dynamically
                constEnv.erase(insn.dst.name);
            }
        }
    }

    return folded;
}

size_t IRLifter::simplifyOpaquePredicates(IRFunction& fn) {
    size_t simplified = 0;

    for (auto& blk : fn.blocks) {
        std::optional<std::pair<uint64_t, uint64_t>> lastCmp;

        for (auto& insn : blk.instructions) {
            if (insn.isDead) continue;

            if (insn.op == IROp::Cmp && insn.src1.isImm() && insn.src2.isImm()) {
                lastCmp = {insn.src1.immValue, insn.src2.immValue};
            } else if (insn.op == IROp::CJmp && lastCmp.has_value()) {
                uint64_t c1 = lastCmp->first;
                uint64_t c2 = lastCmp->second;
                bool alwaysTaken = false;
                bool alwaysFalse = false;

                if (insn.cond == "e" || insn.cond == "z") {
                    alwaysTaken = (c1 == c2);
                    alwaysFalse = (c1 != c2);
                } else if (insn.cond == "ne" || insn.cond == "nz") {
                    alwaysTaken = (c1 != c2);
                    alwaysFalse = (c1 == c2);
                } else if (insn.cond == "g" || insn.cond == "a") {
                    alwaysTaken = (c1 > c2);
                    alwaysFalse = (c1 <= c2);
                } else if (insn.cond == "l" || insn.cond == "b") {
                    alwaysTaken = (c1 < c2);
                    alwaysFalse = (c1 >= c2);
                }

                if (alwaysTaken) {
                    // Turn into unconditional jump
                    insn.op = IROp::Jmp;
                    insn.cond.clear();
                    simplified++;
                } else if (alwaysFalse) {
                    // Dead branch: turn into Nop
                    insn.op = IROp::Nop;
                    insn.cond.clear();
                    simplified++;
                }
            } else if (insn.op != IROp::Nop) {
                // Reset cmp on other operations
                if (insn.op != IROp::Cmp) {
                    lastCmp = std::nullopt;
                }
            }
        }
    }

    return simplified;
}

size_t IRLifter::runAllPasses(IRFunction& fn) {
    size_t total = 0;
    total += foldConstants(fn);
    total += simplifyOpaquePredicates(fn);
    total += eliminateDeadCode(fn);
    return total;
}

} // namespace edb_next
