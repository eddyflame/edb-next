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
#include <thread>
#include <chrono>
#include <unistd.h>

namespace edb_next {

DebugSession::DebugSession(std::string id, std::string name, QObject* parent)
    : QObject(parent),
      id_(std::move(id)),
      name_(std::move(name)),
      bpMgr_(
          [this](Address addr, void* buf, size_t sz) { return engine_.readMemory(addr, buf, sz); },
          [this](Address addr, const void* buf, size_t sz) { return engine_.writeMemory(addr, buf, sz); },
          // P1-A: write DR registers into active TID and propagate to all other alive threads
          [this](int slot, Address addr, HardwareBpType t, HardwareBpSize s) {
              bool activeOk = engine_.setHardwareBreakpoint(engine_.activeTid(), slot, addr, t, s);
              for (Tid tid : engine_.enumerateTids()) {
                  if (tid != engine_.activeTid()) {
                      engine_.setHardwareBreakpoint(tid, slot, addr, t, s);
                  }
              }
              return activeOk;
          },
          // P1-A: clear DR registers from active TID and propagate to all other alive threads
          [this](int slot) {
              bool activeOk = engine_.clearHardwareBreakpoint(engine_.activeTid(), slot);
              for (Tid tid : engine_.enumerateTids()) {
                  if (tid != engine_.activeTid()) {
                      engine_.clearHardwareBreakpoint(tid, slot);
                  }
              }
              return activeOk;
          }
      ),
      pageGuardMgr_(
          [this](Address addr, size_t sz, int prot) {
              return changeMemoryProtection(addr, sz, prot);
          }
      ),
      rendezvousMgr_(
          [this](Address addr, void* buf, size_t sz) { return engine_.readMemory(addr, buf, sz); }
      ),
      eventLoop_(engine_, bpMgr_, nullptr)
{
    qRegisterMetaType<edb_next::SessionState>("edb_next::SessionState");
    connect(&eventLoop_, &EventLoopThread::eventReceived, this, &DebugSession::handleEvent);
    scriptEngines_.setSession(this);

    stopOnLibraryEvents_ = ConfigurationManager::instance().engine().breakOnLibraryLoad;
    connect(&ConfigurationManager::instance(), &ConfigurationManager::configurationChanged, this, [this]() {
        stopOnLibraryEvents_ = ConfigurationManager::instance().engine().breakOnLibraryLoad;
    });
}

DebugSession::~DebugSession() {
    disconnect();
    eventLoop_.stopLoop();
    scriptEngines_.shutdown();
    terminate();
}

bool DebugSession::launch(const std::string& path, const std::vector<std::string>& args) {
    terminate();

    targetPath_ = path;
    targetArgs_ = args;

    bool disable_aslr = ConfigurationManager::instance().engine().disableASLR;
    bool disable_lazy_binding = ConfigurationManager::instance().engine().disableLazyBinding;
    auto res = engine_.launch(path, args, disable_aslr, disable_lazy_binding);
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
    dwarfParser_.load(path, base_addr);
    if (slash_pos != std::string::npos) {
        SourceFileManager::instance().addSearchPath(path.substr(0, slash_pos));
    }
    setupRendezvousHook(path, base_addr);

    refreshRegisters();
    setState(SessionState::Paused);

    connect(&eventLoop_, &EventLoopThread::eventReceived, this, &DebugSession::handleEvent, Qt::UniqueConnection);
    eventLoop_.startLoop();

    auto initBp = ConfigurationManager::instance().engine().initialBreakpoint;
    if (initBp == InitialBreakpoint::MainSymbol) {
        auto mainAddr = resolveSymbol("main");
        if (mainAddr) {
            addBreakpoint(*mainAddr, "main");
            resume(false);
            return true;
        }
    } else if (initBp == InitialBreakpoint::None) {
        resume(false);
        return true;
    }

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
        dwarfParser_.load(exe_path, base_addr);
        auto exe_slash = exe_path.find_last_of('/');
        if (exe_slash != std::string::npos) {
            SourceFileManager::instance().addSearchPath(exe_path.substr(0, exe_slash));
        }
        setupRendezvousHook(exe_path, base_addr);
    }

    refreshRegisters();
    setState(SessionState::Paused);
    stopOnLibraryEvents_ = ConfigurationManager::instance().engine().breakOnLibraryLoad;

    connect(&eventLoop_, &EventLoopThread::eventReceived, this, &DebugSession::handleEvent, Qt::UniqueConnection);
    eventLoop_.startLoop();

