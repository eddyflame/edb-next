#include "ExpressionEvaluator.hpp"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <format>
#include <sstream>
#include <iomanip>
#include <vector>

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

    // 64-bit GPRs
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
    if (equalsIgnoreCase(name, "eflags") || equalsIgnoreCase(name, "rflags")) return regs.eflags();

    // 32-bit GPRs
    if (equalsIgnoreCase(name, "eax")) return static_cast<uint32_t>(regs.rax());
    if (equalsIgnoreCase(name, "ebx")) return static_cast<uint32_t>(regs.rbx());
    if (equalsIgnoreCase(name, "ecx")) return static_cast<uint32_t>(regs.rcx());
    if (equalsIgnoreCase(name, "edx")) return static_cast<uint32_t>(regs.rdx());
    if (equalsIgnoreCase(name, "esi")) return static_cast<uint32_t>(regs.rsi());
    if (equalsIgnoreCase(name, "edi")) return static_cast<uint32_t>(regs.rdi());
    if (equalsIgnoreCase(name, "ebp")) return static_cast<uint32_t>(regs.rbp().value());
    if (equalsIgnoreCase(name, "esp")) return static_cast<uint32_t>(regs.rsp().value());
    if (equalsIgnoreCase(name, "r8d")) return static_cast<uint32_t>(regs.r8());
    if (equalsIgnoreCase(name, "r9d")) return static_cast<uint32_t>(regs.r9());
    if (equalsIgnoreCase(name, "r10d")) return static_cast<uint32_t>(regs.r10());
    if (equalsIgnoreCase(name, "r11d")) return static_cast<uint32_t>(regs.r11());
    if (equalsIgnoreCase(name, "r12d")) return static_cast<uint32_t>(regs.r12());
    if (equalsIgnoreCase(name, "r13d")) return static_cast<uint32_t>(regs.r13());
    if (equalsIgnoreCase(name, "r14d")) return static_cast<uint32_t>(regs.r14());
    if (equalsIgnoreCase(name, "r15d")) return static_cast<uint32_t>(regs.r15());
    if (equalsIgnoreCase(name, "eip")) return static_cast<uint32_t>(regs.rip().value());

    // 16-bit GPRs
    if (equalsIgnoreCase(name, "ax")) return static_cast<uint16_t>(regs.rax());
    if (equalsIgnoreCase(name, "bx")) return static_cast<uint16_t>(regs.rbx());
    if (equalsIgnoreCase(name, "cx")) return static_cast<uint16_t>(regs.rcx());
    if (equalsIgnoreCase(name, "dx")) return static_cast<uint16_t>(regs.rdx());
    if (equalsIgnoreCase(name, "si")) return static_cast<uint16_t>(regs.rsi());
    if (equalsIgnoreCase(name, "di")) return static_cast<uint16_t>(regs.rdi());
    if (equalsIgnoreCase(name, "bp")) return static_cast<uint16_t>(regs.rbp().value());
    if (equalsIgnoreCase(name, "sp")) return static_cast<uint16_t>(regs.rsp().value());
    if (equalsIgnoreCase(name, "r8w")) return static_cast<uint16_t>(regs.r8());
    if (equalsIgnoreCase(name, "r9w")) return static_cast<uint16_t>(regs.r9());
    if (equalsIgnoreCase(name, "r10w")) return static_cast<uint16_t>(regs.r10());
    if (equalsIgnoreCase(name, "r11w")) return static_cast<uint16_t>(regs.r11());
    if (equalsIgnoreCase(name, "r12w")) return static_cast<uint16_t>(regs.r12());
    if (equalsIgnoreCase(name, "r13w")) return static_cast<uint16_t>(regs.r13());
    if (equalsIgnoreCase(name, "r14w")) return static_cast<uint16_t>(regs.r14());
    if (equalsIgnoreCase(name, "r15w")) return static_cast<uint16_t>(regs.r15());

    // 8-bit GPRs
    if (equalsIgnoreCase(name, "al")) return static_cast<uint8_t>(regs.rax());
    if (equalsIgnoreCase(name, "bl")) return static_cast<uint8_t>(regs.rbx());
    if (equalsIgnoreCase(name, "cl")) return static_cast<uint8_t>(regs.rcx());
    if (equalsIgnoreCase(name, "dl")) return static_cast<uint8_t>(regs.rdx());
    if (equalsIgnoreCase(name, "sil")) return static_cast<uint8_t>(regs.rsi());
    if (equalsIgnoreCase(name, "dil")) return static_cast<uint8_t>(regs.rdi());
    if (equalsIgnoreCase(name, "bpl")) return static_cast<uint8_t>(regs.rbp().value());
    if (equalsIgnoreCase(name, "spl")) return static_cast<uint8_t>(regs.rsp().value());
    if (equalsIgnoreCase(name, "r8b")) return static_cast<uint8_t>(regs.r8());
    if (equalsIgnoreCase(name, "r9b")) return static_cast<uint8_t>(regs.r9());
    if (equalsIgnoreCase(name, "r10b")) return static_cast<uint8_t>(regs.r10());
    if (equalsIgnoreCase(name, "r11b")) return static_cast<uint8_t>(regs.r11());
    if (equalsIgnoreCase(name, "r12b")) return static_cast<uint8_t>(regs.r12());
    if (equalsIgnoreCase(name, "r13b")) return static_cast<uint8_t>(regs.r13());
    if (equalsIgnoreCase(name, "r14b")) return static_cast<uint8_t>(regs.r14());
    if (equalsIgnoreCase(name, "r15b")) return static_cast<uint8_t>(regs.r15());
    if (equalsIgnoreCase(name, "ah")) return static_cast<uint8_t>((regs.rax() >> 8) & 0xff);
    if (equalsIgnoreCase(name, "bh")) return static_cast<uint8_t>((regs.rbx() >> 8) & 0xff);
    if (equalsIgnoreCase(name, "ch")) return static_cast<uint8_t>((regs.rcx() >> 8) & 0xff);
    if (equalsIgnoreCase(name, "dh")) return static_cast<uint8_t>((regs.rdx() >> 8) & 0xff);

    return std::nullopt;
}

