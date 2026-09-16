#include "DebugSession.hpp"
#include "ExpressionEvaluator.hpp"
#include "ConfigurationManager.hpp"
#include "LogManager.hpp"
#include <QCoreApplication>
#include <capstone/capstone.h>
#include <csignal>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <climits>
#include <unistd.h>

namespace edb_next {

DebugSession::DebugSession(std::string id, std::string name, QObject* parent)
    : QObject(parent),
      id_(std::move(id)),
      name_(std::move(name)),
      bpMgr_(
          [this](Address addr, void* buf, size_t sz) { return engine_.readMemory(addr, buf, sz); },
          [this](Address addr, const void* buf, size_t sz) { return engine_.writeMemory(addr, buf, sz); },
          [this](int slot, Address addr, HardwareBpType t, HardwareBpSize s) {
              return engine_.setHardwareBreakpoint(engine_.activeTid(), slot, addr, t, s);
          },
          [this](int slot) {
              return engine_.clearHardwareBreakpoint(engine_.activeTid(), slot);
          }
      ),
      eventLoop_(engine_, bpMgr_, nullptr)
{
    qRegisterMetaType<edb_next::SessionState>("edb_next::SessionState");
    connect(&eventLoop_, &EventLoopThread::eventReceived, this, &DebugSession::handleEvent);
}

DebugSession::~DebugSession() {
    disconnect();
    eventLoop_.stopLoop();
    terminate();
}

bool DebugSession::launch(const std::string& path, const std::vector<std::string>& args) {
    terminate();

    targetPath_ = path;
    targetArgs_ = args;

    bool disable_aslr = ConfigurationManager::instance().engine().disableASLR;
    auto res = engine_.launch(path, args, disable_aslr);
    if (!res) {
        std::cerr << "Session launch error: " << res.error << std::endl;
        return false;
    }

    // Determine binary base address from memory maps
    Address base_addr(0);
    auto regions = engine_.getMemoryRegions();
    std::string base_name = path;
    auto slash_pos = path.find_last_of('/');
    if (slash_pos != std::string::npos) {
        base_name = path.substr(slash_pos + 1);
    }
    for (const auto& r : regions) {
        if (!r.pathname.empty() && r.offset == 0) {
            if (r.pathname == path || r.pathname.find(base_name) != std::string::npos) {
                base_addr = r.start;
                break;
            }
        }
    }
    symbols_.loadBinary(path, base_addr);

    refreshRegisters();
    setState(SessionState::Paused);

    connect(&eventLoop_, &EventLoopThread::eventReceived, this, &DebugSession::handleEvent, Qt::UniqueConnection);
    eventLoop_.startLoop();

    DebugEvent initial_ev{
        .pid = engine_.pid(),
        .tid = engine_.mainTid(),
        .reason = StopReason::SingleStep,
        .address = currentRegs_.rip(),
        .message = "Process launched and suspended at entry"
    };
    Q_EMIT eventOccurred(initial_ev);
    return true;
}

bool DebugSession::attach(Pid pid) {
    terminate();

    auto res = engine_.attach(pid);
    if (!res) {
        std::cerr << "Session attach error: " << res.error << std::endl;
        return false;
    }

    // Determine binary path from /proc/<pid>/exe
    char exe_buf[PATH_MAX] = {0};
    ssize_t len = ::readlink(("/proc/" + std::to_string(pid) + "/exe").c_str(), exe_buf, sizeof(exe_buf) - 1);
    if (len > 0) {
        exe_buf[len] = '\0';
        std::string exe_path(exe_buf);
        Address base_addr(0);
        auto regions = engine_.getMemoryRegions();
        for (const auto& r : regions) {
            if (r.pathname == exe_path && r.offset == 0) {
                base_addr = r.start;
                break;
            }
        }
        symbols_.loadBinary(exe_path, base_addr);
    }

    refreshRegisters();
    setState(SessionState::Paused);

    connect(&eventLoop_, &EventLoopThread::eventReceived, this, &DebugSession::handleEvent, Qt::UniqueConnection);
    eventLoop_.startLoop();

    DebugEvent initial_ev{
        .pid = engine_.pid(),
        .tid = engine_.mainTid(),
        .reason = StopReason::Signal,
        .address = currentRegs_.rip(),
        .message = "Attached to process"
    };
    Q_EMIT eventOccurred(initial_ev);
    return true;
}

void DebugSession::terminate() {
    if (state_ == SessionState::Stopped) return;

    disconnect(&eventLoop_, &EventLoopThread::eventReceived, this, &DebugSession::handleEvent);
    engine_.kill();
    eventLoop_.stopLoop();
    bpMgr_.clear();
    symbols_.clear();

    setState(SessionState::Terminated);
    setState(SessionState::Stopped);
}

void DebugSession::detach() {
    eventLoop_.stopLoop();
    engine_.detach();
    bpMgr_.clear();
    symbols_.clear();
    setState(SessionState::Stopped);
}

void DebugSession::resume(bool passSignal) {
    if (state_ != SessionState::Paused) return;

    int sig = 0;
    if (passSignal && lastSignal_ != 0) {
        sig = lastSignal_;
        lastSignal_ = 0;
    }

    Address rip = currentRegs_.rip();
    if (bpMgr_.hasBreakpoint(rip)) {
        // Step-over breakpoint before running
        bpMgr_.prepareStepOver(rip);
        isStepOverBreak_ = true;
        engine_.singleStep(engine_.activeTid());
        setState(SessionState::Running);
        return;
    }

    engine_.continueExecution(engine_.activeTid(), sig);
    setState(SessionState::Running);
}

void DebugSession::stepInto(bool passSignal) {
    if (state_ != SessionState::Paused) return;

    int sig = 0;
    if (passSignal && lastSignal_ != 0) {
        sig = lastSignal_;
        lastSignal_ = 0;
    }

    Address rip = currentRegs_.rip();
    if (bpMgr_.hasBreakpoint(rip)) {
        bpMgr_.prepareStepOver(rip);
    }

    engine_.singleStep(engine_.activeTid(), sig);
    setState(SessionState::Running);
}

void DebugSession::stepOver(bool passSignal) {
    if (state_ != SessionState::Paused) return;

    // Check if current instruction is a CALL
    csh cs_handle;
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &cs_handle) == CS_ERR_OK) {
        uint8_t code[16] = {0};
        Address rip = currentRegs_.rip();
        if (engine_.readMemory(rip, code, sizeof(code))) {
            cs_insn* insn = nullptr;
            size_t count = cs_disasm(cs_handle, code, sizeof(code), rip.value(), 1, &insn);
            if (count > 0) {
                std::string mnemonic = insn[0].mnemonic;
                if (mnemonic == "call") {
                    Address next_addr = rip + insn[0].size;
                    // Temporary internal breakpoint on the instruction after CALL
                    bpMgr_.addBreakpoint(next_addr, true);
                    cs_free(insn, count);
                    cs_close(&cs_handle);
                    resume(passSignal);
                    return;
                }
                cs_free(insn, count);
            }
        }
        cs_close(&cs_handle);
    }

    stepInto(passSignal);
}

