#pragma once

#include <cstdint>
#include <string>
#include <iomanip>
#include <sstream>
#include <sys/types.h>
#include <compare>
#include <utility>

namespace edb_next {

using Pid = pid_t;
using Tid = pid_t;

/**
 * @brief Lightweight Result type for error handling
 */
template<typename T, typename E = std::string>
struct Result {
    bool success{false};
    T value{};
    E error{};

    static Result<T, E> Ok(T val) { return Result<T, E>{true, std::move(val), {}}; }
    static Result<T, E> Err(E err) { return Result<T, E>{false, {}, std::move(err)}; }

    explicit operator bool() const noexcept { return success; }
};

template<typename E>
struct Result<void, E> {
    bool success{false};
    E error{};

    static Result<void, E> Ok() { return Result<void, E>{true, {}}; }
    static Result<void, E> Err(E err) { return Result<void, E>{false, std::move(err)}; }

    explicit operator bool() const noexcept { return success; }
};

/**
 * @brief Strongly-typed 64-bit Address abstraction
 */
class Address {
public:
    constexpr Address() : value_(0) {}
    constexpr explicit Address(uint64_t val) : value_(val) {}

    [[nodiscard]] constexpr uint64_t value() const noexcept { return value_; }
    [[nodiscard]] constexpr bool isNull() const noexcept { return value_ == 0; }

    [[nodiscard]] std::string toHex(bool prefix = true) const {
        std::ostringstream oss;
        if (prefix) oss << "0x";
        oss << std::hex << std::setw(16) << std::setfill('0') << value_;
        return oss.str();
    }

    constexpr auto operator<=>(const Address& other) const = default;

    constexpr Address operator+(uint64_t offset) const noexcept {
        return Address(value_ + offset);
    }

    constexpr Address operator-(uint64_t offset) const noexcept {
        return Address(value_ - offset);
    }

    constexpr int64_t operator-(Address other) const noexcept {
        return static_cast<int64_t>(value_ - other.value_);
    }

    constexpr Address& operator+=(uint64_t offset) noexcept {
        value_ += offset;
        return *this;
    }

    constexpr Address& operator-=(uint64_t offset) noexcept {
        value_ -= offset;
        return *this;
    }

private:
    uint64_t value_{0};
};

enum class SessionState {
    Stopped,     // Not attached or launched
    Running,     // Actively running
    Paused,      // Suspended on a breakpoint or signal
    Terminated   // Child process has exited
};

enum class StopReason {
    None,
    Breakpoint,
    SingleStep,
    Signal,
    ProcessExit,
    ThreadCreated,
    Error
};

enum class HardwareBpType : uint8_t {
    Execute = 0,    // 00b
    Write = 1,      // 01b
    ReadWrite = 3   // 11b
};

enum class HardwareBpSize : uint8_t {
    Byte1 = 0,      // 00b
    Byte2 = 1,      // 01b
    Byte4 = 3,      // 11b
    Byte8 = 2       // 10b
};

struct DebugEvent {
    Pid pid{0};
    Tid tid{0};
    StopReason reason{StopReason::None};
    int signal{0};
    Address address{0};
    int exitCode{0};
    std::string message;

    [[nodiscard]] std::string describe() const {
        std::ostringstream oss;
        switch (reason) {
            case StopReason::Breakpoint:
                oss << "Breakpoint hit at " << address.toHex();
                break;
            case StopReason::SingleStep:
                oss << "Single step completed at " << address.toHex();
                break;
            case StopReason::Signal:
                oss << "Signal " << signal << " received at " << address.toHex();
                break;
            case StopReason::ProcessExit:
                oss << "Process exited with code " << exitCode;
                break;
            case StopReason::ThreadCreated:
                oss << "Thread event on TID " << tid;
                break;
            case StopReason::Error:
                oss << "Error: " << message;
                break;
            default:
                oss << "Event: " << message;
                break;
        }
        return oss.str();
    }
};

struct MemoryRegion {
    Address start{0};
    Address end{0};
    std::string permissions; // e.g. "r-xp"
    uint64_t offset{0};
    std::string pathname;

    [[nodiscard]] uint64_t size() const {
        return end.value() - start.value();
    }

    [[nodiscard]] bool isReadable() const { return !permissions.empty() && permissions[0] == 'r'; }
    [[nodiscard]] bool isWritable() const { return permissions.size() > 1 && permissions[1] == 'w'; }
    [[nodiscard]] bool isExecutable() const { return permissions.size() > 2 && permissions[2] == 'x'; }

    [[nodiscard]] bool contains(Address addr) const {
        return addr >= start && addr < end;
    }
};

struct ThreadInfo {
    Tid tid{0};
    std::string name;
    std::string state;
    Address rip{0};
    Address rsp{0};
    std::string symbol;
    bool isActive{false};
};

} // namespace edb_next
