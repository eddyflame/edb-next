#include "StringScanner.hpp"
#include "DebugSession.hpp"
#include "CapstoneContext.hpp"
#include <capstone/capstone.h>
#include <algorithm>
#include <cctype>
#include <map>

namespace edb_next {

namespace {

bool isPrintableAscii(uint8_t c) {
    return (c >= 0x20 && c <= 0x7E) || c == '\t' || c == '\r' || c == '\n';
}

} // namespace

std::vector<StringItem> StringScanner::scan(DebugSession& session, size_t min_length, size_t max_results) {
    std::vector<StringItem> results;
    auto regions = session.memoryRegions();
    if (regions.empty()) return results;

    // Fast lookup of string address range to index in results
    std::map<uint64_t, size_t> addrToStringIdx;

    // 1. Scan readable memory regions for ASCII strings
    for (const auto& reg : regions) {
        if (!reg.isReadable()) continue;

        // Skip massive mappings like vdso/vvar or giant pseudo-files (> 32MB)
        uint64_t region_size = reg.size();
        if (region_size == 0 || region_size > 32 * 1024 * 1024) continue;

        // Read in chunks
        const size_t chunkSize = 64 * 1024;
        std::vector<uint8_t> buffer(chunkSize);

        for (uint64_t offset = 0; offset < region_size; offset += chunkSize) {
            size_t bytesToRead = std::min<size_t>(chunkSize, region_size - offset);
            Address curAddr = reg.start + offset;

            auto chunk = session.readMemory(curAddr, bytesToRead);
            if (chunk.empty()) continue;

            size_t i = 0;
            while (i < chunk.size()) {
                if (isPrintableAscii(chunk[i])) {
                    size_t str_start = i;
                    while (i < chunk.size() && isPrintableAscii(chunk[i])) {
                        ++i;
                    }
                    size_t str_len = i - str_start;
                    if (str_len >= min_length) {
                        // Check if terminated by \0 or chunk end
                        StringItem item;
                        item.address = curAddr + str_start;
                        item.text = std::string(reinterpret_cast<char*>(&chunk[str_start]), str_len);
                        item.length = str_len;
                        item.regionName = reg.pathname.empty() ? "[anon]" : reg.pathname;

                        addrToStringIdx[item.address.value()] = results.size();
                        results.push_back(std::move(item));

                        if (results.size() >= max_results) break;
                    }
                } else {
                    ++i;
                }
            }
            if (results.size() >= max_results) break;
        }
        if (results.size() >= max_results) break;
    }

    if (results.empty()) return results;

    // 2. Scan executable regions for references to these strings
    auto cs = CapstoneContext::acquire(true);
    if (cs.isValid()) {
        csh cs_handle = cs.get();

        for (const auto& reg : regions) {
            if (!reg.isExecutable() || !reg.isReadable()) continue;

            uint64_t region_size = reg.size();
            if (region_size == 0 || region_size > 16 * 1024 * 1024) continue;

            const size_t chunkSize = 64 * 1024;
            for (uint64_t offset = 0; offset < region_size; offset += chunkSize) {
                size_t bytesToRead = std::min<size_t>(chunkSize, region_size - offset);
                Address curAddr = reg.start + offset;

                auto code = session.readMemory(curAddr, bytesToRead);
                if (code.empty()) continue;

                cs_insn* insn = nullptr;
                size_t count = cs_disasm(cs_handle, code.data(), code.size(), curAddr.value(), 0, &insn);
                if (count > 0) {
                    for (size_t k = 0; k < count; ++k) {
                        const auto& in = insn[k];
                        if (!in.detail) continue;

                        for (int op_idx = 0; op_idx < in.detail->x86.op_count; ++op_idx) {
                            const auto& op = in.detail->x86.operands[op_idx];
                            uint64_t target_addr = 0;

                            if (op.type == X86_OP_MEM && op.mem.base == X86_REG_RIP) {
                                // RIP-relative address: next_ip + disp
                                target_addr = in.address + in.size + op.mem.disp;
                            } else if (op.type == X86_OP_IMM) {
                                target_addr = static_cast<uint64_t>(op.imm);
                            }

                            if (target_addr != 0) {
                                auto it = addrToStringIdx.find(target_addr);
                                if (it != addrToStringIdx.end()) {
                                    results[it->second].references.push_back(Address(in.address));
                                }
                            }
                        }
                    }
                    cs_free(insn, count);
                }
            }
        }
    }

    return results;
}

} // namespace edb_next
