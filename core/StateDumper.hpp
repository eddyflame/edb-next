#pragma once

#include "Types.hpp"
#include <string>

namespace edb_next {

class DebugSession;

class StateDumper {
public:
    static std::string dumpState(DebugSession& session);
};

} // namespace edb_next
