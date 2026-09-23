#pragma once

#include "Types.hpp"
#include "TypeManager.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <span>
#include <unordered_map>
#include <optional>
#include <memory>

namespace edb_next {

struct BtfTypeEntry {
    uint32_t id{0};
    std::string name;
    uint8_t kind{0};
    uint32_t size{0};
    uint32_t targetTypeId{0}; // For PTR, TYPEDEF, VOLATILE, CONST
    bool kindFlag{false};
    std::vector<StructField> fields; // Populated for STRUCT and UNION
};

class BtfParser {
public:
    BtfParser();
    ~BtfParser() = default;

    // Parse raw BTF binary blob (typically starting with struct btf_header)
    bool parseBuffer(std::span<const uint8_t> data);

    // Read and parse BTF data from a file path
    bool parseFile(const std::string& filePath);

    // Extract and parse the .BTF section from an ELF binary
    bool parseElfSection(const std::string& elfPath, const std::string& sectionName = ".BTF");

    // Convenience method to parse the host kernel's /sys/kernel/btf/vmlinux
    bool parseVmlinux(const std::string& path = "/sys/kernel/btf/vmlinux");

    // Query types
    [[nodiscard]] const BtfTypeEntry* findType(const std::string& name) const;
    [[nodiscard]] const BtfTypeEntry* getTypeById(uint32_t id) const;
    [[nodiscard]] std::vector<std::string> allTypeNames() const;
    [[nodiscard]] std::vector<std::string> structAndUnionNames() const;

    // Export parsed structures and unions directly into TypeManager
    size_t exportToTypeManager(TypeManager& typeMgr) const;

    [[nodiscard]] size_t typeCount() const noexcept { return typesById_.size(); }
    [[nodiscard]] bool empty() const noexcept { return typesById_.empty(); }
    void clear();

    // Helper for testing: generate a valid minimal BTF buffer
    static std::vector<uint8_t> createSyntheticBtf(
        const std::string& structName,
        const std::vector<std::pair<std::string, uint32_t>>& fieldsWithSizes);

private:
    std::string getString(uint32_t offset) const;
    std::string resolveTypeName(uint32_t typeId) const;
    FieldKind typeIdToFieldKind(uint32_t typeId) const;

    std::vector<uint8_t> stringTable_;
    std::unordered_map<uint32_t, BtfTypeEntry> typesById_;
    std::unordered_map<std::string, uint32_t> nameToId_;
};

} // namespace edb_next