void DebugSession::stepOut() {
    if (state_ != SessionState::Paused) return;

    auto frames = callStack();
    if (frames.size() >= 2) {
        Address return_addr = frames[1].ip;
        bpMgr_.addBreakpoint(return_addr, true, "[StepOut]");
        tempRunToBp_ = return_addr;
        resume();
    } else {
        stepOver();
    }
}

void DebugSession::runUntilReturn() {
    stepOut();
}

void DebugSession::runTo(Address addr) {
    if (state_ != SessionState::Paused) return;
    if (addr == currentRegs_.rip()) return;

    bpMgr_.addBreakpoint(addr, true, "[RunToSelection]");
    tempRunToBp_ = addr;
    resume();
}

bool DebugSession::restart() {
    if (targetPath_.empty()) return false;
    return launch(targetPath_, targetArgs_);
}

void DebugSession::pause() {
    if (state_ == SessionState::Running) {
        engine_.pause(engine_.mainTid());
    }
}

bool DebugSession::toggleBreakpoint(Address addr) {
    std::string sym;
    if (auto s = symbols_.findNearestSymbol(addr)) {
        sym = s->first.name;
    }
    bool ret = bpMgr_.hasBreakpoint(addr) ? bpMgr_.removeBreakpoint(addr) : bpMgr_.addBreakpoint(addr, false, sym);
    Q_EMIT memoryUpdated();
    Q_EMIT breakpointsUpdated();
    return ret;
}

