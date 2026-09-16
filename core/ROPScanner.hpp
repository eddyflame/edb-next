#pragma once

#include "Types.hpp"
#include <string>
#include <vector>

namespace edb_next {

class LinuxDebugEngine;

struct ROPGadget {
    Address address{0};
    std::string disassembly;
    std::string category;
    size_t length{0};
    size_t insnCount{0};
};

class ROPScanner {
public:
    // Scans executable memory regions of the debugged process for ROP gadgets
    // maxGadgetLength: max instructions before ret/syscall (typically 1 to 4)
    // maxResults: maximum number of gadgets to return
    static std::vector<ROPGadget> scan(
        LinuxDebugEngine& engine,
        size_t maxGadgetLength = 4,
        size_t maxResults = 1000,
        const std::string& filter = "");
};

} // namespace edb_next