// Token definition for recursive descent parser
enum class TokenType {
    Number,
    Register,
    OpenParen,     // (
    CloseParen,    // )
    OpenBracket,   // [
    CloseBracket,  // ]
    Plus,          // +
    Minus,         // -
    Star,          // *
    Slash,         // /
    Percent,       // %
    ShiftLeft,     // <<
    ShiftRight,    // >>
    Ampersand,     // &
    Pipe,          // |
    Caret,         // ^
    Tilde,         // ~
    Exclamation,   // !
    LogicalAnd,    // &&
    LogicalOr,     // ||
    Equal,         // ==
    NotEqual,      // !=
    Less,          // <
    LessEqual,     // <=
    Greater,       // >
    GreaterEqual,  // >=
    BytePtr,       // byte ptr
    WordPtr,       // word ptr
    DwordPtr,      // dword ptr
    QwordPtr,      // qword ptr
    EndOfInput
};

struct Token {
    TokenType type{TokenType::EndOfInput};
    uint64_t numberValue{0};
    std::string text{};
    size_t sizeBytes{8}; // for ptr specifiers (1, 2, 4, 8)

    Token() = default;
    Token(TokenType t, std::string txt = "", uint64_t num = 0, size_t sz = 8)
        : type(t), numberValue(num), text(std::move(txt)), sizeBytes(sz) {}
};

class Tokenizer {
public:
    explicit Tokenizer(std::string_view input, const RegisterContext& regs)
        : src_(input), regs_(regs) {
        advance();
    }

    const Token& current() const { return current_; }

    void consume() { advance(); }

private:
    std::string_view src_;
    size_t cursor_{0};
    const RegisterContext& regs_;
    Token current_;

    void skipWhitespace() {
        while (cursor_ < src_.size() && std::isspace(static_cast<unsigned char>(src_[cursor_]))) {
            cursor_++;
        }
    }

