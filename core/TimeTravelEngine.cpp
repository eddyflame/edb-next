#include "TimeTravelEngine.hpp"
#include <chrono>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>

namespace edb_next {

TimeTravelEngine::TimeTravelEngine(size_t maxFrames)
    : maxFrames_(maxFrames > 0 ? maxFrames : 1000) {}

void TimeTravelEngine::recordFrame(Address rip, const RegisterContext& regs,
                                   const std::string& disasm,
                                   std::span<const MemoryDelta> deltas) {
    if (isReplaying_ && currentCursor_ + 1 < timeline_.size()) {
        timeline_.resize(currentCursor_ + 1);
    }

    TimeFrame frame;
    frame.frameIndex = timeline_.size();
    frame.rip = rip;
    frame.registers = regs;
    frame.disassembly = disasm;

    auto now = std::chrono::steady_clock::now().time_since_epoch();
    frame.timestampNs = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();

    for (const auto& delta : deltas) {
        frame.memoryDeltas.push_back(delta);
    }
    for (auto& pending : pendingDeltas_) {
        frame.memoryDeltas.push_back(std::move(pending));
    }
    pendingDeltas_.clear();

    if (timeline_.size() >= maxFrames_) {
        timeline_.erase(timeline_.begin());
        // Re-index remaining frames
        for (size_t i = 0; i < timeline_.size(); ++i) {
            timeline_[i].frameIndex = i;
        }
    }

    timeline_.push_back(std::move(frame));
    currentCursor_ = timeline_.size() - 1;
    isReplaying_ = false;
}

void TimeTravelEngine::recordMemoryChange(Address addr, std::span<const uint8_t> oldBytes,
                                          std::span<const uint8_t> newBytes) {
    MemoryDelta delta;
    delta.address = addr;
    delta.oldBytes.assign(oldBytes.begin(), oldBytes.end());
    delta.newBytes.assign(newBytes.begin(), newBytes.end());
    pendingDeltas_.push_back(std::move(delta));
}

bool TimeTravelEngine::canStepBack() const noexcept {
    return !timeline_.empty() && currentCursor_ > 0;
}

bool TimeTravelEngine::canStepForward() const noexcept {
    return !timeline_.empty() && currentCursor_ + 1 < timeline_.size();
}

std::optional<TimeFrame> TimeTravelEngine::stepBack() {
    if (!canStepBack()) {
        return std::nullopt;
    }
    currentCursor_--;
    isReplaying_ = true;
    return timeline_[currentCursor_];
}

std::optional<TimeFrame> TimeTravelEngine::stepForward() {
    if (!canStepForward()) {
        return std::nullopt;
    }
    currentCursor_++;
    if (currentCursor_ + 1 == timeline_.size()) {
        isReplaying_ = false;
    }
    return timeline_[currentCursor_];
}

std::optional<TimeFrame> TimeTravelEngine::seekFrame(size_t index) {
    if (timeline_.empty()) return std::nullopt;
    if (index >= timeline_.size()) {
        index = timeline_.size() - 1;
    }
    currentCursor_ = index;
    isReplaying_ = (currentCursor_ + 1 < timeline_.size());
    return timeline_[currentCursor_];
}

std::optional<TimeFrame> TimeTravelEngine::reverseContinue(const std::unordered_set<uint64_t>& breakpointAddresses) {
    if (!canStepBack()) {
        return std::nullopt;
    }

    while (currentCursor_ > 0) {
        currentCursor_--;
        if (breakpointAddresses.contains(timeline_[currentCursor_].rip.value())) {
            break;
        }
    }

    isReplaying_ = true;
    return timeline_[currentCursor_];
}

void TimeTravelEngine::clear() {
    timeline_.clear();
    pendingDeltas_.clear();
    currentCursor_ = 0;
    isReplaying_ = false;
}

bool TimeTravelEngine::isHardwareBranchTracingSupported() noexcept {
    struct perf_event_attr pe{};
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_BRANCH_INSTRUCTIONS;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    long fd = syscall(SYS_perf_event_open, &pe, 0, -1, -1, 0);
    if (fd >= 0) {
        close(static_cast<int>(fd));
        return true;
    }
    return false;
}

} // namespace edb_next
