#pragma once

#include "Types.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace edb_next {

class LinuxDebugEngine;

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

struct ScanResult {
    Address address{0};
    std::vector<uint8_t> previousValue;
    std::vector<uint8_t> currentValue;

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
    size_t firstScan(LinuxDebugEngine& engine, const ScanOptions& options);

    // Next scan: performs differential convergence on existing candidate addresses
    size_t nextScan(LinuxDebugEngine& engine, const ScanOptions& options);

    // Refresh candidate values from live target memory without filtering
    void refreshCurrentValues(LinuxDebugEngine& engine);

    // Clear all results and reset pass state
    void reset();

    [[nodiscard]] const std::vector<ScanResult>& results() const noexcept { return results_; }
    [[nodiscard]] size_t resultCount() const noexcept { return results_.size(); }
    [[nodiscard]] bool hasSearched() const noexcept { return scanPass_ > 0; }
    [[nodiscard]] int scanPass() const noexcept { return scanPass_; }
    [[nodiscard]] const ScanOptions& activeOptions() const noexcept { return activeOptions_; }

    static size_t getDataTypeSize(ScanDataType type, const std::string& inputStr = "");

private:
    std::vector<ScanResult> results_;
    ScanOptions activeOptions_;
    int scanPass_{0};
};

} // namespace edb_next
