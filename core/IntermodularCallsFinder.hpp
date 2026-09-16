#pragma once

#include "Types.hpp"
#include "DebugSession.hpp"
#include <string>
#include <vector>
#include <memory>

namespace edb_next {

struct IntermodularCall {
    Address callAddress{0};
    std::string callerFunction;
    Address targetAddress{0};
    std::string calleeApi;
    std::string library;
    std::string instruction;
};

class IntermodularCallsFinder {
public:
    static std::vector<IntermodularCall> findCalls(std::shared_ptr<DebugSession> session);
};

} // namespace edb_next
