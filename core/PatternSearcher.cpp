#include "PatternSearcher.hpp"
#include "LinuxDebugEngine.hpp"
#include <charconv>
#include <algorithm>

namespace edb_next {

std::vector<PatternByte> PatternSearcher::parsePattern(std::string_view patternStr) {
    std::vector<PatternByte> pattern;
    size_t i = 0;
    while (i < patternStr.size()) {
        while (i < patternStr.size() && (patternStr[i] == ' ' || patternStr[i] == '\t')) {
            ++i;
        }
        if (i >= patternStr.size()) break;
        size_t start = i;
        while (i < patternStr.size() && patternStr[i] != ' ' && patternStr[i] != '\t') {
            ++i;
        }
        std::string_view token = patternStr.substr(start, i - start);
        if (token == "?" || token == "??" || token == "*") {
            pattern.push_back(PatternByte{.value = 0, .isWildcard = true});
        } else {
            uint8_t byteVal = 0;
            auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), byteVal, 16);
            if (ec == std::errc{} && ptr == token.data() + token.size()) {
                pattern.push_back(PatternByte{.value = byteVal, .isWildcard = false});
            }
        }
    }
    return pattern;
}

std::vector<Address> PatternSearcher::search(
    LinuxDebugEngine& engine,
    std::string_view patternStr,
    bool executableOnly,
    size_t maxResults) {

    std::vector<Address> results;
    auto pattern = parsePattern(patternStr);
    if (pattern.empty() || !engine.isAttached()) return results;

    auto regions = engine.getMemoryRegions();
    std::vector<uint8_t> buffer;
    for (const auto& region : regions) {
        if (!region.isReadable()) continue;
        if (executableOnly && !region.isExecutable()) continue;

        size_t scanSize = std::min<size_t>(region.size(), 16 * 1024 * 1024);
        if (scanSize < pattern.size()) continue;

        buffer.resize(scanSize);
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
