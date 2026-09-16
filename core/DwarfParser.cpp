#include "DwarfParser.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <elfutils/libdw.h>
#include <dwarf.h>
#include <libelf.h>
#include <algorithm>
#include <iostream>
#include <set>

namespace edb_next {

DwarfParser::~DwarfParser() {
    clear();
}

void DwarfParser::clear() {
    hasDebugInfo_ = false;
    baseAddress_ = Address(0);
    binaryPath_.clear();
    cuList_.clear();
    allSourceFiles_.clear();
    lineEntries_.clear();
    fileLineToAddress_.clear();
    addressToIdx_.clear();
}

bool DwarfParser::load(const std::string& filepath, Address base_addr) {
    clear();
    binaryPath_ = filepath;
    baseAddress_ = base_addr;

    elf_version(EV_CURRENT);
    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd < 0) {
        return false;
    }

    Dwarf* dbg = dwarf_begin(fd, DWARF_C_READ);
    if (!dbg) {
        close(fd);
        return false;
    }

    // Determine PIE reloc base
    uint64_t reloc_base = 0;
    Elf* elf = dwarf_getelf(dbg);
    if (elf) {
        Elf64_Ehdr* ehdr = elf64_getehdr(elf);
        if (ehdr && ehdr->e_type == ET_DYN && base_addr.value() > 0) {
            reloc_base = base_addr.value();
        }
    }

    std::set<std::string> uniqueFiles;
    Dwarf_Off cu_off = 0, next_off = 0;
    size_t cu_header_size = 0;

    while (dwarf_nextcu(dbg, cu_off, &next_off, &cu_header_size, nullptr, nullptr, nullptr) == 0) {
        Dwarf_Die cu_die;
        if (!dwarf_offdie(dbg, cu_off + cu_header_size, &cu_die)) {
            cu_off = next_off;
            continue;
        }

        CompilationUnitInfo cuInfo;
        const char* cu_name = dwarf_diename(&cu_die);
        if (cu_name) cuInfo.name = cu_name;

        Dwarf_Attribute attr;
        if (dwarf_attr(&cu_die, DW_AT_comp_dir, &attr)) {
            const char* dir = dwarf_formstring(&attr);
            if (dir) cuInfo.compDir = dir;
        }
        if (dwarf_attr(&cu_die, DW_AT_producer, &attr)) {
            const char* prod = dwarf_formstring(&attr);
            if (prod) cuInfo.producer = prod;
        }

        Dwarf_Addr low_pc = 0, high_pc = 0;
        if (dwarf_lowpc(&cu_die, &low_pc) == 0) {
            cuInfo.lowPc = Address(low_pc + reloc_base);
        }
        if (dwarf_attr(&cu_die, DW_AT_high_pc, &attr)) {
            int form = dwarf_whatform(&attr);
            if (dwarf_highpc(&cu_die, &high_pc) == 0) {
                if (form == DW_FORM_addr) {
                    cuInfo.highPc = Address(high_pc + reloc_base);
                } else {
                    cuInfo.highPc = Address(low_pc + high_pc + reloc_base);
                }
            }
        }

        Dwarf_Lines* lines = nullptr;
        size_t nlines = 0;
        if (dwarf_getsrclines(&cu_die, &lines, &nlines) == 0 && lines != nullptr) {
            for (size_t i = 0; i < nlines; ++i) {
                Dwarf_Line* line = dwarf_onesrcline(lines, i);
                if (!line) continue;

                Dwarf_Addr addr = 0;
                dwarf_lineaddr(line, &addr);
                int lineno = 0;
                dwarf_lineno(line, &lineno);
                int col = 0;
                dwarf_linecol(line, &col);
                const char* src = dwarf_linesrc(line, nullptr, nullptr);
                bool is_stmt = false;
                dwarf_linebeginstatement(line, &is_stmt);
                bool is_prologue_end = false;
                dwarf_lineprologueend(line, &is_prologue_end);
                bool is_epilogue_begin = false;
                dwarf_lineepiloguebegin(line, &is_epilogue_begin);

                if (src && lineno > 0) {
                    std::string src_path(src);
                    std::string filename = src_path;
                    auto pos = src_path.find_last_of('/');
                    if (pos != std::string::npos) {
                        filename = src_path.substr(pos + 1);
                    }

                    Address actual_addr(addr + reloc_base);

                    SourceLocation loc{
                        .filePath = src_path,
                        .fileName = filename,
                        .directory = (pos != std::string::npos) ? src_path.substr(0, pos) : "",
                        .line = lineno,
                        .column = col,
                        .address = actual_addr,
                        .isStmt = is_stmt,
                        .isPrologueEnd = is_prologue_end,
                        .isEpilogueBegin = is_epilogue_begin
                    };

                    lineEntries_.push_back(loc);
                    uniqueFiles.insert(src_path);
                    cuInfo.files.push_back(src_path);

                    auto& lineMapFull = fileLineToAddress_[src_path];
                    if (lineMapFull.find(lineno) == lineMapFull.end() || is_stmt) {
                        lineMapFull[lineno] = actual_addr;
                    }

                    auto& lineMapBase = fileLineToAddress_[filename];
                    if (lineMapBase.find(lineno) == lineMapBase.end() || is_stmt) {
                        lineMapBase[lineno] = actual_addr;
                    }
                }
            }
        }

        cuList_.push_back(std::move(cuInfo));
        cu_off = next_off;
    }

