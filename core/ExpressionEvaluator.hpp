#pragma once

#include "Types.hpp"
#include "RegisterContext.hpp"
#include "LinuxDebugEngine.hpp"
#include <string>
#include <optional>
#include <cstdint>

namespace edb_next {

class ExpressionEvaluator {
public:
    // Evaluates a numerical expression (e.g. "rax + 0x20", "[rbp - 8]", "0x555555555000 + 42")
    static std::optional<uint64_t> evaluateValue(
        const std::string& expr,
        const RegisterContext& regs,
        const LinuxDebugEngine* engine = nullptr);

    static std::optional<uint64_t> evaluate(
        const std::string& expr,
        const RegisterContext& regs,
        const LinuxDebugEngine* engine = nullptr)
    {
        return evaluateValue(expr, regs, engine);
    }

    // Evaluates a boolean condition (e.g. "rax == 0", "rdi > 5", "[rsp] != 0")
    // Returns true if condition is met or if expression is empty
    static bool evaluateCondition(
        const std::string& condExpr,
        const RegisterContext& regs,
        const LinuxDebugEngine* engine = nullptr);

    // Formats a log string with placeholders, e.g. "Loop {rdi}, rax={rax:x}"
    static std::string formatLog(
        const std::string& format,
        const RegisterContext& regs,
        const LinuxDebugEngine* engine = nullptr);
};

} // namespace edb_next
