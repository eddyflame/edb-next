#pragma once

#include <capstone/capstone.h>
#include "ConfigurationManager.hpp"

namespace edb_next {

/**
 * @brief RAII lease for a pooled / reused Capstone handle.
 * Guarantees zero-allocation and microsecond-level acquisition in hot loops.
 */
class CapstoneLease {
public:
    enum class Source {
        BasicPool,
        DetailPool,
        Standalone
    };

    CapstoneLease() noexcept = default;
    CapstoneLease(csh handle, Source source) noexcept
        : handle_(handle), source_(source) {}

    ~CapstoneLease();

    CapstoneLease(const CapstoneLease&) = delete;
    CapstoneLease& operator=(const CapstoneLease&) = delete;

    CapstoneLease(CapstoneLease&& other) noexcept
        : handle_(other.handle_), source_(other.source_) {
        other.handle_ = 0;
    }

    CapstoneLease& operator=(CapstoneLease&& other) noexcept;

    void reset() noexcept;

    [[nodiscard]] csh get() const noexcept { return handle_; }
    [[nodiscard]] bool isValid() const noexcept { return handle_ != 0; }
    explicit operator bool() const noexcept { return isValid(); }

private:
    csh handle_{0};
    Source source_{Source::Standalone};
};

/**
 * @brief Thread-local Capstone engine pool.
 * Eliminates repeated cs_open / cs_close overhead across debugging and scanning components.
 */
class CapstoneContext {
public:
    /**
     * @brief Acquire a thread-local cached or standalone Capstone handle.
     * @param detail If true, enables CS_OPT_DETAIL for opcode operands & registers.
     * @param syntax Disassembly syntax (Intel vs AT&T).
     * @param mode Architecture mode, defaults to CS_MODE_64.
     * @return CapstoneLease RAII handle.
     */
    [[nodiscard]] static CapstoneLease acquire(
        bool detail = false,
        DisassemblySyntax syntax = DisassemblySyntax::Intel,
        cs_mode mode = CS_MODE_64);
};

} // namespace edb_next
