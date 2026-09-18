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

bool PatchManager::patchFileToDisk(const std::string& inputBinaryPath,
                                   const std::string& outputBinaryPath,
                                   std::string& errorMsg,
                                   Address runtimeBase) {
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
    uint64_t max_vaddr = 0;
    for (int i = 0; i < ehdr->e_phnum; ++i) {
        if (phdrs[i].p_type == PT_LOAD) {
            if (phdrs[i].p_vaddr < min_vaddr) {
                min_vaddr = phdrs[i].p_vaddr;
            }
            if (phdrs[i].p_vaddr + phdrs[i].p_memsz > max_vaddr) {
                max_vaddr = phdrs[i].p_vaddr + phdrs[i].p_memsz;
            }
        }
    }
    if (min_vaddr == UINT64_MAX) {
        errorMsg = "No PT_LOAD segments found in ELF binary.";
        return false;
    }

    uint64_t effectiveBase = runtimeBase.value();

    // Helper: try to match given vaddr against PT_LOAD segments and apply
    auto tryApply = [&](uint64_t vaddr, const MemoryPatch& p) -> bool {
        for (int i = 0; i < ehdr->e_phnum; ++i) {
            if (phdrs[i].p_type == PT_LOAD) {
                uint64_t seg_start = phdrs[i].p_vaddr;
                uint64_t seg_end = seg_start + phdrs[i].p_filesz;

                if (vaddr >= seg_start && (vaddr + p.patchedBytes.size()) <= seg_end) {
                    uint64_t file_offset = phdrs[i].p_offset + (vaddr - seg_start);
                    if (file_offset + p.patchedBytes.size() <= file_bytes.size()) {
                        std::memcpy(file_bytes.data() + file_offset, p.patchedBytes.data(), p.patchedBytes.size());
                        return true;
                    }
                }
            }
        }
        return false;
    };

    // 3. Apply each active patch
    size_t applied_count = 0;
    for (const auto& p : patches_) {
        if (!p.isApplied || p.patchedBytes.empty()) continue;

        uint64_t target_vaddr = p.address.value();
        bool found_segment = false;

        // 3a. Direct virtual address match (non-PIE ET_EXEC or already RVA-relative)
        if (tryApply(target_vaddr, p)) {
            applied_count++;
            found_segment = true;
        }

        // 3b. Runtime base provided
        if (!found_segment && effectiveBase != 0 && target_vaddr >= effectiveBase) {
            uint64_t vaddr = (target_vaddr - effectiveBase) + min_vaddr;
            if (tryApply(vaddr, p)) {
                applied_count++;
                found_segment = true;
            }
        }

        // 3c. Fallback for PIE / ET_DYN when runtimeBase is not provided
        if (!found_segment && ehdr->e_type == ET_DYN) {
            static constexpr uint64_t kDefaultPieBases[] = {
                0x555555554000ULL, // Linux default non-ASLR PIE base
                0x400000ULL
            };
            for (uint64_t cand_base : kDefaultPieBases) {
                if (target_vaddr >= cand_base) {
                    uint64_t vaddr = (target_vaddr - cand_base) + min_vaddr;
                    if (tryApply(vaddr, p)) {
                        applied_count++;
                        found_segment = true;
                        break;
                    }
                }
            }

            // 3d. Dynamic base deduction using page alignment
            if (!found_segment) {
                for (int i = 0; i < ehdr->e_phnum; ++i) {
                    if (phdrs[i].p_type == PT_LOAD) {
                        uint64_t seg_start = phdrs[i].p_vaddr;
                        if (target_vaddr > seg_start) {
                            uint64_t deduced_base = (target_vaddr - seg_start) & ~0xfffULL;
                            if (deduced_base > 0 && target_vaddr >= deduced_base) {
                                uint64_t vaddr = (target_vaddr - deduced_base) + min_vaddr;
                                if (tryApply(vaddr, p)) {
                                    applied_count++;
                                    found_segment = true;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (patches_.size() > 0 && applied_count == 0) {
        errorMsg = "No patches could be mapped to ELF file offsets.";
        return false;
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