bool DebugSession::addBreakpoint(Address addr, const std::string& symbol) {
    std::string sym = symbol;
    if (sym.empty()) {
        if (auto s = symbols_.findNearestSymbol(addr)) {
            sym = s->first.name;
        }
    }
    bool ret = bpMgr_.addBreakpoint(addr, false, sym);
    if (ret) {
        Q_EMIT memoryUpdated();
        Q_EMIT breakpointsUpdated();
    }
    return ret;
}

bool DebugSession::addHardwareBreakpoint(Address addr, HardwareBpType type, HardwareBpSize size, const std::string& symbol) {
    std::string sym = symbol;
    if (sym.empty()) {
        if (auto s = symbols_.findNearestSymbol(addr)) {
            sym = s->first.name;
        }
    }
    bool ret = bpMgr_.addHardwareBreakpoint(addr, type, size, sym);
    if (ret) {
        Q_EMIT memoryUpdated();
        Q_EMIT breakpointsUpdated();
    }
    return ret;
}

bool DebugSession::removeBreakpoint(Address addr) {
    bool ret = bpMgr_.removeBreakpoint(addr);
    if (ret) {
        Q_EMIT memoryUpdated();
        Q_EMIT breakpointsUpdated();
    }
    return ret;
}

bool DebugSession::enableBreakpoint(Address addr) {
    bool ret = bpMgr_.enableBreakpoint(addr);
    if (ret) {
        Q_EMIT memoryUpdated();
        Q_EMIT breakpointsUpdated();
    }
    return ret;
}

bool DebugSession::disableBreakpoint(Address addr) {
    bool ret = bpMgr_.disableBreakpoint(addr);
    if (ret) {
        Q_EMIT memoryUpdated();
        Q_EMIT breakpointsUpdated();
    }
    return ret;
}

bool DebugSession::hasBreakpoint(Address addr) const {
    return bpMgr_.hasBreakpoint(addr);
}

std::vector<Breakpoint> DebugSession::breakpoints() const {
    return bpMgr_.allBreakpoints();
}

std::vector<StackFrame> DebugSession::callStack() {
    return CallStackUnwinder::unwind(*this, 32);
}

void DebugSession::setState(SessionState s) {
    if (state_ != s) {
        state_ = s;
        Q_EMIT stateChanged(state_);
    }
}

void DebugSession::refreshRegisters() {
    previousRegs_ = currentRegs_;
    previousFpRegs_ = currentFpRegs_;
    engine_.getRegisters(engine_.activeTid(), currentRegs_);
    engine_.getFpRegisters(engine_.activeTid(), currentFpRegs_);
    Q_EMIT registersUpdated();
}

