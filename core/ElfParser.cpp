#include "ElfParser.hpp"
#include <elf.h>
#include <fstream>
#include <algorithm>
#include <iostream>

namespace edb_next {

std::string ElfHeaderInfo::typeString() const {
    switch (type) {
        case ET_REL:  return "Relocatable (ET_REL)";
        case ET_EXEC: return "Executable (ET_EXEC)";
        case ET_DYN:  return "Shared Object / PIE (ET_DYN)";
        case ET_CORE: return "Core Dump (ET_CORE)";
        default:      return "Unknown (" + std::to_string(type) + ")";
    }
}

std::string ElfHeaderInfo::machineString() const {
    switch (machine) {
        case EM_X86_64: return "Advanced Micro Devices X86-64";
        case EM_386:    return "Intel 80386";
        case EM_AARCH64: return "ARM AArch64";
        case EM_ARM:    return "ARM 32-bit";
        default:        return "Machine #" + std::to_string(machine);
    }
}

std::string ElfSectionInfo::flagsString() const {
    std::string res;
    if (flags & SHF_WRITE) res += "W";
    if (flags & SHF_ALLOC) res += "A";
    if (flags & SHF_EXECINSTR) res += "X";
    if (flags & SHF_MERGE) res += "M";
    if (flags & SHF_STRINGS) res += "S";
    if (flags & SHF_INFO_LINK) res += "I";
    return res.empty() ? "-" : res;
}

std::string ElfProgramHeaderInfo::flagsString() const {
    std::string res;
    if (flags & PF_R) res += "R";
    if (flags & PF_W) res += "W";
    if (flags & PF_X) res += "X";
    return res.empty() ? "-" : res;
}

static std::string sectionTypeToString(uint32_t type) {
    switch (type) {
        case SHT_NULL:          return "NULL";
        case SHT_PROGBITS:      return "PROGBITS";
        case SHT_SYMTAB:        return "SYMTAB";
        case SHT_STRTAB:        return "STRTAB";
        case SHT_RELA:          return "RELA";
        case SHT_HASH:          return "HASH";
        case SHT_DYNAMIC:       return "DYNAMIC";
        case SHT_NOTE:          return "NOTE";
        case SHT_NOBITS:        return "NOBITS";
        case SHT_REL:           return "REL";
        case SHT_SHLIB:         return "SHLIB";
        case SHT_DYNSYM:        return "DYNSYM";
        case SHT_INIT_ARRAY:    return "INIT_ARRAY";
        case SHT_FINI_ARRAY:    return "FINI_ARRAY";
        case SHT_PREINIT_ARRAY: return "PREINIT_ARRAY";
        case SHT_GROUP:         return "GROUP";
        case SHT_SYMTAB_SHNDX:  return "SYMTAB_SHNDX";
        case SHT_GNU_HASH:      return "GNU_HASH";
        case SHT_GNU_verdef:    return "GNU_VERDEF";
        case SHT_GNU_verneed:   return "GNU_VERNEED";
        case SHT_GNU_versym:    return "GNU_VERSYM";
        default:                return "TYPE_0x" + std::to_string(type);
    }
}

static std::string segmentTypeToString(uint32_t type) {
    switch (type) {
        case PT_NULL:         return "NULL";
        case PT_LOAD:         return "LOAD";
        case PT_DYNAMIC:      return "DYNAMIC";
        case PT_INTERP:       return "INTERP";
        case PT_NOTE:         return "NOTE";
        case PT_SHLIB:        return "SHLIB";
        case PT_PHDR:         return "PHDR";
        case PT_TLS:          return "TLS";
        case PT_GNU_EH_FRAME: return "GNU_EH_FRAME";
        case PT_GNU_STACK:    return "GNU_STACK";
        case PT_GNU_RELRO:    return "GNU_RELRO";
        case PT_GNU_PROPERTY: return "GNU_PROPERTY";
        default:              return "SEG_0x" + std::to_string(type);
    }
}

void ElfParser::clear() {
    symbols_.clear();
    addressToSymbolIdx_.clear();
    nameToAddress_.clear();
    sections_.clear();
    programHeaders_.clear();
    dynamicDependencies_.clear();
    headerInfo_ = {};
    baseAddress_ = Address(0);
}

bool ElfParser::loadBinary(const std::string& filepath, Address base_addr) {
    clear();
    baseAddress_ = base_addr;

    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    Elf64_Ehdr ehdr;
    file.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr));
    if (!file || ehdr.e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr.e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr.e_ident[EI_MAG3] != ELFMAG3) {
        return false;
    }

    if (ehdr.e_ident[EI_CLASS] != ELFCLASS64) {
        return false; // Only 64-bit ELF supported
    }

    uint64_t actual_entry = ehdr.e_entry;
    if (ehdr.e_type == ET_DYN && base_addr.value() > 0) {
        actual_entry += base_addr.value();
    }

    headerInfo_.elfClass = ehdr.e_ident[EI_CLASS];
    headerInfo_.endianness = ehdr.e_ident[EI_DATA];
    headerInfo_.osAbi = ehdr.e_ident[EI_OSABI];
    headerInfo_.type = ehdr.e_type;
    headerInfo_.machine = ehdr.e_machine;
    headerInfo_.entryPoint = Address(actual_entry);
    headerInfo_.programHeadersOffset = ehdr.e_phoff;
    headerInfo_.sectionHeadersOffset = ehdr.e_shoff;
    headerInfo_.programHeaderCount = ehdr.e_phnum;
    headerInfo_.sectionHeaderCount = ehdr.e_shnum;

    // Read Program Headers
    if (ehdr.e_phnum > 0 && ehdr.e_phoff > 0) {
        std::vector<Elf64_Phdr> phdrs(ehdr.e_phnum);
        file.seekg(ehdr.e_phoff);
        file.read(reinterpret_cast<char*>(phdrs.data()), ehdr.e_phnum * sizeof(Elf64_Phdr));
        if (file) {
            for (const auto& ph : phdrs) {
                uint64_t pvaddr = ph.p_vaddr;
                if (ehdr.e_type == ET_DYN && base_addr.value() > 0) {
                    pvaddr += base_addr.value();
                }
                programHeaders_.push_back(ElfProgramHeaderInfo{
                    .typeString = segmentTypeToString(ph.p_type),
                    .type = ph.p_type,
                    .flags = ph.p_flags,
                    .offset = ph.p_offset,
                    .vaddr = Address(pvaddr),
                    .paddr = Address(ph.p_paddr),
                    .filesz = ph.p_filesz,
                    .memsz = ph.p_memsz,
                    .align = ph.p_align
                });
            }
        }
    }

    // Read Section Headers
    std::vector<Elf64_Shdr> shdrs(ehdr.e_shnum);
    file.seekg(ehdr.e_shoff);
    file.read(reinterpret_cast<char*>(shdrs.data()), ehdr.e_shnum * sizeof(Elf64_Shdr));
    if (!file) return false;

    // Read Section Header String Table (.shstrtab)
    std::vector<char> shstrtab;
    if (ehdr.e_shstrndx < shdrs.size()) {
        shstrtab.resize(shdrs[ehdr.e_shstrndx].sh_size);
        file.seekg(shdrs[ehdr.e_shstrndx].sh_offset);
        file.read(shstrtab.data(), shstrtab.size());
    }

    // Populate Section Infos
    for (const auto& sh : shdrs) {
        std::string sname;
        if (sh.sh_name < shstrtab.size()) {
            sname = &shstrtab[sh.sh_name];
        }

        uint64_t saddr = sh.sh_addr;
        if (ehdr.e_type == ET_DYN && base_addr.value() > 0 && saddr > 0) {
            saddr += base_addr.value();
        }

        sections_.push_back(ElfSectionInfo{
            .name = sname,
            .typeString = sectionTypeToString(sh.sh_type),
            .type = sh.sh_type,
            .flags = sh.sh_flags,
            .address = Address(saddr),
            .offset = sh.sh_offset,
            .size = sh.sh_size,
            .align = sh.sh_addralign
        });
    }

    // Helper lambda to load symbols from a table (symtab or dynsym)
    auto parse_sym_table = [&](const Elf64_Shdr& sym_shdr, const Elf64_Shdr& str_shdr) {
        size_t count = sym_shdr.sh_size / sizeof(Elf64_Sym);
        std::vector<Elf64_Sym> syms(count);
        file.seekg(sym_shdr.sh_offset);
        file.read(reinterpret_cast<char*>(syms.data()), sym_shdr.sh_size);

        std::vector<char> strtab(str_shdr.sh_size);
        file.seekg(str_shdr.sh_offset);
        file.read(strtab.data(), str_shdr.sh_size);

        for (const auto& sym : syms) {
            if (sym.st_name == 0 || sym.st_value == 0) continue;
            if (sym.st_name >= strtab.size()) continue;

            const char* name_ptr = &strtab[sym.st_name];
            std::string sym_name(name_ptr);
            if (sym_name.empty()) continue;

            uint64_t actual_val = sym.st_value;
            // For PIE executables, st_value is an offset; add base_addr if appropriate
            if (ehdr.e_type == ET_DYN && base_addr.value() > 0) {
                actual_val += base_addr.value();
            }

            SymbolInfo info{
                .name = sym_name,
                .address = Address(actual_val),
                .size = sym.st_size,
                .type = static_cast<uint8_t>(ELF64_ST_TYPE(sym.st_info)),
                .binding = static_cast<uint8_t>(ELF64_ST_BIND(sym.st_info))
            };

            size_t idx = symbols_.size();
            symbols_.push_back(info);
            addressToSymbolIdx_[actual_val] = idx;
            nameToAddress_[sym_name] = Address(actual_val);
        }
    };

    // Find .symtab and .dynsym with matching .strtab and .dynstr
    for (size_t i = 0; i < shdrs.size(); ++i) {
        if (shdrs[i].sh_type == SHT_SYMTAB || shdrs[i].sh_type == SHT_DYNSYM) {
            uint32_t str_link = shdrs[i].sh_link;
            if (str_link < shdrs.size() && shdrs[str_link].sh_type == SHT_STRTAB) {
                parse_sym_table(shdrs[i], shdrs[str_link]);
            }
        }
    }

    // Parse Dynamic section (.dynamic) for DT_NEEDED dependencies
    for (size_t i = 0; i < shdrs.size(); ++i) {
        if (shdrs[i].sh_type == SHT_DYNAMIC) {
            uint32_t str_link = shdrs[i].sh_link;
            if (str_link < shdrs.size() && shdrs[str_link].sh_type == SHT_STRTAB) {
                std::vector<char> dynstr(shdrs[str_link].sh_size);
                file.seekg(shdrs[str_link].sh_offset);
                file.read(dynstr.data(), dynstr.size());

                size_t dyn_count = shdrs[i].sh_size / sizeof(Elf64_Dyn);
                std::vector<Elf64_Dyn> dyns(dyn_count);
                file.seekg(shdrs[i].sh_offset);
                file.read(reinterpret_cast<char*>(dyns.data()), shdrs[i].sh_size);

                for (const auto& dyn : dyns) {
                    if (dyn.d_tag == DT_NEEDED && dyn.d_un.d_val < dynstr.size()) {
                        std::string dep = &dynstr[dyn.d_un.d_val];
                        if (!dep.empty() && std::find(dynamicDependencies_.begin(), dynamicDependencies_.end(), dep) == dynamicDependencies_.end()) {
                            dynamicDependencies_.push_back(dep);
                        }
                    }
                }
            }
        }
    }

    // Sort symbols by address for nearest lookups
    std::sort(symbols_.begin(), symbols_.end(), [](const SymbolInfo& a, const SymbolInfo& b) {
        return a.address < b.address;
    });

    addressToSymbolIdx_.clear();
    for (size_t i = 0; i < symbols_.size(); ++i) {
        addressToSymbolIdx_[symbols_[i].address.value()] = i;
    }

    return true;
}

std::optional<SymbolInfo> ElfParser::findExactSymbol(Address addr) const {
    auto it = addressToSymbolIdx_.find(addr.value());
    if (it != addressToSymbolIdx_.end()) {
        return symbols_[it->second];
    }
    return std::nullopt;
}

std::optional<std::pair<SymbolInfo, uint64_t>> ElfParser::findNearestSymbol(Address addr) const {
    if (symbols_.empty()) return std::nullopt;

    auto it = std::upper_bound(symbols_.begin(), symbols_.end(), addr,
        [](Address target, const SymbolInfo& s) {
            return target < s.address;
        });

    if (it == symbols_.begin()) {
        if (symbols_.front().address == addr) {
            return std::make_pair(symbols_.front(), 0);
        }
        return std::nullopt;
    }

    --it;
    uint64_t offset = addr.value() - it->address.value();
    if (it->size > 0 && offset >= it->size) {
        return std::nullopt;
    }

    return std::make_pair(*it, offset);
}

std::optional<Address> ElfParser::findSymbolAddress(const std::string& name) const {
    auto it = nameToAddress_.find(name);
    if (it != nameToAddress_.end()) {
        return it->second;
    }
    return std::nullopt;
}

} // namespace edb_next