    void advance() {
        skipWhitespace();
        if (cursor_ >= src_.size()) {
            current_ = Token(TokenType::EndOfInput);
            return;
        }

        char c = src_[cursor_];

        // 2-character operators
        if (cursor_ + 1 < src_.size()) {
            std::string_view op2 = src_.substr(cursor_, 2);
            if (op2 == "==") { cursor_ += 2; current_ = Token(TokenType::Equal, "=="); return; }
            if (op2 == "!=") { cursor_ += 2; current_ = Token(TokenType::NotEqual, "!="); return; }
            if (op2 == "<=") { cursor_ += 2; current_ = Token(TokenType::LessEqual, "<="); return; }
            if (op2 == ">=") { cursor_ += 2; current_ = Token(TokenType::GreaterEqual, ">="); return; }
            if (op2 == "<<") { cursor_ += 2; current_ = Token(TokenType::ShiftLeft, "<<"); return; }
            if (op2 == ">>") { cursor_ += 2; current_ = Token(TokenType::ShiftRight, ">>"); return; }
            if (op2 == "&&") { cursor_ += 2; current_ = Token(TokenType::LogicalAnd, "&&"); return; }
            if (op2 == "||") { cursor_ += 2; current_ = Token(TokenType::LogicalOr, "||"); return; }
        }

        // 1-character operators
        switch (c) {
            case '(': cursor_++; current_ = Token(TokenType::OpenParen, "("); return;
            case ')': cursor_++; current_ = Token(TokenType::CloseParen, ")"); return;
            case '[': cursor_++; current_ = Token(TokenType::OpenBracket, "["); return;
            case ']': cursor_++; current_ = Token(TokenType::CloseBracket, "]"); return;
            case '+': cursor_++; current_ = Token(TokenType::Plus, "+"); return;
            case '-': cursor_++; current_ = Token(TokenType::Minus, "-"); return;
            case '*': cursor_++; current_ = Token(TokenType::Star, "*"); return;
            case '/': cursor_++; current_ = Token(TokenType::Slash, "/"); return;
            case '%': cursor_++; current_ = Token(TokenType::Percent, "%"); return;
            case '&': cursor_++; current_ = Token(TokenType::Ampersand, "&"); return;
            case '|': cursor_++; current_ = Token(TokenType::Pipe, "|"); return;
            case '^': cursor_++; current_ = Token(TokenType::Caret, "^"); return;
            case '~': cursor_++; current_ = Token(TokenType::Tilde, "~"); return;
            case '!': cursor_++; current_ = Token(TokenType::Exclamation, "!"); return;
            case '<': cursor_++; current_ = Token(TokenType::Less, "<"); return;
            case '>': cursor_++; current_ = Token(TokenType::Greater, ">"); return;
            default: break;
        }

        // Numbers: 0x... hex, 0b... binary, or decimal
        if (std::isdigit(static_cast<unsigned char>(c))) {
            size_t start = cursor_;
            if (c == '0' && cursor_ + 1 < src_.size() &&
                (src_[cursor_ + 1] == 'x' || src_[cursor_ + 1] == 'X')) {
                cursor_ += 2;
                while (cursor_ < src_.size() && std::isxdigit(static_cast<unsigned char>(src_[cursor_]))) {
                    cursor_++;
                }
                std::string_view hexStr = src_.substr(start + 2, cursor_ - (start + 2));
                uint64_t val = 0;
                auto [ptr, ec] = std::from_chars(hexStr.data(), hexStr.data() + hexStr.size(), val, 16);
                if (ec == std::errc{}) {
                    current_ = Token(TokenType::Number, std::string(src_.substr(start, cursor_ - start)), val);
                    return;
                }
            } else if (c == '0' && cursor_ + 1 < src_.size() &&
                       (src_[cursor_ + 1] == 'b' || src_[cursor_ + 1] == 'B')) {
                cursor_ += 2;
                while (cursor_ < src_.size() && (src_[cursor_] == '0' || src_[cursor_] == '1')) {
                    cursor_++;
                }
                std::string_view binStr = src_.substr(start + 2, cursor_ - (start + 2));
                uint64_t val = 0;
                auto [ptr, ec] = std::from_chars(binStr.data(), binStr.data() + binStr.size(), val, 2);
                if (ec == std::errc{}) {
                    current_ = Token(TokenType::Number, std::string(src_.substr(start, cursor_ - start)), val);
                    return;
                }
            } else {
                while (cursor_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[cursor_]))) {
                    cursor_++;
                }
                std::string_view decStr = src_.substr(start, cursor_ - start);
                uint64_t val = 0;
                auto [ptr, ec] = std::from_chars(decStr.data(), decStr.data() + decStr.size(), val, 10);
                if (ec == std::errc{}) {
                    current_ = Token(TokenType::Number, std::string(decStr), val);
                    return;
                }
            }
        }

        // Identifiers: Registers or Size Specifiers
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
            size_t start = cursor_;
            while (cursor_ < src_.size() && (std::isalnum(static_cast<unsigned char>(src_[cursor_])) || src_[cursor_] == '_' || src_[cursor_] == '$')) {
                cursor_++;
            }
            std::string_view ident = src_.substr(start, cursor_ - start);

            // Optional "ptr" check: e.g. "byte ptr", "dword ptr", "qword ptr"
            auto checkPtr = [&](std::string_view prefix, size_t sz, TokenType t) -> bool {
                if (equalsIgnoreCase(ident, prefix)) {
                    skipWhitespace();
                    if (cursor_ + 3 <= src_.size() && equalsIgnoreCase(src_.substr(cursor_, 3), "ptr")) {
                        cursor_ += 3;
                    }
                    current_ = Token(t, std::string(ident), 0, sz);
                    return true;
                }
                return false;
            };

            if (checkPtr("byte", 1, TokenType::BytePtr)) return;
            if (checkPtr("word", 2, TokenType::WordPtr)) return;
            if (checkPtr("dword", 4, TokenType::DwordPtr)) return;
            if (checkPtr("qword", 8, TokenType::QwordPtr)) return;

            auto regVal = getRegisterValue(ident, regs_);
            if (regVal) {
                current_ = Token(TokenType::Register, std::string(ident), *regVal);
                return;
            }

            // Unknown identifier: fail tokenization
            current_ = Token(TokenType::EndOfInput, std::string(ident));
            return;
        }

        // Unsupported character: advance and error
        cursor_++;
        current_ = Token(TokenType::EndOfInput);
    }
};