void DebugSession::handleEvent(const DebugEvent& event) {
    if (event.reason == StopReason::ThreadCreated) {
        // Resume thread (clone event or initial SIGSTOP) from the TRACER thread!
        engine_.continueExecution(event.tid);
        return;
    }

    if (isStepOverBreak_) {
        // We just stepped over the original byte of the breakpoint
        bpMgr_.finishStepOver();
        isStepOverBreak_ = false;
        engine_.continueExecution(engine_.activeTid());
        return;
    }

    if (bpMgr_.isSteppingOver()) {
        bpMgr_.finishStepOver();
    }

    if (event.reason == StopReason::ProcessExit) {
        setState(SessionState::Terminated);
    } else {
        if (event.signal != 0 && event.signal != SIGTRAP && event.signal != SIGSTOP) {
            lastSignal_ = event.signal;
            const auto& policy = ConfigurationManager::instance().signalPolicy(event.signal);
            if (!policy.stopDebugger && policy.passToApp) {
                // Signal configured to be passed through without pausing debugger
                engine_.continueExecution(engine_.activeTid(), event.signal);
                lastSignal_ = 0;
                return;
            }
        }

        refreshRegisters();

        DebugEvent processed_event = event;
        if (event.signal == SIGTRAP) {
            Address bp_addr = currentRegs_.rip() - 1;
            if (bpMgr_.hasBreakpoint(bp_addr)) {
                // Rewind RIP by 1 on breakpoint hit
                currentRegs_.setRip(bp_addr);
                engine_.setRegisters(engine_.activeTid(), currentRegs_);
                processed_event.reason = StopReason::Breakpoint;
                processed_event.address = bp_addr;

                auto* bp = bpMgr_.getBreakpointMutable(bp_addr);
                if (bp) {
                    bp->hitCount++;

                    bool ignore = false;
                    if (bp->ignoreCount > 0 && bp->hitCount <= bp->ignoreCount) {
                        ignore = true;
                    }

                    if (!ignore && !bp->condition.empty()) {
                        if (!ExpressionEvaluator::evaluateCondition(bp->condition, currentRegs_, &engine_)) {
                            ignore = true;
                        }
                    }

                    if (ignore) {
                        bpMgr_.prepareStepOver(bp_addr);
                        isStepOverBreak_ = true;
                        engine_.singleStep(engine_.activeTid());
                        return;
                    }

                    if (bp->isLogOnly) {
                        std::string logMsg = ExpressionEvaluator::formatLog(
                            bp->logFormat.empty() ? "[Breakpoint Log] Hit at " + bp_addr.toHex() : bp->logFormat,
                            currentRegs_,
                            &engine_);
                        DebugEvent logEvt = processed_event;
                        logEvt.message = logMsg;
                        LogManager::instance().bp("Breakpoint Log", logMsg);
                        Q_EMIT eventOccurred(logEvt);

                        bpMgr_.prepareStepOver(bp_addr);
                        isStepOverBreak_ = true;
                        engine_.singleStep(engine_.activeTid());
                        return;
                    }
                }
            } else {
                processed_event.reason = StopReason::SingleStep;
                processed_event.address = currentRegs_.rip();
            }
        }

        if (tempRunToBp_.has_value()) {
            Address cur = currentRegs_.rip();
            if (cur == *tempRunToBp_ || processed_event.address == *tempRunToBp_) {
                bpMgr_.removeBreakpoint(*tempRunToBp_);
                tempRunToBp_.reset();
                Q_EMIT breakpointsUpdated();
            }
        }

        if (processed_event.reason == StopReason::Breakpoint) {
            LogManager::instance().bp("Breakpoint", "Hit at " + processed_event.address.toHex());
        } else if (processed_event.reason == StopReason::SingleStep) {
            LogManager::instance().trace("Step", "Stopped at " + processed_event.address.toHex());
        }

        setState(SessionState::Paused);
        Q_EMIT eventOccurred(processed_event);
        Q_EMIT memoryUpdated();
        return;
    }

    if (event.reason == StopReason::ProcessExit) {
        LogManager::instance().event("Process", "Process exited with code: " + std::to_string(event.exitCode));
    } else if (event.reason == StopReason::Signal) {
        LogManager::instance().event("Signal", "Received signal: " + std::to_string(event.signal));
    }

    Q_EMIT eventOccurred(event);
    Q_EMIT memoryUpdated();
}

bool DebugSession::setRegisters(const RegisterContext& regs) {
    if (!engine_.isAttached()) return false;
    bool ok = engine_.setRegisters(engine_.activeTid(), regs);
    if (ok) {
        refreshRegisters();
        Q_EMIT memoryUpdated();
    }
    return ok;
}

bool DebugSession::setFpRegisters(const user_fpregs_struct& fpregs) {
    if (!engine_.isAttached()) return false;
    bool ok = engine_.setFpRegisters(engine_.activeTid(), fpregs);
    if (ok) {
        refreshRegisters();
        Q_EMIT memoryUpdated();
    }
    return ok;
}

std::optional<Address> DebugSession::searchMemory(Address start, size_t max_bytes, const std::vector<uint8_t>& pattern) {
    if (pattern.empty() || max_bytes < pattern.size()) return std::nullopt;

    const size_t chunk_size = 4096;
    size_t scanned = 0;
    while (scanned < max_bytes) {
        size_t to_read = std::min(chunk_size + pattern.size(), max_bytes - scanned + pattern.size());
        auto chunk = readMemory(start + scanned, to_read);
        if (chunk.empty()) {
            scanned += chunk_size;
            continue;
        }

        auto it = std::search(chunk.begin(), chunk.end(), pattern.begin(), pattern.end());
        if (it != chunk.end()) {
            size_t offset = std::distance(chunk.begin(), it);
            return start + scanned + offset;
        }

        scanned += chunk_size;
    }
    return std::nullopt;
}

