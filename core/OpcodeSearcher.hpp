#pragma once

#include "Types.hpp"
#include <vector>
#include <string>

namespace edb_next {

class DebugSession;

enum class OpcodeSearchType {
    JmpReg,
    CallReg,
    PushRegRet,
    PopRegRet,
    InterruptOrSyscall,
    CustomInstruction
};

struct OpcodeSearchResult {
    Address address{0};
    std::string mnemonic;
    std::string operands;
    std::vector<uint8_t> bytes;
    std::string moduleName;
};

class OpcodeSearcher {
public:
    static std::vector<OpcodeSearchResult> search(
        DebugSession& session,
        OpcodeSearchType type,
        const std::string& customQuery = "",
        Address startAddr = Address(0),
        Address endAddr = Address(0),
        size_t maxResults = 1000);
};

} // namespace edb_next
