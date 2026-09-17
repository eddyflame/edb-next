#pragma once

#include "Types.hpp"
#include "UniqueFd.hpp"
#include "RegisterContext.hpp"
#include <string>
#include <vector>
#include <memory>
#include <signal.h>

namespace edb_next {

class LinuxDebugEngine {
public:
    LinuxDebugEngine();
    ~LinuxDebugEngine();

    LinuxDebugEngine(const LinuxDebugEngine&) = delete;
    LinuxDebugEngine& operator=(const LinuxDebugEngine&) = delete;

    // Process lifecycle
    Result<Pid> launch(
        const std::string& path,
        const std::vector<std::string>& args = {},
        bool disable_aslr = true,
        bool disable_lazy_binding = true);

    Result<void> attach(Pid pid);
    void detach();
    void kill();

    // Execution control
    bool singleStep(Tid tid, int signal = 0);
    bool continueExecution(Tid tid, int signal = 0);
    bool pause(Tid tid);

    // Memory access
    bool readMemory(Address addr, void* buffer, size_t size);
    bool writeMemory(Address addr, const void* buffer, size_t size);
    bool readMemory(Address addr, std::span<uint8_t> buffer) {
        return readMemory(addr, buffer.data(), buffer.size());
    }
    bool writeMemory(Address addr, std::span<const uint8_t> buffer) {
        return writeMemory(addr, buffer.data(), buffer.size());
    }

    template<TriviallyCopyable T>
    [[nodiscard]] std::optional<T> read(Address addr) {
        T val{};
        if (readMemory(addr, &val, sizeof(T))) {
            return val;
        }
        return std::nullopt;
    }

    template<TriviallyCopyable T>
    bool write(Address addr, const T& val) {
        return writeMemory(addr, &val, sizeof(T));
    }

    // Register access
    bool getRegisters(Tid tid, RegisterContext& regs);
    bool setRegisters(Tid tid, const RegisterContext& regs);
    bool getFpRegisters(Tid tid, user_fpregs_struct& fpregs);
    bool setFpRegisters(Tid tid, const user_fpregs_struct& fpregs);

    // Hardware breakpoints (DR0-DR7)
    bool setHardwareBreakpoint(Tid tid, int slot, Address addr, HardwareBpType type = HardwareBpType::Execute, HardwareBpSize size = HardwareBpSize::Byte1);
    bool clearHardwareBreakpoint(Tid tid, int slot);
    [[nodiscard]] uint64_t getDebugRegister(Tid tid, int reg_index) const;

    // Remote syscall execution (e.g. mprotect, mmap, munmap)
    Result<uint64_t> executeRemoteSyscall(Tid tid, uint64_t sys_no,
                                          uint64_t a1 = 0, uint64_t a2 = 0, uint64_t a3 = 0,
                                          uint64_t a4 = 0, uint64_t a5 = 0, uint64_t a6 = 0);
    bool remoteMprotect(Address addr, size_t size, int prot);
    Result<Address> remoteMmap(Address addr, size_t size, int prot, int flags);
    bool remoteMunmap(Address addr, size_t size);

    // Signal and event inspection
    bool getSigInfo(Tid tid, siginfo_t* siginfo);
    bool getEventMessage(Tid tid, unsigned long* message);
    bool detachProcess(Pid pid);
    bool adoptProcess(Pid pid);

    // Thread control & freeze/thaw
    bool pauseThread(Tid tid);
    bool resumeThread(Tid tid, int signal = 0);
    [[nodiscard]] std::vector<Tid> enumerateTids() const;

    // Introspection
    [[nodiscard]] std::vector<MemoryRegion> getMemoryRegions() const;
    [[nodiscard]] std::vector<ThreadInfo> getThreads() const;
    [[nodiscard]] Pid pid() const noexcept { return pid_; }
    [[nodiscard]] Tid mainTid() const noexcept { return mainTid_; }
    [[nodiscard]] Tid activeTid() const noexcept { return activeTid_ > 0 ? activeTid_ : mainTid_; }
    void setActiveTid(Tid tid) { activeTid_ = tid; }
    [[nodiscard]] bool isAttached() const noexcept { return pid_ > 0; }

private:
    bool openProcMem();

    Pid pid_{0};
    Tid mainTid_{0};
    Tid activeTid_{0};
    UniqueFd memFd_;
};

} // namespace edb_next
