#pragma once

#include "TypeManager.hpp"
#include <string>
#include <optional>

namespace edb_next {

/**
 * @brief Industrial C/C++ AST Struct Parser powered by libclang
 *
 * Dynamically connects to libclang C API (LLVM 14~18+) to parse full C11/C++20
 * type specifications, including bitfields, anonymous unions/structs,
 * alignment attributes (#pragma pack), and System V AMD64 ABI layout calculations.
 */
class ClangAstParser {
public:
    [[nodiscard]] static bool isAvailable();
    [[nodiscard]] static std::optional<StructDefinition> parseCStruct(const std::string& cCode, std::string* errorMsg = nullptr);
};

} // namespace edb_next
