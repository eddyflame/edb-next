#include "MemoryScanner.hpp"
#include "LinuxDebugEngine.hpp"
#include "PatternSearcher.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <algorithm>

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

template<typename T>
T readVal(const std::vector<uint8_t>& buf) {
    if (buf.size() < sizeof(T)) return T{0};
    T val;
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

size_t MemoryScanner::firstScan(LinuxDebugEngine& engine, const ScanOptions& options) {
    results_.clear();
    scanPass_ = 0;
    activeOptions_ = options;

    if (!engine.isAttached()) return 0;

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
    const size_t chunkSize = 1024 * 1024; // 1MB chunk reading
    std::vector<uint8_t> buffer(chunkSize);

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

        size_t totalRegionBytes = endAddr - startAddr;
        if (totalRegionBytes < dataSize) continue;

        for (size_t offset = 0; offset < totalRegionBytes; ) {
            size_t bytesToRead = std::min<size_t>(chunkSize, totalRegionBytes - offset);
            Address chunkAddr = startAddr + offset;

            if (!engine.readMemory(chunkAddr, buffer.data(), bytesToRead)) {
                offset += bytesToRead;
                continue;
            }

            size_t validLimit = bytesToRead >= dataSize ? (bytesToRead - dataSize + 1) : 0;
            for (size_t i = 0; i < validLimit; i += align) {
                Address curAddr = chunkAddr + i;
                bool match = false;
                std::vector<uint8_t> valBytes(buffer.begin() + i, buffer.begin() + i + dataSize);

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
                    res.previousValue = valBytes;
                    res.currentValue = valBytes;
                    results_.push_back(std::move(res));

                    if (results_.size() >= options.maxResults) {
                        scanPass_ = 1;
                        return results_.size();
                    }
                }
            }

            offset += (bytesToRead > dataSize ? (bytesToRead - dataSize + align) : bytesToRead);
        }
    }

    scanPass_ = 1;
    return results_.size();
}

size_t MemoryScanner::nextScan(LinuxDebugEngine& engine, const ScanOptions& options) {
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

void MemoryScanner::refreshCurrentValues(LinuxDebugEngine& engine) {
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
