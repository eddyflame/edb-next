#pragma once

#include "Types.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <cstring>

namespace edb_next {

class IDebugBackend;

enum class ScanDataType {
    Int8,
    Int16,
    Int32,
    Int64,
    Float,
    Double,
    String,
    ByteArray
};

enum class ScanCompareType {
    ExactValue,
    IncreasedValue,
    DecreasedValue,
    ChangedValue,
    UnchangedValue,
    IncreasedBy,
    DecreasedBy,
    UnknownInitialValue
};

std::string scanDataTypeToString(ScanDataType type);
std::string scanCompareTypeToString(ScanCompareType type);
ScanDataType stringToScanDataType(const std::string& str);
ScanCompareType stringToScanCompareType(const std::string& str);

struct SmallBuffer {
    static constexpr size_t kInlineCap = 16;
    uint8_t inlineBuf[kInlineCap]{};
    std::vector<uint8_t> heapBuf{};
    size_t len{0};

    SmallBuffer() = default;

    SmallBuffer(const uint8_t* data, size_t sz) : len(sz) {
        if (sz <= kInlineCap) {
            std::memcpy(inlineBuf, data, sz);
        } else {
            heapBuf.assign(data, data + sz);
        }
    }

    SmallBuffer(const std::vector<uint8_t>& vec) : SmallBuffer(vec.data(), vec.size()) {}
    SmallBuffer(std::span<const uint8_t> s) : SmallBuffer(s.data(), s.size()) {}

    [[nodiscard]] const uint8_t* data() const noexcept {
        return len <= kInlineCap ? inlineBuf : heapBuf.data();
    }
    [[nodiscard]] uint8_t* data() noexcept {
        return len <= kInlineCap ? inlineBuf : heapBuf.data();
    }
    [[nodiscard]] size_t size() const noexcept { return len; }
    [[nodiscard]] bool empty() const noexcept { return len == 0; }

    uint8_t operator[](size_t idx) const noexcept { return data()[idx]; }

    void assign(const uint8_t* data, size_t sz) {
        len = sz;
        if (sz <= kInlineCap) {
            std::memcpy(inlineBuf, data, sz);
            heapBuf.clear();
        } else {
            heapBuf.assign(data, data + sz);
        }
    }

    bool operator==(const SmallBuffer& o) const noexcept {
        if (len != o.len) return false;
        return std::memcmp(data(), o.data(), len) == 0;
    }
};

struct ScanResult {
    Address address{0};
    SmallBuffer previousValue;
    SmallBuffer currentValue;

    std::string formatCurrentValue(ScanDataType type) const;
    std::string formatPreviousValue(ScanDataType type) const;
    std::string formatDelta(ScanDataType type) const;
};

struct ScanOptions {
    ScanDataType dataType{ScanDataType::Int32};
    ScanCompareType compareType{ScanCompareType::ExactValue};
    std::string valueStr;
    std::string deltaStr;
    bool writableOnly{true};
    size_t alignment{4};
    size_t maxResults{50000};
    Address customStart{0};
    Address customEnd{0};
};

class MemoryScanner {
public:
    MemoryScanner() = default;
    ~MemoryScanner() = default;

    // First scan: reads process memory and establishes baseline candidates
    size_t firstScan(IDebugBackend& engine, const ScanOptions& options);

    // Next scan: performs differential convergence on existing candidate addresses
    size_t nextScan(IDebugBackend& engine, const ScanOptions& options);

    // Refresh candidate values from live target memory without filtering
    void refreshCurrentValues(IDebugBackend& engine);

    // Clear all results and reset pass state
    void reset();

    [[nodiscard]] const std::vector<ScanResult>& results() const noexcept { return results_; }
    [[nodiscard]] size_t resultCount() const noexcept { return results_.size(); }
    [[nodiscard]] bool hasSearched() const noexcept { return scanPass_ > 0; }
    [[nodiscard]] int scanPass() const noexcept { return scanPass_; }
    [[nodiscard]] const ScanOptions& activeOptions() const noexcept { return activeOptions_; }

    static size_t getDataTypeSize(ScanDataType type, const std::string& inputStr = "");
    static bool isNeonSupported() noexcept;
    static bool isAvx2Supported() noexcept;
    static std::string activeSimdEngineName() noexcept;

private:
    std::vector<ScanResult> results_;
    ScanOptions activeOptions_;
    int scanPass_{0};
};

} // namespace edb_next
