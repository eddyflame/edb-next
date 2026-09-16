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
    std::string symbol;
};

} // namespace edb_next