class Parser {
public:
    Parser(std::string_view expr, const RegisterContext& regs, const LinuxDebugEngine* engine)
        : tokenizer_(expr, regs), engine_(engine) {}

    std::optional<uint64_t> parse() {
        if (tokenizer_.current().type == TokenType::EndOfInput) {
            return std::nullopt;
        }
        auto val = parseLogicalOr();
        if (!val || tokenizer_.current().type != TokenType::EndOfInput) {
            return std::nullopt;
        }
        return val;
    }

private:
    Tokenizer tokenizer_;
    const LinuxDebugEngine* engine_;

    // Grammar precedence levels:
    // 1. Logical OR: ||
    std::optional<uint64_t> parseLogicalOr() {
        auto lhs = parseLogicalAnd();
        if (!lhs) return std::nullopt;

        while (tokenizer_.current().type == TokenType::LogicalOr) {
            tokenizer_.consume();
            auto rhs = parseLogicalAnd();
            if (!rhs) return std::nullopt;
            *lhs = (*lhs != 0 || *rhs != 0) ? 1 : 0;
        }
        return lhs;
    }

    // 2. Logical AND: &&
    std::optional<uint64_t> parseLogicalAnd() {
        auto lhs = parseBitwiseOr();
        if (!lhs) return std::nullopt;

        while (tokenizer_.current().type == TokenType::LogicalAnd) {
            tokenizer_.consume();
            auto rhs = parseBitwiseOr();
            if (!rhs) return std::nullopt;
            *lhs = (*lhs != 0 && *rhs != 0) ? 1 : 0;
        }
        return lhs;
    }

    // 3. Bitwise OR: |
    std::optional<uint64_t> parseBitwiseOr() {
        auto lhs = parseBitwiseXor();
        if (!lhs) return std::nullopt;

        while (tokenizer_.current().type == TokenType::Pipe) {
            tokenizer_.consume();
            auto rhs = parseBitwiseXor();
            if (!rhs) return std::nullopt;
            *lhs = *lhs | *rhs;
        }
        return lhs;
    }

    // 4. Bitwise XOR: ^
    std::optional<uint64_t> parseBitwiseXor() {
        auto lhs = parseBitwiseAnd();
        if (!lhs) return std::nullopt;

        while (tokenizer_.current().type == TokenType::Caret) {
            tokenizer_.consume();
            auto rhs = parseBitwiseAnd();
            if (!rhs) return std::nullopt;
            *lhs = *lhs ^ *rhs;
        }
        return lhs;
    }