std::vector<DisassembledInstruction> DebugSession::disassemble(Address start_addr, size_t count) {
    std::vector<DisassembledInstruction> result;
    if (!engine_.isAttached()) return result;

    csh cs_handle;
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &cs_handle) != CS_ERR_OK) {
        return result;
    }

    size_t buffer_size = count * 15; // Max x86 instruction is 15 bytes
    std::vector<uint8_t> code(buffer_size, 0);

    if (!engine_.readMemory(start_addr, code.data(), buffer_size)) {
        cs_close(&cs_handle);
        return result;
    }

    // Restore any patched 0xCC bytes in the buffer so Capstone disassembles original opcodes
    for (size_t i = 0; i < buffer_size; ++i) {
        Address cur_addr = start_addr + i;
        if (const auto* bp = bpMgr_.getBreakpoint(cur_addr)) {
            if (bp->enabled) {
                code[i] = bp->originalByte;
            }
        }
    }

    cs_insn* insn = nullptr;
    size_t disasm_count = cs_disasm(cs_handle, code.data(), buffer_size, start_addr.value(), count, &insn);

    if (disasm_count > 0) {
        result.reserve(disasm_count);
        for (size_t i = 0; i < disasm_count; ++i) {
            Address addr(insn[i].address);
            std::vector<uint8_t> insn_bytes(insn[i].bytes, insn[i].bytes + insn[i].size);

            std::string sym_str;
            if (auto sym = symbols_.findNearestSymbol(addr)) {
                if (sym->second == 0) {
                    sym_str = "<" + sym->first.name + ">";
                } else {
                    std::ostringstream ss;
                    ss << "<" << sym->first.name << "+0x" << std::hex << sym->second << ">";
                    sym_str = ss.str();
                }
            }

            result.push_back(DisassembledInstruction{
                .address = addr,
                .mnemonic = insn[i].mnemonic,
                .operands = insn[i].op_str,
                .bytes = std::move(insn_bytes),
                .symbol = std::move(sym_str),
                .isCurrentRip = (addr == currentRegs_.rip()),
                .hasBreakpoint = bpMgr_.hasBreakpoint(addr)
            });
        }
        cs_free(insn, disasm_count);
    }

    cs_close(&cs_handle);
    return result;
}

std::vector<uint8_t> DebugSession::readMemory(Address addr, size_t size) {
    std::vector<uint8_t> buf(size, 0);
    if (engine_.readMemory(addr, buf.data(), size)) {
        return buf;
    }
    return {};
}

bool DebugSession::writeMemory(Address addr, const void* data, size_t size) {
    bool ok = engine_.writeMemory(addr, data, size);
    if (ok) {
        Q_EMIT memoryUpdated();
    }
    return ok;
}

std::vector<MemoryRegion> DebugSession::memoryRegions() const {
    return engine_.getMemoryRegions();
}

std::vector<ThreadInfo> DebugSession::getThreads() const {
    auto threads = engine_.getThreads();
    for (auto& t : threads) {
        if (!t.rip.isNull()) {
            if (auto sym = symbols_.findNearestSymbol(t.rip)) {
                t.symbol = sym->first.name;
                if (sym->second > 0) {
                    t.symbol += "+" + std::to_string(sym->second);
                }
            }
        }
    }
    return threads;
}

bool DebugSession::switchThread(Tid tid) {
    if (!engine_.isAttached()) return false;
    engine_.setActiveTid(tid);
    refreshRegisters();
    Q_EMIT activeThreadChanged(tid);
    return true;
}

bool DebugSession::setBreakpointCondition(Address addr, const std::string& cond) {
    bool ok = bpMgr_.setBreakpointCondition(addr, cond);
    if (ok) Q_EMIT breakpointsUpdated();
    return ok;
}

bool DebugSession::setBreakpointIgnoreCount(Address addr, uint32_t count) {
    bool ok = bpMgr_.setBreakpointIgnoreCount(addr, count);
    if (ok) Q_EMIT breakpointsUpdated();
    return ok;
}

bool DebugSession::setBreakpointLogOnly(Address addr, bool logOnly, const std::string& fmt) {
    bool ok = bpMgr_.setBreakpointLogOnly(addr, logOnly, fmt);
    if (ok) Q_EMIT breakpointsUpdated();
    return ok;
}

Result<std::vector<uint8_t>> DebugSession::assemble(const std::string& insn, Address origin) {
    return Assembler::assemble(insn, origin);
}

std::vector<ROPGadget> DebugSession::scanROP(size_t maxGadgetLength, size_t maxResults, const std::string& filter) {
    return ROPScanner::scan(engine_, maxGadgetLength, maxResults, filter);
}

InstructionDetails DebugSession::inspectInstruction(Address addr) {
    return InstructionInspector::inspect(addr, currentRegs_, engine_);
}

