#pragma once

#include "Types.hpp"
#include "RegisterContext.hpp"
#include "IDebugBackend.hpp"
#include <string>
#include <string_view>
#include <optional>
#include <cstdint>

namespace edb_next {

class ExpressionEvaluator {
public:
    // Evaluates a numerical expression (e.g. "rax + 0x20", "[rbp - 8]", "0x555555555000 + 42")
    static std::optional<uint64_t> evaluateValue(
        std::string_view expr,
        const RegisterContext& regs,
        IDebugBackend* backend = nullptr);

    static std::optional<uint64_t> evaluate(
        std::string_view expr,
        const RegisterContext& regs,
        IDebugBackend* backend = nullptr)
    {
        return evaluateValue(expr, regs, backend);
    }

    // Evaluates a boolean condition (e.g. "rax == 0", "rdi > 5", "[rsp] != 0")
    // Returns true if condition is met or if expression is empty
    static bool evaluateCondition(
        std::string_view condExpr,
        const RegisterContext& regs,
        IDebugBackend* backend = nullptr);

    // Formats a log string with placeholders, e.g. "Loop {rdi}, rax={rax:x}"
    static std::string formatLog(
        std::string_view format,
        const RegisterContext& regs,
        IDebugBackend* backend = nullptr);
};

} // namespace edb_next
