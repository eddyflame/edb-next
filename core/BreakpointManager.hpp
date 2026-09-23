#pragma once

#include "Types.hpp"
#include "Breakpoint.hpp"
#include <unordered_map>
#include <vector>
#include <functional>
#include <optional>
#include <memory>
#include <array>

namespace edb_next {

class PageGuardManager;

class BreakpointManager {
public:
    using ReadMemFunc = std::function<bool(Address, void*, size_t)>;
    using WriteMemFunc = std::function<bool(Address, const void*, size_t)>;
    using SetHwBpFunc = std::function<bool(int, Address, HardwareBpType, HardwareBpSize)>;
    using ClearHwBpFunc = std::function<bool(int)>;

    BreakpointManager(ReadMemFunc read_mem, WriteMemFunc write_mem,
                      SetHwBpFunc set_hw_bp = nullptr, ClearHwBpFunc clear_hw_bp = nullptr);

    void setPageGuardManager(PageGuardManager* mgr) noexcept { pageGuardMgr_ = mgr; }
    [[nodiscard]] PageGuardManager* pageGuardManager() const noexcept { return pageGuardMgr_; }
    void setAutoFallbackToPageGuard(bool enable) noexcept { autoFallbackToPageGuard_ = enable; }
    [[nodiscard]] bool isAutoFallbackToPageGuard() const noexcept { return autoFallbackToPageGuard_; }

    bool addBreakpoint(Address addr, bool is_internal = false, const std::string& symbol = "");
    bool addBreakpointWithOriginalByte(Address addr, uint8_t origByte, bool is_internal = false, const std::string& symbol = "");
    bool addHardwareBreakpoint(Address addr, HardwareBpType type = HardwareBpType::Execute, HardwareBpSize size = HardwareBpSize::Byte1, const std::string& symbol = "");
    bool removeBreakpoint(Address addr);
    bool enableBreakpoint(Address addr);
    bool disableBreakpoint(Address addr);
    bool toggleBreakpoint(Address addr);

    [[nodiscard]] bool hasBreakpoint(Address addr) const;
    [[nodiscard]] const Breakpoint* getBreakpoint(Address addr) const;
    Breakpoint* getBreakpointMutable(Address addr);
    bool setBreakpointCondition(Address addr, const std::string& condition);
    bool setBreakpointIgnoreCount(Address addr, uint32_t ignoreCount);
    bool setBreakpointLogOnly(Address addr, bool isLogOnly, const std::string& format);
    bool setBreakpointScript(Address addr, const std::string& code, const std::string& language = "python");
    [[nodiscard]] std::vector<Breakpoint> allBreakpoints(bool include_internal = false) const;
    void clear();

    // DR6 status attribution & slot mapping
    [[nodiscard]] std::optional<Address> getHardwareSlotAddress(int slot) const;
    [[nodiscard]] std::optional<Address> attributeDr6(const Dr6Status& dr6) const;

    // Pending breakpoints
    bool addPendingBreakpoint(const std::string& symbol, const std::string& condition = "",
                              const std::string& scriptCode = "", const std::string& scriptLang = "python",
                              bool isLogOnly = false, const std::string& logFormat = "");
    bool removePendingBreakpoint(const std::string& symbol);
    [[nodiscard]] const std::vector<PendingBreakpoint>& allPendingBreakpoints() const noexcept { return pendingBreakpoints_; }
    std::vector<PendingBreakpoint>& allPendingBreakpointsMutable() noexcept { return pendingBreakpoints_; }

    // State machine for stepping over a breakpoint before resuming
    bool prepareStepOver(Address addr);
    bool finishStepOver();
    [[nodiscard]] bool isSteppingOver() const noexcept { return pendingReenableAddr_.has_value(); }

private:
    ReadMemFunc readMem_;
    WriteMemFunc writeMem_;
    SetHwBpFunc setHwBp_;
    ClearHwBpFunc clearHwBp_;
    PageGuardManager* pageGuardMgr_{nullptr};
    bool autoFallbackToPageGuard_{true};

    std::unordered_map<uint64_t, Breakpoint> breakpoints_;
    std::vector<PendingBreakpoint> pendingBreakpoints_;
    std::array<bool, 4> slotOccupied_{false, false, false, false};
    std::optional<Address> pendingReenableAddr_;
};

} // namespace edb_next

