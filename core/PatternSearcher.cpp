#include "PatternSearcher.hpp"
#include "IDebugBackend.hpp"
#include "LinuxDebugEngine.hpp"
#include <charconv>
#include <algorithm>
#include <immintrin.h>
#include <bit>
#include <thread>
#include <future>
#include <atomic>
#include <span>
#include <cstdint>

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

bool PatternSearcher::isAVX2Supported() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    return __builtin_cpu_supports("avx2");
#else
    return false;
#endif
}

void PatternSearcher::searchChunkScalar(
    std::span<const uint8_t> buffer,
    std::span<const PatternByte> pattern,
    Address baseAddress,
    std::vector<Address>& outResults,
    size_t maxResults) {

    if (pattern.empty() || buffer.size() < pattern.size() || outResults.size() >= maxResults) {
        return;
    }

    const size_t limit = buffer.size() - pattern.size() + 1;
    const uint8_t* data = buffer.data();

    for (size_t i = 0; i < limit; ++i) {
        if (outResults.size() >= maxResults) break;
        bool matched = true;
        for (size_t p = 0; p < pattern.size(); ++p) {
            if (!pattern[p].isWildcard && data[i + p] != pattern[p].value) {
                matched = false;
                break;
            }
        }
        if (matched) {
            outResults.push_back(baseAddress + i);
        }
    }
}

[[gnu::target("avx2")]]
void PatternSearcher::searchChunkAVX2(
    std::span<const uint8_t> buffer,
    std::span<const PatternByte> pattern,
    Address baseAddress,
    std::vector<Address>& outResults,
    size_t maxResults) {

    if (pattern.empty() || buffer.size() < pattern.size() || outResults.size() >= maxResults) {
        return;
    }

    struct Anchor {
        size_t offset{0};
        uint8_t value{0};
    };
    Anchor anchor0{0, 0};
    Anchor anchor1{0, 0};
    bool found0 = false;
    for (size_t p = 0; p < pattern.size(); ++p) {
        if (!pattern[p].isWildcard) {
            if (!found0) {
                anchor0 = {p, pattern[p].value};
                found0 = true;
            }
            anchor1 = {p, pattern[p].value};
        }
    }

    const size_t limit = buffer.size() - pattern.size() + 1;
    const uint8_t* data = buffer.data();

    // If pattern contains solely wildcards, all offsets match
    if (!found0) {
        for (size_t idx = 0; idx < limit && outResults.size() < maxResults; ++idx) {
            outResults.push_back(baseAddress + idx);
        }
        return;
    }

    __m256i v0 = _mm256_set1_epi8(static_cast<char>(anchor0.value));
    __m256i v1 = _mm256_set1_epi8(static_cast<char>(anchor1.value));

    size_t i = 0;
    if (limit >= 32) {
        const size_t simdLimit = limit - 32 + 1;
        for (; i < simdLimit; i += 32) {
            if (outResults.size() >= maxResults) break;

            __m256i chunk0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + anchor0.offset));
            __m256i chunk1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + anchor1.offset));

            __m256i cmp0 = _mm256_cmpeq_epi8(chunk0, v0);
            __m256i cmp1 = _mm256_cmpeq_epi8(chunk1, v1);
            __m256i matchVec = _mm256_and_si256(cmp0, cmp1);

            uint32_t mask = static_cast<uint32_t>(_mm256_movemask_epi8(matchVec));
            if (mask == 0) [[likely]] {
                continue;
            }

            while (mask != 0) {
                int bit = std::countr_zero(mask);
                size_t candIdx = i + static_cast<size_t>(bit);

                bool matched = true;
                for (size_t p = 0; p < pattern.size(); ++p) {
                    if (!pattern[p].isWildcard && data[candIdx + p] != pattern[p].value) {
                        matched = false;
                        break;
                    }
                }
                if (matched) {
                    outResults.push_back(baseAddress + candIdx);
                    if (outResults.size() >= maxResults) break;
                }

                mask &= (mask - 1);
            }
        }
    }

    // Process remainder with scalar verification
    for (; i < limit; ++i) {
        if (outResults.size() >= maxResults) break;
        bool matched = true;
        for (size_t p = 0; p < pattern.size(); ++p) {
            if (!pattern[p].isWildcard && data[i + p] != pattern[p].value) {
                matched = false;
                break;
            }
        }
        if (matched) {
            outResults.push_back(baseAddress + i);
        }
    }
}

void PatternSearcher::searchChunk(
    std::span<const uint8_t> buffer,
    std::span<const PatternByte> pattern,
    Address baseAddress,
    std::vector<Address>& outResults,
    size_t maxResults) {

    if (pattern.empty() || buffer.size() < pattern.size() || outResults.size() >= maxResults) {
        return;
    }

    if (isAVX2Supported()) {
        searchChunkAVX2(buffer, pattern, baseAddress, outResults, maxResults);
    } else {
        searchChunkScalar(buffer, pattern, baseAddress, outResults, maxResults);
    }
}

