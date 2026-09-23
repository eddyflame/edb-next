#include "DecompilerEngine.hpp"
#include <sstream>
#include <iomanip>
#include <unordered_set>
#include <algorithm>

namespace edb_next {

std::optional<int> DecompiledFunction::lineForAddress(Address addr) const {
    int bestLine = -1;
    uint64_t minDiff = UINT64_MAX;

    for (const auto& mapping : sourceMap) {
        if (mapping.address.value() == addr.value()) {
            return mapping.line;
        }
        if (mapping.address.value() <= addr.value()) {
            uint64_t diff = addr.value() - mapping.address.value();
            if (diff < minDiff) {
                minDiff = diff;
                bestLine = mapping.line;
            }
        }
    }
    if (bestLine != -1 && minDiff < 32) {
        return bestLine;
    }
    return std::nullopt;
}

std::optional<Address> DecompiledFunction::addressForLine(int line) const {
    for (const auto& mapping : sourceMap) {
        if (mapping.line == line) {
            return mapping.address;
        }
    }
    return std::nullopt;
}

std::string DecompilerEngine::operandToC(const IROperand& op) {
    switch (op.kind) {
        case IROperandKind::None: return "";
        case IROperandKind::Register:
        case IROperandKind::Variable: return op.name;
        case IROperandKind::Immediate: {
            if (op.immValue <= 9) return std::to_string(op.immValue);
            std::ostringstream oss;
            oss << "0x" << std::hex << op.immValue;
            return oss.str();
        }
        case IROperandKind::Memory: {
            std::string typeCast;
            switch (op.size) {
                case 1: typeCast = "*(uint8_t*)("; break;
                case 2: typeCast = "*(uint16_t*)("; break;
                case 4: typeCast = "*(uint32_t*)("; break;
                default: typeCast = "*(uint64_t*)("; break;
            }
            return typeCast + op.name + ")";
        }
    }
    return "";
}

std::string DecompilerEngine::formatCondition(const std::string& cond, const IROperand& op1, const IROperand& op2) {
    std::string s1 = operandToC(op1);
    std::string s2 = operandToC(op2);
    if (s1.empty()) s1 = "res";
    if (s2.empty()) s2 = "0";

    if (cond == "e" || cond == "z") return s1 + " == " + s2;
    if (cond == "ne" || cond == "nz") return s1 + " != " + s2;
    if (cond == "g") return s1 + " > " + s2;
    if (cond == "ge") return s1 + " >= " + s2;
    if (cond == "l") return s1 + " < " + s2;
    if (cond == "le") return s1 + " <= " + s2;
    if (cond == "a") return "(uint64_t)" + s1 + " > (uint64_t)" + s2;
    if (cond == "ae") return "(uint64_t)" + s1 + " >= (uint64_t)" + s2;
    if (cond == "b") return "(uint64_t)" + s1 + " < (uint64_t)" + s2;
    if (cond == "be") return "(uint64_t)" + s1 + " <= (uint64_t)" + s2;

    return s1 + " != 0";
}

DecompiledFunction DecompilerEngine::decompile(const IRFunction& irFunc) {
    DecompiledFunction result;
    result.name = irFunc.name;
    result.entryAddress = irFunc.entryAddr;

    if (irFunc.blocks.empty()) {
        result.pseudoCode = "// No code to decompile\n";
        return result;
    }

    std::ostringstream out;
    int currentLine = 1;

    auto emitLine = [&](const std::string& text, Address addr = Address{0}) {
        out << text << "\n";
        if (!addr.isNull()) {
            result.sourceMap.push_back(SourceMapping{currentLine, addr});
        }
        currentLine++;
    };

    // Collect all local variables and registers used
    std::unordered_set<std::string> declaredVars;
    for (const auto& blk : irFunc.blocks) {
        for (const auto& insn : blk.instructions) {
            if (insn.isDead) continue;
            if (insn.dst.isVar()) {
                declaredVars.insert(insn.dst.name);
            }
        }
    }

    emitLine("// Decompiled by edb-next C++23 Native Decompiler", result.entryAddress);
    emitLine(std::string("// Entry Address: ") + result.entryAddress.toHex());
    emitLine("");

    // Function Signature
    emitLine("int64_t " + result.name + "() {", result.entryAddress);

    // Local variable declarations
    if (!declaredVars.empty()) {
        std::vector<std::string> sortedVars(declaredVars.begin(), declaredVars.end());
        std::sort(sortedVars.begin(), sortedVars.end());
        std::string varDecl = "    int64_t ";
        for (size_t i = 0; i < sortedVars.size(); ++i) {
            varDecl += sortedVars[i] + " = 0" + (i + 1 < sortedVars.size() ? ", " : ";");
            result.variableTypes[sortedVars[i]] = "int64_t";
        }
        emitLine(varDecl);
        emitLine("");
    }

    std::optional<std::pair<IROperand, IROperand>> lastCmpOperands;

    for (size_t bIdx = 0; bIdx < irFunc.blocks.size(); ++bIdx) {
        const auto& blk = irFunc.blocks[bIdx];
        if (bIdx > 0) {
            emitLine("");
        }

        std::string labelName = "loc_" + blk.startAddr.toHex(false);
        emitLine(labelName + ":", blk.startAddr);

        for (const auto& insn : blk.instructions) {
            if (insn.isDead) continue;

            std::string line = "    ";
            switch (insn.op) {
                case IROp::Nop:
                    continue;

                case IROp::Mov: {
                    std::string dst = operandToC(insn.dst);
                    std::string src = operandToC(insn.src1);
                    line += dst + " = " + src + ";";
                    emitLine(line, insn.originAddr);
                    break;
                }
                case IROp::Add:
                case IROp::Sub:
                case IROp::Mul:
                case IROp::Div:
                case IROp::Mod:
                case IROp::And:
                case IROp::Or:
                case IROp::Xor:
                case IROp::Shl:
                case IROp::Shr:
                case IROp::Sar: {
                    std::string dst = operandToC(insn.dst);
                    std::string s1 = operandToC(insn.src1);
                    std::string s2 = operandToC(insn.src2);
                    std::string opSym;

                    switch (insn.op) {
                        case IROp::Add: opSym = "+"; break;
                        case IROp::Sub: opSym = "-"; break;
                        case IROp::Mul: opSym = "*"; break;
                        case IROp::Div: opSym = "/"; break;
                        case IROp::Mod: opSym = "%"; break;
                        case IROp::And: opSym = "&"; break;
                        case IROp::Or:  opSym = "|"; break;
                        case IROp::Xor: opSym = "^"; break;
                        case IROp::Shl: opSym = "<<"; break;
                        case IROp::Shr: opSym = ">>"; break;
                        case IROp::Sar: opSym = ">>"; break;
                        default: opSym = "+"; break;
                    }

                    if (dst == s1) {
                        line += dst + " " + opSym + "= " + s2 + ";";
                    } else {
                        line += dst + " = " + s1 + " " + opSym + " " + s2 + ";";
                    }
                    emitLine(line, insn.originAddr);
                    break;
                }
                case IROp::Not: {
                    line += operandToC(insn.dst) + " = ~" + operandToC(insn.src1) + ";";
                    emitLine(line, insn.originAddr);
                    break;
                }
                case IROp::Neg: {
                    line += operandToC(insn.dst) + " = -" + operandToC(insn.src1) + ";";
                    emitLine(line, insn.originAddr);
                    break;
                }
                case IROp::Load: {
                    line += operandToC(insn.dst) + " = " + operandToC(insn.src1) + ";";
                    emitLine(line, insn.originAddr);
                    break;
                }
                case IROp::Store: {
                    line += operandToC(insn.dst) + " = " + operandToC(insn.src1) + ";";
                    emitLine(line, insn.originAddr);
                    break;
                }
                case IROp::Cmp: {
                    lastCmpOperands = {insn.src1, insn.src2};
                    break;
                }
                case IROp::CJmp: {
                    std::string condStr;
                    if (lastCmpOperands.has_value()) {
                        condStr = formatCondition(insn.cond, lastCmpOperands->first, lastCmpOperands->second);
                    } else {
                        condStr = formatCondition(insn.cond, insn.src1, insn.src2);
                    }

                    Address targetAddr(insn.dst.immValue);
                    std::string targetLoc = "loc_" + targetAddr.toHex(false);
                    line += "if (" + condStr + ") goto " + targetLoc + ";";
                    emitLine(line, insn.originAddr);
                    lastCmpOperands = std::nullopt;
                    break;
                }
                case IROp::Jmp: {
                    Address targetAddr(insn.dst.immValue);
                    std::string targetLoc = "loc_" + targetAddr.toHex(false);
                    line += "goto " + targetLoc + ";";
                    emitLine(line, insn.originAddr);
                    break;
                }
                case IROp::Call: {
                    Address targetAddr(insn.dst.immValue);
                    std::string targetName = "sub_" + targetAddr.toHex(false);
                    line += targetName + "();";
                    emitLine(line, insn.originAddr);
                    break;
                }
                case IROp::Ret: {
                    line += "return rax;";
                    emitLine(line, insn.originAddr);
                    break;
                }
                case IROp::Syscall: {
                    line += "syscall();";
                    emitLine(line, insn.originAddr);
                    break;
                }
                default:
                    break;
            }
        }
    }

    emitLine("}");
    result.pseudoCode = out.str();
    return result;
}

DecompiledFunction DecompilerEngine::decompile(const CFGGraph& cfg, const std::string& funcName) {
    IRFunction irFn = IRLifter::lift(cfg);
    irFn.name = funcName;
    IRLifter::runAllPasses(irFn);
    return decompile(irFn);
}

DecompiledFunction DecompilerEngine::decompile(std::span<const DisassembledInstruction> instructions,
                                               const std::string& funcName) {
    IRFunction irFn = IRLifter::lift(instructions);
    irFn.name = funcName;
    IRLifter::runAllPasses(irFn);
    return decompile(irFn);
}

} // namespace edb_next
