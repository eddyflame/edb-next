#include "ExpressionEvaluator.hpp"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <sstream>
#include <iomanip>

namespace edb_next {

namespace {

std::string_view trim(std::string_view str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

bool equalsIgnoreCase(std::string_view a, std::string_view b) {
    return std::ranges::equal(a, b, [](char c1, char c2) {
        return std::tolower(static_cast<unsigned char>(c1)) == std::tolower(static_cast<unsigned char>(c2));
    });
}

std::optional<uint64_t> getRegisterValue(std::string_view regName, const RegisterContext& regs) {
    std::string_view name = trim(regName);
    if (name.empty()) return std::nullopt;
    if (name[0] == '$') name.remove_prefix(1);

    if (equalsIgnoreCase(name, "rax")) return regs.rax();
    if (equalsIgnoreCase(name, "rbx")) return regs.rbx();
    if (equalsIgnoreCase(name, "rcx")) return regs.rcx();
    if (equalsIgnoreCase(name, "rdx")) return regs.rdx();
    if (equalsIgnoreCase(name, "rsi")) return regs.rsi();
    if (equalsIgnoreCase(name, "rdi")) return regs.rdi();
    if (equalsIgnoreCase(name, "rbp")) return regs.rbp().value();
    if (equalsIgnoreCase(name, "rsp")) return regs.rsp().value();
    if (equalsIgnoreCase(name, "r8")) return regs.r8();
    if (equalsIgnoreCase(name, "r9")) return regs.r9();
    if (equalsIgnoreCase(name, "r10")) return regs.r10();
    if (equalsIgnoreCase(name, "r11")) return regs.r11();
    if (equalsIgnoreCase(name, "r12")) return regs.r12();
    if (equalsIgnoreCase(name, "r13")) return regs.r13();
    if (equalsIgnoreCase(name, "r14")) return regs.r14();
    if (equalsIgnoreCase(name, "r15")) return regs.r15();
    if (equalsIgnoreCase(name, "rip")) return regs.rip().value();
    if (equalsIgnoreCase(name, "eflags")) return regs.eflags();

    // 32-bit registers
    if (equalsIgnoreCase(name, "eax")) return static_cast<uint32_t>(regs.rax());
    if (equalsIgnoreCase(name, "ebx")) return static_cast<uint32_t>(regs.rbx());
    if (equalsIgnoreCase(name, "ecx")) return static_cast<uint32_t>(regs.rcx());
    if (equalsIgnoreCase(name, "edx")) return static_cast<uint32_t>(regs.rdx());
    if (equalsIgnoreCase(name, "esi")) return static_cast<uint32_t>(regs.rsi());
    if (equalsIgnoreCase(name, "edi")) return static_cast<uint32_t>(regs.rdi());
    if (equalsIgnoreCase(name, "ebp")) return static_cast<uint32_t>(regs.rbp().value());
    if (equalsIgnoreCase(name, "esp")) return static_cast<uint32_t>(regs.rsp().value());

    return std::nullopt;
}

std::optional<uint64_t> evaluateSingleToken(
    std::string_view token,
    const RegisterContext& regs,
    const LinuxDebugEngine* engine) {
    std::string_view t = trim(token);
    if (t.empty()) return std::nullopt;

    // Dereference e.g. [rbp - 8]
    if (t.front() == '[' && t.back() == ']') {
        std::string_view inner = t.substr(1, t.size() - 2);
        auto addrOpt = ExpressionEvaluator::evaluateValue(inner, regs, engine);
        if (!addrOpt || !engine) return std::nullopt;

        return const_cast<LinuxDebugEngine*>(engine)->read<uint64_t>(Address(*addrOpt));
    }

    // Try register
    auto regVal = getRegisterValue(t, regs);
    if (regVal) return *regVal;

    // Try numeric with std::from_chars
    uint64_t num = 0;
    if (t.size() > 2 && (t.substr(0, 2) == "0x" || t.substr(0, 2) == "0X")) {
        auto [ptr, ec] = std::from_chars(t.data() + 2, t.data() + t.size(), num, 16);
        if (ec == std::errc{} && ptr == t.data() + t.size()) {
            return num;
        }
    } else {
        auto [ptr, ec] = std::from_chars(t.data(), t.data() + t.size(), num, 10);
        if (ec == std::errc{} && ptr == t.data() + t.size()) {
            return num;
        }
    }

    return std::nullopt;
}

} // namespace

std::optional<uint64_t> ExpressionEvaluator::evaluateValue(
    std::string_view expr,
    const RegisterContext& regs,
    const LinuxDebugEngine* engine) {
    std::string_view s = trim(expr);
    if (s.empty()) return std::nullopt;

    // Handle dereference wrapper if whole expr is bracketed
    if (s.front() == '[' && s.back() == ']') {
        return evaluateSingleToken(s, regs, engine);
    }

    // Look for top-level + or - (from right to left)
    int bracket_depth = 0;
    for (int i = static_cast<int>(s.size()) - 1; i >= 0; --i) {
        if (s[i] == ']') bracket_depth++;
        else if (s[i] == '[') bracket_depth--;
        else if (bracket_depth == 0 && (s[i] == '+' || s[i] == '-')) {
            if (i == 0) break; // leading sign
            std::string_view lhs_str = s.substr(0, i);
            std::string_view rhs_str = s.substr(i + 1);
            auto lhs = evaluateValue(lhs_str, regs, engine);
            auto rhs = evaluateValue(rhs_str, regs, engine);
            if (lhs && rhs) {
                return (s[i] == '+') ? (*lhs + *rhs) : (*lhs - *rhs);
            }
        }
    }

    return evaluateSingleToken(s, regs, engine);
}

bool ExpressionEvaluator::evaluateCondition(
    std::string_view condExpr,
    const RegisterContext& regs,
    const LinuxDebugEngine* engine) {
    std::string_view s = trim(condExpr);
    if (s.empty()) return true;

    static const std::vector<std::pair<std::string, int>> ops = {
        {"==", 1}, {"!=", 2}, {"<=", 3}, {">=", 4}, {"<", 5}, {">", 6}
    };

    for (const auto& [opStr, opCode] : ops) {
        auto pos = s.find(opStr);
        if (pos != std::string_view::npos) {
            std::string_view lhs_str = s.substr(0, pos);
            std::string_view rhs_str = s.substr(pos + opStr.size());
            auto lhs = evaluateValue(lhs_str, regs, engine);
            auto rhs = evaluateValue(rhs_str, regs, engine);
            if (!lhs || !rhs) return false;

            switch (opCode) {
                case 1: return *lhs == *rhs;
                case 2: return *lhs != *rhs;
                case 3: return *lhs <= *rhs;
                case 4: return *lhs >= *rhs;
                case 5: return *lhs < *rhs;
                case 6: return *lhs > *rhs;
                default: return false;
            }
        }
    }

    // No comparison operator: evaluate as truthy
    auto val = evaluateValue(s, regs, engine);
    return val.has_value() && (*val != 0);
}

std::string ExpressionEvaluator::formatLog(
    std::string_view format,
    const RegisterContext& regs,
    const LinuxDebugEngine* engine) {
    std::string result;
    size_t i = 0;
    while (i < format.size()) {
        if (format[i] == '{') {
            auto close_pos = format.find('}', i + 1);
            if (close_pos != std::string_view::npos) {
                std::string_view token = format.substr(i + 1, close_pos - i - 1);
                auto val = evaluateValue(token, regs, engine);
                if (val) {
                    result += std::format("0x{:x} ({})", *val, *val);
                } else {
                    result += '{';
                    result += token;
                    result += '}';
                }
                i = close_pos + 1;
                continue;
            }
        }
        result += format[i++];
    }
    return result;
}

} // namespace edb_next
