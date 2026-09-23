#pragma once

#include "Types.hpp"
#include <cstdint>

namespace edb_next {

enum class BreakpointType : uint8_t {
    Software,
    HardwareExecute,
    HardwareWrite,
    HardwareReadWrite
};

struct Dr6Status {
    uint64_t rawValue{0};
    bool slot0Hit{false};
    bool slot1Hit{false};
    bool slot2Hit{false};
    bool slot3Hit{false};
    bool singleStepHit{false};
    bool debugRegisterAccess{false};

    [[nodiscard]] int hitSlot() const noexcept {
        if (slot0Hit) return 0;
        if (slot1Hit) return 1;
        if (slot2Hit) return 2;
        if (slot3Hit) return 3;
        return -1;
    }

    [[nodiscard]] bool anySlotHit() const noexcept {
        return slot0Hit || slot1Hit || slot2Hit || slot3Hit;
    }

    static Dr6Status fromRaw(uint64_t raw) noexcept {
        Dr6Status s{};
        s.rawValue = raw;
        s.slot0Hit = (raw & (1ULL << 0)) != 0;
        s.slot1Hit = (raw & (1ULL << 1)) != 0;
        s.slot2Hit = (raw & (1ULL << 2)) != 0;
        s.slot3Hit = (raw & (1ULL << 3)) != 0;
        s.debugRegisterAccess = (raw & (1ULL << 13)) != 0;
        s.singleStepHit = (raw & (1ULL << 14)) != 0;
        return s;
    }
};

struct Breakpoint {
    Address address{0};
    uint8_t originalByte{0};
    bool enabled{false};
    bool isInternal{false};
    uint32_t hitCount{0};
    uint32_t ignoreCount{0};
    std::string condition;
    bool isLogOnly{false};
    std::string logFormat;
    std::string scriptCode;
    std::string scriptLanguage{"python"};
    BreakpointType type{BreakpointType::Software};
    int hardwareSlot{-1};
    bool isPageGuardFallback{false};
    std::string symbol;
};

struct PendingBreakpoint {
    std::string symbol;
    bool enabled{true};
    std::string condition;
    std::string scriptCode;
    std::string scriptLanguage{"python"};
    bool isLogOnly{false};
    std::string logFormat;
};

} // namespace edb_next

