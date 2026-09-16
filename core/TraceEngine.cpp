#include "TraceEngine.hpp"

namespace edb_next {

TraceEngine::TraceEngine(QObject* parent) : QObject(parent) {
}

void TraceEngine::recordHit(Address addr) {
    if (!hitTraceEnabled_ || addr.isNull()) return;
    auto [_, inserted] = hitAddresses_.insert(addr);
    if (inserted) {
        Q_EMIT traceUpdated();
    }
}

bool TraceEngine::isHit(Address addr) const {
    return hitAddresses_.find(addr) != hitAddresses_.end();
}

void TraceEngine::clearHitTrace() {
    hitAddresses_.clear();
    Q_EMIT traceUpdated();
}

void TraceEngine::clearRunTrace() {
    traceHistory_.clear();
    lastRegs_.reset();
    stepCounter_ = 0;
    Q_EMIT traceUpdated();
}

void TraceEngine::recordFrame(Address addr, const std::string& mnemonic, const std::string& operands, const RegisterContext& regs) {
    recordHit(addr);

    if (!runTraceEnabled_) return;

    std::vector<std::string> changed;
    if (lastRegs_.has_value()) {
        if (regs.rax() != lastRegs_->rax()) changed.push_back("RAX");
        if (regs.rbx() != lastRegs_->rbx()) changed.push_back("RBX");
        if (regs.rcx() != lastRegs_->rcx()) changed.push_back("RCX");
        if (regs.rdx() != lastRegs_->rdx()) changed.push_back("RDX");
        if (regs.rsi() != lastRegs_->rsi()) changed.push_back("RSI");
        if (regs.rdi() != lastRegs_->rdi()) changed.push_back("RDI");
        if (regs.rbp() != lastRegs_->rbp()) changed.push_back("RBP");
        if (regs.rsp() != lastRegs_->rsp()) changed.push_back("RSP");
        if (regs.r8()  != lastRegs_->r8())  changed.push_back("R8");
        if (regs.r9()  != lastRegs_->r9())  changed.push_back("R9");
        if (regs.r10() != lastRegs_->r10()) changed.push_back("R10");
        if (regs.r11() != lastRegs_->r11()) changed.push_back("R11");
        if (regs.r12() != lastRegs_->r12()) changed.push_back("R12");
        if (regs.r13() != lastRegs_->r13()) changed.push_back("R13");
        if (regs.r14() != lastRegs_->r14()) changed.push_back("R14");
        if (regs.r15() != lastRegs_->r15()) changed.push_back("R15");
        if (regs.eflags() != lastRegs_->eflags()) changed.push_back("FLAGS");
    }

    TraceFrame frame{
        .stepIndex = ++stepCounter_,
        .address = addr,
        .mnemonic = mnemonic,
        .operands = operands,
        .registers = regs,
        .changedRegs = changed
    };

    if (traceHistory_.size() >= kMaxTraceFrames) {
        traceHistory_.erase(traceHistory_.begin());
    }
    traceHistory_.push_back(std::move(frame));
    currentFrameIndex_ = traceHistory_.size() - 1;
    lastRegs_ = regs;

    Q_EMIT traceUpdated();
}

void TraceEngine::setCurrentFrameIndex(size_t idx) {
    if (idx < traceHistory_.size()) {
        currentFrameIndex_ = idx;
        Q_EMIT traceUpdated();
    }
}

std::optional<TraceFrame> TraceEngine::currentFrame() const {
    if (currentFrameIndex_ < traceHistory_.size()) {
        return traceHistory_[currentFrameIndex_];
    }
    return std::nullopt;
}

bool TraceEngine::stepBack() {
    if (currentFrameIndex_ > 0) {
        currentFrameIndex_--;
        Q_EMIT traceUpdated();
        return true;
    }
    return false;
}

bool TraceEngine::stepForward() {
    if (currentFrameIndex_ + 1 < traceHistory_.size()) {
        currentFrameIndex_++;
        Q_EMIT traceUpdated();
        return true;
    }
    return false;
}

} // namespace edb_next
