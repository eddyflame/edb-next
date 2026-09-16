#include "ExpressionEvaluator.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>

namespace edb_next {

namespace {

std::string trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::optional<uint64_t> getRegisterValue(const std::string& regName, const RegisterContext& regs) {
    std::string name = toLower(trim(regName));
    if (name.empty()) return std::nullopt;
    if (name[0] == '$') name = name.substr(1);

    if (name == "rax") return regs.rax();
    if (name == "rbx") return regs.rbx();
    if (name == "rcx") return regs.rcx();
    if (name == "rdx") return regs.rdx();
    if (name == "rsi") return regs.rsi();
    if (name == "rdi") return regs.rdi();
    if (name == "rbp") return regs.rbp().value();
    if (name == "rsp") return regs.rsp().value();
    if (name == "r8") return regs.r8();
    if (name == "r9") return regs.r9();
    if (name == "r10") return regs.r10();
    if (name == "r11") return regs.r11();
    if (name == "r12") return regs.r12();
    if (name == "r13") return regs.r13();
    if (name == "r14") return regs.r14();
    if (name == "r15") return regs.r15();
    if (name == "rip") return regs.rip().value();
    if (name == "eflags") return regs.eflags();

    // 32-bit registers
    if (name == "eax") return static_cast<uint32_t>(regs.rax());
    if (name == "ebx") return static_cast<uint32_t>(regs.rbx());
    if (name == "ecx") return static_cast<uint32_t>(regs.rcx());
    if (name == "edx") return static_cast<uint32_t>(regs.rdx());
    if (name == "esi") return static_cast<uint32_t>(regs.rsi());
    if (name == "edi") return static_cast<uint32_t>(regs.rdi());
    if (name == "ebp") return static_cast<uint32_t>(regs.rbp().value());
    if (name == "esp") return static_cast<uint32_t>(regs.rsp().value());

    return std::nullopt;
}

std::optional<uint64_t> evaluateSingleToken(
    const std::string& token,
    const RegisterContext& regs,
    const LinuxDebugEngine* engine) {
    std::string t = trim(token);
    if (t.empty()) return std::nullopt;

    // Dereference e.g. [rbp - 8]
    if (t.front() == '[' && t.back() == ']') {
        std::string inner = t.substr(1, t.size() - 2);
        auto addrOpt = ExpressionEvaluator::evaluateValue(inner, regs, engine);
        if (!addrOpt || !engine) return std::nullopt;

        uint64_t val = 0;
        if (const_cast<LinuxDebugEngine*>(engine)->readMemory(Address(*addrOpt), &val, sizeof(val))) {
            return val;
        }
        return std::nullopt;
    }

    // Try register
    auto regVal = getRegisterValue(t, regs);
    if (regVal) return *regVal;

    // Try numeric
    char* endptr = nullptr;
    errno = 0;
    uint64_t num = 0;
    if (t.size() > 2 && (t.substr(0, 2) == "0x" || t.substr(0, 2) == "0X")) {
        num = std::strtoull(t.c_str(), &endptr, 16);
    } else {
        num = std::strtoull(t.c_str(), &endptr, 10);
    }
    if (endptr && *endptr == '\0' && errno == 0) {
        return num;
    }

    return std::nullopt;
}

} // namespace

std::optional<uint64_t> ExpressionEvaluator::evaluateValue(
    const std::string& expr,
    const RegisterContext& regs,
    const LinuxDebugEngine* engine) {
    std::string s = trim(expr);
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
            std::string lhs_str = s.substr(0, i);
            std::string rhs_str = s.substr(i + 1);
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
    const std::string& condExpr,
    const RegisterContext& regs,
    const LinuxDebugEngine* engine) {
    std::string s = trim(condExpr);
    if (s.empty()) return true;

    static const std::vector<std::pair<std::string, int>> ops = {
        {"==", 1}, {"!=", 2}, {"<=", 3}, {">=", 4}, {"<", 5}, {">", 6}
    };

    for (const auto& [opStr, opCode] : ops) {
        auto pos = s.find(opStr);
        if (pos != std::string::npos) {
            std::string lhs_str = s.substr(0, pos);
            std::string rhs_str = s.substr(pos + opStr.size());
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
    const std::string& format,
    const RegisterContext& regs,
    const LinuxDebugEngine* engine) {
    std::string result;
    size_t i = 0;
    while (i < format.size()) {
        if (format[i] == '{') {
            auto close_pos = format.find('}', i + 1);
            if (close_pos != std::string::npos) {
                std::string token = format.substr(i + 1, close_pos - i - 1);
                auto val = evaluateValue(token, regs, engine);
                if (val) {
                    std::ostringstream oss;
                    oss << "0x" << std::hex << *val << " (" << std::dec << *val << ")";
                    result += oss.str();
                } else {
                    result += "{" + token + "}";
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
