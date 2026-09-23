#pragma once

#include <cstdint>
#include <string>
#include <format>
#include <concepts>
#include <span>
#include <optional>
#include <sys/types.h>
#include <compare>
#include <utility>

#include <expected>

#if __has_include(<QString>)
#include <QString>
#endif

namespace edb_next {

using Pid = pid_t;
using Tid = pid_t;

using ByteSpan = std::span<const uint8_t>;
using MutableByteSpan = std::span<uint8_t>;

/**
 * @brief Modern Result type for error handling with C++23 std::expected compatibility & monads
 */
template<typename T, typename E = std::string>
struct Result {
    bool success{false};
    T value{};
    E error{};

    Result() = default;
    Result(bool s, T val, E err) : success(s), value(std::move(val)), error(std::move(err)) {}

    // Implicit/explicit bridge with std::expected
    Result(const std::expected<T, E>& exp)
        : success(exp.has_value()),
          value(exp.has_value() ? *exp : T{}),
          error(!exp.has_value() ? exp.error() : E{}) {}

    Result(std::expected<T, E>&& exp)
        : success(exp.has_value()),
          value(exp.has_value() ? std::move(*exp) : T{}),
          error(!exp.has_value() ? std::move(exp.error()) : E{}) {}

    [[nodiscard]] std::expected<T, E> toExpected() const {
        if (success) return value;
        return std::unexpected(error);
    }

    static Result<T, E> Ok(T val) { return Result<T, E>{true, std::move(val), {}}; }
    static Result<T, E> Err(E err) { return Result<T, E>{false, {}, std::move(err)}; }

    explicit operator bool() const noexcept { return success; }

    // C++23 Monadic Operations
    template<typename F>
    auto and_then(F&& f) const {
        using RetType = std::decay_t<decltype(f(value))>;
        if (success) {
            return f(value);
        }
        return RetType::Err(error);
    }

    template<typename F>
    auto transform(F&& f) const {
        using NewVal = std::decay_t<decltype(f(value))>;
        if (success) {
            return Result<NewVal, E>::Ok(f(value));
        }
        return Result<NewVal, E>::Err(error);
    }

    template<typename F>
    Result<T, E> or_else(F&& f) const {
        if (success) {
            return *this;
        }
        return f(error);
    }
};

template<typename E>
struct Result<void, E> {
    bool success{false};
    E error{};

    Result() = default;
    Result(bool s, E err) : success(s), error(std::move(err)) {}

    Result(const std::expected<void, E>& exp)
        : success(exp.has_value()),
          error(!exp.has_value() ? exp.error() : E{}) {}

    Result(std::expected<void, E>&& exp)
        : success(exp.has_value()),
          error(!exp.has_value() ? std::move(exp.error()) : E{}) {}

    [[nodiscard]] std::expected<void, E> toExpected() const {
        if (success) return {};
        return std::unexpected(error);
    }

    static Result<void, E> Ok() { return Result<void, E>{true, {}}; }
    static Result<void, E> Err(E err) { return Result<void, E>{false, std::move(err)}; }

    explicit operator bool() const noexcept { return success; }

    template<typename F>
    auto and_then(F&& f) const {
        using RetType = std::decay_t<decltype(f())>;
        if (success) {
            return f();
        }
        return RetType::Err(error);
    }

    template<typename F>
    Result<void, E> or_else(F&& f) const {
        if (success) {
            return *this;
        }
        return f(error);
    }
};

// ==========================================
// C++20 Core Concepts
// ==========================================
template<typename T>
concept TriviallyCopyable = std::is_trivially_copyable_v<T>;

template<typename T>
concept NumericType = std::is_arithmetic_v<T>;

/**
 * @brief Strongly-typed 64-bit Address abstraction
 */
class Address {
public:
    constexpr Address() : value_(0) {}
    constexpr explicit Address(uint64_t val) : value_(val) {}

    [[nodiscard]] constexpr uint64_t value() const noexcept { return value_; }
    [[nodiscard]] constexpr bool isNull() const noexcept { return value_ == 0; }

    [[nodiscard]] std::string toHex(bool prefix = true, bool colon = false) const {
        if (colon) {
            uint32_t hi = static_cast<uint32_t>(value_ >> 32);
            uint32_t lo = static_cast<uint32_t>(value_ & 0xffffffffULL);
            return prefix ? std::format("0x{:08x}:{:08x}", hi, lo) : std::format("{:08x}:{:08x}", hi, lo);
        }
        return prefix ? std::format("0x{:016x}", value_) : std::format("{:016x}", value_);
    }

#if __has_include(<QString>)
    [[nodiscard]] QString toQString(bool prefix = true, bool colon = false) const {
        if (colon) {
            uint32_t hi = static_cast<uint32_t>(value_ >> 32);
            uint32_t lo = static_cast<uint32_t>(value_ & 0xffffffffULL);
            return prefix ? QString::asprintf("0x%08x:%08x", hi, lo)
                          : QString::asprintf("%08x:%08x", hi, lo);
        }
        return prefix ? QString::asprintf("0x%016llx", static_cast<unsigned long long>(value_))
                      : QString::asprintf("%016llx", static_cast<unsigned long long>(value_));
    }
#endif

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
    ProcessForked,
    Error
};

enum class FollowForkMode : uint8_t {
    Parent = 0,
    Child = 1,
    Both = 2
};

inline const char* followForkModeToString(FollowForkMode mode) {
    switch (mode) {
        case FollowForkMode::Parent: return "parent";
        case FollowForkMode::Child:  return "child";
        case FollowForkMode::Both:   return "both";
    }
    return "parent";
}

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
    Pid childPid{0};
    StopReason reason{StopReason::None};
    int signal{0};
    Address address{0};
    int exitCode{0};
    std::string message;

    [[nodiscard]] std::string describe() const {
        switch (reason) {
            case StopReason::Breakpoint:
                return std::format("Breakpoint hit at {}", address.toHex());
            case StopReason::SingleStep:
                return std::format("Single step completed at {}", address.toHex());
            case StopReason::Signal:
                return std::format("Signal {} received at {}", signal, address.toHex());
            case StopReason::ProcessExit:
                return std::format("Process exited with code {}", exitCode);
            case StopReason::ThreadCreated:
                return std::format("Thread event on TID {}", tid);
            case StopReason::ProcessForked:
                return std::format("Process forked child PID {}", childPid);
            case StopReason::Error:
                return std::format("Error: {}", message);
            default:
                return std::format("Event: {}", message);
        }
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
    bool isFrozen{false};
};

} // namespace edb_next

template<>
struct std::formatter<edb_next::Address> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }
    auto format(const edb_next::Address& addr, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "0x{:016x}", addr.value());
    }
};