    // 5. Bitwise AND: &
    std::optional<uint64_t> parseBitwiseAnd() {
        auto lhs = parseEquality();
        if (!lhs) return std::nullopt;

        while (tokenizer_.current().type == TokenType::Ampersand) {
            tokenizer_.consume();
            auto rhs = parseEquality();
            if (!rhs) return std::nullopt;
            *lhs = *lhs & *rhs;
        }
        return lhs;
    }

    // 6. Equality: ==, !=
    std::optional<uint64_t> parseEquality() {
        auto lhs = parseRelational();
        if (!lhs) return std::nullopt;

        while (tokenizer_.current().type == TokenType::Equal || tokenizer_.current().type == TokenType::NotEqual) {
            TokenType op = tokenizer_.current().type;
            tokenizer_.consume();
            auto rhs = parseRelational();
            if (!rhs) return std::nullopt;
            if (op == TokenType::Equal) {
                *lhs = (*lhs == *rhs) ? 1 : 0;
            } else {
                *lhs = (*lhs != *rhs) ? 1 : 0;
            }
        }
        return lhs;
    }

    // 7. Relational: <, <=, >, >=
    std::optional<uint64_t> parseRelational() {
        auto lhs = parseShift();
        if (!lhs) return std::nullopt;

        while (tokenizer_.current().type == TokenType::Less ||
               tokenizer_.current().type == TokenType::LessEqual ||
               tokenizer_.current().type == TokenType::Greater ||
               tokenizer_.current().type == TokenType::GreaterEqual) {
            TokenType op = tokenizer_.current().type;
            tokenizer_.consume();
            auto rhs = parseShift();
            if (!rhs) return std::nullopt;

            switch (op) {
                case TokenType::Less:         *lhs = (*lhs < *rhs) ? 1 : 0; break;
                case TokenType::LessEqual:    *lhs = (*lhs <= *rhs) ? 1 : 0; break;
                case TokenType::Greater:      *lhs = (*lhs > *rhs) ? 1 : 0; break;
                case TokenType::GreaterEqual: *lhs = (*lhs >= *rhs) ? 1 : 0; break;
                default: break;
            }
        }
        return lhs;
    }

    // 8. Shift: <<, >>
    std::optional<uint64_t> parseShift() {
        auto lhs = parseAdditive();
        if (!lhs) return std::nullopt;

        while (tokenizer_.current().type == TokenType::ShiftLeft || tokenizer_.current().type == TokenType::ShiftRight) {
            TokenType op = tokenizer_.current().type;
            tokenizer_.consume();
            auto rhs = parseAdditive();
            if (!rhs) return std::nullopt;

            if (op == TokenType::ShiftLeft) {
                *lhs = (*rhs < 64) ? (*lhs << *rhs) : 0;
            } else {
                *lhs = (*rhs < 64) ? (*lhs >> *rhs) : 0;
            }
        }
        return lhs;
    }

    // 9. Additive: +, -
    std::optional<uint64_t> parseAdditive() {
        auto lhs = parseMultiplicative();
        if (!lhs) return std::nullopt;

        while (tokenizer_.current().type == TokenType::Plus || tokenizer_.current().type == TokenType::Minus) {
            TokenType op = tokenizer_.current().type;
            tokenizer_.consume();
            auto rhs = parseMultiplicative();
            if (!rhs) return std::nullopt;

            if (op == TokenType::Plus) {
                *lhs = *lhs + *rhs;
            } else {
                *lhs = *lhs - *rhs;
            }
        }
        return lhs;
    }

    // 10. Multiplicative: *, /, %
    std::optional<uint64_t> parseMultiplicative() {
        auto lhs = parseUnary();
        if (!lhs) return std::nullopt;

        while (tokenizer_.current().type == TokenType::Star ||
               tokenizer_.current().type == TokenType::Slash ||
               tokenizer_.current().type == TokenType::Percent) {
            TokenType op = tokenizer_.current().type;
            tokenizer_.consume();
            auto rhs = parseUnary();
            if (!rhs) return std::nullopt;

            if (op == TokenType::Star) {
                *lhs = *lhs * *rhs;
            } else if (op == TokenType::Slash) {
                if (*rhs == 0) return std::nullopt; // divide by zero protection
                *lhs = *lhs / *rhs;
            } else if (op == TokenType::Percent) {
                if (*rhs == 0) return std::nullopt; // modulo by zero protection
                *lhs = *lhs % *rhs;
            }
        }
        return lhs;
    }

