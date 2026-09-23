#pragma once

#include "Types.hpp"
#include "DisassemblyTypes.hpp"
#include "CFGBuilder.hpp"
#include "MicroIR.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <span>

namespace edb_next {

struct SourceMapping {
    int line{1};            // 1-based line number in generated C pseudo-code
    Address address{0};     // Corresponding machine instruction address
};

struct DecompiledFunction {
    std::string name{"sub_entry"};
    Address entryAddress{0};
    std::string pseudoCode;
    std::vector<SourceMapping> sourceMap;
    std::unordered_map<std::string, std::string> variableTypes;

    [[nodiscard]] std::optional<int> lineForAddress(Address addr) const;
    [[nodiscard]] std::optional<Address> addressForLine(int line) const;
};

class DecompilerEngine {
public:
    [[nodiscard]] static DecompiledFunction decompile(std::span<const DisassembledInstruction> instructions,
                                                     const std::string& funcName = "sub_func");

    [[nodiscard]] static DecompiledFunction decompile(const IRFunction& irFunc);
    [[nodiscard]] static DecompiledFunction decompile(const CFGGraph& cfg, const std::string& funcName = "sub_func");

private:
    static std::string formatCondition(const std::string& cond, const IROperand& op1, const IROperand& op2);
    static std::string operandToC(const IROperand& op);
};

} // namespace edb_next
