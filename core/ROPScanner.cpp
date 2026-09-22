#include "ROPScanner.hpp"
#include "LinuxDebugEngine.hpp"
#include "CapstoneContext.hpp"
#include <capstone/capstone.h>
#include <unordered_set>
#include <algorithm>

namespace edb_next {

namespace {

std::string categorizeGadget(const std::string& disasm) {
    if (disasm.find("syscall") != std::string::npos || disasm.find("int 0x80") != std::string::npos) {
        return "Syscall";
    }
    if (disasm.find("pop ") != std::string::npos) {
        return "Stack (pop)";
    }
    if (disasm.find("add ") != std::string::npos || disasm.find("sub ") != std::string::npos ||
        disasm.find("xor ") != std::string::npos || disasm.find("inc ") != std::string::npos ||
        disasm.find("dec ") != std::string::npos || disasm.find("and ") != std::string::npos ||
        disasm.find("or ") != std::string::npos) {
        return "Arithmetic";
    }
    if (disasm.find("mov ") != std::string::npos || disasm.find("xchg ") != std::string::npos ||
        disasm.find("lea ") != std::string::npos) {
        return "Data Transfer";
    }
    if (disasm.find("call ") != std::string::npos || disasm.find("jmp ") != std::string::npos) {
        return "Branch";
    }
    return "General";
}

} // namespace

std::vector<ROPGadget> ROPScanner::scan(
    LinuxDebugEngine& engine,
    size_t maxGadgetLength,
    size_t maxResults,
    const std::string& filter) {

    std::vector<ROPGadget> gadgets;
    if (!engine.isAttached()) return gadgets;

    auto cs = CapstoneContext::acquire(false);
    if (!cs.isValid()) {
        return gadgets;
    }
    csh handle = cs.get();

    auto regions = engine.getMemoryRegions();
    std::unordered_set<uint64_t> seenAddresses;

    std::string lowerFilter = filter;
    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    for (const auto& region : regions) {
        if (!region.isExecutable() || !region.isReadable()) continue;
        // Don't scan huge mapped files > 20MB in one go
        size_t scanSize = std::min<size_t>(region.size(), 8 * 1024 * 1024);
        if (scanSize < 4) continue;

        std::vector<uint8_t> buffer(scanSize);
        if (!engine.readMemory(region.start, buffer.data(), scanSize)) {
            continue;
        }

        for (size_t offset = 0; offset < scanSize && gadgets.size() < maxResults; ++offset) {
            uint8_t b = buffer[offset];
            size_t termLen = 0;

            if (b == 0xC3) { // ret
                termLen = 1;
            } else if (b == 0xC2 && offset + 2 < scanSize) { // ret imm16
                termLen = 3;
            } else if (b == 0x0F && offset + 1 < scanSize && buffer[offset + 1] == 0x05) { // syscall
                termLen = 2;
            } else {
                continue;
            }

            // Look back 1 to 14 bytes
            size_t maxBack = std::min<size_t>(offset, 14);
            for (size_t back = 0; back <= maxBack; ++back) {
                size_t gadgetStartOffset = offset - back;
                Address gadgetInstanceAddr = region.start + gadgetStartOffset;

                if (seenAddresses.contains(gadgetInstanceAddr.value())) {
                    continue;
                }

                const uint8_t* codePtr = buffer.data() + gadgetStartOffset;
                size_t codeLen = back + termLen;

                cs_insn* insn = nullptr;
                size_t count = cs_disasm(handle, codePtr, codeLen, gadgetInstanceAddr.value(), maxGadgetLength + 1, &insn);

                if (count > 0 && count <= maxGadgetLength) {
                    // Check if the disassembled instructions cover the whole byte range
                    size_t totalBytes = 0;
                    bool hasBadInsn = false;
                    for (size_t k = 0; k < count; ++k) {
                        totalBytes += insn[k].size;
                        // Avoid gadget with internal branch or invalid
                        if (insn[k].id == X86_INS_INVALID) {
                            hasBadInsn = true;
                            break;
                        }
                    }

                    if (!hasBadInsn && totalBytes == codeLen) {
                        // Check if the last instruction is indeed ret or syscall
                        int lastId = insn[count - 1].id;
                        if (lastId == X86_INS_RET || lastId == X86_INS_SYSCALL) {
                            std::string disasmStr;
                            for (size_t k = 0; k < count; ++k) {
                                if (k > 0) disasmStr += " ; ";
                                disasmStr += insn[k].mnemonic;
                                if (insn[k].op_str[0] != '\0') {
                                    disasmStr += " ";
                                    disasmStr += insn[k].op_str;
                                }
                            }

                            bool matchFilter = true;
                            if (!lowerFilter.empty()) {
                                std::string lowerDisasm = disasmStr;
                                std::transform(lowerDisasm.begin(), lowerDisasm.end(), lowerDisasm.begin(),
                                               [](unsigned char c) { return std::tolower(c); });
                                if (lowerDisasm.find(lowerFilter) == std::string::npos) {
                                    matchFilter = false;
                                }
                            }

                            if (matchFilter) {
                                seenAddresses.insert(gadgetInstanceAddr.value());
                                gadgets.push_back(ROPGadget{
                                    .address = gadgetInstanceAddr,
                                    .disassembly = disasmStr,
                                    .category = categorizeGadget(disasmStr),
                                    .length = codeLen,
                                    .insnCount = count
                                });
                            }
                        }
                    }
                }

                if (insn) {
                    cs_free(insn, count);
                }

                if (gadgets.size() >= maxResults) break;
            }
        }
    }

    return gadgets;
}

} // namespace edb_next
