#include "PatternSearcher.hpp"
#include "LinuxDebugEngine.hpp"
#include <sstream>
#include <algorithm>

namespace edb_next {

std::vector<PatternByte> PatternSearcher::parsePattern(const std::string& patternStr) {
    std::vector<PatternByte> pattern;
    std::istringstream iss(patternStr);
    std::string token;

    while (iss >> token) {
        if (token == "?" || token == "??" || token == "*") {
            pattern.push_back(PatternByte{.value = 0, .isWildcard = true});
        } else {
            char* endptr = nullptr;
            errno = 0;
            unsigned long val = std::strtoul(token.c_str(), &endptr, 16);
            if (endptr && *endptr == '\0' && errno == 0 && val <= 0xFF) {
                pattern.push_back(PatternByte{.value = static_cast<uint8_t>(val), .isWildcard = false});
            }
        }
    }
    return pattern;
}

std::vector<Address> PatternSearcher::search(
    LinuxDebugEngine& engine,
    const std::string& patternStr,
    bool executableOnly,
    size_t maxResults) {

    std::vector<Address> results;
    auto pattern = parsePattern(patternStr);
    if (pattern.empty() || !engine.isAttached()) return results;

    auto regions = engine.getMemoryRegions();
    for (const auto& region : regions) {
        if (!region.isReadable()) continue;
        if (executableOnly && !region.isExecutable()) continue;

        size_t scanSize = std::min<size_t>(region.size(), 16 * 1024 * 1024);
        if (scanSize < pattern.size()) continue;

        std::vector<uint8_t> buffer(scanSize);
        if (!engine.readMemory(region.start, buffer.data(), scanSize)) {
            continue;
        }

        size_t limit = scanSize - pattern.size() + 1;
        for (size_t i = 0; i < limit; ++i) {
            bool matched = true;
            for (size_t p = 0; p < pattern.size(); ++p) {
                if (!pattern[p].isWildcard && buffer[i + p] != pattern[p].value) {
                    matched = false;
                    break;
                }
            }

            if (matched) {
                results.push_back(region.start + i);
                if (results.size() >= maxResults) {
                    return results;
                }
            }
        }
    }

    return results;
}

} // namespace edb_next
