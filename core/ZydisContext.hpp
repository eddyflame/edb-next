#pragma once

#include "Types.hpp"
#include "DisassemblyTypes.hpp"
#include "ConfigurationManager.hpp"
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <optional>
#include <span>

namespace edb_next {

struct FastInstructionInfo {
    Address address{0};
    uint8_t length{0};
    bool isValid{false};
    bool isCall{false};
    bool isRet{false};
    bool isBranch{false};
    bool isConditional{false};
    bool isSyscall{false};
    bool isRep{false};
    Address branchTarget{0};
    std::string mnemonic;
};

class ZydisContext {
public:
    ZydisContext() = delete;

    /// Checks if Zydis is compiled in and available.
    [[nodiscard]] static bool isAvailable() noexcept;

    /// Extremely fast instruction decode for control flow checks (stepOver, branch target, etc.)
    /// Decodes on the stack with zero dynamic memory allocation (~15ns).
    [[nodiscard]] static FastInstructionInfo decodeFast(const uint8_t* code, size_t size, Address runtimeAddr) noexcept;
    [[nodiscard]] static FastInstructionInfo decodeFast(std::span<const uint8_t> code, Address runtimeAddr) noexcept {
        return decodeFast(code.data(), code.size(), runtimeAddr);
    }

    /// Batch disassembly of up to `count` instructions into DisassembledInstruction structs.
    [[nodiscard]] static std::vector<DisassembledInstruction> disassemble(
        const uint8_t* code,
        size_t size,
        Address startAddr,
        size_t count,
        DisassemblySyntax syntax = DisassemblySyntax::Intel,
        bool uppercaseMnemonics = false,
        bool simplifyRipRel = true);
};

} // namespace edb_next