    // 11. Unary: +, -, ~, !, [expr], ptr [expr]
    std::optional<uint64_t> parseUnary() {
        TokenType t = tokenizer_.current().type;
        if (t == TokenType::Plus) {
            tokenizer_.consume();
            return parseUnary();
        }
        if (t == TokenType::Minus) {
            tokenizer_.consume();
            auto val = parseUnary();
            if (!val) return std::nullopt;
            return static_cast<uint64_t>(-static_cast<int64_t>(*val));
        }
        if (t == TokenType::Tilde) {
            tokenizer_.consume();
            auto val = parseUnary();
            if (!val) return std::nullopt;
            return ~(*val);
        }
        if (t == TokenType::Exclamation) {
            tokenizer_.consume();
            auto val = parseUnary();
            if (!val) return std::nullopt;
            return (*val == 0) ? 1 : 0;
        }

        // Pointer size specifiers: byte ptr [...], word ptr [...], etc.
        if (t == TokenType::BytePtr || t == TokenType::WordPtr ||
            t == TokenType::DwordPtr || t == TokenType::QwordPtr) {
            size_t sizeBytes = tokenizer_.current().sizeBytes;
            tokenizer_.consume();
            return parseDereference(sizeBytes);
        }

        return parsePrimary();
    }

    // 12. Dereference: [expr]
    std::optional<uint64_t> parseDereference(size_t sizeBytes = 8) {
        if (tokenizer_.current().type != TokenType::OpenBracket) {
            return std::nullopt;
        }
        tokenizer_.consume(); // consume '['

        auto addrVal = parseLogicalOr();
        if (!addrVal) return std::nullopt;

        if (tokenizer_.current().type != TokenType::CloseBracket) {
            return std::nullopt;
        }
        tokenizer_.consume(); // consume ']'

        if (!engine_) return std::nullopt;

        Address targetAddr(*addrVal);
        switch (sizeBytes) {
            case 1: {
                auto val = const_cast<LinuxDebugEngine*>(engine_)->read<uint8_t>(targetAddr);
                return val ? std::optional<uint64_t>(*val) : std::nullopt;
            }
            case 2: {
                auto val = const_cast<LinuxDebugEngine*>(engine_)->read<uint16_t>(targetAddr);
                return val ? std::optional<uint64_t>(*val) : std::nullopt;
            }
            case 4: {
                auto val = const_cast<LinuxDebugEngine*>(engine_)->read<uint32_t>(targetAddr);
                return val ? std::optional<uint64_t>(*val) : std::nullopt;
            }
            case 8:
            default: {
                auto val = const_cast<LinuxDebugEngine*>(engine_)->read<uint64_t>(targetAddr);
                return val ? std::optional<uint64_t>(*val) : std::nullopt;
            }
        }
    }

    // 13. Primary: number, register, (expr), [expr]
    std::optional<uint64_t> parsePrimary() {
        TokenType t = tokenizer_.current().type;
        if (t == TokenType::Number) {
            uint64_t val = tokenizer_.current().numberValue;
            tokenizer_.consume();
            return val;
        }
        if (t == TokenType::Register) {
            uint64_t val = tokenizer_.current().numberValue;
            tokenizer_.consume();
            return val;
        }
        if (t == TokenType::OpenParen) {
            tokenizer_.consume();
            auto val = parseLogicalOr();
            if (!val || tokenizer_.current().type != TokenType::CloseParen) {
                return std::nullopt;
            }
            tokenizer_.consume();
            return val;
        }
        if (t == TokenType::OpenBracket) {
            return parseDereference(8);
        }

        return std::nullopt;
    }
};

} // namespace

std::optional<uint64_t> ExpressionEvaluator::evaluateValue(
    std::string_view expr,
    const RegisterContext& regs,
    const LinuxDebugEngine* engine) {
    std::string_view s = trim(expr);
    if (s.empty()) return std::nullopt;

    Parser parser(s, regs, engine);
    return parser.parse();
}

bool ExpressionEvaluator::evaluateCondition(
    std::string_view condExpr,
    const RegisterContext& regs,
    const LinuxDebugEngine* engine) {
    std::string_view s = trim(condExpr);
    if (s.empty()) return true;

    Parser parser(s, regs, engine);
    auto res = parser.parse();
    return res.has_value() && (*res != 0);
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

