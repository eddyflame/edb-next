#pragma once

#include "Types.hpp"
#include <string>
#include <vector>
#include <optional>

namespace edb_next {

class DebugSession;

struct FunctionInfo {
    Address startAddress{0};
    Address endAddress{0};
    size_t size{0};
    std::string name;
    bool hasPrologue{false};
    bool hasEpilogue{false};
};

class FunctionFinder {
public:
    static std::vector<FunctionInfo> findFunctions(DebugSession& session, Address start, size_t scan_bytes = 16384);
    static std::optional<FunctionInfo> findEnclosingFunction(DebugSession& session, Address addr);
};

} // namespace edb_next
