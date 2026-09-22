#include "CodeXRefFinder.hpp"
#include "LinuxDebugEngine.hpp"
#include "ElfParser.hpp"
#include "CapstoneContext.hpp"
#include <capstone/capstone.h>
#include <algorithm>

namespace edb_next {

std::vector<CodeXRef> CodeXRefFinder::findXRefsTo(
    Address targetAddr,
    LinuxDebugEngine& engine,
    const ElfParser* symbols) {

    std::vector<CodeXRef> results;
    if (!engine.isAttached() || targetAddr.isNull()) return results;

    auto cs = CapstoneContext::acquire(true);
    if (!cs.isValid()) {
        return results;
    }
    csh handle = cs.get();

    auto regions = engine.getMemoryRegions();
    for (const auto& region : regions) {
        if (!region.isExecutable() || !region.isReadable()) continue;

        size_t scanSize = std::min<size_t>(region.size(), 8 * 1024 * 1024);
        if (scanSize < 4) continue;

        std::vector<uint8_t> buffer(scanSize);
        if (!engine.readMemory(region.start, buffer.data(), scanSize)) {
            continue;
        }

        cs_insn* insn = nullptr;
        size_t count = cs_disasm(handle, buffer.data(), scanSize, region.start.value(), 0, &insn);
        if (count == 0 || !insn) continue;

        for (size_t i = 0; i < count; ++i) {
            const auto& cur = insn[i];
            const auto& detail = cur.detail->x86;
            Address curAddr(cur.address);

            std::string refType;
            bool matched = false;

            if (cur.id == X86_INS_CALL) {
                if (detail.op_count > 0 && detail.operands[0].type == X86_OP_IMM) {
                    if (static_cast<uint64_t>(detail.operands[0].imm) == targetAddr.value()) {
                        matched = true;
                        refType = "CALL";
                    }
                }
            } else if (cur.id == X86_INS_JMP) {
                if (detail.op_count > 0 && detail.operands[0].type == X86_OP_IMM) {
                    if (static_cast<uint64_t>(detail.operands[0].imm) == targetAddr.value()) {
                        matched = true;
                        refType = "JUMP";
                    }
                }
            } else if (cur.id >= X86_INS_JA && cur.id <= X86_INS_JS) { // conditional jumps range
                if (detail.op_count > 0 && detail.operands[0].type == X86_OP_IMM) {
                    if (static_cast<uint64_t>(detail.operands[0].imm) == targetAddr.value()) {
                        matched = true;
                        refType = "COND_JUMP";
                    }
                }
            } else if (cur.id == X86_INS_LEA) {
                for (int opIdx = 0; opIdx < detail.op_count; ++opIdx) {
                    const auto& op = detail.operands[opIdx];
                    if (op.type == X86_OP_MEM && op.mem.base == X86_REG_RIP) {
                        uint64_t resolved = cur.address + cur.size + op.mem.disp;
                        if (resolved == targetAddr.value()) {
                            matched = true;
                            refType = "DATA_REF";
                            break;
                        }
                    }
                }
            }

            if (matched) {
                std::string funcName;
                if (symbols) {
                    auto sym = symbols->findNearestSymbol(curAddr);
                    if (sym) {
                        funcName = sym->first.name;
                        if (sym->second > 0) {
                            funcName += "+" + std::to_string(sym->second);
                        }
                    }
                }

                results.push_back(CodeXRef{
                    .sourceAddress = curAddr,
                    .sourceFunction = funcName,
                    .mnemonic = cur.mnemonic,
                    .operands = cur.op_str,
                    .type = refType
                });
            }
        }

        cs_free(insn, count);
    }

    return results;
}

} // namespace edb_next
