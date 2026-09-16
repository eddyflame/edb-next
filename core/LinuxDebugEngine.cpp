#include "LinuxDebugEngine.hpp"
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/personality.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <elf.h>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <dirent.h>
#include <algorithm>

namespace edb_next {

LinuxDebugEngine::LinuxDebugEngine() = default;

LinuxDebugEngine::~LinuxDebugEngine() {
    if (isAttached()) {
        detach();
    }
}

bool LinuxDebugEngine::openProcMem() {
    if (pid_ <= 0) return false;
    std::string path = "/proc/" + std::to_string(pid_) + "/mem";
    int fd = ::open(path.c_str(), O_RDWR | O_LARGEFILE);
    if (fd < 0) {
        // Fallback to read-only if writable open fails
        fd = ::open(path.c_str(), O_RDONLY | O_LARGEFILE);
    }
    memFd_.reset(fd);
    return memFd_.isValid();
}

Result<Pid> LinuxDebugEngine::launch(
    const std::string& path,
    const std::vector<std::string>& args,
    bool disable_aslr)
{
    int pipe_fd[2];
    if (::pipe2(pipe_fd, O_CLOEXEC) != 0) {
        return Result<Pid>::Err("Failed to create pipe: " + std::string(strerror(errno)));
    }

    pid_t child_pid = ::fork();
    if (child_pid < 0) {
        ::close(pipe_fd[0]);
        ::close(pipe_fd[1]);
        return Result<Pid>::Err("fork() failed: " + std::string(strerror(errno)));
    }

    if (child_pid == 0) {
        // Child process
        ::close(pipe_fd[0]); // Close read end

        // Isolate child into its own process group so multi-session waitpid does not cross-interfere
        ::setpgid(0, 0);

        if (disable_aslr) {
            const int persona = ::personality(0xffffffff);
            if (persona != -1) {
                ::personality(persona | ADDR_NO_RANDOMIZE);
            }
        }

        ::setpgid(0, 0);

        if (::ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0) {
            int err = errno;
            (void)::write(pipe_fd[1], &err, sizeof(err));
            ::_exit(1);
        }

        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(path.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        ::execv(path.c_str(), argv.data());

        // If execv reaches here, it failed
        int err = errno;
        (void)::write(pipe_fd[1], &err, sizeof(err));
        ::_exit(1);
    }

    // Parent process
    ::setpgid(child_pid, child_pid);
    ::close(pipe_fd[1]); // Close write end

    int child_err = 0;
    ssize_t bytes_read = ::read(pipe_fd[0], &child_err, sizeof(child_err));
    ::close(pipe_fd[0]);

    if (bytes_read > 0) {
        return Result<Pid>::Err("Child execv failed: " + std::string(strerror(child_err)));
    }

    int status = 0;
    if (::waitpid(child_pid, &status, 0) < 0) {
        return Result<Pid>::Err("waitpid on initial stop failed: " + std::string(strerror(errno)));
    }

    if (!WIFSTOPPED(status)) {
        return Result<Pid>::Err("Child did not stop as expected on initial startup");
    }

    // Configure ptrace options to follow fork/clone and kill child on debugger exit
    constexpr unsigned long options =
        PTRACE_O_TRACECLONE |
        PTRACE_O_TRACEFORK  |
        PTRACE_O_TRACEVFORK |
        PTRACE_O_TRACEEXEC  |
        PTRACE_O_EXITKILL;

    if (::ptrace(PTRACE_SETOPTIONS, child_pid, nullptr, options) < 0) {
        std::cerr << "Warning: PTRACE_SETOPTIONS failed: " << strerror(errno) << std::endl;
    }

    pid_ = child_pid;
    mainTid_ = child_pid;
    openProcMem();

    return Result<Pid>::Ok(pid_);
}

Result<void> LinuxDebugEngine::attach(Pid pid) {
    if (::ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) < 0) {
        return Result<void>::Err("ptrace ATTACH failed: " + std::string(strerror(errno)));
    }

    int status = 0;
    if (::waitpid(pid, &status, 0) < 0) {
        return Result<void>::Err("waitpid after ATTACH failed: " + std::string(strerror(errno)));
    }

    constexpr unsigned long options =
        PTRACE_O_TRACECLONE |
        PTRACE_O_TRACEFORK  |
        PTRACE_O_TRACEVFORK |
        PTRACE_O_TRACEEXEC  |
        PTRACE_O_EXITKILL;
    ::ptrace(PTRACE_SETOPTIONS, pid, nullptr, options);

    pid_ = pid;
    mainTid_ = pid;
    openProcMem();

    return Result<void>::Ok();
}

void LinuxDebugEngine::detach() {
    if (pid_ > 0) {
        memFd_.reset();
        ::ptrace(PTRACE_DETACH, pid_, nullptr, nullptr);
        pid_ = 0;
        mainTid_ = 0;
    }
}

void LinuxDebugEngine::kill() {
    if (pid_ > 0) {
        memFd_.reset();
        ::kill(pid_, SIGKILL);
        int status = 0;
        int ret = 0;
        int retries = 0;
        while ((ret = ::waitpid(pid_, &status, __WALL | WNOHANG)) == 0 && retries++ < 50) {
            usleep(2000); // 2ms
        }
        if (ret == 0) {
            ::waitpid(pid_, &status, __WALL);
        }
        // Drain any leftover child threads
        while (::waitpid(-1, &status, __WALL | WNOHANG) > 0) {}
        pid_ = 0;
        mainTid_ = 0;
    }
}

bool LinuxDebugEngine::singleStep(Tid tid, int signal) {
    if (tid <= 0) tid = mainTid_;
    return ::ptrace(PTRACE_SINGLESTEP, tid, nullptr, reinterpret_cast<void*>(static_cast<intptr_t>(signal))) == 0;
}

bool LinuxDebugEngine::continueExecution(Tid tid, int signal) {
    if (tid <= 0) tid = mainTid_;
    return ::ptrace(PTRACE_CONT, tid, nullptr, reinterpret_cast<void*>(static_cast<intptr_t>(signal))) == 0;
}

bool LinuxDebugEngine::pause(Tid tid) {
    if (tid <= 0) tid = mainTid_;
    return ::kill(tid, SIGSTOP) == 0;
}

bool LinuxDebugEngine::readMemory(Address addr, void* buffer, size_t size) {
    if (size == 0 || !buffer) return true;

    // 1. Try /proc/<pid>/mem pread64
    if (memFd_.isValid()) {
        ssize_t ret = ::pread64(memFd_.get(), buffer, size, static_cast<off64_t>(addr.value()));
        if (ret == static_cast<ssize_t>(size)) {
            return true;
        }
    }

    // 2. Try process_vm_readv
    struct iovec local_iov{buffer, size};
    struct iovec remote_iov{reinterpret_cast<void*>(addr.value()), size};
    ssize_t vm_ret = ::process_vm_readv(pid_, &local_iov, 1, &remote_iov, 1, 0);
    if (vm_ret == static_cast<ssize_t>(size)) {
        return true;
    }

    // 3. Fallback to PTRACE_PEEKDATA word by word
    auto* dst = static_cast<uint8_t*>(buffer);
    size_t words = size / sizeof(long);
    size_t remainder = size % sizeof(long);

    for (size_t i = 0; i < words; ++i) {
        uint64_t target = addr.value() + i * sizeof(long);
        errno = 0;
        long data = ::ptrace(PTRACE_PEEKDATA, mainTid_, reinterpret_cast<void*>(target), nullptr);
        if (errno != 0) return false;
        std::memcpy(dst + i * sizeof(long), &data, sizeof(long));
    }

    if (remainder > 0) {
        uint64_t target = addr.value() + words * sizeof(long);
        errno = 0;
        long data = ::ptrace(PTRACE_PEEKDATA, mainTid_, reinterpret_cast<void*>(target), nullptr);
        if (errno != 0) return false;
        std::memcpy(dst + words * sizeof(long), &data, remainder);
    }

    return true;
}

bool LinuxDebugEngine::writeMemory(Address addr, const void* buffer, size_t size) {
    if (size == 0 || !buffer) return true;

    // 1. Try /proc/<pid>/mem pwrite64
    if (memFd_.isValid()) {
        ssize_t ret = ::pwrite64(memFd_.get(), buffer, size, static_cast<off64_t>(addr.value()));
        if (ret == static_cast<ssize_t>(size)) {
            return true;
        }
    }

    // 2. Try process_vm_writev
    struct iovec local_iov{const_cast<void*>(buffer), size};
    struct iovec remote_iov{reinterpret_cast<void*>(addr.value()), size};
    ssize_t vm_ret = ::process_vm_writev(pid_, &local_iov, 1, &remote_iov, 1, 0);
    if (vm_ret == static_cast<ssize_t>(size)) {
        return true;
    }

    // 3. Fallback to PTRACE_POKEDATA word by word
    const auto* src = static_cast<const uint8_t*>(buffer);
    size_t words = size / sizeof(long);
    size_t remainder = size % sizeof(long);

    for (size_t i = 0; i < words; ++i) {
        uint64_t target = addr.value() + i * sizeof(long);
        long data = 0;
        std::memcpy(&data, src + i * sizeof(long), sizeof(long));
        if (::ptrace(PTRACE_POKEDATA, mainTid_, reinterpret_cast<void*>(target), reinterpret_cast<void*>(data)) < 0) {
            return false;
        }
    }

    if (remainder > 0) {
        uint64_t target = addr.value() + words * sizeof(long);
        errno = 0;
        long data = ::ptrace(PTRACE_PEEKDATA, mainTid_, reinterpret_cast<void*>(target), nullptr);
        if (errno != 0) return false;
        std::memcpy(&data, src + words * sizeof(long), remainder);
        if (::ptrace(PTRACE_POKEDATA, mainTid_, reinterpret_cast<void*>(target), reinterpret_cast<void*>(data)) < 0) {
            return false;
        }
    }

    return true;
}

bool LinuxDebugEngine::getRegisters(Tid tid, RegisterContext& regs) {
    if (tid <= 0) tid = mainTid_;
    if (::ptrace(PTRACE_GETREGS, tid, nullptr, &regs.raw()) == 0) {
        return true;
    }
    struct iovec iov{
        .iov_base = &regs.raw(),
        .iov_len = sizeof(regs.raw())
    };
    return ::ptrace(PTRACE_GETREGSET, tid, reinterpret_cast<void*>(NT_PRSTATUS), &iov) == 0;
}

bool LinuxDebugEngine::setRegisters(Tid tid, const RegisterContext& regs) {
    if (tid <= 0) tid = mainTid_;
    if (::ptrace(PTRACE_SETREGS, tid, nullptr, &regs.raw()) == 0) {
        return true;
    }
    struct iovec iov{
        .iov_base = const_cast<user_regs_struct*>(&regs.raw()),
        .iov_len = sizeof(regs.raw())
    };
    return ::ptrace(PTRACE_SETREGSET, tid, reinterpret_cast<void*>(NT_PRSTATUS), &iov) == 0;
}

bool LinuxDebugEngine::getFpRegisters(Tid tid, user_fpregs_struct& fpregs) {
    if (tid <= 0) tid = mainTid_;
    if (::ptrace(PTRACE_GETFPREGS, tid, nullptr, &fpregs) == 0) {
        return true;
    }
    struct iovec iov{
        .iov_base = &fpregs,
        .iov_len = sizeof(fpregs)
    };
    return ::ptrace(PTRACE_GETREGSET, tid, reinterpret_cast<void*>(NT_FPREGSET), &iov) == 0;
}

bool LinuxDebugEngine::setFpRegisters(Tid tid, const user_fpregs_struct& fpregs) {
    if (tid <= 0) tid = mainTid_;
    if (::ptrace(PTRACE_SETFPREGS, tid, nullptr, &fpregs) == 0) {
        return true;
    }
    struct iovec iov{
        .iov_base = const_cast<user_fpregs_struct*>(&fpregs),
        .iov_len = sizeof(fpregs)
    };
    return ::ptrace(PTRACE_SETREGSET, tid, reinterpret_cast<void*>(NT_FPREGSET), &iov) == 0;
}

static inline size_t debugRegOffset(int index) {
    return offsetof(struct user, u_debugreg[0]) + index * sizeof(unsigned long long int);
}

bool LinuxDebugEngine::setHardwareBreakpoint(Tid tid, int slot, Address addr, HardwareBpType type, HardwareBpSize size) {
    if (tid <= 0) tid = mainTid_;
    if (slot < 0 || slot > 3) return false;

    // 1. Set DR[slot] = addr
    if (::ptrace(PTRACE_POKEUSER, tid, debugRegOffset(slot), addr.value()) < 0) {
        return false;
    }

    // 2. Read current DR7
    uint64_t dr7 = getDebugRegister(tid, 7);

    // 3. Set Local enable bit (bit 2*slot)
    dr7 |= (1ULL << (2 * slot));

    // 4. Configure condition (type) and length (size)
    dr7 &= ~(0xFULL << (16 + 4 * slot));
    uint64_t type_bits = static_cast<uint8_t>(type) & 0x3;
    uint64_t size_bits = static_cast<uint8_t>(size) & 0x3;
    uint64_t cond_len = type_bits | (size_bits << 2);
    dr7 |= (cond_len << (16 + 4 * slot));

    // 5. Write updated DR7
    return ::ptrace(PTRACE_POKEUSER, tid, debugRegOffset(7), dr7) == 0;
}

bool LinuxDebugEngine::clearHardwareBreakpoint(Tid tid, int slot) {
    if (tid <= 0) tid = mainTid_;
    if (slot < 0 || slot > 3) return false;

    ::ptrace(PTRACE_POKEUSER, tid, debugRegOffset(slot), 0);

    uint64_t dr7 = getDebugRegister(tid, 7);
    dr7 &= ~(1ULL << (2 * slot));
    dr7 &= ~(0xFULL << (16 + 4 * slot));

    return ::ptrace(PTRACE_POKEUSER, tid, debugRegOffset(7), dr7) == 0;
}

uint64_t LinuxDebugEngine::getDebugRegister(Tid tid, int reg_index) const {
    if (tid <= 0) tid = mainTid_;
    if (reg_index < 0 || reg_index > 7) return 0;

    errno = 0;
    long val = ::ptrace(PTRACE_PEEKUSER, tid, debugRegOffset(reg_index), nullptr);
    if (errno != 0) return 0;
    return static_cast<uint64_t>(val);
}

std::vector<MemoryRegion> LinuxDebugEngine::getMemoryRegions() const {
    std::vector<MemoryRegion> regions;
    if (pid_ <= 0) return regions;

    std::ifstream maps_file("/proc/" + std::to_string(pid_) + "/maps");
    if (!maps_file.is_open()) return regions;

    std::string line;
    while (std::getline(maps_file, line)) {
        std::istringstream iss(line);
        std::string range, perms, offset, dev, inode, pathname;
        if (!(iss >> range >> perms >> offset >> dev >> inode)) continue;
        iss >> pathname;

        auto dash = range.find('-');
        if (dash == std::string::npos) continue;

        uint64_t start = std::stoull(range.substr(0, dash), nullptr, 16);
        uint64_t end = std::stoull(range.substr(dash + 1), nullptr, 16);
        uint64_t off = std::stoull(offset, nullptr, 16);

        regions.push_back(MemoryRegion{
            .start = Address(start),
            .end = Address(end),
            .permissions = perms,
            .offset = off,
            .pathname = pathname
        });
    }

    return regions;
}

std::vector<ThreadInfo> LinuxDebugEngine::getThreads() const {
    std::vector<ThreadInfo> threads;
    if (pid_ <= 0) return threads;

    std::string task_dir = "/proc/" + std::to_string(pid_) + "/task";
    DIR* dir = ::opendir(task_dir.c_str());
    if (!dir) return threads;

    struct dirent* entry = nullptr;
    while ((entry = ::readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        char* endptr = nullptr;
        long tid_val = std::strtol(entry->d_name, &endptr, 10);
        if (!endptr || *endptr != '\0' || tid_val <= 0) continue;

        Tid tid = static_cast<Tid>(tid_val);
        ThreadInfo info;
        info.tid = tid;
        info.isActive = (tid == activeTid());

        // Read thread name from /proc/<pid>/task/<tid>/comm
        std::string comm_path = task_dir + "/" + entry->d_name + "/comm";
        std::ifstream comm_file(comm_path);
        if (comm_file.is_open()) {
            std::getline(comm_file, info.name);
        } else {
            info.name = (tid == mainTid_) ? "Main Thread" : "Worker Thread";
        }

        // Read thread state from /proc/<pid>/task/<tid>/stat
        std::string stat_path = task_dir + "/" + entry->d_name + "/stat";
        std::ifstream stat_file(stat_path);
        if (stat_file.is_open()) {
            std::string line;
            std::getline(stat_file, line);
            auto rparen = line.rfind(')');
            if (rparen != std::string::npos && rparen + 2 < line.size()) {
                char s = line[rparen + 2];
                switch (s) {
                    case 'R': info.state = "Running (R)"; break;
                    case 'S': info.state = "Sleeping (S)"; break;
                    case 'D': info.state = "Disk Sleep (D)"; break;
                    case 'T': info.state = "Stopped (T)"; break;
                    case 't': info.state = "Tracing Stop (t)"; break;
                    case 'Z': info.state = "Zombie (Z)"; break;
                    default: info.state = std::string(1, s); break;
                }
            }
        }

        // Read registers (RIP / RSP)
        RegisterContext regs;
        if (const_cast<LinuxDebugEngine*>(this)->getRegisters(tid, regs)) {
            info.rip = regs.rip();
            info.rsp = regs.rsp();
        }

        threads.push_back(std::move(info));
    }
    ::closedir(dir);

    std::sort(threads.begin(), threads.end(), [this](const ThreadInfo& a, const ThreadInfo& b) {
        if (a.tid == mainTid_) return true;
        if (b.tid == mainTid_) return false;
        return a.tid < b.tid;
    });

    return threads;
}

Result<uint64_t> LinuxDebugEngine::executeRemoteSyscall(Tid tid, uint64_t sys_no,
                                                        uint64_t a1, uint64_t a2, uint64_t a3,
                                                        uint64_t a4, uint64_t a5, uint64_t a6)
{
    if (pid_ <= 0) {
        return Result<uint64_t>::Err("Target process is not running");
    }
    if (tid <= 0) {
        tid = activeTid();
    }

    // 1. Save original registers
    RegisterContext origRegs;
    if (!getRegisters(tid, origRegs)) {
        return Result<uint64_t>::Err("Failed to retrieve registers for remote syscall");
    }

    // 2. Read original 2 bytes at RIP
    uint8_t origBytes[2];
    if (!readMemory(origRegs.rip(), origBytes, 2)) {
        return Result<uint64_t>::Err("Failed to read memory at RIP for remote syscall");
    }

    // 3. Write 'syscall' instruction (0x0f 0x05) at RIP
    const uint8_t syscallInsn[2] = {0x0f, 0x05};
    if (!writeMemory(origRegs.rip(), syscallInsn, 2)) {
        return Result<uint64_t>::Err("Failed to write syscall opcode at RIP");
    }

    // 4. Set registers for Linux x86_64 ABI:
    // RAX = sys_no, RDI = a1, RSI = a2, RDX = a3, R10 = a4, R8 = a5, R9 = a6
    RegisterContext sysRegs = origRegs;
    sysRegs.setRax(sys_no);
    sysRegs.setRdi(a1);
    sysRegs.setRsi(a2);
    sysRegs.setRdx(a3);
    sysRegs.raw().r10 = a4;
    sysRegs.raw().r8 = a5;
    sysRegs.raw().r9 = a6;
    if (!setRegisters(tid, sysRegs)) {
        writeMemory(origRegs.rip(), origBytes, 2);
        return Result<uint64_t>::Err("Failed to configure registers for remote syscall");
    }

    // 5. Execute single step to execute the syscall
    if (::ptrace(PTRACE_SINGLESTEP, tid, nullptr, nullptr) < 0) {
        writeMemory(origRegs.rip(), origBytes, 2);
        setRegisters(tid, origRegs);
        return Result<uint64_t>::Err("PTRACE_SINGLESTEP failed: " + std::string(strerror(errno)));
    }

    int status = 0;
    if (::waitpid(tid, &status, __WALL) < 0) {
        writeMemory(origRegs.rip(), origBytes, 2);
        setRegisters(tid, origRegs);
        return Result<uint64_t>::Err("waitpid failed after remote syscall");
    }

    // 6. Get return value from RAX
    RegisterContext postRegs;
    getRegisters(tid, postRegs);
    uint64_t retVal = postRegs.rax();

    // 7. Restore original instruction and registers
    writeMemory(origRegs.rip(), origBytes, 2);
    setRegisters(tid, origRegs);

    return Result<uint64_t>::Ok(retVal);
}

bool LinuxDebugEngine::remoteMprotect(Address addr, size_t size, int prot) {
    auto res = executeRemoteSyscall(activeTid(), SYS_mprotect, addr.value(), size, prot);
    if (!res) return false;
    int64_t ret = static_cast<int64_t>(res.value);
    return ret == 0;
}

Result<Address> LinuxDebugEngine::remoteMmap(Address addr, size_t size, int prot, int flags) {
    if (flags == 0) {
        flags = MAP_PRIVATE | MAP_ANONYMOUS;
    }
    auto res = executeRemoteSyscall(activeTid(), SYS_mmap, addr.value(), size, prot, flags, static_cast<uint64_t>(-1), 0);
    if (!res) return Result<Address>::Err(res.error);
    int64_t ret = static_cast<int64_t>(res.value);
    if (ret < 0 && ret >= -4095) {
        return Result<Address>::Err("mmap failed with errno: " + std::to_string(-ret));
    }
    return Result<Address>::Ok(Address(static_cast<uint64_t>(ret)));
}

bool LinuxDebugEngine::remoteMunmap(Address addr, size_t size) {
    auto res = executeRemoteSyscall(activeTid(), SYS_munmap, addr.value(), size);
    if (!res) return false;
    int64_t ret = static_cast<int64_t>(res.value);
    return ret == 0;
}

bool LinuxDebugEngine::getSigInfo(Tid tid, siginfo_t* siginfo) {
    if (tid <= 0 || !siginfo) return false;
    long res = ::ptrace(PTRACE_GETSIGINFO, tid, nullptr, siginfo);
    return res == 0;
}

bool LinuxDebugEngine::getEventMessage(Tid tid, unsigned long* message) {
    if (tid <= 0 || !message) return false;
    long res = ::ptrace(PTRACE_GETEVENTMSG, tid, nullptr, message);
    return res == 0;
}

bool LinuxDebugEngine::detachProcess(Pid pid) {
    if (pid <= 0) return false;
    long res = ::ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
    return res == 0;
}

bool LinuxDebugEngine::adoptProcess(Pid pid) {
    if (pid <= 0) return false;
    memFd_.reset();
    pid_ = pid;
    mainTid_ = pid;
    activeTid_ = pid;
    openProcMem();

    constexpr unsigned long options =
        PTRACE_O_TRACECLONE |
        PTRACE_O_TRACEFORK  |
        PTRACE_O_TRACEVFORK |
        PTRACE_O_TRACEEXEC  |
        PTRACE_O_EXITKILL;
    ::ptrace(PTRACE_SETOPTIONS, pid, nullptr, options);
    return true;
}

} // namespace edb_next
