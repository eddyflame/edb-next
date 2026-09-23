#include "MemoryScanner.hpp"
#include "IDebugBackend.hpp"
#include "PatternSearcher.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <immintrin.h>
#include <bit>
#include <thread>
#include <future>
#include <atomic>

namespace edb_next {

std::string scanDataTypeToString(ScanDataType type) {
    switch (type) {
        case ScanDataType::Int8: return "Int8 (Byte)";
        case ScanDataType::Int16: return "Int16 (Short)";
        case ScanDataType::Int32: return "Int32 (Int)";
        case ScanDataType::Int64: return "Int64 (Long)";
        case ScanDataType::Float: return "Float";
        case ScanDataType::Double: return "Double";
        case ScanDataType::String: return "String";
        case ScanDataType::ByteArray: return "Hex Bytes";
    }
    return "Unknown";
}

std::string scanCompareTypeToString(ScanCompareType type) {
    switch (type) {
        case ScanCompareType::ExactValue: return "Exact Value";
        case ScanCompareType::IncreasedValue: return "Increased Value";
        case ScanCompareType::DecreasedValue: return "Decreased Value";
        case ScanCompareType::ChangedValue: return "Changed Value";
        case ScanCompareType::UnchangedValue: return "Unchanged Value";
        case ScanCompareType::IncreasedBy: return "Increased By...";
        case ScanCompareType::DecreasedBy: return "Decreased By...";
        case ScanCompareType::UnknownInitialValue: return "Unknown Initial Value";
    }
    return "Exact Value";
}

ScanDataType stringToScanDataType(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "int8" || s == "byte" || s == "i8" || s == "u8") return ScanDataType::Int8;
    if (s == "int16" || s == "short" || s == "i16" || s == "u16") return ScanDataType::Int16;
    if (s == "int32" || s == "int" || s == "i32" || s == "u32") return ScanDataType::Int32;
    if (s == "int64" || s == "long" || s == "i64" || s == "u64" || s == "qword") return ScanDataType::Int64;
    if (s == "float" || s == "f32") return ScanDataType::Float;
    if (s == "double" || s == "f64") return ScanDataType::Double;
    if (s == "string" || s == "str" || s == "text") return ScanDataType::String;
    if (s == "hex" || s == "bytes" || s == "bytearray") return ScanDataType::ByteArray;
    return ScanDataType::Int32;
}

ScanCompareType stringToScanCompareType(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "exact" || s == "==" || s == "=") return ScanCompareType::ExactValue;
    if (s == "increased" || s == ">" || s == "inc" || s == "bigger") return ScanCompareType::IncreasedValue;
    if (s == "decreased" || s == "<" || s == "dec" || s == "smaller") return ScanCompareType::DecreasedValue;
    if (s == "changed" || s == "!=" || s == "diff") return ScanCompareType::ChangedValue;
    if (s == "unchanged" || s == "same") return ScanCompareType::UnchangedValue;
    if (s == "increasedby" || s == "+") return ScanCompareType::IncreasedBy;
    if (s == "decreasedby" || s == "-") return ScanCompareType::DecreasedBy;
    if (s == "unknown" || s == "?" || s == "initial") return ScanCompareType::UnknownInitialValue;
    return ScanCompareType::ExactValue;
}

size_t MemoryScanner::getDataTypeSize(ScanDataType type, const std::string& inputStr) {
    switch (type) {
        case ScanDataType::Int8: return 1;
        case ScanDataType::Int16: return 2;
        case ScanDataType::Int32: return 4;
        case ScanDataType::Int64: return 8;
        case ScanDataType::Float: return 4;
        case ScanDataType::Double: return 8;
        case ScanDataType::String: return inputStr.empty() ? 1 : inputStr.size();
        case ScanDataType::ByteArray: {
            auto pattern = PatternSearcher::parsePattern(inputStr);
            return pattern.empty() ? 1 : pattern.size();
        }
    }
    return 4;
}

