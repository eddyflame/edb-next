#pragma once

#include "Types.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <cstdint>

namespace edb_next {

class IDebugBackend;
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
    // using streaming chunk reading, AVX2 SIMD acceleration, and multi-threaded VMA dispatch
    static std::vector<Address> search(
        IDebugBackend& engine,
        std::string_view patternStr,
        bool executableOnly = true,
        size_t maxResults = 100,
        size_t threadCount = 0);

    // Overload for LinuxDebugEngine
    static std::vector<Address> search(
        LinuxDebugEngine& engine,
        std::string_view patternStr,
        bool executableOnly = true,
        size_t maxResults = 100,
        size_t threadCount = 0);

    // Core buffer scanning functions:
    // Scalar fallback scanner for arbitrary byte buffer
    static void searchChunkScalar(
        std::span<const uint8_t> buffer,
        std::span<const PatternByte> pattern,
        Address baseAddress,
        std::vector<Address>& outResults,
        size_t maxResults);

    // AVX2 vectorized dual-anchor scanner
    [[gnu::target("avx2")]]
    static void searchChunkAVX2(
        std::span<const uint8_t> buffer,
        std::span<const PatternByte> pattern,
        Address baseAddress,
        std::vector<Address>& outResults,
        size_t maxResults);

    // Auto-dispatching chunk search (AVX2 if CPU supports it, else Scalar)
    static void searchChunk(
        std::span<const uint8_t> buffer,
        std::span<const PatternByte> pattern,
        Address baseAddress,
        std::vector<Address>& outResults,
        size_t maxResults);

    // Check if host CPU supports AVX2 at runtime
    [[nodiscard]] static bool isAVX2Supported() noexcept;
};

} // namespace edb_next
