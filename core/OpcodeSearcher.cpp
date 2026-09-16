#include "OpcodeSearcher.hpp"
#include "DebugSession.hpp"
#include <capstone/capstone.h>
#include <algorithm>
#include <cctype>

namespace edb_next {

static bool isGprRegister(const std::string& op) {
    static const std::vector<std::string> gprs = {
        "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
        "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp",
        "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"
    };
    std::string s = op;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return std::find(gprs.begin(), gprs.end(), s) != gprs.end();
}

static std::string toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::tolower(c); });
    return str;
}

std::vector<OpcodeSearchResult> OpcodeSearcher::search(
    DebugSession& session,
    OpcodeSearchType type,
    const std::string& customQuery,
    Address startAddr,
    Address endAddr,
    size_t maxResults)
{
    std::vector<OpcodeSearchResult> results;
    auto regions = session.memoryRegions();

    csh cs_handle;
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &cs_handle) != CS_ERR_OK) {
        return results;
    }
    cs_option(cs_handle, CS_OPT_SYNTAX, CS_OPT_SYNTAX_INTEL);

    std::string lowerQuery = toLower(customQuery);

    for (const auto& reg : regions) {
        if (!reg.isExecutable()) continue;
        if (!startAddr.isNull() && reg.end <= startAddr) continue;
        if (!endAddr.isNull() && reg.start >= endAddr) continue;

        Address regionStart = reg.start;
        Address regionEnd = reg.end;
        if (!startAddr.isNull() && regionStart < startAddr) regionStart = startAddr;
        if (!endAddr.isNull() && regionEnd > endAddr) regionEnd = endAddr;

        if (regionEnd <= regionStart) continue;

        size_t totalBytes = static_cast<size_t>(regionEnd - regionStart);
        constexpr size_t kChunkSize = 64 * 1024; // 64 KB chunks

        for (size_t offset = 0; offset < totalBytes; offset += kChunkSize) {
            if (results.size() >= maxResults) break;

            size_t chunkSize = std::min(kChunkSize, totalBytes - offset);
            Address curAddr = regionStart + offset;
            auto mem = session.readMemory(curAddr, chunkSize);
            if (mem.empty()) continue;

            cs_insn* insn = nullptr;
            size_t count = cs_disasm(cs_handle, mem.data(), mem.size(), curAddr.value(), 0, &insn);
            if (count == 0) continue;

            for (size_t i = 0; i < count; ++i) {
                if (results.size() >= maxResults) break;

                std::string mnem = toLower(insn[i].mnemonic);
                std::string ops = toLower(insn[i].op_str);
                bool matched = false;

                switch (type) {
                    case OpcodeSearchType::JmpReg:
                        if (mnem == "jmp" && isGprRegister(ops)) {
                            matched = true;
                        }
                        break;
                    case OpcodeSearchType::CallReg:
                        if (mnem == "call" && isGprRegister(ops)) {
                            matched = true;
                        }
                        break;
                    case OpcodeSearchType::PushRegRet:
                        if (mnem == "push" && isGprRegister(ops) && (i + 1 < count)) {
                            std::string nextMnem = toLower(insn[i + 1].mnemonic);
                            if (nextMnem == "ret") {
                                matched = true;
                            }
                        }
                        break;
                    case OpcodeSearchType::PopRegRet:
                        if (mnem == "pop" && isGprRegister(ops) && (i + 1 < count)) {
                            std::string nextMnem = toLower(insn[i + 1].mnemonic);
                            if (nextMnem == "ret") {
                                matched = true;
                            }
                        }
                        break;
                    case OpcodeSearchType::InterruptOrSyscall:
                        if (mnem == "syscall" || mnem == "sysenter" || mnem == "int3" || mnem.rfind("int", 0) == 0) {
                            matched = true;
                        }
                        break;
                    case OpcodeSearchType::CustomInstruction:
                        if (!lowerQuery.empty()) {
                            std::string full = mnem + " " + ops;
                            if (full.find(lowerQuery) != std::string::npos) {
                                matched = true;
                            }
                        }
                        break;
                }

                if (matched) {
                    std::vector<uint8_t> bytes(insn[i].bytes, insn[i].bytes + insn[i].size);
                    results.push_back(OpcodeSearchResult{
                        .address = Address(insn[i].address),
                        .mnemonic = insn[i].mnemonic,
                        .operands = insn[i].op_str,
                        .bytes = std::move(bytes),
                        .moduleName = reg.pathname.empty() ? "[anonymous]" : reg.pathname
                    });
                }
            }

            cs_free(insn, count);
        }
    }

    cs_close(&cs_handle);
    return results;
}

} // namespace edb_next