namespace {

template<typename T, typename Container>
T readVal(const Container& buf) {
    if (buf.size() < sizeof(T)) return T{0};
    T val{};
    std::memcpy(&val, buf.data(), sizeof(T));
    return val;
}

template<typename T>
std::vector<uint8_t> toBytes(T val) {
    std::vector<uint8_t> buf(sizeof(T));
    std::memcpy(buf.data(), &val, sizeof(T));
    return buf;
}

bool parseNumber(const std::string& str, int64_t& outSigned, uint64_t& outUnsigned) {
    if (str.empty()) return false;
    char* endptr = nullptr;
    errno = 0;
    if (str.rfind("0x", 0) == 0 || str.rfind("0X", 0) == 0) {
        outUnsigned = std::strtoull(str.c_str(), &endptr, 16);
        outSigned = static_cast<int64_t>(outUnsigned);
    } else {
        outSigned = std::strtoll(str.c_str(), &endptr, 10);
        outUnsigned = static_cast<uint64_t>(outSigned);
    }
    return (endptr && *endptr == '\0' && errno == 0);
}

bool parseFloat(const std::string& str, float& outVal) {
    if (str.empty()) return false;
    char* endptr = nullptr;
    errno = 0;
    outVal = std::strtof(str.c_str(), &endptr);
    return (endptr && *endptr == '\0' && errno == 0);
}

bool parseDouble(const std::string& str, double& outVal) {
    if (str.empty()) return false;
    char* endptr = nullptr;
    errno = 0;
    outVal = std::strtod(str.c_str(), &endptr);
    return (endptr && *endptr == '\0' && errno == 0);
}

} // anonymous namespace

std::string ScanResult::formatCurrentValue(ScanDataType type) const {
    if (currentValue.empty()) return "-";
    std::ostringstream oss;
    switch (type) {
        case ScanDataType::Int8: {
            int8_t v = readVal<int8_t>(currentValue);
            uint8_t u = readVal<uint8_t>(currentValue);
            oss << static_cast<int>(v) << " (0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(u) << ")";
            break;
        }
        case ScanDataType::Int16: {
            int16_t v = readVal<int16_t>(currentValue);
            uint16_t u = readVal<uint16_t>(currentValue);
            oss << v << " (0x" << std::hex << std::setw(4) << std::setfill('0') << u << ")";
            break;
        }
        case ScanDataType::Int32: {
            int32_t v = readVal<int32_t>(currentValue);
            uint32_t u = readVal<uint32_t>(currentValue);
            oss << v << " (0x" << std::hex << std::setw(8) << std::setfill('0') << u << ")";
            break;
        }
        case ScanDataType::Int64: {
            int64_t v = readVal<int64_t>(currentValue);
            uint64_t u = readVal<uint64_t>(currentValue);
            oss << v << " (0x" << std::hex << std::setw(16) << std::setfill('0') << u << ")";
            break;
        }
        case ScanDataType::Float: {
            float v = readVal<float>(currentValue);
            oss << std::fixed << std::setprecision(4) << v;
            break;
        }
        case ScanDataType::Double: {
            double v = readVal<double>(currentValue);
            oss << std::fixed << std::setprecision(6) << v;
            break;
        }
        case ScanDataType::String: {
            std::string s(reinterpret_cast<const char*>(currentValue.data()), currentValue.size());
            oss << "\"" << s << "\"";
            break;
        }
        case ScanDataType::ByteArray: {
            for (size_t i = 0; i < currentValue.size(); ++i) {
                if (i > 0) oss << " ";
                oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(currentValue[i]);
            }
            break;
        }
    }
    return oss.str();
}

std::string ScanResult::formatPreviousValue(ScanDataType type) const {
    if (previousValue.empty()) return "-";
    ScanResult prevDummy;
    prevDummy.currentValue = previousValue;
    return prevDummy.formatCurrentValue(type);
}

std::string ScanResult::formatDelta(ScanDataType type) const {
    if (previousValue.empty() || currentValue.empty()) return "-";
    std::ostringstream oss;
    switch (type) {
        case ScanDataType::Int8: {
            int8_t cur = readVal<int8_t>(currentValue);
            int8_t prev = readVal<int8_t>(previousValue);
            int diff = cur - prev;
            if (diff > 0) oss << "+" << diff;
            else oss << diff;
            break;
        }
        case ScanDataType::Int16: {
            int16_t cur = readVal<int16_t>(currentValue);
            int16_t prev = readVal<int16_t>(previousValue);
            int diff = cur - prev;
            if (diff > 0) oss << "+" << diff;
            else oss << diff;
            break;
        }
        case ScanDataType::Int32: {
            int32_t cur = readVal<int32_t>(currentValue);
            int32_t prev = readVal<int32_t>(previousValue);
            int64_t diff = static_cast<int64_t>(cur) - static_cast<int64_t>(prev);
            if (diff > 0) oss << "+" << diff;
            else oss << diff;
            break;
        }
        case ScanDataType::Int64: {
            int64_t cur = readVal<int64_t>(currentValue);
            int64_t prev = readVal<int64_t>(previousValue);
            int64_t diff = cur - prev;
            if (diff > 0) oss << "+" << diff;
            else oss << diff;
            break;
        }
        case ScanDataType::Float: {
            float cur = readVal<float>(currentValue);
            float prev = readVal<float>(previousValue);
            float diff = cur - prev;
            if (diff > 0) oss << "+" << diff;
            else oss << diff;
            break;
        }
        case ScanDataType::Double: {
            double cur = readVal<double>(currentValue);
            double prev = readVal<double>(previousValue);
            double diff = cur - prev;
            if (diff > 0) oss << "+" << diff;
            else oss << diff;
            break;
        }
        default:
            return "-";
    }
    return oss.str();
}

