#pragma once

#include "Types.hpp"
#include <string>
#include <vector>

namespace edb_next {

class DebugSession;

struct StackFrame {
    size_t frameIndex{0};
    Address ip{0};
    Address frameBase{0};
    std::string functionSymbol;
    std::string moduleName;
};

class CallStackUnwinder {
public:
    static std::vector<StackFrame> unwind(DebugSession& session, size_t max_depth = 32);
};

} // namespace edb_next
