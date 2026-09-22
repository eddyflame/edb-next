#pragma once

#include "Types.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace edb_next {

struct DisassembledInstruction {
    Address address{0};
    std::string mnemonic;
    std::string operands;
    std::vector<uint8_t> bytes;
    std::string symbol;
    bool isCurrentRip{false};
    bool hasBreakpoint{false};
    bool isBreakpointEnabled{true};
    std::string sourceFile;
    std::string sourceFullPath;
    int sourceLine{0};
    std::string sourceText;
    bool isSourceLineStart{false};
};

} // namespace edb_next