    dwarf_end(dbg);
    close(fd);

    if (lineEntries_.empty()) {
        hasDebugInfo_ = false;
        return true; // Successfully read ELF, but no DWARF lines found
    }

    // Sort line entries by address for efficient binary searching
    std::sort(lineEntries_.begin(), lineEntries_.end(), [](const SourceLocation& a, const SourceLocation& b) {
        if (a.address != b.address) return a.address < b.address;
        if (a.isStmt != b.isStmt) return a.isStmt > b.isStmt; // prefer is_stmt
        return a.line < b.line;
    });

    addressToIdx_.clear();
    for (size_t i = 0; i < lineEntries_.size(); ++i) {
        uint64_t val = lineEntries_[i].address.value();
        if (addressToIdx_.find(val) == addressToIdx_.end() || lineEntries_[i].isStmt) {
            addressToIdx_[val] = i;
        }
    }

    allSourceFiles_.assign(uniqueFiles.begin(), uniqueFiles.end());
    hasDebugInfo_ = true;
    return true;
}

std::optional<SourceLocation> DwarfParser::findSourceLocation(Address addr) const {
    if (lineEntries_.empty()) return std::nullopt;

    auto it = addressToIdx_.find(addr.value());
    if (it != addressToIdx_.end()) {
        return lineEntries_[it->second];
    }

    // Binary search nearest preceding address
    auto upper = std::upper_bound(lineEntries_.begin(), lineEntries_.end(), addr,
        [](Address target, const SourceLocation& loc) {
            return target < loc.address;
        });

    if (upper != lineEntries_.begin()) {
        --upper;
        return *upper;
    }

    return std::nullopt;
}

std::optional<SourceLocation> DwarfParser::findNearestSourceLocation(Address addr, uint64_t maxOffset) const {
    auto loc = findSourceLocation(addr);
    if (!loc) return std::nullopt;

    if (addr >= loc->address && (addr.value() - loc->address.value()) <= maxOffset) {
        return loc;
    }
    return std::nullopt;
}

std::optional<Address> DwarfParser::findAddressByLine(const std::string& fileNameOrPath, int line) const {
    // 1. Try exact path match
    auto it = fileLineToAddress_.find(fileNameOrPath);
    if (it != fileLineToAddress_.end()) {
        auto lineIt = it->second.find(line);
        if (lineIt != it->second.end()) {
            return lineIt->second;
        }
    }

    // 2. Try basename match
    std::string base = fileNameOrPath;
    auto pos = fileNameOrPath.find_last_of('/');
    if (pos != std::string::npos) {
        base = fileNameOrPath.substr(pos + 1);
    }
    auto itBase = fileLineToAddress_.find(base);
    if (itBase != fileLineToAddress_.end()) {
        auto lineIt = itBase->second.find(line);
        if (lineIt != itBase->second.end()) {
            return lineIt->second;
        }
    }

    return std::nullopt;
}

std::vector<SourceLocation> DwarfParser::findLocationsForFile(const std::string& fileNameOrPath) const {
    std::vector<SourceLocation> results;
    std::string base = fileNameOrPath;
    auto pos = fileNameOrPath.find_last_of('/');
    if (pos != std::string::npos) {
        base = fileNameOrPath.substr(pos + 1);
    }

    for (const auto& loc : lineEntries_) {
        if (loc.filePath == fileNameOrPath || loc.fileName == base) {
            results.push_back(loc);
        }
    }
    return results;
}

} // namespace edb_next