    DebugEvent initial_ev{
        .pid = engine_.pid(),
        .tid = engine_.mainTid(),
        .reason = StopReason::SingleStep,
        .address = currentRegs_.rip(),
        .message = "Attached to process " + std::to_string(pid)
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
    pageGuardMgr_.clear();
    pendingPageGuardRestoreAddr_ = Address(0);
    isPageGuardStepOver_ = false;
    isPageGuardResuming_ = false;
    rendezvousMgr_.clear();
    rendezvousBrkAddr_ = Address(0);
    symbols_.clear();
    dwarfParser_.clear();

    setState(SessionState::Terminated);
    setState(SessionState::Stopped);
}

void DebugSession::detach() {
    eventLoop_.stopLoop();
    engine_.detach();
    bpMgr_.clear();
    pageGuardMgr_.clear();
    pendingPageGuardRestoreAddr_ = Address(0);
    isPageGuardStepOver_ = false;
    isPageGuardResuming_ = false;
    rendezvousMgr_.clear();
    rendezvousBrkAddr_ = Address(0);
    symbols_.clear();
    dwarfParser_.clear();
    frozenThreads_.clear();
    setState(SessionState::Stopped);
}

void DebugSession::resume(bool passSignal) {
    if (state_ != SessionState::Paused) return;

    int sig = 0;
    if (passSignal && lastSignal_ != 0) {
        sig = lastSignal_;
        lastSignal_ = 0;
    }

    if (pendingPageGuardRestoreAddr_.value() != 0) {
        isPageGuardResuming_ = true;
        engine_.singleStep(engine_.activeTid());
        setState(SessionState::Running);
        return;
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

    auto allTids = engine_.enumerateTids();
    Tid act = engine_.activeTid();
    bool actResumed = false;
    for (Tid t : allTids) {
        if (isThreadFrozen(t)) {
            continue;
        }
        if (t == act) {
            engine_.continueExecution(t, sig);
            actResumed = true;
        } else {
            engine_.resumeThread(t, 0);
        }
    }
    if (!actResumed && !isThreadFrozen(act)) {
        engine_.continueExecution(act, sig);
    }
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
        sym = s->first.displayName();
    }
    bool ret = bpMgr_.hasBreakpoint(addr) ? bpMgr_.removeBreakpoint(addr) : bpMgr_.addBreakpoint(addr, false, sym);
    invalidateDisasmCache();   // P1-B: INT3 inserted/removed, opcodes changed in cache
    Q_EMIT memoryUpdated();
    Q_EMIT breakpointsUpdated();
    return ret;
}

bool DebugSession::addBreakpoint(Address addr, const std::string& symbol) {
    std::string sym = symbol;
    if (sym.empty()) {
        if (auto s = symbols_.findNearestSymbol(addr)) {
            sym = s->first.displayName();
        }
    }
    bool ret = bpMgr_.addBreakpoint(addr, false, sym);
    if (ret) {
        invalidateDisasmCache();   // P1-B
        Q_EMIT memoryUpdated();
        Q_EMIT breakpointsUpdated();
    }
    return ret;
}

bool DebugSession::addHardwareBreakpoint(Address addr, HardwareBpType type, HardwareBpSize size, const std::string& symbol) {
    std::string sym = symbol;
    if (sym.empty()) {
        if (auto s = symbols_.findNearestSymbol(addr)) {
            sym = s->first.displayName();
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
        invalidateDisasmCache();   // P1-B
        Q_EMIT memoryUpdated();
        Q_EMIT breakpointsUpdated();
    }
    return ret;
}

bool DebugSession::enableBreakpoint(Address addr) {
    bool ret = bpMgr_.enableBreakpoint(addr);
    if (ret) {
        invalidateDisasmCache();   // P1-B
        Q_EMIT memoryUpdated();
        Q_EMIT breakpointsUpdated();
    }
    return ret;
}

bool DebugSession::disableBreakpoint(Address addr) {
    bool ret = bpMgr_.disableBreakpoint(addr);
    if (ret) {
        invalidateDisasmCache();   // P1-B
        Q_EMIT memoryUpdated();
        Q_EMIT breakpointsUpdated();
    }
    return ret;
}

bool DebugSession::hasBreakpoint(Address addr) const {
    return bpMgr_.hasBreakpoint(addr);
}

bool DebugSession::isBreakpointEnabled(Address addr) const {
    const auto* bp = bpMgr_.getBreakpoint(addr);
    return bp != nullptr && bp->enabled;
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

    if (auto loc = dwarfParser_.findSourceLocation(currentRegs_.rip())) {
        Q_EMIT sourceLocationChanged(*loc);
    }
}

void DebugSession::handleEvent(const DebugEvent& event) {
    if (event.reason == StopReason::ProcessForked) {
        handleForkEvent(event);
        return;
    }

    if (event.reason == StopReason::ThreadCreated) {
        handleThreadCreatedEvent(event);
        return;
    }

    if (handleInternalStep(event)) {
        return;
    }

    restorePendingPageGuard();
    if (bpMgr_.isSteppingOver()) {
        bpMgr_.finishStepOver();
    }

    if (event.reason == StopReason::ProcessExit) {
        handleProcessExit(event);
        return;
    }

    if (event.signal == SIGSEGV && handlePageGuardFault(event)) {
        return;
    }

    if (handleSignalPolicy(event)) {
        return;
    }

    handleBreakpointOrTrap(event);
}

void DebugSession::handleForkEvent(const DebugEvent& event) {
    unsigned long child_msg = 0;
    engine_.getEventMessage(event.tid, &child_msg);
    Pid child_pid = static_cast<Pid>(child_msg);

    DebugEvent fork_ev = event;
    fork_ev.childPid = child_pid;

    std::string mode_str = followForkModeToString(followForkMode_);
    LogManager::instance().info("FollowFork",
        QString("[Fork] Process %1 forked child %2 (Follow mode: %3)")
            .arg(event.pid).arg(child_pid).arg(mode_str.c_str()).toStdString());

    if (followForkMode_ == FollowForkMode::Parent) {
        // Detach child immediately so child runs freely
        if (child_pid > 0) {
            engine_.detachProcess(child_pid);
        }
        if (stopOnForkEvents_) {
            setState(SessionState::Paused);
            refreshRegisters();
            Q_EMIT eventOccurred(fork_ev);
        } else {
            engine_.continueExecution(event.tid);
        }
        return;
    }

    if (followForkMode_ == FollowForkMode::Child) {
        // Detach parent, follow child
        engine_.detachProcess(event.pid);
        adoptChild(child_pid);
        if (stopOnForkEvents_) {
            setState(SessionState::Paused);
            Q_EMIT eventOccurred(fork_ev);
        } else {
            engine_.continueExecution(child_pid);
        }
        return;
    }

    if (followForkMode_ == FollowForkMode::Both) {
        // Notify multi-process manager to create a child session
        Q_EMIT childProcessForked(event.pid, child_pid);
        if (stopOnForkEvents_) {
            setState(SessionState::Paused);
            refreshRegisters();
            Q_EMIT eventOccurred(fork_ev);
        } else {
            engine_.continueExecution(event.tid);
        }
        return;
    }
}

void DebugSession::handleThreadCreatedEvent(const DebugEvent& event) {
    if (event.pid != engine_.pid()) {
        // Child process from fork that stopped on initial SIGSTOP
        if (followForkMode_ == FollowForkMode::Parent) {
            engine_.detachProcess(event.pid);
            return;
        }
    }
    if (isThreadFrozen(event.tid)) {
        return;
    }
    // P1-A: propagate all existing hardware breakpoints into the new thread's DR registers
    syncHardwareBreakpointsToAllThreads();
    // Resume thread (clone event or initial SIGSTOP) from the TRACER thread!
    engine_.continueExecution(event.tid);
}

void DebugSession::restorePendingPageGuard() {
    if (pendingPageGuardRestoreAddr_.value() != 0) {
        auto* g = pageGuardMgr_.getGuardMutable(pendingPageGuardRestoreAddr_);
        if (g) {
            pageGuardMgr_.reprotect(g);
        }
        pendingPageGuardRestoreAddr_ = Address(0);
    }
}

bool DebugSession::handleInternalStep(const DebugEvent& /*event*/) {
    if (isStepOverBreak_) {
        // We just stepped over the original byte of the breakpoint
        bpMgr_.finishStepOver();
        isStepOverBreak_ = false;
        if (!isThreadFrozen(engine_.activeTid())) {
            engine_.continueExecution(engine_.activeTid());
        }
        return true;
    }

    if (isPageGuardStepOver_) {
        // We just stepped over the instruction while page protection was temporarily lifted
        isPageGuardStepOver_ = false;
        restorePendingPageGuard();
        engine_.continueExecution(engine_.activeTid());
        return true;
    }

    if (isPageGuardResuming_) {
        // User clicked Continue while paused on a Page-Guard hit;
        // stepped 1 instruction, now reprotect and continue!
        isPageGuardResuming_ = false;
        restorePendingPageGuard();
        engine_.continueExecution(engine_.activeTid());
        return true;
    }

    return false;
}

void DebugSession::handleProcessExit(const DebugEvent& event) {
    setState(SessionState::Terminated);
    LogManager::instance().event("Process", "Process exited with code: " + std::to_string(event.exitCode));
    Q_EMIT eventOccurred(event);
    Q_EMIT memoryUpdated();
}

bool DebugSession::handlePageGuardFault(const DebugEvent& event) {
    siginfo_t siginfo{};
    if (!engine_.getSigInfo(event.tid, &siginfo)) {
        return false;
    }

    Address faultAddr(reinterpret_cast<uint64_t>(siginfo.si_addr));
    auto* guard = pageGuardMgr_.findGuardForFault(faultAddr);
    if (!guard || !guard->enabled) {
        return false;
    }

    // Page-Guard trap!
    pageGuardMgr_.temporarilyUnprotect(guard);
    pendingPageGuardRestoreAddr_ = guard->address;

    bool inRange = (faultAddr >= guard->address && faultAddr < guard->address + guard->size);
    refreshRegisters();
    if (!inRange && (currentRegs_.rip() >= guard->address && currentRegs_.rip() < guard->address + guard->size)) {
        inRange = true;
        faultAddr = currentRegs_.rip();
    }

    if (inRange) {
        guard->hitCount++;

        bool ignore = false;
        if (!guard->condition.empty()) {
            if (!ExpressionEvaluator::evaluateCondition(guard->condition, currentRegs_, &engine_)) {
                ignore = true;
            }
        }

        if (!ignore && !guard->scriptCode.empty()) {
            auto* eng = scriptEngines_.engine(guard->scriptLanguage);
            if (eng) {
                eng->setSession(this);
                bool shouldPause = eng->executeHook(guard->scriptCode);
                refreshRegisters();
                if (!shouldPause) {
                    ignore = true;
                }
            }
        }

        if (ignore) {
            isPageGuardStepOver_ = true;
            engine_.singleStep(engine_.activeTid());
            return true;
        }

        DebugEvent processed_event = event;
        processed_event.reason = StopReason::Breakpoint;
        processed_event.address = faultAddr;
        processed_event.message = "Page-Guard Breakpoint Hit at " + faultAddr.toHex() +
                                  " (Page " + guard->pageBase.toHex() + ", " +
                                  pageGuardAccessToString(guard->access) + ")";
        LogManager::instance().bp("PageGuard", processed_event.message);

        setState(SessionState::Paused);
        Q_EMIT eventOccurred(processed_event);
        Q_EMIT memoryUpdated();
        return true;
    } else {
        // False-positive page touch (another address on same page)
        isPageGuardStepOver_ = true;
        engine_.singleStep(engine_.activeTid());
        return true;
    }
}

bool DebugSession::handleSignalPolicy(const DebugEvent& event) {
    if (event.signal != 0 && event.signal != SIGTRAP && event.signal != SIGSTOP) {
        lastSignal_ = event.signal;
        const auto& policy = ConfigurationManager::instance().signalPolicy(event.signal);
        if (!policy.stopDebugger && policy.passToApp) {
            // Signal configured to be passed through without pausing debugger
            engine_.continueExecution(engine_.activeTid(), event.signal);
            lastSignal_ = 0;
            return true;
        }
    }
    return false;
}

void DebugSession::handleBreakpointOrTrap(const DebugEvent& event) {
    refreshRegisters();

    DebugEvent processed_event = event;
    if (event.signal == SIGTRAP) {
        Address bp_addr = currentRegs_.rip() - 1;

        if (rendezvousBrkAddr_.value() != 0 && bp_addr == rendezvousBrkAddr_) {
            currentRegs_.setRip(bp_addr);
            engine_.setRegisters(engine_.activeTid(), currentRegs_);

            rendezvousMgr_.updateDebugState();
            auto linkState = rendezvousMgr_.currentState();

            if (linkState == LinkerState::Consistent) {
                auto diff = rendezvousMgr_.detectChanges();

                for (const auto& lib : diff.added) {
                    symbols_.addSharedLibrary(lib.path, lib.baseAddress);
                    dwarfParser_.addModule(lib.path, lib.baseAddress);
                    LogManager::instance().info("DynamicLinker", "[Library Loaded] " + lib.name + " (" + lib.path + ") at " + lib.baseAddress.toHex());
                    Q_EMIT libraryLoaded(QString::fromStdString(lib.name), QString::fromStdString(lib.path), lib.baseAddress);
                }

                for (const auto& lib : diff.removed) {
                    LogManager::instance().info("DynamicLinker", "[Library Unloaded] " + lib.name);
                    Q_EMIT libraryUnloaded(QString::fromStdString(lib.name));
                }

                checkAndResolvePendingBreakpoints();
                Q_EMIT memoryUpdated();

                if (stopOnLibraryEvents_ && (!diff.added.empty() || !diff.removed.empty())) {
                    processed_event.reason = StopReason::Breakpoint;
                    processed_event.address = bp_addr;
                    if (!diff.added.empty()) {
                        processed_event.message = "[Library Event] Loaded: " + diff.added.front().name;
                    } else {
                        processed_event.message = "[Library Event] Unloaded: " + diff.removed.front().name;
                    }
                    setState(SessionState::Paused);
                    Q_EMIT eventOccurred(processed_event);
                    return;
                }
            }

            bpMgr_.prepareStepOver(bp_addr);
            isStepOverBreak_ = true;
            engine_.singleStep(engine_.activeTid());
            return;
        }

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

                if (!ignore && !bp->scriptCode.empty()) {
                    auto* eng = scriptEngines_.engine(bp->scriptLanguage);
                    if (eng) {
                        eng->setSession(this);
                        bool shouldPause = eng->executeHook(bp->scriptCode);
                        refreshRegisters();
                        if (!shouldPause) {
                            ignore = true;
                        }
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
    } else if (processed_event.reason == StopReason::Signal) {
        LogManager::instance().event("Signal", "Received signal: " + std::to_string(processed_event.signal));
    }

    setState(SessionState::Paused);
    Q_EMIT eventOccurred(processed_event);
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
        size_t remaining = max_bytes - scanned;
        size_t to_read = std::min(chunk_size + pattern.size() - 1, remaining);
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

// P1-B: Full Capstone decode — cache-miss path. Renamed from disassemble().
std::vector<DisassembledInstruction> DebugSession::disassembleFull(Address start_addr, size_t count) {
    std::vector<DisassembledInstruction> result;
    if (!engine_.isAttached()) return result;

    csh cs_handle;
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &cs_handle) != CS_ERR_OK) {
        return result;
    }

    if (ConfigurationManager::instance().disasm().syntax == DisassemblySyntax::ATT) {
        cs_option(cs_handle, CS_OPT_SYNTAX, CS_OPT_SYNTAX_ATT);
    } else {
        cs_option(cs_handle, CS_OPT_SYNTAX, CS_OPT_SYNTAX_INTEL);
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
        std::string prev_file;
        int prev_line = -1;

        for (size_t i = 0; i < disasm_count; ++i) {
            Address addr(insn[i].address);
            std::vector<uint8_t> insn_bytes(insn[i].bytes, insn[i].bytes + insn[i].size);

            std::string sym_str;
            if (auto sym = symbols_.findNearestSymbol(addr)) {
                const std::string& name = sym->first.displayName();
                if (sym->second == 0) {
                    sym_str = "<" + name + ">";
                } else {
                    std::ostringstream ss;
                    ss << "<" << name << "+0x" << std::hex << sym->second << ">";
                    sym_str = ss.str();
                }
            }

            std::string src_file;
            std::string src_full;
            int src_line = 0;
            std::string src_text;
            bool is_line_start = false;

            if (auto loc = dwarfParser_.findSourceLocation(addr)) {
                src_file = loc->fileName;
                src_full = loc->filePath;
                src_line = loc->line;
                src_text = SourceFileManager::instance().getLineText(loc->filePath, loc->line);
                if (src_file != prev_file || src_line != prev_line) {
                    is_line_start = true;
                    prev_file = src_file;
                    prev_line = src_line;
                }
            }

            std::string mnem_str = insn[i].mnemonic;
            if (ConfigurationManager::instance().disasm().uppercaseMnemonics) {
                for (char& c : mnem_str) {
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                }
            }

            bool has_bp = bpMgr_.hasBreakpoint(addr);
            bool is_bp_enabled = true;
            if (has_bp) {
                const auto* bp = bpMgr_.getBreakpoint(addr);
                is_bp_enabled = (bp != nullptr && bp->enabled);
            }

            result.push_back(DisassembledInstruction{
                .address = addr,
                .mnemonic = std::move(mnem_str),
                .operands = insn[i].op_str,
                .bytes = std::move(insn_bytes),
                .symbol = std::move(sym_str),
                .isCurrentRip = (addr == currentRegs_.rip()),
                .hasBreakpoint = has_bp,
                .isBreakpointEnabled = is_bp_enabled,
                .sourceFile = std::move(src_file),
                .sourceFullPath = std::move(src_full),
                .sourceLine = src_line,
                .sourceText = std::move(src_text),
                .isSourceLineStart = is_line_start
            });
        }
        cs_free(insn, disasm_count);
    }

    cs_close(&cs_handle);
    return result;
}

// P1-B: Public disassemble() — cache-hit fast path then falls back to disassembleFull().
std::vector<DisassembledInstruction> DebugSession::disassemble(Address start_addr, size_t count) {
    if (disasmCache_.isValid(start_addr, count)) {
        // Cache hit: only refresh the two fields that change on every step
        for (auto& insn : disasmCache_.insns) {
            insn.isCurrentRip = (insn.address == currentRegs_.rip());
            const auto* bp = bpMgr_.getBreakpoint(insn.address);
            insn.hasBreakpoint     = (bp != nullptr);
            insn.isBreakpointEnabled = (bp != nullptr && bp->enabled);
        }
        return disasmCache_.insns;
    }

    // Cache miss: full decode, then populate cache
    auto result = disassembleFull(start_addr, count);
    disasmCache_.baseAddr        = start_addr;
    disasmCache_.requestedCount  = count;
    disasmCache_.insns           = result;
    disasmCache_.version         = 1;   // mark valid
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
        invalidateDisasmCache();   // P1-B: memory changed, opcodes may differ
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
        t.isFrozen = isThreadFrozen(t.tid);
        if (!t.rip.isNull()) {
            if (auto sym = symbols_.findNearestSymbol(t.rip)) {
                t.symbol = sym->first.displayName();
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

bool DebugSession::freezeThread(Tid tid) {
    if (!engine_.isAttached() || tid <= 0) return false;
    frozenThreads_.insert(tid);
    if (state_ == SessionState::Running) {
        engine_.pauseThread(tid);
    }
    LogManager::instance().info("Session", "Thread " + std::to_string(tid) + " frozen");
    Q_EMIT threadFreezeStateChanged(tid, true);
    return true;
}

bool DebugSession::thawThread(Tid tid) {
    if (!engine_.isAttached() || tid <= 0) return false;
    auto it = frozenThreads_.find(tid);
    if (it == frozenThreads_.end()) return false;
    frozenThreads_.erase(it);
    if (state_ == SessionState::Running) {
        engine_.resumeThread(tid, 0);
    }
    LogManager::instance().info("Session", "Thread " + std::to_string(tid) + " thawed");
    Q_EMIT threadFreezeStateChanged(tid, false);
    return true;
}

bool DebugSession::freezeAllOtherThreads() {
    if (!engine_.isAttached()) return false;
    Tid cur = activeTid();
    auto allTids = engine_.enumerateTids();
    for (Tid t : allTids) {
        if (t != cur) {
            freezeThread(t);
        }
    }
    return true;
}

bool DebugSession::thawAllThreads() {
    if (!engine_.isAttached()) return false;
    auto toThaw = frozenThreads_;
    for (Tid t : toThaw) {
        thawThread(t);
    }
    return true;
}

bool DebugSession::isThreadFrozen(Tid tid) const {
    return frozenThreads_.find(tid) != frozenThreads_.end();
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

bool DebugSession::setBreakpointScript(Address addr, const std::string& code, const std::string& language) {
    bool ok = bpMgr_.setBreakpointScript(addr, code, language);
    if (ok) Q_EMIT breakpointsUpdated();
    return ok;
}

bool DebugSession::addPageGuard(Address addr, size_t size, PageGuardAccess access, const std::string& comment) {
    if (!engine_.isAttached()) return false;
    if (size == 0) size = 1;

    // Determine original protection of the memory page
    int origProt = PROT_READ | PROT_WRITE;
    auto regions = engine_.getMemoryRegions();
    for (const auto& reg : regions) {
        if (addr >= reg.start && addr < reg.end) {
            origProt = 0;
            if (reg.permissions.find('r') != std::string::npos) origProt |= PROT_READ;
            if (reg.permissions.find('w') != std::string::npos) origProt |= PROT_WRITE;
            if (reg.permissions.find('x') != std::string::npos) origProt |= PROT_EXEC;
            break;
        }
    }

    bool ok = pageGuardMgr_.addGuard(addr, size, access, origProt, comment);
    if (ok) {
        LogManager::instance().bp("PageGuard", "Added Page-Guard at " + addr.toHex() +
            " (size: " + std::to_string(size) + ", type: " + pageGuardAccessToString(access) + ")");
        Q_EMIT pageGuardsUpdated();
        Q_EMIT memoryUpdated();
    } else {
        LogManager::instance().error("PageGuard", "Failed to add Page-Guard at " + addr.toHex());
    }
    return ok;
}

bool DebugSession::removePageGuard(Address addr) {
    if (!pageGuardMgr_.hasGuard(addr)) return false;
    bool ok = pageGuardMgr_.removeGuard(addr);
    if (ok) {
        if (pendingPageGuardRestoreAddr_ == addr) {
            pendingPageGuardRestoreAddr_ = Address(0);
        }
        LogManager::instance().bp("PageGuard", "Removed Page-Guard at " + addr.toHex());
        Q_EMIT pageGuardsUpdated();
        Q_EMIT memoryUpdated();
    }
    return ok;
}

bool DebugSession::enablePageGuard(Address addr) {
    bool ok = pageGuardMgr_.enableGuard(addr);
    if (ok) {
        Q_EMIT pageGuardsUpdated();
        Q_EMIT memoryUpdated();
    }
    return ok;
}

bool DebugSession::disablePageGuard(Address addr) {
    bool ok = pageGuardMgr_.disableGuard(addr);
    if (ok) {
        Q_EMIT pageGuardsUpdated();
        Q_EMIT memoryUpdated();
    }
    return ok;
}

bool DebugSession::togglePageGuard(Address addr) {
    bool ok = pageGuardMgr_.toggleGuard(addr);
    if (ok) {
        Q_EMIT pageGuardsUpdated();
        Q_EMIT memoryUpdated();
    }
    return ok;
}

bool DebugSession::hasPageGuard(Address addr) const {
    return pageGuardMgr_.hasGuard(addr);
}

bool DebugSession::isPageGuarded(Address addr) const {
    return pageGuardMgr_.isPageGuarded(addr);
}

bool DebugSession::isAddressPageWatched(Address addr) const {
    return pageGuardMgr_.isAddressWatched(addr);
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

std::optional<SourceLocation> DebugSession::currentSourceLocation() const {
    if (state_ != SessionState::Paused || currentRegs_.rip().isNull()) {
        return std::nullopt;
    }
    return dwarfParser_.findSourceLocation(currentRegs_.rip());
}

std::optional<SourceLocation> DebugSession::resolveSourceLocation(Address addr) const {
    return dwarfParser_.findSourceLocation(addr);
}

std::optional<Address> DebugSession::resolveSourceLine(const std::string& file, int line) const {
    return dwarfParser_.findAddressByLine(file, line);
}

bool DebugSession::toggleSourceBreakpoint(const std::string& file, int line) {
    auto addr = resolveSourceLine(file, line);
    if (!addr) return false;
    if (hasBreakpoint(*addr)) {
        return removeBreakpoint(*addr);
    } else {
        return addBreakpoint(*addr);
    }
}

bool DebugSession::hasSourceBreakpoint(const std::string& file, int line) const {
    auto addr = resolveSourceLine(file, line);
    if (!addr) return false;
    return hasBreakpoint(*addr);
}

bool DebugSession::stepSourceOver(int maxInsnSteps) {
    if (state_ != SessionState::Paused) return false;
    auto curLoc = currentSourceLocation();
    if (!curLoc || curLoc->line <= 0) {
        stepOver();
        return true;
    }
    std::string origFile = curLoc->fileName;
    int origLine = curLoc->line;
    for (int i = 0; i < maxInsnSteps; ++i) {
        stepOver();
        int waitMs = 0;
        while (state_ != SessionState::Paused && waitMs < 2000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            waitMs += 2;
        }
        if (state_ != SessionState::Paused) break;
        auto nextLoc = currentSourceLocation();
        if (!nextLoc) break;
        if (nextLoc->fileName != origFile || nextLoc->line != origLine) {
            break;
        }
    }
    return true;
}

bool DebugSession::stepSourceInto(int maxInsnSteps) {
    if (state_ != SessionState::Paused) return false;
    auto curLoc = currentSourceLocation();
    if (!curLoc || curLoc->line <= 0) {
        stepInto();
        return true;
    }
    std::string origFile = curLoc->fileName;
    int origLine = curLoc->line;
    for (int i = 0; i < maxInsnSteps; ++i) {
        stepInto();
        int waitMs = 0;
        while (state_ != SessionState::Paused && waitMs < 2000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            waitMs += 2;
        }
        if (state_ != SessionState::Paused) break;
        auto nextLoc = currentSourceLocation();
        if (!nextLoc) break;
        if (nextLoc->fileName != origFile || nextLoc->line != origLine) {
            break;
        }
    }
    return true;
}

void DebugSession::setupRendezvousHook(const std::string& targetPath, Address baseAddr) {
    rendezvousMgr_.initialize(engine_.pid(), baseAddr, targetPath);
    rendezvousBrkAddr_ = rendezvousMgr_.rBrkAddr();
    if (rendezvousBrkAddr_.value() != 0) {
        bpMgr_.addBreakpoint(rendezvousBrkAddr_, true, "_dl_debug_state");
    }

    // Pre-load existing shared libraries
    auto diff = rendezvousMgr_.detectChanges();
    for (const auto& lib : diff.added) {
        symbols_.addSharedLibrary(lib.path, lib.baseAddress);
        dwarfParser_.addModule(lib.path, lib.baseAddress);
        LogManager::instance().info("DynamicLinker", "[Library Mapped] " + lib.name + " (" + lib.path + ") at " + lib.baseAddress.toHex());
    }
    checkAndResolvePendingBreakpoints();
}

void DebugSession::checkAndResolvePendingBreakpoints() {
    auto& pending = bpMgr_.allPendingBreakpointsMutable();
    if (pending.empty()) return;

    std::vector<PendingBreakpoint> remaining;
    for (const auto& pb : pending) {
        auto addr = symbols_.findSymbolAddress(pb.symbol);
        if (addr.has_value() && addr->value() != 0) {
            bool ok = bpMgr_.addBreakpoint(*addr, false, pb.symbol);
            if (ok) {
                if (!pb.condition.empty()) bpMgr_.setBreakpointCondition(*addr, pb.condition);
                if (!pb.scriptCode.empty()) bpMgr_.setBreakpointScript(*addr, pb.scriptCode, pb.scriptLanguage);
                if (pb.isLogOnly) bpMgr_.setBreakpointLogOnly(*addr, true, pb.logFormat);
                LogManager::instance().bp("PendingBreakpoint", "[Pending Breakpoint Bound] Symbol '" + pb.symbol + "' -> " + addr->toHex());
                Q_EMIT breakpointsUpdated();
            } else {
                remaining.push_back(pb);
            }
        } else {
            remaining.push_back(pb);
        }
    }
    pending = std::move(remaining);
}

std::vector<SharedLibraryInfo> DebugSession::loadedLibraries() const {
    return rendezvousMgr_.loadedLibraries();
}

bool DebugSession::addPendingBreakpoint(const std::string& symbol, const std::string& condition,
                                        const std::string& scriptCode, const std::string& scriptLang,
                                        bool isLogOnly, const std::string& logFormat) {
    auto addr = symbols_.findSymbolAddress(symbol);
    if (addr.has_value() && addr->value() != 0) {
        bool ok = bpMgr_.addBreakpoint(*addr, false, symbol);
        if (ok) {
            if (!condition.empty()) bpMgr_.setBreakpointCondition(*addr, condition);
            if (!scriptCode.empty()) bpMgr_.setBreakpointScript(*addr, scriptCode, scriptLang);
            if (isLogOnly) bpMgr_.setBreakpointLogOnly(*addr, true, logFormat);
            Q_EMIT breakpointsUpdated();
        }
        return ok;
    }

    bool ok = bpMgr_.addPendingBreakpoint(symbol, condition, scriptCode, scriptLang, isLogOnly, logFormat);
    if (ok) {
        LogManager::instance().bp("PendingBreakpoint", "[Pending Breakpoint Added] Symbol '" + symbol + "' (waiting for module load)");
        Q_EMIT breakpointsUpdated();
    }
    return ok;
}

bool DebugSession::removePendingBreakpoint(const std::string& symbol) {
    bool ok = bpMgr_.removePendingBreakpoint(symbol);
    if (ok) {
        Q_EMIT breakpointsUpdated();
    }
    return ok;
}

const std::vector<PendingBreakpoint>& DebugSession::pendingBreakpoints() const noexcept {
    return bpMgr_.allPendingBreakpoints();
}

bool DebugSession::adoptChild(Pid child_pid) {
    if (child_pid <= 0) return false;
    eventLoop_.stopLoop();
    engine_.adoptProcess(child_pid);

    pageGuardMgr_.clear();
    pendingPageGuardRestoreAddr_ = Address(0);
    isPageGuardStepOver_ = false;
    isPageGuardResuming_ = false;

    // Reset rendezvous for child
    rendezvousMgr_.clear();
    rendezvousBrkAddr_ = Address(0);
    if (!targetPath_.empty()) {
        Address baseAddr(0);
        auto regions = engine_.getMemoryRegions();
        for (const auto& r : regions) {
            if (r.pathname.find(targetPath_) != std::string::npos && r.isExecutable()) {
                baseAddr = r.start;
                break;
            }
        }
        setupRendezvousHook(targetPath_, baseAddr);
    }

    eventLoop_.startLoop();
    refreshRegisters();

    Q_EMIT memoryUpdated();
    Q_EMIT breakpointsUpdated();
    return true;
}

bool DebugSession::initAsChild(std::shared_ptr<DebugSession> parent, Pid child_pid) {
    if (!parent || child_pid <= 0) return false;
    targetPath_ = parent->targetPath();
    targetArgs_ = parent->targetArgs();
    followForkMode_ = parent->followForkMode();
    stopOnForkEvents_ = parent->stopOnForkEvents();

    engine_.adoptProcess(child_pid);

    Address base_addr(0);
    auto regions = engine_.getMemoryRegions();
    std::string base_name = targetPath_;
    auto slash_pos = targetPath_.find_last_of('/');
    if (slash_pos != std::string::npos) {
        base_name = targetPath_.substr(slash_pos + 1);
    }
    for (const auto& r : regions) {
        if (!r.pathname.empty() && r.offset == 0) {
            if (r.pathname == targetPath_ || r.pathname.find(base_name) != std::string::npos) {
                base_addr = r.start;
                break;
            }
        }
    }

    if (!targetPath_.empty()) {
        symbols_.loadBinary(targetPath_, base_addr);
        dwarfParser_.load(targetPath_, base_addr);
        setupRendezvousHook(targetPath_, base_addr);
    }

    // Clone breakpoints from parent
    for (const auto& bp : parent->breakpoints()) {
        if (bp.isInternal) continue;
        if (bp.type == BreakpointType::HardwareExecute) {
            bpMgr_.addHardwareBreakpoint(bp.address, HardwareBpType::Execute, HardwareBpSize::Byte1, bp.symbol);
        } else if (bp.type == BreakpointType::HardwareWrite) {
            bpMgr_.addHardwareBreakpoint(bp.address, HardwareBpType::Write, HardwareBpSize::Byte1, bp.symbol);
        } else if (bp.type == BreakpointType::HardwareReadWrite) {
            bpMgr_.addHardwareBreakpoint(bp.address, HardwareBpType::ReadWrite, HardwareBpSize::Byte1, bp.symbol);
        } else {
            bpMgr_.addBreakpoint(bp.address, false, bp.symbol);
        }
        if (!bp.condition.empty()) bpMgr_.setBreakpointCondition(bp.address, bp.condition);
        if (bp.ignoreCount > 0) bpMgr_.setBreakpointIgnoreCount(bp.address, bp.ignoreCount);
        if (bp.isLogOnly) bpMgr_.setBreakpointLogOnly(bp.address, true, bp.logFormat);
        if (!bp.scriptCode.empty()) bpMgr_.setBreakpointScript(bp.address, bp.scriptCode, bp.scriptLanguage);
        if (!bp.enabled) bpMgr_.disableBreakpoint(bp.address);
    }

    // Clone pending breakpoints
    for (const auto& pbp : parent->pendingBreakpoints()) {
        bpMgr_.addPendingBreakpoint(pbp.symbol, pbp.condition, pbp.scriptCode, pbp.scriptLanguage, pbp.isLogOnly, pbp.logFormat);
    }

    connect(&eventLoop_, &EventLoopThread::eventReceived, this, &DebugSession::handleEvent);
    eventLoop_.startLoop();

    refreshRegisters();
    setState(SessionState::Paused);

    Q_EMIT memoryUpdated();
    Q_EMIT breakpointsUpdated();
    return true;
}

// P1-A: Replay all currently-active hardware breakpoints into every alive thread's DR registers.
// Called when a new thread is created so it inherits the parent thread's hardware BPs.
void DebugSession::syncHardwareBreakpointsToAllThreads() {
    const auto allBps = bpMgr_.allBreakpoints(/*include_internal=*/true);
    const auto tids   = engine_.enumerateTids();

    for (const auto& bp : allBps) {
        if (bp.type == BreakpointType::Software || bp.hardwareSlot < 0) {
            continue;
        }
        HardwareBpType hwType = HardwareBpType::Execute;
        if (bp.type == BreakpointType::HardwareWrite)     hwType = HardwareBpType::Write;
        if (bp.type == BreakpointType::HardwareReadWrite) hwType = HardwareBpType::ReadWrite;

        for (Tid tid : tids) {
            engine_.setHardwareBreakpoint(tid, bp.hardwareSlot, bp.address, hwType, HardwareBpSize::Byte1);
        }
    }
}

} // namespace edb_next
