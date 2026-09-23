#pragma once

#include "Types.hpp"
#include "RegisterContext.hpp"
#include <string>
#include <vector>
#include <optional>
#include <unordered_set>
#include <span>
#include <cstdint>

namespace edb_next {

struct MemoryDelta {
    Address address{0};
    std::vector<uint8_t> oldBytes;
    std::vector<uint8_t> newBytes;
};

struct TimeFrame {
    size_t frameIndex{0};
    Address rip{0};
    RegisterContext registers;
    std::vector<MemoryDelta> memoryDeltas;
    std::string disassembly;
    uint64_t timestampNs{0};
};

class TimeTravelEngine {
public:
    explicit TimeTravelEngine(size_t maxFrames = 10000);
    ~TimeTravelEngine() = default;

    void recordFrame(Address rip, const RegisterContext& regs,
                     const std::string& disasm = "",
                     std::span<const MemoryDelta> deltas = {});

    void recordMemoryChange(Address addr, std::span<const uint8_t> oldBytes, std::span<const uint8_t> newBytes);

    [[nodiscard]] bool canStepBack() const noexcept;
    [[nodiscard]] bool canStepForward() const noexcept;

    std::optional<TimeFrame> stepBack();
    std::optional<TimeFrame> stepForward();
    std::optional<TimeFrame> seekFrame(size_t index);
    std::optional<TimeFrame> reverseContinue(const std::unordered_set<uint64_t>& breakpointAddresses);

    void clear();

    [[nodiscard]] size_t frameCount() const noexcept { return timeline_.size(); }
    [[nodiscard]] size_t currentFrameIndex() const noexcept { return currentCursor_; }
    [[nodiscard]] bool isReplaying() const noexcept { return isReplaying_; }
    [[nodiscard]] const std::vector<TimeFrame>& timeline() const noexcept { return timeline_; }

    [[nodiscard]] static bool isHardwareBranchTracingSupported() noexcept;

private:
    size_t maxFrames_{10000};
    std::vector<TimeFrame> timeline_;
    size_t currentCursor_{0};
    bool isReplaying_{false};
    std::vector<MemoryDelta> pendingDeltas_;
};

} // namespace edb_next