[[gnu::target("avx2")]]
static void scanInt32ExactAVX2(
    const uint8_t* data,
    size_t bytesToRead,
    Address chunkAddr,
    int32_t targetVal,
    std::vector<ScanResult>& results,
    size_t maxResults) {

    size_t i = 0;
    __m256i targetVec = _mm256_set1_epi32(targetVal);

    if (bytesToRead >= 32) {
        size_t simdLimit = bytesToRead - 32 + 1;
        for (; i < simdLimit; i += 32) {
            if (results.size() >= maxResults) return;

            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
            __m256i cmp = _mm256_cmpeq_epi32(chunk, targetVec);
            int mask = _mm256_movemask_ps(_mm256_castsi256_ps(cmp));
            if (mask == 0) [[likely]] {
                continue;
            }

            while (mask != 0) {
                int dwordIdx = std::countr_zero(static_cast<unsigned int>(mask));
                size_t matchOffset = i + static_cast<size_t>(dwordIdx) * 4;
                ScanResult res;
                res.address = chunkAddr + matchOffset;
                res.previousValue = SmallBuffer(data + matchOffset, 4);
                res.currentValue = SmallBuffer(data + matchOffset, 4);
                results.push_back(std::move(res));
                if (results.size() >= maxResults) return;
                mask &= (mask - 1);
            }
        }
    }

    for (; i + 4 <= bytesToRead; i += 4) {
        if (results.size() >= maxResults) return;
        int32_t v = 0;
        std::memcpy(&v, data + i, sizeof(v));
        if (v == targetVal) {
            ScanResult res;
            res.address = chunkAddr + i;
            res.previousValue = SmallBuffer(data + i, 4);
            res.currentValue = SmallBuffer(data + i, 4);
            results.push_back(std::move(res));
        }
    }
}

[[gnu::target("avx2")]]
static void scanInt64ExactAVX2(
    const uint8_t* data,
    size_t bytesToRead,
    Address chunkAddr,
    int64_t targetVal,
    std::vector<ScanResult>& results,
    size_t maxResults) {

    size_t i = 0;
    __m256i targetVec = _mm256_set1_epi64x(targetVal);

    if (bytesToRead >= 32) {
        size_t simdLimit = bytesToRead - 32 + 1;
        for (; i < simdLimit; i += 32) {
            if (results.size() >= maxResults) return;

            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
            __m256i cmp = _mm256_cmpeq_epi64(chunk, targetVec);
            int mask = _mm256_movemask_pd(_mm256_castsi256_pd(cmp));
            if (mask == 0) [[likely]] {
                continue;
            }

            while (mask != 0) {
                int qwordIdx = std::countr_zero(static_cast<unsigned int>(mask));
                size_t matchOffset = i + static_cast<size_t>(qwordIdx) * 8;
                ScanResult res;
                res.address = chunkAddr + matchOffset;
                res.previousValue = SmallBuffer(data + matchOffset, 8);
                res.currentValue = SmallBuffer(data + matchOffset, 8);
                results.push_back(std::move(res));
                if (results.size() >= maxResults) return;
                mask &= (mask - 1);
            }
        }
    }

    for (; i + 8 <= bytesToRead; i += 8) {
        if (results.size() >= maxResults) return;
        int64_t v = 0;
        std::memcpy(&v, data + i, sizeof(v));
        if (v == targetVal) {
            ScanResult res;
            res.address = chunkAddr + i;
            res.previousValue = SmallBuffer(data + i, 8);
            res.currentValue = SmallBuffer(data + i, 8);
            results.push_back(std::move(res));
        }
    }
}

#if defined(__aarch64__) || defined(__ARM_NEON)
#include <arm_neon.h>

