#include "CallStackUnwinder.hpp"
#include "DebugSession.hpp"
#include <elfutils/libdwfl.h>
#include <sstream>
#include <iomanip>
#include <vector>
#include <unordered_set>

namespace edb_next {

namespace {

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

// DWARF CFI Unwinder State & Callbacks
struct DwflFrameData {
    Address pc{0};
    Address cfa{0};
    bool isActivation{false};
};

struct DwflUnwindContext {
    DebugSession* session{nullptr};
    pid_t tid{0};
    bool threadReported{false};
    std::vector<DwflFrameData> frames;
    size_t maxDepth{32};
};

static int nextThreadCb(Dwfl* /*dwfl*/, void* arg, void** thread_argp) {
    auto* ctx = static_cast<DwflUnwindContext*>(arg);
    if (!ctx->threadReported) {
        ctx->threadReported = true;
        *thread_argp = ctx;
        return ctx->tid > 0 ? ctx->tid : 1;
    }
    return 0;
}

static bool memoryReadCb(Dwfl* /*dwfl*/, Dwarf_Addr addr, Dwarf_Word* result, void* dwfl_arg) {
    auto* ctx = static_cast<DwflUnwindContext*>(dwfl_arg);
    if (!ctx || !ctx->session) return false;

    auto val = ctx->session->read<uint64_t>(Address(addr));
    if (val) {
        *result = *val;
        return true;
    }
    return false;
}

static bool setInitialRegistersCb(Dwfl_Thread* thread, void* thread_arg) {
    auto* ctx = static_cast<DwflUnwindContext*>(thread_arg);
    if (!ctx || !ctx->session) return false;

    const auto& regs = ctx->session->registers();

    // System V AMD64 ABI DWARF register numbers (0..15):
    // 0: RAX, 1: RDX, 2: RCX, 3: RBX, 4: RSI, 5: RDI, 6: RBP, 7: RSP,
    // 8: R8, 9: R9, 10: R10, 11: R11, 12: R12, 13: R13, 14: R14, 15: R15
    Dwarf_Word dwarf_regs[16];
    dwarf_regs[0] = regs.rax();
    dwarf_regs[1] = regs.rdx();
    dwarf_regs[2] = regs.rcx();
    dwarf_regs[3] = regs.rbx();
    dwarf_regs[4] = regs.rsi();
    dwarf_regs[5] = regs.rdi();
    dwarf_regs[6] = regs.rbp().value();
    dwarf_regs[7] = regs.rsp().value();
    dwarf_regs[8] = regs.r8();
    dwarf_regs[9] = regs.r9();
    dwarf_regs[10] = regs.r10();
    dwarf_regs[11] = regs.r11();
    dwarf_regs[12] = regs.r12();
    dwarf_regs[13] = regs.r13();
    dwarf_regs[14] = regs.r14();
    dwarf_regs[15] = regs.r15();

    if (!dwfl_thread_state_registers(thread, 0, 16, dwarf_regs)) {
        return false;
    }
    dwfl_thread_state_register_pc(thread, regs.rip().value());
    return true;
}

static const Dwfl_Thread_Callbacks kThreadCallbacks = {
    .next_thread = nextThreadCb,
    .get_thread = nullptr,
    .memory_read = memoryReadCb,
    .set_initial_registers = setInitialRegistersCb,
    .detach = nullptr,
    .thread_detach = nullptr
};

static const Dwfl_Callbacks kProcCallbacks = {
    .find_elf = dwfl_linux_proc_find_elf,
    .find_debuginfo = dwfl_standard_find_debuginfo,
    .section_address = dwfl_offline_section_address,
    .debuginfo_path = nullptr
};

std::vector<StackFrame> unwindViaDwarfCfi(
    DebugSession& session,
    const std::vector<MemoryRegion>& regions,
    size_t max_depth) {
    std::vector<StackFrame> frames;
    pid_t pid = session.pid();
    if (pid <= 0) return frames;

    Dwfl* dwfl = dwfl_begin(&kProcCallbacks);
    if (!dwfl) return frames;

    if (dwfl_linux_proc_report(dwfl, pid) != 0) {
        dwfl_end(dwfl);
        return frames;
    }
    dwfl_report_end(dwfl, nullptr, nullptr);

    DwflUnwindContext ctx{
        .session = &session,
        .tid = session.activeTid() > 0 ? static_cast<pid_t>(session.activeTid()) : pid,
        .threadReported = false,
        .frames = {},
        .maxDepth = max_depth
    };

    if (!dwfl_attach_state(dwfl, nullptr, pid, &kThreadCallbacks, &ctx)) {
        dwfl_end(dwfl);
        return frames;
    }

    auto threadCb = [](Dwfl_Thread* thread, void* arg) -> int {
        auto* c = static_cast<DwflUnwindContext*>(arg);
        auto frameCb = [](Dwfl_Frame* frame, void* fArg) -> int {
            auto* context = static_cast<DwflUnwindContext*>(fArg);
            Dwarf_Addr pc = 0;
            bool isactivation = false;
            if (dwfl_frame_pc(frame, &pc, &isactivation) == 0 && pc != 0) {
                context->frames.push_back(DwflFrameData{
                    .pc = Address(pc),
                    .cfa = Address(0),
                    .isActivation = isactivation
                });
            }
            if (context->frames.size() >= context->maxDepth) {
                return DWARF_CB_ABORT;
            }
            return DWARF_CB_OK;
        };
        dwfl_thread_getframes(thread, frameCb, c);
        return DWARF_CB_OK;
    };

    dwfl_getthreads(dwfl, threadCb, &ctx);
    dwfl_end(dwfl);

    if (ctx.frames.empty()) return frames;

    for (size_t idx = 0; idx < ctx.frames.size(); ++idx) {
        Address ip = ctx.frames[idx].pc;
        Address frameBase = (idx == 0) ? session.registers().rbp() : ctx.frames[idx].cfa;

        frames.push_back(StackFrame{
            .frameIndex = idx,
            .ip = ip,
            .frameBase = frameBase,
            .functionSymbol = formatSymbol(session.symbols().findNearestSymbol(ip)),
            .moduleName = findModuleName(ip, regions)
        });
    }

    return frames;
}

std::vector<StackFrame> unwindViaRbpChain(
    DebugSession& session,
    const std::vector<MemoryRegion>& regions,
    size_t max_depth) {
    std::vector<StackFrame> frames;
    Address rip = session.registers().rip();
    Address rbp = session.registers().rbp();

    if (rip.isNull()) return frames;

    frames.push_back(StackFrame{
        .frameIndex = 0,
        .ip = rip,
        .frameBase = rbp,
        .functionSymbol = formatSymbol(session.symbols().findNearestSymbol(rip)),
        .moduleName = findModuleName(rip, regions)
    });

    Address cur_rbp = rbp;
    size_t depth = 1;

    while (depth < max_depth && !cur_rbp.isNull()) {
        if (cur_rbp.value() % 8 != 0) break;

        struct StackFrameData {
            uint64_t savedRbp;
            uint64_t returnAddr;
        };
        auto frameData = session.read<StackFrameData>(cur_rbp);
        if (!frameData) break;

        Address saved_rbp(frameData->savedRbp);
        Address return_addr(frameData->returnAddr);

        if (return_addr.isNull()) break;

        if (saved_rbp <= cur_rbp && !saved_rbp.isNull()) {
            break;
        }

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

} // namespace

std::vector<StackFrame> CallStackUnwinder::unwind(DebugSession& session, size_t max_depth) {
    std::vector<StackFrame> frames;
    if (session.state() == SessionState::Stopped) return frames;

    auto regions = session.memoryRegions();
    Address rip = session.registers().rip();
    if (rip.isNull()) return frames;

    // 1. Primary: Try DWARF CFI unwinding via libdwfl (.eh_frame / .debug_frame state machine)
    auto cfiFrames = unwindViaDwarfCfi(session, regions, max_depth);
    if (cfiFrames.size() >= 2) {
        return cfiFrames;
    }

    // 2. Secondary: Fallback to RBP stack frame chain walking
    auto rbpFrames = unwindViaRbpChain(session, regions, max_depth);
    if (!cfiFrames.empty() && rbpFrames.size() <= cfiFrames.size()) {
        return cfiFrames;
    }
    if (!rbpFrames.empty()) {
        return rbpFrames;
    }

    // 3. Fallback: Frame 0 only
    frames.push_back(StackFrame{
        .frameIndex = 0,
        .ip = rip,
        .frameBase = session.registers().rbp(),
        .functionSymbol = formatSymbol(session.symbols().findNearestSymbol(rip)),
        .moduleName = findModuleName(rip, regions)
    });
    return frames;
}

} // namespace edb_next

