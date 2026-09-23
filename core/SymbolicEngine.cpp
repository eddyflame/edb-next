#include "SymbolicEngine.hpp"
#include <z3++.h>
#include <iostream>
#include <sstream>
#include <queue>

namespace edb_next {

class SymbolicEngine::Impl {
public:
    Impl() : solver(ctx) {}

    z3::context ctx;
    z3::solver solver;
    std::unordered_map<std::string, z3::expr> regs;
    std::unordered_map<std::string, z3::expr> initialSymbols;
    std::unordered_map<uint64_t, z3::expr> mem;
    std::optional<std::pair<z3::expr, z3::expr>> lastCmp;

    void reset() {
        solver.reset();
        regs.clear();
        initialSymbols.clear();
        mem.clear();
        lastCmp = std::nullopt;
    }

    z3::expr getOrCreateReg(const std::string& name, uint8_t size = 8) {
        auto it = regs.find(name);
        if (it != regs.end()) {
            return it->second;
        }
        z3::expr sym = ctx.bv_const(name.c_str(), size * 8);
        regs.emplace(name, sym);
        initialSymbols.emplace(name, sym);
        return sym;
    }

    z3::expr getOperandExpr(const IROperand& op, uint8_t defaultSize = 8) {
        uint8_t sz = (op.size > 0) ? op.size : defaultSize;
        if (op.isImm()) {
            return ctx.bv_val(static_cast<uint64_t>(op.immValue), sz * 8);
        }
        if (op.isReg() || op.isVar()) {
            return getOrCreateReg(op.name, sz);
        }
        // Default 0
        return ctx.bv_val(0, sz * 8);
    }
};

SymbolicEngine::SymbolicEngine() : pImpl_(std::make_unique<Impl>()) {}
SymbolicEngine::~SymbolicEngine() = default;
SymbolicEngine::SymbolicEngine(SymbolicEngine&&) noexcept = default;
SymbolicEngine& SymbolicEngine::operator=(SymbolicEngine&&) noexcept = default;

bool SymbolicEngine::isZ3Available() noexcept {
    return true;
}

void SymbolicEngine::reset() {
    pImpl_->reset();
    tainted_.clear();
}

void SymbolicEngine::makeRegisterSymbolic(const std::string& regName, uint8_t size) {
    pImpl_->regs.erase(regName);
    pImpl_->getOrCreateReg(regName, size);
}

void SymbolicEngine::setRegisterConcrete(const std::string& regName, uint64_t val, uint8_t size) {
    pImpl_->regs.insert_or_assign(regName, pImpl_->ctx.bv_val(val, size * 8));
}

std::optional<uint64_t> SymbolicEngine::evaluateRegister(const std::string& regName) const {
    auto it = pImpl_->regs.find(regName);
    if (it == pImpl_->regs.end()) return std::nullopt;

    z3::expr simplified = it->second.simplify();
    if (simplified.is_numeral()) {
        try {
            return simplified.get_numeral_uint64();
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

void SymbolicEngine::setMemoryConcrete(uint64_t addr, uint8_t byteVal) {
    pImpl_->mem.insert_or_assign(addr, pImpl_->ctx.bv_val(byteVal, 8));
}

void SymbolicEngine::setMemorySymbolic(uint64_t addr, const std::string& symName) {
    pImpl_->mem.insert_or_assign(addr, pImpl_->ctx.bv_const(symName.c_str(), 8));
}

void SymbolicEngine::markTainted(const std::string& target) {
    tainted_.insert(target);
}

void SymbolicEngine::clearTaint(const std::string& target) {
    tainted_.erase(target);
}

bool SymbolicEngine::isTainted(const std::string& target) const noexcept {
    return tainted_.contains(target);
}

void SymbolicEngine::stepIR(const IRInstruction& insn) {
    if (insn.isDead) return;

    // Taint propagation
    bool srcTainted = false;
    if (!insn.src1.name.empty() && tainted_.contains(insn.src1.name)) srcTainted = true;
    if (!insn.src2.name.empty() && tainted_.contains(insn.src2.name)) srcTainted = true;

    if (!insn.dst.name.empty()) {
        if (srcTainted) {
            tainted_.insert(insn.dst.name);
        } else if (insn.op == IROp::Mov && insn.src1.isImm()) {
            tainted_.erase(insn.dst.name);
        }
    }

    auto& p = *pImpl_;
    switch (insn.op) {
        case IROp::Mov: {
            if (!insn.dst.name.empty()) {
                p.regs.insert_or_assign(insn.dst.name, p.getOperandExpr(insn.src1, insn.dst.size));
            }
            break;
        }
        case IROp::Add: {
            if (!insn.dst.name.empty()) {
                z3::expr e1 = p.getOperandExpr(insn.src1, insn.dst.size);
                z3::expr e2 = p.getOperandExpr(insn.src2, insn.dst.size);
                p.regs.insert_or_assign(insn.dst.name, e1 + e2);
            }
            break;
        }
        case IROp::Sub: {
            if (!insn.dst.name.empty()) {
                z3::expr e1 = p.getOperandExpr(insn.src1, insn.dst.size);
                z3::expr e2 = p.getOperandExpr(insn.src2, insn.dst.size);
                p.regs.insert_or_assign(insn.dst.name, e1 - e2);
            }
            break;
        }
        case IROp::Mul: {
            if (!insn.dst.name.empty()) {
                z3::expr e1 = p.getOperandExpr(insn.src1, insn.dst.size);
                z3::expr e2 = p.getOperandExpr(insn.src2, insn.dst.size);
                p.regs.insert_or_assign(insn.dst.name, e1 * e2);
            }
            break;
        }
        case IROp::Div: {
            if (!insn.dst.name.empty()) {
                z3::expr e1 = p.getOperandExpr(insn.src1, insn.dst.size);
                z3::expr e2 = p.getOperandExpr(insn.src2, insn.dst.size);
                p.regs.insert_or_assign(insn.dst.name, z3::udiv(e1, e2));
            }
            break;
        }
        case IROp::Mod: {
            if (!insn.dst.name.empty()) {
                z3::expr e1 = p.getOperandExpr(insn.src1, insn.dst.size);
                z3::expr e2 = p.getOperandExpr(insn.src2, insn.dst.size);
                p.regs.insert_or_assign(insn.dst.name, z3::urem(e1, e2));
            }
            break;
        }
        case IROp::And: {
            if (!insn.dst.name.empty()) {
                z3::expr e1 = p.getOperandExpr(insn.src1, insn.dst.size);
                z3::expr e2 = p.getOperandExpr(insn.src2, insn.dst.size);
                p.regs.insert_or_assign(insn.dst.name, e1 & e2);
            }
            break;
        }
        case IROp::Or: {
            if (!insn.dst.name.empty()) {
                z3::expr e1 = p.getOperandExpr(insn.src1, insn.dst.size);
                z3::expr e2 = p.getOperandExpr(insn.src2, insn.dst.size);
                p.regs.insert_or_assign(insn.dst.name, e1 | e2);
            }
            break;
        }
        case IROp::Xor: {
            if (!insn.dst.name.empty()) {
                z3::expr e1 = p.getOperandExpr(insn.src1, insn.dst.size);
                z3::expr e2 = p.getOperandExpr(insn.src2, insn.dst.size);
                p.regs.insert_or_assign(insn.dst.name, e1 ^ e2);
            }
            break;
        }
        case IROp::Not: {
            if (!insn.dst.name.empty()) {
                z3::expr e1 = p.getOperandExpr(insn.src1, insn.dst.size);
                p.regs.insert_or_assign(insn.dst.name, ~e1);
            }
            break;
        }
        case IROp::Neg: {
            if (!insn.dst.name.empty()) {
                z3::expr e1 = p.getOperandExpr(insn.src1, insn.dst.size);
                p.regs.insert_or_assign(insn.dst.name, -e1);
            }
            break;
        }
        case IROp::Shl: {
            if (!insn.dst.name.empty()) {
                z3::expr e1 = p.getOperandExpr(insn.src1, insn.dst.size);
                z3::expr e2 = p.getOperandExpr(insn.src2, insn.dst.size);
                p.regs.insert_or_assign(insn.dst.name, z3::shl(e1, e2));
            }
            break;
        }
        case IROp::Shr: {
            if (!insn.dst.name.empty()) {
                z3::expr e1 = p.getOperandExpr(insn.src1, insn.dst.size);
                z3::expr e2 = p.getOperandExpr(insn.src2, insn.dst.size);
                p.regs.insert_or_assign(insn.dst.name, z3::lshr(e1, e2));
            }
            break;
        }
        case IROp::Sar: {
            if (!insn.dst.name.empty()) {
                z3::expr e1 = p.getOperandExpr(insn.src1, insn.dst.size);
                z3::expr e2 = p.getOperandExpr(insn.src2, insn.dst.size);
                p.regs.insert_or_assign(insn.dst.name, z3::ashr(e1, e2));
            }
            break;
        }
        case IROp::Cmp: {
            z3::expr e1 = p.getOperandExpr(insn.src1, 8);
            z3::expr e2 = p.getOperandExpr(insn.src2, 8);
            p.lastCmp = {e1, e2};
            break;
        }
        default:
            break;
    }
}

void SymbolicEngine::executeBlock(const IRBlock& block) {
    for (const auto& insn : block.instructions) {
        stepIR(insn);
    }
}

SymbolicModelResult SymbolicEngine::checkSatisfiability() {
    SymbolicModelResult res;
    auto checkRes = pImpl_->solver.check();
    if (checkRes == z3::sat) {
        res.satisfiable = true;
        z3::model m = pImpl_->solver.get_model();

        const auto& evalMap = pImpl_->initialSymbols.empty() ? pImpl_->regs : pImpl_->initialSymbols;
        for (const auto& [regName, regExpr] : evalMap) {
            z3::expr val = m.eval(regExpr);
            if (val.is_numeral()) {
                try {
                    res.registerValues[regName] = val.get_numeral_uint64();
                } catch (...) {}
            }
        }

        std::ostringstream oss;
        oss << "SAT: Found concrete input model (" << res.registerValues.size() << " register(s))";
        res.summary = oss.str();
    } else {
        res.satisfiable = false;
        res.summary = (checkRes == z3::unsat) ? "UNSAT: Path is unreachable" : "UNKNOWN: Solver timed out";
    }
    return res;
}

SymbolicModelResult SymbolicEngine::solveReachability(const IRFunction& fn, Address targetAddr) {
    if (fn.blocks.empty()) {
        SymbolicModelResult res;
        res.summary = "Function has no blocks";
        return res;
    }

    const IRBlock* targetBlock = fn.findBlockByAddress(targetAddr);
    if (!targetBlock) {
        SymbolicModelResult res;
        res.summary = "Target address not found in function blocks";
        return res;
    }

    // Find path using BFS from block 0 to targetBlock
    std::unordered_map<int, int> parent;
    std::unordered_map<int, bool> visited;
    std::queue<int> q;

    int startId = fn.blocks.front().id;
    int targetId = targetBlock->id;

    q.push(startId);
    visited[startId] = true;
    parent[startId] = -1;

    bool pathFound = false;
    while (!q.empty()) {
        int cur = q.front();
        q.pop();

        if (cur == targetId) {
            pathFound = true;
            break;
        }

        const IRBlock* b = fn.findBlock(cur);
        if (b) {
            for (int succ : b->successors) {
                if (!visited[succ]) {
                    visited[succ] = true;
                    parent[succ] = cur;
                    q.push(succ);
                }
            }
        }
    }

    if (!pathFound) {
        SymbolicModelResult res;
        res.summary = "No control flow path connects entry to target block";
        return res;
    }

    // Reconstruct path
    std::vector<int> path;
    int curr = targetId;
    while (curr != -1) {
        path.push_back(curr);
        curr = parent[curr];
    }
    std::reverse(path.begin(), path.end());

    // Execute along the path and accumulate constraints
    reset();

    for (size_t i = 0; i < path.size(); ++i) {
        const IRBlock* blk = fn.findBlock(path[i]);
        if (!blk) continue;

        executeBlock(*blk);

        // If next block is branch target, assert condition
        if (i + 1 < path.size() && pImpl_->lastCmp.has_value()) {
            int nextId = path[i + 1];
            // Check if last instruction in block was CJmp
            if (!blk->instructions.empty()) {
                const auto& lastInsn = blk->instructions.back();
                if (lastInsn.op == IROp::CJmp) {
                    auto [e1, e2] = *pImpl_->lastCmp;
                    z3::expr condExpr = (lastInsn.cond == "e" || lastInsn.cond == "z") ? (e1 == e2) :
                                        (lastInsn.cond == "ne" || lastInsn.cond == "nz") ? (e1 != e2) :
                                        (lastInsn.cond == "g") ? z3::sgt(e1, e2) :
                                        (lastInsn.cond == "ge") ? z3::sge(e1, e2) :
                                        (lastInsn.cond == "l") ? z3::slt(e1, e2) :
                                        (lastInsn.cond == "le") ? z3::sle(e1, e2) :
                                        (lastInsn.cond == "a") ? z3::ugt(e1, e2) :
                                        (lastInsn.cond == "b") ? z3::ult(e1, e2) : (e1 == e2);

                    const IRBlock* nextBlk = fn.findBlock(nextId);
                    bool targetTaken = (nextBlk && lastInsn.dst.immValue >= nextBlk->startAddr.value() && lastInsn.dst.immValue <= nextBlk->endAddr.value());

                    if (targetTaken) {
                        pImpl_->solver.add(condExpr);
                    } else {
                        pImpl_->solver.add(!condExpr);
                    }
                }
            }
        }
    }

    return checkSatisfiability();
}

} // namespace edb_next
