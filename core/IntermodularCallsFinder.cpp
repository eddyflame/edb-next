#include "IntermodularCallsFinder.hpp"
#include "CapstoneContext.hpp"
#include <capstone/capstone.h>
#include <sstream>
#include <algorithm>

namespace edb_next {

std::vector<IntermodularCall> IntermodularCallsFinder::findCalls(std::shared_ptr<DebugSession> session) {
    std::vector<IntermodularCall> results;
    if (!session || session->state() == SessionState::Stopped) {
        return results;
    }

    auto cs = CapstoneContext::acquire(true);
    if (!cs.isValid()) {
        return results;
    }
    csh handle = cs.get();

    auto regions = session->memoryRegions();
    const auto& elf = session->elfParser();

    // Identify main executable code regions
    for (const auto& reg : regions) {
        if (!reg.isExecutable()) continue;

        // Focus on main executable text region
        if (!reg.pathname.empty() && reg.pathname.find(".so") != std::string::npos) {
            continue; // Skip shared libraries to focus on application's intermodular calls
        }

        size_t scan_size = std::min(reg.size(), static_cast<uint64_t>(2 * 1024 * 1024));
        auto code_bytes = session->readMemory(reg.start, scan_size);
        if (code_bytes.empty()) continue;

        cs_insn* insn = nullptr;
        size_t count = cs_disasm(handle, code_bytes.data(), code_bytes.size(), reg.start.value(), 0, &insn);
        if (count == 0) continue;

        for (size_t i = 0; i < count; ++i) {
            const auto& cur = insn[i];
            std::string mnemonic = cur.mnemonic;

            if (mnemonic == "call" || mnemonic == "jmp") {
                const auto& detail = cur.detail->x86;
                if (detail.op_count > 0 && detail.operands[0].type == X86_OP_IMM) {
                    uint64_t target = detail.operands[0].imm;
                    Address target_addr(target);

                    auto sym = elf.findExactSymbol(target_addr);
                    if (!sym) {
                        auto nearest = elf.findNearestSymbol(target_addr);
                        if (nearest && nearest->second == 0) {
                            sym = nearest->first;
                        }
                    }

                    bool is_plt = false;
                    std::string api_name;
                    if (sym) {
                        api_name = sym->name;
                        if (api_name.find("@plt") != std::string::npos ||
                            api_name.find("@plt.sec") != std::string::npos) {
                            is_plt = true;
                        }
                    }

                    // Check if target is inside .plt section
                    for (const auto& sec : elf.sections()) {
                        if (sec.name == ".plt" || sec.name == ".plt.sec" || sec.name == ".plt.got") {
                            if (target_addr >= sec.address && target_addr < sec.address + sec.size) {
                                is_plt = true;
                                break;
                            }
                        }
                    }

                    if (is_plt || (!api_name.empty() && api_name.find("@plt") != std::string::npos)) {
                        std::string clean_api = api_name;
                        auto at_pos = clean_api.find('@');
                        if (at_pos != std::string::npos) {
                            clean_api = clean_api.substr(0, at_pos);
                        }

                        // Determine caller function
                        std::string caller = "unknown";
                        auto caller_sym = elf.findNearestSymbol(Address(cur.address));
                        if (caller_sym) {
                            caller = caller_sym->first.name;
                            if (caller_sym->second > 0) {
                                std::ostringstream oss;
                                oss << "+" << caller_sym->second;
                                caller += oss.str();
                            }
                        }

                        std::string instr_text = std::string(cur.mnemonic) + " " + cur.op_str;

                        results.push_back(IntermodularCall{
                            .callAddress = Address(cur.address),
                            .callerFunction = caller,
                            .targetAddress = target_addr,
                            .calleeApi = clean_api.empty() ? target_addr.toHex() : clean_api,
                            .library = "Shared Library",
                            .instruction = instr_text
                        });
                    }
                }
            }
        }

        cs_free(insn, count);
    }

    return results;
}

} // namespace edb_next
