#pragma once

#include "Types.hpp"
#include <string>
#include <vector>

namespace edb_next {

class LinuxDebugEngine;
class ElfParser;

struct CodeXRef {
    Address sourceAddress{0};
    std::string sourceFunction;
    std::string mnemonic;
    std::string operands;
    std::string type; // "CALL", "JUMP", "COND_JUMP", "DATA_REF"
};

class CodeXRefFinder {
public:
    static std::vector<CodeXRef> findXRefsTo(
        Address targetAddr,
        LinuxDebugEngine& engine,
        const ElfParser* symbols = nullptr);
};

} // namespace edb_next