std::vector<Address> PatternSearcher::search(
    IDebugBackend& engine,
    std::string_view patternStr,
    bool executableOnly,
    size_t maxResults,
    size_t threadCount) {

    if (maxResults == 0) return {};
    auto pattern = parsePattern(patternStr);
    if (pattern.empty() || !engine.isAttached()) return {};

    auto allRegions = engine.getMemoryRegions();
    std::vector<MemoryRegion> candidateRegions;
    for (const auto& reg : allRegions) {
        if (!reg.isReadable()) continue;
        if (executableOnly && !reg.isExecutable()) continue;
        if (reg.size() < pattern.size()) continue;
        candidateRegions.push_back(reg);
    }

    if (candidateRegions.empty()) return {};

    // Break candidate regions into work tasks.
    // Regions larger than 8MB are sliced so worker threads can scan them in parallel.
    struct ScanTask {
        Address start;
        size_t size;
    };
    std::vector<ScanTask> tasks;
    constexpr size_t kTaskSliceSize = 8 * 1024 * 1024; // 8MB

    for (const auto& reg : candidateRegions) {
        size_t regSize = reg.size();
        if (regSize <= kTaskSliceSize) {
            tasks.push_back(ScanTask{.start = reg.start, .size = regSize});
        } else {
            size_t offset = 0;
            while (offset + pattern.size() <= regSize) {
                size_t remaining = regSize - offset;
                size_t slice = std::min(kTaskSliceSize, remaining);
                size_t taskBytes = (offset + slice >= regSize) ? remaining : (slice + pattern.size() - 1);
                tasks.push_back(ScanTask{.start = reg.start + offset, .size = taskBytes});
                offset += slice;
            }
        }
    }

    if (threadCount == 0) {
        unsigned int hw = std::thread::hardware_concurrency();
        threadCount = (hw == 0) ? 4 : std::min<size_t>(hw, 8);
    }

    std::vector<Address> results;
    constexpr size_t kChunkSize = 2 * 1024 * 1024; // 2MB streaming chunk
    size_t actualChunkSize = std::max(kChunkSize, pattern.size() * 2);

    if (tasks.size() == 1 || threadCount <= 1) {
        std::vector<uint8_t> buffer(actualChunkSize);
        for (const auto& task : tasks) {
            if (results.size() >= maxResults) break;
            size_t taskOffset = 0;
            while (taskOffset + pattern.size() <= task.size) {
                if (results.size() >= maxResults) break;
                size_t bytesToRead = std::min(actualChunkSize, task.size - taskOffset);
                if (!engine.readMemory(task.start + taskOffset, buffer.data(), bytesToRead)) {
                    taskOffset += bytesToRead;
                    continue;
                }
                size_t limit = bytesToRead - pattern.size() + 1;
                std::span<const uint8_t> chunkSpan(buffer.data(), bytesToRead);
                searchChunk(chunkSpan, pattern, task.start + taskOffset, results, maxResults);
                taskOffset += limit;
            }
        }
    } else {
        size_t numWorkers = std::min(threadCount, tasks.size());
        std::atomic<size_t> nextTaskIdx{0};
        std::atomic<bool> stopFlag{false};
        std::atomic<size_t> totalFound{0};
        std::vector<std::future<std::vector<Address>>> futures;
        futures.reserve(numWorkers);

        for (size_t w = 0; w < numWorkers; ++w) {
            futures.push_back(std::async(std::launch::async, [&]() {
                std::vector<Address> workerResults;
                std::vector<uint8_t> buffer(actualChunkSize);

                while (!stopFlag.load(std::memory_order_relaxed)) {
                    size_t tIdx = nextTaskIdx.fetch_add(1, std::memory_order_relaxed);
                    if (tIdx >= tasks.size()) break;

                    const auto& task = tasks[tIdx];
                    size_t taskOffset = 0;
                    while (taskOffset + pattern.size() <= task.size && !stopFlag.load(std::memory_order_relaxed)) {
                        size_t bytesToRead = std::min(actualChunkSize, task.size - taskOffset);
                        if (!engine.readMemory(task.start + taskOffset, buffer.data(), bytesToRead)) {
                            taskOffset += bytesToRead;
                            continue;
                        }
                        size_t limit = bytesToRead - pattern.size() + 1;
                        std::span<const uint8_t> chunkSpan(buffer.data(), bytesToRead);
                        size_t prevCount = workerResults.size();
                        searchChunk(chunkSpan, pattern, task.start + taskOffset, workerResults, maxResults);

                        size_t added = workerResults.size() - prevCount;
                        if (added > 0) {
                            size_t cur = totalFound.fetch_add(added, std::memory_order_relaxed) + added;
                            if (cur >= maxResults) {
                                stopFlag.store(true, std::memory_order_relaxed);
                                break;
                            }
                        }
                        taskOffset += limit;
                    }
                }
                return workerResults;
            }));
        }

        for (auto& f : futures) {
            auto res = f.get();
            results.insert(results.end(), res.begin(), res.end());
        }
    }

    std::sort(results.begin(), results.end());
    results.erase(std::unique(results.begin(), results.end()), results.end());
    if (results.size() > maxResults) {
        results.resize(maxResults);
    }
    return results;
}

std::vector<Address> PatternSearcher::search(
    LinuxDebugEngine& engine,
    std::string_view patternStr,
    bool executableOnly,
    size_t maxResults,
    size_t threadCount) {
    return search(static_cast<IDebugBackend&>(engine), patternStr, executableOnly, maxResults, threadCount);
}

} // namespace edb_next
