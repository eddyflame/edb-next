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
    void setRflags(uint64_t val) noexcept { regs_.eflags = val; }
    void setEflags(uint64_t val) noexcept { regs_.eflags = val; }

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

    [[nodiscard]] std::optional<uint64_t> getByName(std::string_view name) const noexcept {
        auto eq = [](std::string_view a, std::string_view b) noexcept {
            if (a.size() != b.size()) return false;
            for (size_t i = 0; i < a.size(); ++i) {
                char ca = a[i];
                char cb = b[i];
                if (ca >= 'A' && ca <= 'Z') ca += ('a' - 'A');
                if (cb >= 'A' && cb <= 'Z') cb += ('a' - 'A');
                if (ca != cb) return false;
            }
            return true;
        };

        // 64-bit GPRs
        if (eq(name, "rax")) return regs_.rax;
        if (eq(name, "rbx")) return regs_.rbx;
        if (eq(name, "rcx")) return regs_.rcx;
        if (eq(name, "rdx")) return regs_.rdx;
        if (eq(name, "rsi")) return regs_.rsi;
        if (eq(name, "rdi")) return regs_.rdi;
        if (eq(name, "rbp")) return regs_.rbp;
        if (eq(name, "rsp")) return regs_.rsp;
        if (eq(name, "r8"))  return regs_.r8;
        if (eq(name, "r9"))  return regs_.r9;
        if (eq(name, "r10")) return regs_.r10;
        if (eq(name, "r11")) return regs_.r11;
        if (eq(name, "r12")) return regs_.r12;
        if (eq(name, "r13")) return regs_.r13;
        if (eq(name, "r14")) return regs_.r14;
        if (eq(name, "r15")) return regs_.r15;
        if (eq(name, "rip")) return regs_.rip;
        if (eq(name, "rflags") || eq(name, "eflags")) return regs_.eflags;
        if (eq(name, "cs"))  return regs_.cs;
        if (eq(name, "ss"))  return regs_.ss;
        if (eq(name, "ds"))  return regs_.ds;
        if (eq(name, "es"))  return regs_.es;
        if (eq(name, "fs"))  return regs_.fs;
        if (eq(name, "gs"))  return regs_.gs;

        // 32-bit GPRs
        if (eq(name, "eax"))  return static_cast<uint32_t>(regs_.rax);
        if (eq(name, "ebx"))  return static_cast<uint32_t>(regs_.rbx);
        if (eq(name, "ecx"))  return static_cast<uint32_t>(regs_.rcx);
        if (eq(name, "edx"))  return static_cast<uint32_t>(regs_.rdx);
        if (eq(name, "esi"))  return static_cast<uint32_t>(regs_.rsi);
        if (eq(name, "edi"))  return static_cast<uint32_t>(regs_.rdi);
        if (eq(name, "ebp"))  return static_cast<uint32_t>(regs_.rbp);
        if (eq(name, "esp"))  return static_cast<uint32_t>(regs_.rsp);
        if (eq(name, "r8d"))  return static_cast<uint32_t>(regs_.r8);
        if (eq(name, "r9d"))  return static_cast<uint32_t>(regs_.r9);
        if (eq(name, "r10d")) return static_cast<uint32_t>(regs_.r10);
        if (eq(name, "r11d")) return static_cast<uint32_t>(regs_.r11);
        if (eq(name, "r12d")) return static_cast<uint32_t>(regs_.r12);
        if (eq(name, "r13d")) return static_cast<uint32_t>(regs_.r13);
        if (eq(name, "r14d")) return static_cast<uint32_t>(regs_.r14);
        if (eq(name, "r15d")) return static_cast<uint32_t>(regs_.r15);
        if (eq(name, "eip"))  return static_cast<uint32_t>(regs_.rip);

        // 16-bit GPRs
        if (eq(name, "ax"))   return static_cast<uint16_t>(regs_.rax);
        if (eq(name, "bx"))   return static_cast<uint16_t>(regs_.rbx);
        if (eq(name, "cx"))   return static_cast<uint16_t>(regs_.rcx);
        if (eq(name, "dx"))   return static_cast<uint16_t>(regs_.rdx);
        if (eq(name, "si"))   return static_cast<uint16_t>(regs_.rsi);
        if (eq(name, "di"))   return static_cast<uint16_t>(regs_.rdi);
        if (eq(name, "bp"))   return static_cast<uint16_t>(regs_.rbp);
        if (eq(name, "sp"))   return static_cast<uint16_t>(regs_.rsp);
        if (eq(name, "r8w"))  return static_cast<uint16_t>(regs_.r8);
        if (eq(name, "r9w"))  return static_cast<uint16_t>(regs_.r9);
        if (eq(name, "r10w")) return static_cast<uint16_t>(regs_.r10);
        if (eq(name, "r11w")) return static_cast<uint16_t>(regs_.r11);
        if (eq(name, "r12w")) return static_cast<uint16_t>(regs_.r12);
        if (eq(name, "r13w")) return static_cast<uint16_t>(regs_.r13);
        if (eq(name, "r14w")) return static_cast<uint16_t>(regs_.r14);
        if (eq(name, "r15w")) return static_cast<uint16_t>(regs_.r15);
        if (eq(name, "ip"))   return static_cast<uint16_t>(regs_.rip);

        // 8-bit GPRs
        if (eq(name, "al"))   return static_cast<uint8_t>(regs_.rax);
        if (eq(name, "bl"))   return static_cast<uint8_t>(regs_.rbx);
        if (eq(name, "cl"))   return static_cast<uint8_t>(regs_.rcx);
        if (eq(name, "dl"))   return static_cast<uint8_t>(regs_.rdx);
        if (eq(name, "sil"))  return static_cast<uint8_t>(regs_.rsi);
        if (eq(name, "dil"))  return static_cast<uint8_t>(regs_.rdi);
        if (eq(name, "bpl"))  return static_cast<uint8_t>(regs_.rbp);
        if (eq(name, "spl"))  return static_cast<uint8_t>(regs_.rsp);
        if (eq(name, "r8b"))  return static_cast<uint8_t>(regs_.r8);
        if (eq(name, "r9b"))  return static_cast<uint8_t>(regs_.r9);
        if (eq(name, "r10b")) return static_cast<uint8_t>(regs_.r10);
        if (eq(name, "r11b")) return static_cast<uint8_t>(regs_.r11);
        if (eq(name, "r12b")) return static_cast<uint8_t>(regs_.r12);
        if (eq(name, "r13b")) return static_cast<uint8_t>(regs_.r13);
        if (eq(name, "r14b")) return static_cast<uint8_t>(regs_.r14);
        if (eq(name, "r15b")) return static_cast<uint8_t>(regs_.r15);
        if (eq(name, "ah"))   return static_cast<uint8_t>((regs_.rax >> 8) & 0xff);
        if (eq(name, "bh"))   return static_cast<uint8_t>((regs_.rbx >> 8) & 0xff);
        if (eq(name, "ch"))   return static_cast<uint8_t>((regs_.rcx >> 8) & 0xff);
        if (eq(name, "dh"))   return static_cast<uint8_t>((regs_.rdx >> 8) & 0xff);

        // Flags
        if (eq(name, "cf")) return flagCF() ? 1 : 0;
        if (eq(name, "pf")) return flagPF() ? 1 : 0;
        if (eq(name, "af")) return flagAF() ? 1 : 0;
        if (eq(name, "zf")) return flagZF() ? 1 : 0;
        if (eq(name, "sf")) return flagSF() ? 1 : 0;
        if (eq(name, "tf")) return flagTF() ? 1 : 0;
        if (eq(name, "if")) return flagIF() ? 1 : 0;
        if (eq(name, "df")) return flagDF() ? 1 : 0;
        if (eq(name, "of")) return flagOF() ? 1 : 0;

        return std::nullopt;
    }

    bool setByName(std::string_view name, uint64_t val) noexcept {
        auto eq = [](std::string_view a, std::string_view b) noexcept {
            if (a.size() != b.size()) return false;
            for (size_t i = 0; i < a.size(); ++i) {
                char ca = a[i];
                char cb = b[i];
                if (ca >= 'A' && ca <= 'Z') ca += ('a' - 'A');
                if (cb >= 'A' && cb <= 'Z') cb += ('a' - 'A');
                if (ca != cb) return false;
            }
            return true;
        };

        // 64-bit GPRs
        if (eq(name, "rax")) { regs_.rax = val; return true; }
        if (eq(name, "rbx")) { regs_.rbx = val; return true; }
        if (eq(name, "rcx")) { regs_.rcx = val; return true; }
        if (eq(name, "rdx")) { regs_.rdx = val; return true; }
        if (eq(name, "rsi")) { regs_.rsi = val; return true; }
        if (eq(name, "rdi")) { regs_.rdi = val; return true; }
        if (eq(name, "rbp")) { regs_.rbp = val; return true; }
        if (eq(name, "rsp")) { regs_.rsp = val; return true; }
        if (eq(name, "r8"))  { regs_.r8  = val; return true; }
        if (eq(name, "r9"))  { regs_.r9  = val; return true; }
        if (eq(name, "r10")) { regs_.r10 = val; return true; }
        if (eq(name, "r11")) { regs_.r11 = val; return true; }
        if (eq(name, "r12")) { regs_.r12 = val; return true; }
        if (eq(name, "r13")) { regs_.r13 = val; return true; }
        if (eq(name, "r14")) { regs_.r14 = val; return true; }
        if (eq(name, "r15")) { regs_.r15 = val; return true; }
        if (eq(name, "rip")) { regs_.rip = val; return true; }
        if (eq(name, "rflags") || eq(name, "eflags")) { regs_.eflags = val; return true; }
        if (eq(name, "cs"))  { regs_.cs = val & 0xffff; return true; }
        if (eq(name, "ss"))  { regs_.ss = val & 0xffff; return true; }
        if (eq(name, "ds"))  { regs_.ds = val & 0xffff; return true; }
        if (eq(name, "es"))  { regs_.es = val & 0xffff; return true; }
        if (eq(name, "fs"))  { regs_.fs = val; return true; }
        if (eq(name, "gs"))  { regs_.gs = val; return true; }

        // 32-bit GPRs (x86_64 ABI: 32-bit register writes zero-extend high 32 bits)
        if (eq(name, "eax"))  { regs_.rax = static_cast<uint32_t>(val); return true; }
        if (eq(name, "ebx"))  { regs_.rbx = static_cast<uint32_t>(val); return true; }
        if (eq(name, "ecx"))  { regs_.rcx = static_cast<uint32_t>(val); return true; }
        if (eq(name, "edx"))  { regs_.rdx = static_cast<uint32_t>(val); return true; }
        if (eq(name, "esi"))  { regs_.rsi = static_cast<uint32_t>(val); return true; }
        if (eq(name, "edi"))  { regs_.rdi = static_cast<uint32_t>(val); return true; }
        if (eq(name, "ebp"))  { regs_.rbp = static_cast<uint32_t>(val); return true; }
        if (eq(name, "esp"))  { regs_.rsp = static_cast<uint32_t>(val); return true; }
        if (eq(name, "r8d"))  { regs_.r8  = static_cast<uint32_t>(val); return true; }
        if (eq(name, "r9d"))  { regs_.r9  = static_cast<uint32_t>(val); return true; }
        if (eq(name, "r10d")) { regs_.r10 = static_cast<uint32_t>(val); return true; }
        if (eq(name, "r11d")) { regs_.r11 = static_cast<uint32_t>(val); return true; }
        if (eq(name, "r12d")) { regs_.r12 = static_cast<uint32_t>(val); return true; }
        if (eq(name, "r13d")) { regs_.r13 = static_cast<uint32_t>(val); return true; }
        if (eq(name, "r14d")) { regs_.r14 = static_cast<uint32_t>(val); return true; }
        if (eq(name, "r15d")) { regs_.r15 = static_cast<uint32_t>(val); return true; }
        if (eq(name, "eip"))  { regs_.rip = static_cast<uint32_t>(val); return true; }

        // 16-bit GPRs (preserves upper 48 bits)
        auto set16 = [](auto& reg, uint64_t v) {
            reg = (reg & ~0xffffULL) | (v & 0xffffULL);
        };
        if (eq(name, "ax"))   { set16(regs_.rax, val); return true; }
        if (eq(name, "bx"))   { set16(regs_.rbx, val); return true; }
        if (eq(name, "cx"))   { set16(regs_.rcx, val); return true; }
        if (eq(name, "dx"))   { set16(regs_.rdx, val); return true; }
        if (eq(name, "si"))   { set16(regs_.rsi, val); return true; }
        if (eq(name, "di"))   { set16(regs_.rdi, val); return true; }
        if (eq(name, "bp"))   { set16(regs_.rbp, val); return true; }
        if (eq(name, "sp"))   { set16(regs_.rsp, val); return true; }
        if (eq(name, "r8w"))  { set16(regs_.r8,  val); return true; }
        if (eq(name, "r9w"))  { set16(regs_.r9,  val); return true; }
        if (eq(name, "r10w")) { set16(regs_.r10, val); return true; }
        if (eq(name, "r11w")) { set16(regs_.r11, val); return true; }
        if (eq(name, "r12w")) { set16(regs_.r12, val); return true; }
        if (eq(name, "r13w")) { set16(regs_.r13, val); return true; }
        if (eq(name, "r14w")) { set16(regs_.r14, val); return true; }
        if (eq(name, "r15w")) { set16(regs_.r15, val); return true; }
        if (eq(name, "ip"))   { set16(regs_.rip, val); return true; }

        // 8-bit GPRs (low byte preserves upper 56 bits)
        auto set8 = [](auto& reg, uint64_t v) {
            reg = (reg & ~0xffULL) | (v & 0xffULL);
        };
        if (eq(name, "al"))   { set8(regs_.rax, val); return true; }
        if (eq(name, "bl"))   { set8(regs_.rbx, val); return true; }
        if (eq(name, "cl"))   { set8(regs_.rcx, val); return true; }
        if (eq(name, "dl"))   { set8(regs_.rdx, val); return true; }
        if (eq(name, "sil"))  { set8(regs_.rsi, val); return true; }
        if (eq(name, "dil"))  { set8(regs_.rdi, val); return true; }
        if (eq(name, "bpl"))  { set8(regs_.rbp, val); return true; }
        if (eq(name, "spl"))  { set8(regs_.rsp, val); return true; }
        if (eq(name, "r8b"))  { set8(regs_.r8,  val); return true; }
        if (eq(name, "r9b"))  { set8(regs_.r9,  val); return true; }
        if (eq(name, "r10b")) { set8(regs_.r10, val); return true; }
        if (eq(name, "r11b")) { set8(regs_.r11, val); return true; }
        if (eq(name, "r12b")) { set8(regs_.r12, val); return true; }
        if (eq(name, "r13b")) { set8(regs_.r13, val); return true; }
        if (eq(name, "r14b")) { set8(regs_.r14, val); return true; }
        if (eq(name, "r15b")) { set8(regs_.r15, val); return true; }

        // 8-bit high registers (ah, bh, ch, dh)
        auto setHigh8 = [](auto& reg, uint64_t v) {
            reg = (reg & ~0xff00ULL) | ((v & 0xffULL) << 8);
        };
        if (eq(name, "ah")) { setHigh8(regs_.rax, val); return true; }
        if (eq(name, "bh")) { setHigh8(regs_.rbx, val); return true; }
        if (eq(name, "ch")) { setHigh8(regs_.rcx, val); return true; }
        if (eq(name, "dh")) { setHigh8(regs_.rdx, val); return true; }

        // Flags (bits 0, 2, 4, 6, 7, 8, 9, 10, 11)
        if (eq(name, "cf")) { setFlag(0, val != 0); return true; }
        if (eq(name, "pf")) { setFlag(2, val != 0); return true; }
        if (eq(name, "af")) { setFlag(4, val != 0); return true; }
        if (eq(name, "zf")) { setFlag(6, val != 0); return true; }
        if (eq(name, "sf")) { setFlag(7, val != 0); return true; }
        if (eq(name, "tf")) { setFlag(8, val != 0); return true; }
        if (eq(name, "if")) { setFlag(9, val != 0); return true; }
        if (eq(name, "df")) { setFlag(10, val != 0); return true; }
        if (eq(name, "of")) { setFlag(11, val != 0); return true; }

        return false;
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
