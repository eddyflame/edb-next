#pragma once

#include "Types.hpp"
#include <string>
#include <vector>

namespace edb_next {

struct SourceLocation {
    std::string filePath;        // Full path to source file, e.g. "/path/to/tests/test_target.c"
    std::string fileName;        // Base file name, e.g. "test_target.c"
    std::string directory;       // Directory containing source file
    int line{0};                 // 1-based source line number
    int column{0};               // Column number (0 if unknown)
    Address address{0};          // Runtime virtual address (relocated with PIE base)
    bool isStmt{false};          // Whether line marks start of a statement
    bool isPrologueEnd{false};   // Whether line marks the end of a function prologue
    bool isEpilogueBegin{false}; // Whether line marks the start of a function epilogue

    [[nodiscard]] bool isValid() const noexcept {
        return !filePath.empty() && line > 0 && !address.isNull();
    }

    [[nodiscard]] std::string format() const {
        return fileName + ":" + std::to_string(line);
    }
};

struct CompilationUnitInfo {
    std::string name;
    std::string compDir;
    std::string producer;
    Address lowPc{0};
    Address highPc{0};
    std::vector<std::string> files;
};

} // namespace edb_next
