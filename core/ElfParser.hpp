#pragma once

#include "Types.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <cstdint>

namespace edb_next {

struct SymbolInfo {
    std::string name;
    std::string demangledName;
    Address address{0};
    uint64_t size{0};
    uint8_t type{0};
    uint8_t binding{0};

    [[nodiscard]] const std::string& displayName() const noexcept {
        return demangledName.empty() ? name : demangledName;
    }
};

struct ElfHeaderInfo {
    uint8_t elfClass{0};      // 1 = 32-bit, 2 = 64-bit
    uint8_t endianness{0};    // 1 = Little, 2 = Big
    uint8_t osAbi{0};
    uint16_t type{0};         // 2 = ET_EXEC, 3 = ET_DYN
    uint16_t machine{0};      // 62 = EM_X86_64
    Address entryPoint{0};
    uint64_t programHeadersOffset{0};
    uint64_t sectionHeadersOffset{0};
    uint16_t programHeaderCount{0};
    uint16_t sectionHeaderCount{0};

    [[nodiscard]] std::string typeString() const;
    [[nodiscard]] std::string machineString() const;
};

struct ElfSectionInfo {
    std::string name;
    std::string typeString;
    uint32_t type{0};
    uint64_t flags{0};
    Address address{0};
    uint64_t offset{0};
    uint64_t size{0};
    uint64_t align{0};

    [[nodiscard]] std::string flagsString() const;
    [[nodiscard]] bool isExecutable() const noexcept { return (flags & 0x4) != 0; }
    [[nodiscard]] bool isWritable() const noexcept { return (flags & 0x1) != 0; }
    [[nodiscard]] bool isAllocated() const noexcept { return (flags & 0x2) != 0; }
};

struct ElfProgramHeaderInfo {
    std::string typeString;
    uint32_t type{0};
    uint32_t flags{0};
    uint64_t offset{0};
    Address vaddr{0};
    Address paddr{0};
    uint64_t filesz{0};
    uint64_t memsz{0};
    uint64_t align{0};

    [[nodiscard]] std::string flagsString() const;
};

class ElfParser {
public:
    ElfParser() = default;

    bool loadBinary(const std::string& filepath, Address base_addr = Address(0));
    [[nodiscard]] static std::string demangle(const std::string& mangled);
    [[nodiscard]] std::optional<SymbolInfo> findExactSymbol(Address addr) const;
    [[nodiscard]] std::optional<std::pair<SymbolInfo, uint64_t>> findNearestSymbol(Address addr) const;
    [[nodiscard]] std::optional<Address> findSymbolAddress(const std::string& name) const;
    [[nodiscard]] const std::vector<SymbolInfo>& allSymbols() const noexcept { return symbols_; }

    [[nodiscard]] const ElfHeaderInfo& headerInfo() const noexcept { return headerInfo_; }
    [[nodiscard]] const std::vector<ElfSectionInfo>& sections() const noexcept { return sections_; }
    [[nodiscard]] const std::vector<ElfProgramHeaderInfo>& programHeaders() const noexcept { return programHeaders_; }
    [[nodiscard]] const std::vector<std::string>& dynamicDependencies() const noexcept { return dynamicDependencies_; }
    [[nodiscard]] Address entryPoint() const noexcept { return headerInfo_.entryPoint; }
    [[nodiscard]] Address baseAddress() const noexcept { return baseAddress_; }
    [[nodiscard]] bool hasDebugInfo() const noexcept;

    bool addSharedLibrary(const std::string& filepath, Address base_addr);
    [[nodiscard]] bool isModuleLoaded(const std::string& filepath) const;
    [[nodiscard]] const std::vector<std::string>& loadedModules() const noexcept { return loadedModules_; }

    void clear();

private:
    ElfHeaderInfo headerInfo_{};
    std::vector<ElfSectionInfo> sections_;
    std::vector<ElfProgramHeaderInfo> programHeaders_;
    std::vector<std::string> dynamicDependencies_;
    std::vector<std::string> loadedModules_;

    std::vector<SymbolInfo> symbols_;
    std::unordered_map<uint64_t, size_t> addressToSymbolIdx_;
    std::unordered_map<std::string, Address> nameToAddress_;
    Address baseAddress_{0};
};

} // namespace edb_next
