#pragma once

// MockDebugBackend: minimal stub for unit testing DebugSession and related components
// without requiring a real process or ptrace access.

#include "core/IDebugBackend.hpp"
#include <cstring>

namespace edb_next {

class MockDebugBackend : public IDebugBackend {
public:
    // Configurable state
    bool attached_{false};
    Pid  pid_{0};
    Tid  mainTid_{0};
    Tid  activeTid_{0};
    std::vector<uint8_t> fakeMemory_;
    std::vector<MemoryRegion> fakeRegions_;
    std::function<bool(Address, void*, size_t)> onReadMemory_;
    RegisterContext fakeRegs_;

    // Lifecycle
    Result<Pid> launch(const std::string&, const std::vector<std::string>& = {}, bool = true, bool = true) override {
        attached_ = true;
        pid_ = 9999; mainTid_ = 9999; activeTid_ = 9999;
        return Result<Pid>::Ok(pid_);
    }
    Result<void> attach(Pid p) override { attached_ = true; pid_ = p; mainTid_ = p; activeTid_ = p; return Result<void>::Ok(); }
    void detach() override { attached_ = false; }
    void kill()   override { attached_ = false; pid_ = 0; }

    // Execution (no-op)
    bool singleStep(Tid, int) override        { return true; }
    bool continueExecution(Tid, int) override { return true; }
    bool pause(Tid) override                  { return true; }

    // Memory: reads from fakeMemory_, discards writes
    bool readMemory(Address addr, void* buffer, size_t size) override {
        if (onReadMemory_) return onReadMemory_(addr, buffer, size);
        if (!attached_ || fakeMemory_.empty()) { std::memset(buffer, 0, size); return true; }
        size_t off = addr.value() % fakeMemory_.size();
        size_t avail = std::min(size, fakeMemory_.size() - off);
        std::memcpy(buffer, fakeMemory_.data() + off, avail);
        if (avail < size) std::memset(static_cast<uint8_t*>(buffer) + avail, 0, size - avail);
        return true;
    }
    bool writeMemory(Address, const void*, size_t) override { return true; }

    // Registers
    bool getRegisters(Tid, RegisterContext& regs) override { regs = fakeRegs_; return true; }
    bool setRegisters(Tid, const RegisterContext& regs) override { fakeRegs_ = regs; return true; }
    bool getFpRegisters(Tid, user_fpregs_struct& fp) override { std::memset(&fp, 0, sizeof(fp)); return true; }
    bool setFpRegisters(Tid, const user_fpregs_struct&) override { return true; }

    // Hardware breakpoints (track slot occupancy for test assertions)
    bool hwBpSet_[4]{};
    bool setHardwareBreakpoint(Tid, int slot, Address, HardwareBpType, HardwareBpSize) override {
        if (slot >= 0 && slot < 4) hwBpSet_[slot] = true;
        return true;
    }
    bool clearHardwareBreakpoint(Tid, int slot) override {
        if (slot >= 0 && slot < 4) hwBpSet_[slot] = false;
        return true;
    }
    uint64_t getDebugRegister(Tid, int) const override { return 0; }

    // Remote syscalls (stub)
    Result<uint64_t> executeRemoteSyscall(Tid, uint64_t, uint64_t, uint64_t, uint64_t,
                                          uint64_t, uint64_t, uint64_t) override { return Result<uint64_t>::Ok(0); }
    bool remoteMprotect(Address, size_t, int) override { return true; }
    Result<Address> remoteMmap(Address a, size_t, int, int) override { return Result<Address>::Ok(a); }
    bool remoteMunmap(Address, size_t) override { return true; }

    // Signals & events
    bool getSigInfo(Tid, siginfo_t* si) override { if (si) std::memset(si, 0, sizeof(*si)); return true; }
    bool getEventMessage(Tid, unsigned long* msg) override { if (msg) *msg = 0; return true; }
    bool detachProcess(Pid) override { return true; }
    bool adoptProcess(Pid) override  { return true; }

    // Thread control
    bool pauseThread(Tid) override  { return true; }
    bool resumeThread(Tid, int) override { return true; }
    std::vector<Tid> enumerateTids() const override {
        if (!attached_) return {};
        return {mainTid_};
    }

    // Introspection
    std::vector<MemoryRegion> getMemoryRegions() const override { return fakeRegions_; }
    std::vector<ThreadInfo>   getThreads() const override { return {}; }
    Pid  pid()       const noexcept override { return pid_; }
    Tid  mainTid()   const noexcept override { return mainTid_; }
    Tid  activeTid() const noexcept override { return activeTid_ > 0 ? activeTid_ : mainTid_; }
    void setActiveTid(Tid t) override { activeTid_ = t; }
    bool isAttached() const noexcept override { return attached_; }
};

} // namespace edb_next
