#pragma once

#include "Types.hpp"
#include "DwarfTypes.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <optional>

namespace edb_next {

class DwarfParser {
public:
    DwarfParser() = default;
    ~DwarfParser();

    bool load(const std::string& filepath, Address base_addr = Address(0));
    bool addModule(const std::string& filepath, Address base_addr);
    void clear();

    [[nodiscard]] bool hasDebugInfo() const noexcept { return hasDebugInfo_; }
    [[nodiscard]] Address baseAddress() const noexcept { return baseAddress_; }
    [[nodiscard]] const std::string& binaryPath() const noexcept { return binaryPath_; }

    [[nodiscard]] const std::vector<CompilationUnitInfo>& compilationUnits() const noexcept { return cuList_; }
    [[nodiscard]] const std::vector<std::string>& allSourceFiles() const noexcept { return allSourceFiles_; }
    [[nodiscard]] const std::vector<SourceLocation>& allLineEntries() const noexcept { return lineEntries_; }

    // Lookups
    [[nodiscard]] std::optional<SourceLocation> findSourceLocation(Address addr) const;
    [[nodiscard]] std::optional<Address> findAddressByLine(const std::string& fileNameOrPath, int line) const;
    [[nodiscard]] std::vector<SourceLocation> findLocationsForFile(const std::string& fileNameOrPath) const;
    [[nodiscard]] std::optional<SourceLocation> findNearestSourceLocation(Address addr, uint64_t maxOffset = 64) const;

private:
    bool hasDebugInfo_{false};
    Address baseAddress_{0};
    std::string binaryPath_;
    std::vector<CompilationUnitInfo> cuList_;
    std::vector<std::string> allSourceFiles_;
    std::vector<SourceLocation> lineEntries_; // Sorted by Address

    std::unordered_map<std::string, std::map<int, Address>> fileLineToAddress_;
    std::unordered_map<uint64_t, size_t> addressToIdx_;
};

} // namespace edb_next
