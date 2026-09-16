#include "CallStackUnwinder.hpp"
#include "DebugSession.hpp"
#include <sstream>
#include <iomanip>

namespace edb_next {

static std::string formatSymbol(const std::optional<std::pair<SymbolInfo, uint64_t>>& sym_opt) {
    if (!sym_opt) return "[Unknown]";
    const auto& [info, offset] = *sym_opt;
    const std::string& sym_name = info.displayName();
    if (offset == 0) {
        return "<" + sym_name + ">";
    }
    std::ostringstream oss;
    oss << "<" << sym_name << "+0x" << std::hex << offset << ">";
    return oss.str();
}

static std::string findModuleName(Address addr, const std::vector<MemoryRegion>& regions) {
    for (const auto& r : regions) {
        if (r.contains(addr)) {
            if (!r.pathname.empty()) {
                auto slash = r.pathname.find_last_of('/');
                if (slash != std::string::npos) {
                    return r.pathname.substr(slash + 1);
                }
                return r.pathname;
            }
            return "[anon]";
        }
    }
    return "[unknown]";
}

std::vector<StackFrame> CallStackUnwinder::unwind(DebugSession& session, size_t max_depth) {
    std::vector<StackFrame> frames;
    if (session.state() == SessionState::Stopped) return frames;

    auto regions = session.memoryRegions();
    Address rip = session.registers().rip();
    Address rbp = session.registers().rbp();

    if (rip.isNull()) return frames;

    // Frame 0: Current execution point
    frames.push_back(StackFrame{
        .frameIndex = 0,
        .ip = rip,
        .frameBase = rbp,
        .functionSymbol = formatSymbol(session.symbols().findNearestSymbol(rip)),
        .moduleName = findModuleName(rip, regions)
    });

    // Walk RBP chain
    Address cur_rbp = rbp;
    size_t depth = 1;

    while (depth < max_depth && !cur_rbp.isNull()) {
        // Alignment check (x86_64 stack frame must be 8-byte aligned)
        if (cur_rbp.value() % 8 != 0) break;

        // Read saved RBP and return address: 16 bytes
        uint64_t buf[2] = {0, 0};
        if (!session.readMemory(cur_rbp, 16).empty()) {
            auto mem = session.readMemory(cur_rbp, 16);
            if (mem.size() != 16) break;
            std::memcpy(buf, mem.data(), 16);
        } else {
            break;
        }

        Address saved_rbp(buf[0]);
        Address return_addr(buf[1]);

        if (return_addr.isNull()) break;

        // Stack grows down, caller's RBP must be strictly higher in address
        if (saved_rbp <= cur_rbp && !saved_rbp.isNull()) {
            // Frame pointer omission or stack corruption
            break;
        }

        // Verify return_addr is in a readable/mapped memory region
        bool valid_return_addr = false;
        for (const auto& r : regions) {
            if (r.contains(return_addr) && r.isReadable()) {
                valid_return_addr = true;
                break;
            }
        }
        if (!valid_return_addr) break;

        frames.push_back(StackFrame{
            .frameIndex = depth,
            .ip = return_addr,
            .frameBase = saved_rbp,
            .functionSymbol = formatSymbol(session.symbols().findNearestSymbol(return_addr)),
            .moduleName = findModuleName(return_addr, regions)
        });

        if (saved_rbp.isNull() || saved_rbp.value() > 0x7fffffffffffULL) {
            break;
        }

        cur_rbp = saved_rbp;
        depth++;
    }

    return frames;
}

} // namespace edb_next
