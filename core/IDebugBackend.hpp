#pragma once

#include "Types.hpp"
#include "RegisterContext.hpp"
#include <string>
#include <vector>
#include <span>
#include <optional>
#include <signal.h>

namespace edb_next {

// P1-C: Abstract interface over the debug backend.
// LinuxDebugEngine is the production impl; MockDebugBackend is used in tests.
class IDebugBackend {
public:
    virtual ~IDebugBackend() = default;
    IDebugBackend(const IDebugBackend&) = delete;
    IDebugBackend& operator=(const IDebugBackend&) = delete;
    IDebugBackend() = default;

    // Process lifecycle
    virtual Result<Pid> launch(const std::string& path,
        const std::vector<std::string>& args = {},
        bool disable_aslr = true, bool disable_lazy_binding = true) = 0;
    virtual Result<void> attach(Pid pid) = 0;
    virtual void         detach() = 0;
    virtual void         kill() = 0;

    // Execution control
    virtual bool singleStep(Tid tid, int signal = 0) = 0;
    virtual bool continueExecution(Tid tid, int signal = 0) = 0;
    virtual bool pause(Tid tid) = 0;

    // Memory access
    virtual bool readMemory(Address addr, void* buffer, size_t size) = 0;
    virtual bool writeMemory(Address addr, const void* buffer, size_t size) = 0;
    bool readMemory(Address addr, std::span<uint8_t> b)        { return readMemory(addr, b.data(), b.size()); }
    bool writeMemory(Address addr, std::span<const uint8_t> b) { return writeMemory(addr, b.data(), b.size()); }
    template<TriviallyCopyable T>
    [[nodiscard]] std::optional<T> read(Address addr) {
        T val{};
        return readMemory(addr, &val, sizeof(T)) ? std::optional<T>{val} : std::nullopt;
    }
    template<TriviallyCopyable T>
    bool write(Address addr, const T& val) { return writeMemory(addr, &val, sizeof(T)); }

    // Register access
    virtual bool getRegisters(Tid tid, RegisterContext& regs) = 0;
    virtual bool setRegisters(Tid tid, const RegisterContext& regs) = 0;
    virtual bool getFpRegisters(Tid tid, user_fpregs_struct& fpregs) = 0;
    virtual bool setFpRegisters(Tid tid, const user_fpregs_struct& fpregs) = 0;

    // Hardware breakpoints (DR0-DR7)
    virtual bool setHardwareBreakpoint(Tid tid, int slot, Address addr,
        HardwareBpType type = HardwareBpType::Execute,
        HardwareBpSize size = HardwareBpSize::Byte1) = 0;
    virtual bool clearHardwareBreakpoint(Tid tid, int slot) = 0;
    [[nodiscard]] virtual uint64_t getDebugRegister(Tid tid, int reg_index) const = 0;

    // Remote syscalls
    virtual Result<uint64_t> executeRemoteSyscall(Tid tid, uint64_t sys_no,
        uint64_t a1=0,uint64_t a2=0,uint64_t a3=0,uint64_t a4=0,uint64_t a5=0,uint64_t a6=0) = 0;
    virtual bool remoteMprotect(Address addr, size_t size, int prot) = 0;
    virtual Result<Address> remoteMmap(Address addr, size_t size, int prot, int flags) = 0;
    virtual bool remoteMunmap(Address addr, size_t size) = 0;

    // Signal and ptrace event helpers
    virtual bool getSigInfo(Tid tid, siginfo_t* siginfo) = 0;
    virtual bool getEventMessage(Tid tid, unsigned long* message) = 0;
    virtual bool detachProcess(Pid pid) = 0;
    virtual bool adoptProcess(Pid pid) = 0;

    // Thread control and freeze/thaw
    virtual bool pauseThread(Tid tid) = 0;
    virtual bool resumeThread(Tid tid, int signal = 0) = 0;
    [[nodiscard]] virtual std::vector<Tid> enumerateTids() const = 0;

    // Introspection
    [[nodiscard]] virtual std::vector<MemoryRegion> getMemoryRegions() const = 0;
    [[nodiscard]] virtual std::vector<ThreadInfo>   getThreads() const = 0;
    [[nodiscard]] virtual Pid  pid()       const noexcept = 0;
    [[nodiscard]] virtual Tid  mainTid()   const noexcept = 0;
    [[nodiscard]] virtual Tid  activeTid() const noexcept = 0;
    virtual void               setActiveTid(Tid tid) = 0;
    [[nodiscard]] virtual bool isAttached() const noexcept = 0;
};

} // namespace edb_next
