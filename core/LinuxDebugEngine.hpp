#pragma once

#include "IDebugBackend.hpp"
#include "Types.hpp"
#include "UniqueFd.hpp"
#include "RegisterContext.hpp"
#include <string>
#include <vector>
#include <memory>
#include <signal.h>

namespace edb_next {

class LinuxDebugEngine : public IDebugBackend {
public:
    LinuxDebugEngine();
    ~LinuxDebugEngine() override;

    LinuxDebugEngine(const LinuxDebugEngine&) = delete;
    LinuxDebugEngine& operator=(const LinuxDebugEngine&) = delete;

    // Process lifecycle
    Result<Pid> launch(
        const std::string& path,
        const std::vector<std::string>& args = {},
        bool disable_aslr = true,
        bool disable_lazy_binding = true) override;

    Result<void> attach(Pid pid) override;
    void detach() override;
    void kill() override;

    // Execution control
    bool singleStep(Tid tid, int signal = 0) override;
    bool continueExecution(Tid tid, int signal = 0) override;
    bool pause(Tid tid) override;

    // Memory access
    bool readMemory(Address addr, void* buffer, size_t size) override;
    bool writeMemory(Address addr, const void* buffer, size_t size) override;
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
    bool getRegisters(Tid tid, RegisterContext& regs) override;
    bool setRegisters(Tid tid, const RegisterContext& regs) override;
    bool getFpRegisters(Tid tid, user_fpregs_struct& fpregs) override;
    bool setFpRegisters(Tid tid, const user_fpregs_struct& fpregs) override;

    // Hardware breakpoints (DR0-DR7)
    bool setHardwareBreakpoint(Tid tid, int slot, Address addr, HardwareBpType type = HardwareBpType::Execute, HardwareBpSize size = HardwareBpSize::Byte1) override;
    bool clearHardwareBreakpoint(Tid tid, int slot) override;
    [[nodiscard]] uint64_t getDebugRegister(Tid tid, int reg_index) const override;

    // Remote syscall execution (e.g. mprotect, mmap, munmap)
    Result<uint64_t> executeRemoteSyscall(Tid tid, uint64_t sys_no,
                                          uint64_t a1 = 0, uint64_t a2 = 0, uint64_t a3 = 0,
                                          uint64_t a4 = 0, uint64_t a5 = 0, uint64_t a6 = 0) override;
    bool remoteMprotect(Address addr, size_t size, int prot) override;
    Result<Address> remoteMmap(Address addr, size_t size, int prot, int flags) override;
    bool remoteMunmap(Address addr, size_t size) override;

    // Signal and event inspection
    bool getSigInfo(Tid tid, siginfo_t* siginfo) override;
    bool getEventMessage(Tid tid, unsigned long* message) override;
    bool detachProcess(Pid pid) override;
    bool adoptProcess(Pid pid) override;

    // Thread control & freeze/thaw
    bool pauseThread(Tid tid) override;
    bool resumeThread(Tid tid, int signal = 0) override;
    [[nodiscard]] std::vector<Tid> enumerateTids() const override;

    // Introspection
    [[nodiscard]] std::vector<MemoryRegion> getMemoryRegions() const override;
    [[nodiscard]] std::vector<ThreadInfo> getThreads() const override;
    [[nodiscard]] Pid pid() const noexcept override { return pid_; }
    [[nodiscard]] Tid mainTid() const noexcept override { return mainTid_; }
    [[nodiscard]] Tid activeTid() const noexcept override { return activeTid_ > 0 ? activeTid_ : mainTid_; }
    void setActiveTid(Tid tid) override { activeTid_ = tid; }
    [[nodiscard]] bool isAttached() const noexcept override { return pid_ > 0; }

private:
    bool openProcMem();

    Pid pid_{0};
    Tid mainTid_{0};
    Tid activeTid_{0};
    UniqueFd memFd_;
};

} // namespace edb_next