static void scanInt32ExactNeon(
    const uint8_t* data,
    size_t bytesToRead,
    Address chunkAddr,
    int32_t targetVal,
    std::vector<ScanResult>& results,
    size_t maxResults) {

    size_t i = 0;
    int32x4_t targetVec = vdupq_n_s32(targetVal);

    if (bytesToRead >= 16) {
        size_t simdLimit = bytesToRead - 16 + 1;
        for (; i < simdLimit; i += 16) {
            if (results.size() >= maxResults) return;

            int32x4_t chunk = vld1q_s32(reinterpret_cast<const int32_t*>(data + i));
            uint32x4_t cmp = vceqq_s32(chunk, targetVec);

            if (vmaxvq_u32(cmp) == 0) [[likely]] {
                continue;
            }

            uint32_t matches[4];
            vst1q_u32(matches, cmp);
            for (int lane = 0; lane < 4; ++lane) {
                if (matches[lane]) {
                    size_t matchOffset = i + static_cast<size_t>(lane) * 4;
                    ScanResult res;
                    res.address = chunkAddr + matchOffset;
                    res.previousValue = SmallBuffer(data + matchOffset, 4);
                    res.currentValue = SmallBuffer(data + matchOffset, 4);
                    results.push_back(std::move(res));
                    if (results.size() >= maxResults) return;
                }
            }
        }
    }

    for (; i + 4 <= bytesToRead; i += 4) {
        if (results.size() >= maxResults) return;
        int32_t v = 0;
        std::memcpy(&v, data + i, sizeof(v));
        if (v == targetVal) {
            ScanResult res;
            res.address = chunkAddr + i;
            res.previousValue = SmallBuffer(data + i, 4);
            res.currentValue = SmallBuffer(data + i, 4);
            results.push_back(std::move(res));
        }
    }
}
#endif

bool MemoryScanner::isNeonSupported() noexcept {
#if defined(__aarch64__) || defined(__ARM_NEON)
    return true;
#else
    return false;
#endif
}

bool MemoryScanner::isAvx2Supported() noexcept {
    return PatternSearcher::isAVX2Supported();
}

std::string MemoryScanner::activeSimdEngineName() noexcept {
    if (isAvx2Supported()) {
        return "AVX2 (256-bit)";
    }
    if (isNeonSupported()) {
        return "ARM NEON (128-bit)";
    }
    return "Scalar Fallback";
}

