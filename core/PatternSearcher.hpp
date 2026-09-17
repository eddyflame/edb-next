#pragma once

#include "Types.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace edb_next {

class LinuxDebugEngine;

struct PatternByte {
    uint8_t value{0};
    bool isWildcard{false};
};

class PatternSearcher {
public:
    // Parses hex pattern string with wildcards, e.g. "55 48 89 e5 ?? ?? ?? ?? 5d c3"
    static std::vector<PatternByte> parsePattern(std::string_view patternStr);

    // Searches all readable (or executable) memory regions for the pattern
    static std::vector<Address> search(
        LinuxDebugEngine& engine,
        std::string_view patternStr,
        bool executableOnly = true,
        size_t maxResults = 100);
};

} // namespace edb_next
