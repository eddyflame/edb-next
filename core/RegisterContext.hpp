#pragma once

#include "Types.hpp"
#include <sys/user.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstring>

namespace edb_next {

struct RegisterItem {
    std::string name;
    uint64_t value{0};
    bool modified{false};
};

class RegisterContext {
public:
    RegisterContext() {
        std::memset(&regs_, 0, sizeof(regs_));
    }

    explicit RegisterContext(const user_regs_struct& regs) : regs_(regs) {}

    [[nodiscard]] const user_regs_struct& raw() const noexcept { return regs_; }
    [[nodiscard]] user_regs_struct& raw() noexcept { return regs_; }

    [[nodiscard]] Address rip() const noexcept { return Address(regs_.rip); }
    void setRip(Address addr) noexcept { regs_.rip = addr.value(); }

    [[nodiscard]] Address rsp() const noexcept { return Address(regs_.rsp); }
    void setRsp(Address addr) noexcept { regs_.rsp = addr.value(); }

    [[nodiscard]] Address rbp() const noexcept { return Address(regs_.rbp); }
    void setRbp(Address addr) noexcept { regs_.rbp = addr.value(); }

    [[nodiscard]] uint64_t rax() const noexcept { return regs_.rax; }
    [[nodiscard]] uint64_t rbx() const noexcept { return regs_.rbx; }
    [[nodiscard]] uint64_t rcx() const noexcept { return regs_.rcx; }
    [[nodiscard]] uint64_t rdx() const noexcept { return regs_.rdx; }
    [[nodiscard]] uint64_t rsi() const noexcept { return regs_.rsi; }
    [[nodiscard]] uint64_t rdi() const noexcept { return regs_.rdi; }
    [[nodiscard]] uint64_t rflags() const noexcept { return regs_.eflags; }
    [[nodiscard]] uint64_t eflags() const noexcept { return regs_.eflags; }
    [[nodiscard]] uint64_t r8() const noexcept { return regs_.r8; }
    [[nodiscard]] uint64_t r9() const noexcept { return regs_.r9; }
    [[nodiscard]] uint64_t r10() const noexcept { return regs_.r10; }
    [[nodiscard]] uint64_t r11() const noexcept { return regs_.r11; }
    [[nodiscard]] uint64_t r12() const noexcept { return regs_.r12; }
    [[nodiscard]] uint64_t r13() const noexcept { return regs_.r13; }
    [[nodiscard]] uint64_t r14() const noexcept { return regs_.r14; }
    [[nodiscard]] uint64_t r15() const noexcept { return regs_.r15; }

    void setRax(uint64_t val) noexcept { regs_.rax = val; }
    void setRbx(uint64_t val) noexcept { regs_.rbx = val; }
    void setRcx(uint64_t val) noexcept { regs_.rcx = val; }
    void setRdx(uint64_t val) noexcept { regs_.rdx = val; }
    void setRsi(uint64_t val) noexcept { regs_.rsi = val; }
    void setRdi(uint64_t val) noexcept { regs_.rdi = val; }
    void setR8(uint64_t val) noexcept { regs_.r8 = val; }
    void setR9(uint64_t val) noexcept { regs_.r9 = val; }
    void setR10(uint64_t val) noexcept { regs_.r10 = val; }
    void setR11(uint64_t val) noexcept { regs_.r11 = val; }
    void setR12(uint64_t val) noexcept { regs_.r12 = val; }
    void setR13(uint64_t val) noexcept { regs_.r13 = val; }
    void setR14(uint64_t val) noexcept { regs_.r14 = val; }
    void setR15(uint64_t val) noexcept { regs_.r15 = val; }

    [[nodiscard]] bool flagCF() const noexcept { return (regs_.eflags & (1ULL << 0)) != 0; }
    [[nodiscard]] bool flagPF() const noexcept { return (regs_.eflags & (1ULL << 2)) != 0; }
    [[nodiscard]] bool flagAF() const noexcept { return (regs_.eflags & (1ULL << 4)) != 0; }
    [[nodiscard]] bool flagZF() const noexcept { return (regs_.eflags & (1ULL << 6)) != 0; }
    [[nodiscard]] bool flagSF() const noexcept { return (regs_.eflags & (1ULL << 7)) != 0; }
    [[nodiscard]] bool flagTF() const noexcept { return (regs_.eflags & (1ULL << 8)) != 0; }
    [[nodiscard]] bool flagIF() const noexcept { return (regs_.eflags & (1ULL << 9)) != 0; }
    [[nodiscard]] bool flagDF() const noexcept { return (regs_.eflags & (1ULL << 10)) != 0; }
    [[nodiscard]] bool flagOF() const noexcept { return (regs_.eflags & (1ULL << 11)) != 0; }

    void toggleFlag(int bit) noexcept {
        regs_.eflags ^= (1ULL << bit);
    }
    void setFlag(int bit, bool val) noexcept {
        if (val) regs_.eflags |= (1ULL << bit);
        else regs_.eflags &= ~(1ULL << bit);
    }

    [[nodiscard]] std::vector<RegisterItem> toList(const RegisterContext* previous = nullptr) const {
        std::vector<RegisterItem> list = {
            {"RAX", regs_.rax, false},
            {"RBX", regs_.rbx, false},
            {"RCX", regs_.rcx, false},
            {"RDX", regs_.rdx, false},
            {"RSI", regs_.rsi, false},
            {"RDI", regs_.rdi, false},
            {"RBP", regs_.rbp, false},
            {"RSP", regs_.rsp, false},
            {"R8",  regs_.r8,  false},
            {"R9",  regs_.r9,  false},
            {"R10", regs_.r10, false},
            {"R11", regs_.r11, false},
            {"R12", regs_.r12, false},
            {"R13", regs_.r13, false},
            {"R14", regs_.r14, false},
            {"R15", regs_.r15, false},
            {"RIP", regs_.rip, false},
            {"RFLAGS", regs_.eflags, false},
            {"CS",  regs_.cs,  false},
            {"SS",  regs_.ss,  false},
            {"DS",  regs_.ds,  false},
            {"ES",  regs_.es,  false},
            {"FS",  regs_.fs,  false},
            {"GS",  regs_.gs,  false},
        };

        if (previous) {
            auto prev_list = previous->toList(nullptr);
            for (size_t i = 0; i < list.size() && i < prev_list.size(); ++i) {
                if (list[i].value != prev_list[i].value) {
                    list[i].modified = true;
                }
            }
        }

        return list;
    }

private:
    user_regs_struct regs_{};
};

} // namespace edb_next