size_t MemoryScanner::firstScan(IDebugBackend& engine, const ScanOptions& options) {

    results_.clear();
    scanPass_ = 0;
    activeOptions_ = options;

    if (!engine.isAttached() || options.maxResults == 0) return 0;

    size_t dataSize = getDataTypeSize(options.dataType, options.valueStr);
    if (dataSize == 0) dataSize = 1;

    size_t align = options.alignment == 0 ? 1 : options.alignment;
    if (options.dataType == ScanDataType::String || options.dataType == ScanDataType::ByteArray) {
        align = 1;
    }

    // Prepare target representation for ExactValue
    int64_t targetInt = 0;
    uint64_t targetUInt = 0;
    float targetFloat = 0.0f;
    double targetDouble = 0.0;
    std::vector<PatternByte> targetPattern;

    if (options.compareType == ScanCompareType::ExactValue) {
        if (options.dataType == ScanDataType::Float) {
            if (!parseFloat(options.valueStr, targetFloat)) return 0;
        } else if (options.dataType == ScanDataType::Double) {
            if (!parseDouble(options.valueStr, targetDouble)) return 0;
        } else if (options.dataType == ScanDataType::ByteArray) {
            targetPattern = PatternSearcher::parsePattern(options.valueStr);
            if (targetPattern.empty()) return 0;
            dataSize = targetPattern.size();
        } else if (options.dataType != ScanDataType::String) {
            if (!parseNumber(options.valueStr, targetInt, targetUInt)) return 0;
        }
    }

    auto regions = engine.getMemoryRegions();
    struct FilteredRegion {
        Address start;
        Address end;
    };
    std::vector<FilteredRegion> filtered;
    for (const auto& region : regions) {
        if (!region.isReadable()) continue;
        if (options.writableOnly && !region.isWritable()) continue;

        Address startAddr = region.start;
        Address endAddr = region.end;

        if (!options.customStart.isNull() && options.customStart > startAddr) {
            startAddr = options.customStart;
        }
        if (!options.customEnd.isNull() && options.customEnd < endAddr) {
            endAddr = options.customEnd;
        }
        if (startAddr >= endAddr) continue;
        if (static_cast<size_t>(endAddr - startAddr) < dataSize) continue;

        filtered.push_back(FilteredRegion{.start = startAddr, .end = endAddr});
    }

    if (filtered.empty()) return 0;

    auto scanRegionChunks = [&](Address startAddr, Address endAddr, std::vector<ScanResult>& localResults, const std::atomic<bool>& stopFlag) {
        size_t totalRegionBytes = endAddr - startAddr;
        if (totalRegionBytes < dataSize) return;

        constexpr size_t kScanChunkSize = 2 * 1024 * 1024; // 2MB chunk
        std::vector<uint8_t> buffer(kScanChunkSize);

        for (size_t offset = 0; offset < totalRegionBytes && !stopFlag.load(std::memory_order_relaxed); ) {
            if (localResults.size() >= options.maxResults) break;

            size_t bytesToRead = std::min<size_t>(kScanChunkSize, totalRegionBytes - offset);
            Address chunkAddr = startAddr + offset;

            if (!engine.readMemory(chunkAddr, buffer.data(), bytesToRead)) {
                offset += bytesToRead;
                continue;
            }

            // Check for AVX2 fast-paths
            if (options.compareType == ScanCompareType::ExactValue) {
                if (options.dataType == ScanDataType::ByteArray) {
                    std::vector<Address> matchAddrs;
                    PatternSearcher::searchChunk(
                        std::span<const uint8_t>(buffer.data(), bytesToRead),
                        targetPattern,
                        chunkAddr,
                        matchAddrs,
                        options.maxResults - localResults.size());

                    for (Address addr : matchAddrs) {
                        size_t localOffset = addr - chunkAddr;
                        ScanResult res;
                        res.address = addr;
                        res.previousValue = SmallBuffer(buffer.data() + localOffset, dataSize);
                        res.currentValue = SmallBuffer(buffer.data() + localOffset, dataSize);
                        localResults.push_back(std::move(res));
                    }
                    offset += (bytesToRead > dataSize ? (bytesToRead - dataSize + align) : bytesToRead);
                    continue;
                } else if (options.dataType == ScanDataType::Int32 && align == 4 && PatternSearcher::isAVX2Supported()) {
                    scanInt32ExactAVX2(buffer.data(), bytesToRead, chunkAddr, static_cast<int32_t>(targetInt), localResults, options.maxResults);
                    offset += (bytesToRead > dataSize ? (bytesToRead - dataSize + align) : bytesToRead);
                    continue;
#if defined(__aarch64__) || defined(__ARM_NEON)
                } else if (options.dataType == ScanDataType::Int32 && align == 4 && isNeonSupported()) {
                    scanInt32ExactNeon(buffer.data(), bytesToRead, chunkAddr, static_cast<int32_t>(targetInt), localResults, options.maxResults);
                    offset += (bytesToRead > dataSize ? (bytesToRead - dataSize + align) : bytesToRead);
                    continue;
#endif
                } else if (options.dataType == ScanDataType::Int64 && align == 8 && PatternSearcher::isAVX2Supported()) {
                    scanInt64ExactAVX2(buffer.data(), bytesToRead, chunkAddr, targetInt, localResults, options.maxResults);
                    offset += (bytesToRead > dataSize ? (bytesToRead - dataSize + align) : bytesToRead);
                    continue;
                }

            }

            // General fallback path for float/double/string/differentials
            size_t validLimit = bytesToRead >= dataSize ? (bytesToRead - dataSize + 1) : 0;
            for (size_t i = 0; i < validLimit; i += align) {
                Address curAddr = chunkAddr + i;
                bool match = false;
                std::span<const uint8_t> valBytes(buffer.data() + i, dataSize);

                if (options.compareType == ScanCompareType::UnknownInitialValue) {
                    match = true;
                } else if (options.compareType == ScanCompareType::ExactValue) {
                    switch (options.dataType) {
                        case ScanDataType::Int8: {
                            int8_t v = readVal<int8_t>(valBytes);
                            uint8_t u = readVal<uint8_t>(valBytes);
                            match = (v == static_cast<int8_t>(targetInt) || u == static_cast<uint8_t>(targetUInt));
                            break;
                        }
                        case ScanDataType::Int16: {
                            int16_t v = readVal<int16_t>(valBytes);
                            uint16_t u = readVal<uint16_t>(valBytes);
                            match = (v == static_cast<int16_t>(targetInt) || u == static_cast<uint16_t>(targetUInt));
                            break;
                        }
                        case ScanDataType::Int32: {
                            int32_t v = readVal<int32_t>(valBytes);
                            uint32_t u = readVal<uint32_t>(valBytes);
                            match = (v == static_cast<int32_t>(targetInt) || u == static_cast<uint32_t>(targetUInt));
                            break;
                        }
                        case ScanDataType::Int64: {
                            int64_t v = readVal<int64_t>(valBytes);
                            uint64_t u = readVal<uint64_t>(valBytes);
                            match = (v == targetInt || u == targetUInt);
                            break;
                        }
                        case ScanDataType::Float: {
                            float v = readVal<float>(valBytes);
                            match = (std::fabs(v - targetFloat) < 0.0001f);
                            break;
                        }
                        case ScanDataType::Double: {
                            double v = readVal<double>(valBytes);
                            match = (std::fabs(v - targetDouble) < 0.000001);
                            break;
                        }
                        case ScanDataType::String: {
                            match = (std::memcmp(valBytes.data(), options.valueStr.data(), dataSize) == 0);
                            break;
                        }
                        case ScanDataType::ByteArray: {
                            match = true;
                            for (size_t p = 0; p < targetPattern.size(); ++p) {
                                if (!targetPattern[p].isWildcard && valBytes[p] != targetPattern[p].value) {
                                    match = false;
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }

                if (match) {
                    ScanResult res;
                    res.address = curAddr;
                    res.previousValue = SmallBuffer(valBytes);
                    res.currentValue = SmallBuffer(valBytes);
                    localResults.push_back(std::move(res));

                    if (localResults.size() >= options.maxResults) {
                        break;
                    }
                }
            }

            offset += (bytesToRead > dataSize ? (bytesToRead - dataSize + align) : bytesToRead);
        }
    };

    std::atomic<bool> stopFlag{false};
    if (filtered.size() == 1) {
        scanRegionChunks(filtered[0].start, filtered[0].end, results_, stopFlag);
    } else {
        unsigned int hw = std::thread::hardware_concurrency();
        size_t threadCount = (hw == 0) ? 4 : std::min<size_t>(hw, 8);
        size_t numWorkers = std::min(threadCount, filtered.size());

        std::atomic<size_t> nextRegIdx{0};
        std::atomic<size_t> totalFound{0};
        std::vector<std::future<std::vector<ScanResult>>> futures;
        futures.reserve(numWorkers);

        for (size_t w = 0; w < numWorkers; ++w) {
            futures.push_back(std::async(std::launch::async, [&]() {
                std::vector<ScanResult> workerResults;
                while (!stopFlag.load(std::memory_order_relaxed)) {
                    size_t rIdx = nextRegIdx.fetch_add(1, std::memory_order_relaxed);
                    if (rIdx >= filtered.size()) break;

                    scanRegionChunks(filtered[rIdx].start, filtered[rIdx].end, workerResults, stopFlag);
                    if (workerResults.size() >= options.maxResults) {
                        stopFlag.store(true, std::memory_order_relaxed);
                        break;
                    }
                    if (totalFound.fetch_add(workerResults.size(), std::memory_order_relaxed) + workerResults.size() >= options.maxResults) {
                        stopFlag.store(true, std::memory_order_relaxed);
                        break;
                    }
                }
                return workerResults;
            }));
        }

        for (auto& f : futures) {
            auto workerRes = f.get();
            results_.insert(results_.end(), std::make_move_iterator(workerRes.begin()), std::make_move_iterator(workerRes.end()));
        }
    }

    std::sort(results_.begin(), results_.end(), [](const ScanResult& a, const ScanResult& b) {
        return a.address < b.address;
    });
    auto last = std::unique(results_.begin(), results_.end(), [](const ScanResult& a, const ScanResult& b) {
        return a.address == b.address;
    });
    results_.erase(last, results_.end());

    if (results_.size() > options.maxResults) {
        results_.resize(options.maxResults);
    }

    scanPass_ = 1;
    return results_.size();
}

size_t MemoryScanner::nextScan(IDebugBackend& engine, const ScanOptions& options) {
    if (results_.empty() || !engine.isAttached()) {
        return 0;
    }

    activeOptions_ = options;
    size_t dataSize = getDataTypeSize(options.dataType, options.valueStr);
    if (dataSize == 0) dataSize = 1;

    // Parse targets if needed
    int64_t targetInt = 0;
    uint64_t targetUInt = 0;
    float targetFloat = 0.0f;
    double targetDouble = 0.0;
    std::vector<PatternByte> targetPattern;

    int64_t deltaInt = 0;
    uint64_t deltaUInt = 0;
    float deltaFloat = 0.0f;
    double deltaDouble = 0.0;

    if (options.compareType == ScanCompareType::ExactValue) {
        if (options.dataType == ScanDataType::Float) {
            parseFloat(options.valueStr, targetFloat);
        } else if (options.dataType == ScanDataType::Double) {
            parseDouble(options.valueStr, targetDouble);
        } else if (options.dataType == ScanDataType::ByteArray) {
            targetPattern = PatternSearcher::parsePattern(options.valueStr);
            dataSize = targetPattern.size();
        } else if (options.dataType != ScanDataType::String) {
            parseNumber(options.valueStr, targetInt, targetUInt);
        }
    } else if (options.compareType == ScanCompareType::IncreasedBy || options.compareType == ScanCompareType::DecreasedBy) {
        if (options.dataType == ScanDataType::Float) {
            parseFloat(options.deltaStr, deltaFloat);
        } else if (options.dataType == ScanDataType::Double) {
            parseDouble(options.deltaStr, deltaDouble);
        } else {
            parseNumber(options.deltaStr, deltaInt, deltaUInt);
        }
    }

    std::vector<ScanResult> newResults;
    newResults.reserve(results_.size());
    std::vector<uint8_t> newBytes(dataSize);

    for (auto& cand : results_) {
        if (!engine.readMemory(cand.address, newBytes.data(), dataSize)) {
            continue;
        }

        bool match = false;
        const auto& oldBytes = cand.currentValue;

        switch (options.compareType) {
            case ScanCompareType::ExactValue: {
                switch (options.dataType) {
                    case ScanDataType::Int8: {
                        int8_t v = readVal<int8_t>(newBytes);
                        uint8_t u = readVal<uint8_t>(newBytes);
                        match = (v == static_cast<int8_t>(targetInt) || u == static_cast<uint8_t>(targetUInt));
                        break;
                    }
                    case ScanDataType::Int16: {
                        int16_t v = readVal<int16_t>(newBytes);
                        uint16_t u = readVal<uint16_t>(newBytes);
                        match = (v == static_cast<int16_t>(targetInt) || u == static_cast<uint16_t>(targetUInt));
                        break;
                    }
                    case ScanDataType::Int32: {
                        int32_t v = readVal<int32_t>(newBytes);
                        uint32_t u = readVal<uint32_t>(newBytes);
                        match = (v == static_cast<int32_t>(targetInt) || u == static_cast<uint32_t>(targetUInt));
                        break;
                    }
                    case ScanDataType::Int64: {
                        int64_t v = readVal<int64_t>(newBytes);
                        uint64_t u = readVal<uint64_t>(newBytes);
                        match = (v == targetInt || u == targetUInt);
                        break;
                    }
                    case ScanDataType::Float: {
                        float v = readVal<float>(newBytes);
                        match = (std::fabs(v - targetFloat) < 0.0001f);
                        break;
                    }
                    case ScanDataType::Double: {
                        double v = readVal<double>(newBytes);
                        match = (std::fabs(v - targetDouble) < 0.000001);
                        break;
                    }
                    case ScanDataType::String: {
                        match = (std::memcmp(newBytes.data(), options.valueStr.data(), dataSize) == 0);
                        break;
                    }
                    case ScanDataType::ByteArray: {
                        match = true;
                        for (size_t p = 0; p < targetPattern.size(); ++p) {
                            if (!targetPattern[p].isWildcard && newBytes[p] != targetPattern[p].value) {
                                match = false;
                                break;
                            }
                        }
                        break;
                    }
                }
                break;
            }

            case ScanCompareType::IncreasedValue: {
                switch (options.dataType) {
                    case ScanDataType::Int8:
                        match = (readVal<int8_t>(newBytes) > readVal<int8_t>(oldBytes)); break;
                    case ScanDataType::Int16:
                        match = (readVal<int16_t>(newBytes) > readVal<int16_t>(oldBytes)); break;
                    case ScanDataType::Int32:
                        match = (readVal<int32_t>(newBytes) > readVal<int32_t>(oldBytes)); break;
                    case ScanDataType::Int64:
                        match = (readVal<int64_t>(newBytes) > readVal<int64_t>(oldBytes)); break;
                    case ScanDataType::Float:
                        match = (readVal<float>(newBytes) > readVal<float>(oldBytes)); break;
                    case ScanDataType::Double:
                        match = (readVal<double>(newBytes) > readVal<double>(oldBytes)); break;
                    default: break;
                }
                break;
            }

            case ScanCompareType::DecreasedValue: {
                switch (options.dataType) {
                    case ScanDataType::Int8:
                        match = (readVal<int8_t>(newBytes) < readVal<int8_t>(oldBytes)); break;
                    case ScanDataType::Int16:
                        match = (readVal<int16_t>(newBytes) < readVal<int16_t>(oldBytes)); break;
                    case ScanDataType::Int32:
                        match = (readVal<int32_t>(newBytes) < readVal<int32_t>(oldBytes)); break;
                    case ScanDataType::Int64:
                        match = (readVal<int64_t>(newBytes) < readVal<int64_t>(oldBytes)); break;
                    case ScanDataType::Float:
                        match = (readVal<float>(newBytes) < readVal<float>(oldBytes)); break;
                    case ScanDataType::Double:
                        match = (readVal<double>(newBytes) < readVal<double>(oldBytes)); break;
                    default: break;
                }
                break;
            }

            case ScanCompareType::ChangedValue: {
                match = (newBytes != oldBytes);
                break;
            }

            case ScanCompareType::UnchangedValue: {
                match = (newBytes == oldBytes);
                break;
            }

            case ScanCompareType::IncreasedBy: {
                switch (options.dataType) {
                    case ScanDataType::Int8:
                        match = (readVal<int8_t>(newBytes) == readVal<int8_t>(oldBytes) + static_cast<int8_t>(deltaInt)); break;
                    case ScanDataType::Int16:
                        match = (readVal<int16_t>(newBytes) == readVal<int16_t>(oldBytes) + static_cast<int16_t>(deltaInt)); break;
                    case ScanDataType::Int32:
                        match = (readVal<int32_t>(newBytes) == readVal<int32_t>(oldBytes) + static_cast<int32_t>(deltaInt)); break;
                    case ScanDataType::Int64:
                        match = (readVal<int64_t>(newBytes) == readVal<int64_t>(oldBytes) + deltaInt); break;
                    case ScanDataType::Float:
                        match = (std::fabs(readVal<float>(newBytes) - (readVal<float>(oldBytes) + deltaFloat)) < 0.0001f); break;
                    case ScanDataType::Double:
                        match = (std::fabs(readVal<double>(newBytes) - (readVal<double>(oldBytes) + deltaDouble)) < 0.000001); break;
                    default: break;
                }
                break;
            }

            case ScanCompareType::DecreasedBy: {
                switch (options.dataType) {
                    case ScanDataType::Int8:
                        match = (readVal<int8_t>(newBytes) == readVal<int8_t>(oldBytes) - static_cast<int8_t>(deltaInt)); break;
                    case ScanDataType::Int16:
                        match = (readVal<int16_t>(newBytes) == readVal<int16_t>(oldBytes) - static_cast<int16_t>(deltaInt)); break;
                    case ScanDataType::Int32:
                        match = (readVal<int32_t>(newBytes) == readVal<int32_t>(oldBytes) - static_cast<int32_t>(deltaInt)); break;
                    case ScanDataType::Int64:
                        match = (readVal<int64_t>(newBytes) == readVal<int64_t>(oldBytes) - deltaInt); break;
                    case ScanDataType::Float:
                        match = (std::fabs(readVal<float>(newBytes) - (readVal<float>(oldBytes) - deltaFloat)) < 0.0001f); break;
                    case ScanDataType::Double:
                        match = (std::fabs(readVal<double>(newBytes) - (readVal<double>(oldBytes) - deltaDouble)) < 0.000001); break;
                    default: break;
                }
                break;
            }

            case ScanCompareType::UnknownInitialValue:
                match = true;
                break;
        }

        if (match) {
            ScanResult updated;
            updated.address = cand.address;
            updated.previousValue = cand.currentValue;
            updated.currentValue = newBytes;
            newResults.push_back(std::move(updated));
        }
    }

    results_ = std::move(newResults);
    ++scanPass_;
    return results_.size();
}

void MemoryScanner::refreshCurrentValues(IDebugBackend& engine) {
    if (results_.empty() || !engine.isAttached()) return;
    size_t dataSize = getDataTypeSize(activeOptions_.dataType, activeOptions_.valueStr);
    if (dataSize == 0) dataSize = 1;

    std::vector<uint8_t> buf(dataSize);
    for (auto& cand : results_) {
        if (engine.readMemory(cand.address, buf.data(), dataSize)) {
            cand.currentValue = buf;
        }
    }
}

void MemoryScanner::reset() {
    results_.clear();
    scanPass_ = 0;
}

} // namespace edb_next