std::vector<CodeXRef> DebugSession::findCodeXRefs(Address targetAddr) {
    return CodeXRefFinder::findXRefsTo(targetAddr, engine_, &symbols_);
}

std::vector<Address> DebugSession::searchPattern(const std::string& pattern, bool execOnly) {
    return PatternSearcher::search(engine_, pattern, execOnly);
}

bool DebugSession::dumpMemoryToFile(Address start, size_t size, const std::string& filepath) {
    if (!engine_.isAttached() || size == 0) return false;
    auto data = readMemory(start, size);
    if (data.empty()) return false;
    std::ofstream out(filepath, std::ios::binary);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    bool ok = out.good();
    if (ok) {
        LogManager::instance().info("MemoryDump", "Dumped " + std::to_string(data.size()) + " bytes from " + start.toHex() + " to " + filepath);
    }
    return ok;
}

bool DebugSession::changeMemoryProtection(Address addr, size_t size, int prot) {
    if (!engine_.isAttached()) return false;
    eventLoop_.setSuspended(true);
    usleep(5000);
    bool ok = engine_.remoteMprotect(addr, size, prot);
    eventLoop_.setSuspended(false);
    if (ok) {
        LogManager::instance().info("Memory", "mprotect " + addr.toHex() + " (size " + std::to_string(size) + "): Success");
        Q_EMIT memoryUpdated();
    } else {
        LogManager::instance().error("Memory", "mprotect " + addr.toHex() + " (size " + std::to_string(size) + "): Failed");
    }
    return ok;
}

std::optional<Address> DebugSession::allocateMemory(size_t size, int prot) {
    if (!engine_.isAttached() || size == 0) return std::nullopt;
    eventLoop_.setSuspended(true);
    usleep(5000);
    auto res = engine_.remoteMmap(Address(0), size, prot, 0);
    eventLoop_.setSuspended(false);
    if (!res) {
        LogManager::instance().error("Memory", "mmap failed: " + res.error);
        return std::nullopt;
    }
    Address allocated = res.value;
    LogManager::instance().info("Memory", "Allocated " + std::to_string(size) + " bytes at " + allocated.toHex());
    Q_EMIT memoryUpdated();
    return allocated;
}

bool DebugSession::freeMemory(Address addr, size_t size) {
    if (!engine_.isAttached() || addr.isNull() || size == 0) return false;
    eventLoop_.setSuspended(true);
    usleep(5000);
    bool ok = engine_.remoteMunmap(addr, size);
    eventLoop_.setSuspended(false);
    if (ok) {
        LogManager::instance().info("Memory", "munmap " + addr.toHex() + " (size " + std::to_string(size) + "): Success");
        Q_EMIT memoryUpdated();
    } else {
        LogManager::instance().error("Memory", "munmap " + addr.toHex() + ": Failed");
    }
    return ok;
}

void DebugSession::setInstructionPointer(Address addr) {
    if (state_ != SessionState::Paused) return;
    auto regs = registers();
    regs.setRip(addr);
    setRegisters(regs);
    LogManager::instance().info("Execution", "Set RIP to " + addr.toHex());
}

DebugSession::AutoTraceResult DebugSession::autoTrace(bool stepOverTarget, size_t maxSteps, const std::string& stopCondition) {
    AutoTraceResult result;
    if (state_ != SessionState::Paused) {
        result.message = "Session must be paused to trace";
        return result;
    }

    for (size_t i = 0; i < maxSteps; ++i) {
        if (state_ != SessionState::Paused) {
            break;
        }

        // Check stop condition if provided
        if (!stopCondition.empty()) {
            auto condVal = ExpressionEvaluator::evaluate(stopCondition, currentRegs_, &engine_);
            if (condVal.has_value() && *condVal != 0) {
                result.stoppedByCondition = true;
                result.message = "Stopped by condition: " + stopCondition;
                break;
            }
        }

        // Step
        if (stepOverTarget) {
            stepOver();
        } else {
            stepInto();
        }
        result.stepsExecuted++;

        // Process Qt events so UI updates and events are dispatched
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);

        // Break if hit a user breakpoint
        if (bpMgr_.hasBreakpoint(currentRegs_.rip())) {
            result.hitBreakpoint = true;
            result.message = "Hit breakpoint at " + currentRegs_.rip().toHex();
            break;
        }
    }

    if (result.message.empty()) {
        result.message = "Completed " + std::to_string(result.stepsExecuted) + " trace steps";
    }
    return result;
}

} // namespace edb_next
