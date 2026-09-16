#include "PatchManager.hpp"
#include "DebugSession.hpp"
#include <elf.h>
#include <fstream>
#include <sys/stat.h>
#include <cstring>
#include <iostream>

namespace edb_next {

PatchManager::PatchManager(QObject* parent) : QObject(parent) {
}

void PatchManager::addPatch(Address addr, const std::vector<uint8_t>& origBytes, const std::vector<uint8_t>& newBytes, const std::string& comment) {
    // Check if a patch already exists at this exact address
    for (auto& p : patches_) {
        if (p.address == addr) {
            p.patchedBytes = newBytes;
            p.comment = comment;
            p.isApplied = true;
            Q_EMIT patchesUpdated();
            return;
        }
    }

    MemoryPatch patch{
        .address = addr,
        .originalBytes = origBytes,
        .patchedBytes = newBytes,
        .comment = comment,
        .isApplied = true
    };
    patches_.push_back(patch);
    Q_EMIT patchesUpdated();
}

bool PatchManager::revertPatch(size_t index, DebugSession* session) {
    if (index >= patches_.size()) return false;
    auto& p = patches_[index];
    if (!p.isApplied) return true;

    if (session && !p.originalBytes.empty()) {
        bool ok = session->writeMemory(p.address, p.originalBytes.data(), p.originalBytes.size());
        if (!ok) return false;
    }
    p.isApplied = false;
    Q_EMIT patchesUpdated();
    return true;
}

bool PatchManager::reapplyPatch(size_t index, DebugSession* session) {
    if (index >= patches_.size()) return false;
    auto& p = patches_[index];
    if (p.isApplied) return true;

    if (session && !p.patchedBytes.empty()) {
        bool ok = session->writeMemory(p.address, p.patchedBytes.data(), p.patchedBytes.size());
        if (!ok) return false;
    }
    p.isApplied = true;
    Q_EMIT patchesUpdated();
    return true;
}

void PatchManager::clearAll(DebugSession* session) {
    if (session) {
        for (size_t i = 0; i < patches_.size(); ++i) {
            revertPatch(i, session);
        }
    }
    patches_.clear();
    Q_EMIT patchesUpdated();
}

bool PatchManager::patchFileToDisk(const std::string& inputBinaryPath, const std::string& outputBinaryPath, std::string& errorMsg) {
    // 1. Read input binary
    std::ifstream in(inputBinaryPath, std::ios::binary);
    if (!in) {
        errorMsg = "Failed to open input binary for reading: " + inputBinaryPath;
        return false;
    }

    std::vector<uint8_t> file_bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    if (file_bytes.size() < sizeof(Elf64_Ehdr)) {
        errorMsg = "File is too small to be a valid ELF64 executable.";
        return false;
    }

    const auto* ehdr = reinterpret_cast<const Elf64_Ehdr*>(file_bytes.data());
    if (std::memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0 || ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        errorMsg = "Target is not a valid 64-bit ELF binary.";
        return false;
    }

    if (ehdr->e_phoff + ehdr->e_phnum * sizeof(Elf64_Phdr) > file_bytes.size()) {
        errorMsg = "Program headers extend beyond end of file.";
        return false;
    }

    const auto* phdrs = reinterpret_cast<const Elf64_Phdr*>(file_bytes.data() + ehdr->e_phoff);

    // 2. Identify base load address from lowest PT_LOAD segment
    uint64_t min_vaddr = UINT64_MAX;
    for (int i = 0; i < ehdr->e_phnum; ++i) {
        if (phdrs[i].p_type == PT_LOAD) {
            if (phdrs[i].p_vaddr < min_vaddr) {
                min_vaddr = phdrs[i].p_vaddr;
            }
        }
    }

    // 3. Apply each active patch
    size_t applied_count = 0;
    for (const auto& p : patches_) {
        if (!p.isApplied || p.patchedBytes.empty()) continue;

        uint64_t target_vaddr = p.address.value();
        bool found_segment = false;

        for (int i = 0; i < ehdr->e_phnum; ++i) {
            if (phdrs[i].p_type == PT_LOAD) {
                uint64_t seg_start = phdrs[i].p_vaddr;
                uint64_t seg_end = seg_start + phdrs[i].p_filesz;

                // Adjust for PIE if target_vaddr was runtime relocated
                uint64_t check_addr = target_vaddr;
                if (ehdr->e_type == ET_DYN && target_vaddr >= 0x555500000000ULL) {
                    // Normalize PIE virtual address to ELF file offset basis
                    // Find PIE load base from caller or segment start
                    check_addr = (target_vaddr & 0x000000ffffffULL);
                }

                if (check_addr >= seg_start && (check_addr + p.patchedBytes.size()) <= seg_end) {
                    uint64_t file_offset = phdrs[i].p_offset + (check_addr - seg_start);
                    if (file_offset + p.patchedBytes.size() <= file_bytes.size()) {
                        std::memcpy(file_bytes.data() + file_offset, p.patchedBytes.data(), p.patchedBytes.size());
                        applied_count++;
                        found_segment = true;
                        break;
                    }
                }
            }
        }

        if (!found_segment) {
            // Try direct offset fallback for ET_DYN
            uint64_t offset_candidate = target_vaddr & 0x000000ffffffULL;
            if (offset_candidate + p.patchedBytes.size() <= file_bytes.size()) {
                std::memcpy(file_bytes.data() + offset_candidate, p.patchedBytes.data(), p.patchedBytes.size());
                applied_count++;
            }
        }
    }

    // 4. Write output binary
    std::ofstream out(outputBinaryPath, std::ios::binary);
    if (!out) {
        errorMsg = "Failed to open output binary for writing: " + outputBinaryPath;
        return false;
    }
    out.write(reinterpret_cast<const char*>(file_bytes.data()), file_bytes.size());
    out.close();

    // 5. Make executable
    ::chmod(outputBinaryPath.c_str(), 0755);

    return true;
}

} // namespace edb_next
