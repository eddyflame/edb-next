#pragma once

#include "Types.hpp"
#include "RegisterContext.hpp"
#include <string>
#include <optional>

namespace edb_next {

class LinuxDebugEngine;

struct InstructionDetails {
    Address address{0};
    std::string mnemonic;
    std::string operands;

    // Branch prediction
    bool isBranch{false};
    bool isConditional{false};
    bool branchTaken{false};
    Address branchTarget{0};
    std::string branchTargetSymbol;

    // Effective address / memory operand
    bool hasMemoryOperand{false};
    Address effectiveAddress{0};
    uint64_t memoryValue{0};
    bool memoryReadSuccess{false};

    std::string summary;
    std::string richSummary;
};

class InstructionInspector {
public:
    static InstructionDetails inspect(
        Address addr,
        const RegisterContext& regs,
        LinuxDebugEngine& engine);
};

} // namespace edb_next
