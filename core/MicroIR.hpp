#pragma once

#include "Types.hpp"
#include "DisassemblyTypes.hpp"
#include "CFGBuilder.hpp"
#include <string>
#include <vector>
#include <optional>
#include <span>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace edb_next {

enum class IROp {
    Nop,
    Mov,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    And,
    Or,
    Xor,
    Not,
    Neg,
    Shl,
    Shr,
    Sar,
    Load,
    Store,
    Cmp,
    CJmp,
    Jmp,
    Call,
    Ret,
    Syscall,
    Phi
};

enum class IROperandKind {
    None,
    Register,
    Immediate,
    Variable,
    Memory
};

struct IROperand {
    IROperandKind kind{IROperandKind::None};
    std::string name;
    uint64_t immValue{0};
    uint8_t size{8}; // Size in bytes: 1, 2, 4, 8

    [[nodiscard]] static IROperand Reg(std::string_view regName, uint8_t sz = 8) {
        IROperand op;
        op.kind = IROperandKind::Register;
        op.name = std::string(regName);
        op.size = sz;
        return op;
    }

    [[nodiscard]] static IROperand Imm(uint64_t val, uint8_t sz = 8) {
        IROperand op;
        op.kind = IROperandKind::Immediate;
        op.immValue = val;
        op.size = sz;
        return op;
    }

    [[nodiscard]] static IROperand Var(int id, uint8_t sz = 8) {
        IROperand op;
        op.kind = IROperandKind::Variable;
        op.name = "v" + std::to_string(id);
        op.size = sz;
        return op;
    }

    [[nodiscard]] static IROperand VarNamed(std::string_view varName, uint8_t sz = 8) {
        IROperand op;
        op.kind = IROperandKind::Variable;
        op.name = std::string(varName);
        op.size = sz;
        return op;
    }

    [[nodiscard]] static IROperand Mem(std::string_view expr, uint8_t sz = 8) {
        IROperand op;
        op.kind = IROperandKind::Memory;
        op.name = std::string(expr);
        op.size = sz;
        return op;
    }

    [[nodiscard]] static IROperand None() {
        return IROperand{};
    }

    [[nodiscard]] bool isReg() const noexcept { return kind == IROperandKind::Register; }
    [[nodiscard]] bool isImm() const noexcept { return kind == IROperandKind::Immediate; }
    [[nodiscard]] bool isVar() const noexcept { return kind == IROperandKind::Variable; }
    [[nodiscard]] bool isMem() const noexcept { return kind == IROperandKind::Memory; }
    [[nodiscard]] bool isNone() const noexcept { return kind == IROperandKind::None; }

    [[nodiscard]] std::string toString() const;
    [[nodiscard]] bool operator==(const IROperand& other) const;
};

struct IRInstruction {
    Address originAddr{0};
    IROp op{IROp::Nop};
    IROperand dst;
    IROperand src1;
    IROperand src2;
    std::string cond; // Condition for CJmp: e.g. "e", "ne", "g", "ge", "l", "le", "a", "b"
    bool isDead{false};

    [[nodiscard]] std::string toString() const;
};

struct IRBlock {
    int id{0};
    Address startAddr{0};
    Address endAddr{0};
    std::vector<IRInstruction> instructions;
    std::vector<int> successors;
    std::vector<int> predecessors;

    [[nodiscard]] std::string dump() const;
};

struct IRFunction {
    Address entryAddr{0};
    std::string name{"sub_entry"};
    std::vector<IRBlock> blocks;

    [[nodiscard]] const IRBlock* findBlock(int id) const noexcept;
    [[nodiscard]] IRBlock* findBlock(int id) noexcept;
    [[nodiscard]] const IRBlock* findBlockByAddress(Address addr) const noexcept;
    [[nodiscard]] std::string dump() const;
};

class IRLifter {
public:
    [[nodiscard]] static IRFunction lift(std::span<const DisassembledInstruction> instructions);
    [[nodiscard]] static IRFunction lift(const CFGGraph& cfg);
    [[nodiscard]] static std::vector<IRInstruction> liftInstruction(const DisassembledInstruction& insn, int& nextVarId);
    [[nodiscard]] static std::vector<IRInstruction> liftInstruction(Address addr, const std::string& mnemonic, const std::string& operands, int& nextVarId);

    // Optimization passes
    static size_t eliminateDeadCode(IRFunction& fn);
    static size_t foldConstants(IRFunction& fn);
    static size_t simplifyOpaquePredicates(IRFunction& fn);
    static size_t runAllPasses(IRFunction& fn);
};

} // namespace edb_next
