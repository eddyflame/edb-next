#include "BreakpointManager.hpp"
#include <iostream>

namespace edb_next {

BreakpointManager::BreakpointManager(ReadMemFunc read_mem, WriteMemFunc write_mem,
                                     SetHwBpFunc set_hw_bp, ClearHwBpFunc clear_hw_bp)
    : readMem_(std::move(read_mem)),
      writeMem_(std::move(write_mem)),
      setHwBp_(std::move(set_hw_bp)),
      clearHwBp_(std::move(clear_hw_bp)) {}

bool BreakpointManager::addBreakpoint(Address addr, bool is_internal, const std::string& symbol) {
    if (hasBreakpoint(addr)) {
        return enableBreakpoint(addr);
    }

    uint8_t original_byte = 0;
    if (!readMem_(addr, &original_byte, 1)) {
        return false;
    }

    constexpr uint8_t int3_opcode = 0xCC;
    if (!writeMem_(addr, &int3_opcode, 1)) {
        return false;
    }

    Breakpoint bp{
        .address = addr,
        .originalByte = original_byte,
        .enabled = true,
        .isInternal = is_internal,
        .hitCount = 0,
        .ignoreCount = 0,
        .condition = {},
        .isLogOnly = false,
        .logFormat = {},
        .scriptCode = {},
        .scriptLanguage = "python",
        .type = BreakpointType::Software,
        .hardwareSlot = -1,
        .symbol = symbol
    };

    breakpoints_[addr.value()] = bp;
    return true;
}

bool BreakpointManager::addHardwareBreakpoint(Address addr, HardwareBpType type, HardwareBpSize size, const std::string& symbol) {
    if (!setHwBp_ || hasBreakpoint(addr)) {
        return false;
    }

    // Find available hardware slot 0..3
    int free_slot = -1;
    for (int i = 0; i < 4; ++i) {
        if (!slotOccupied_[i]) {
            free_slot = i;
            break;
        }
    }

    if (free_slot < 0) {
        return false; // All 4 hardware slots occupied
    }

    if (!setHwBp_(free_slot, addr, type, size)) {
        return false;
    }

    slotOccupied_[free_slot] = true;

    BreakpointType bp_type = BreakpointType::HardwareExecute;
    if (type == HardwareBpType::Write) bp_type = BreakpointType::HardwareWrite;
    else if (type == HardwareBpType::ReadWrite) bp_type = BreakpointType::HardwareReadWrite;

    Breakpoint bp{
        .address = addr,
        .originalByte = 0,
        .enabled = true,
        .isInternal = false,
        .hitCount = 0,
        .ignoreCount = 0,
        .condition = {},
        .isLogOnly = false,
        .logFormat = {},
        .scriptCode = {},
        .scriptLanguage = "python",
        .type = bp_type,
        .hardwareSlot = free_slot,
        .symbol = symbol
    };

    breakpoints_[addr.value()] = bp;
    return true;
}

bool BreakpointManager::removeBreakpoint(Address addr) {
    auto it = breakpoints_.find(addr.value());
    if (it == breakpoints_.end()) {
        return false;
    }

    if (it->second.type != BreakpointType::Software) {
        if (it->second.hardwareSlot >= 0 && it->second.hardwareSlot < 4) {
            if (clearHwBp_) {
                clearHwBp_(it->second.hardwareSlot);
            }
            slotOccupied_[it->second.hardwareSlot] = false;
        }
    } else {
        if (it->second.enabled) {
            writeMem_(addr, &it->second.originalByte, 1);
        }
    }

    breakpoints_.erase(it);
    return true;
}

bool BreakpointManager::enableBreakpoint(Address addr) {
    auto it = breakpoints_.find(addr.value());
    if (it == breakpoints_.end() || it->second.enabled) {
        return false;
    }

    if (it->second.type != BreakpointType::Software) {
        // Re-enable hardware breakpoint
        if (it->second.hardwareSlot >= 0 && it->second.hardwareSlot < 4) {
            HardwareBpType hw_type = HardwareBpType::Execute;
            if (it->second.type == BreakpointType::HardwareWrite) hw_type = HardwareBpType::Write;
            else if (it->second.type == BreakpointType::HardwareReadWrite) hw_type = HardwareBpType::ReadWrite;

            if (setHwBp_ && setHwBp_(it->second.hardwareSlot, addr, hw_type, HardwareBpSize::Byte1)) {
                slotOccupied_[it->second.hardwareSlot] = true;
                it->second.enabled = true;
                return true;
            }
        }
        return false;
    }

    constexpr uint8_t int3_opcode = 0xCC;
    if (!writeMem_(addr, &int3_opcode, 1)) {
        return false;
    }

    it->second.enabled = true;
    return true;
}

bool BreakpointManager::disableBreakpoint(Address addr) {
    auto it = breakpoints_.find(addr.value());
    if (it == breakpoints_.end() || !it->second.enabled) {
        return false;
    }

    if (it->second.type != BreakpointType::Software) {
        if (it->second.hardwareSlot >= 0 && it->second.hardwareSlot < 4) {
            if (clearHwBp_) {
                clearHwBp_(it->second.hardwareSlot);
            }
            slotOccupied_[it->second.hardwareSlot] = false;
            it->second.enabled = false;
            return true;
        }
        return false;
    }

    if (!writeMem_(addr, &it->second.originalByte, 1)) {
        return false;
    }

    it->second.enabled = false;
    return true;
}

bool BreakpointManager::toggleBreakpoint(Address addr) {
    if (hasBreakpoint(addr)) {
        return removeBreakpoint(addr);
    }
    return addBreakpoint(addr);
}

bool BreakpointManager::hasBreakpoint(Address addr) const {
    return breakpoints_.contains(addr.value());
}

const Breakpoint* BreakpointManager::getBreakpoint(Address addr) const {
    auto it = breakpoints_.find(addr.value());
    if (it != breakpoints_.end()) {
        return &it->second;
    }
    return nullptr;
}

Breakpoint* BreakpointManager::getBreakpointMutable(Address addr) {
    auto it = breakpoints_.find(addr.value());
    if (it != breakpoints_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool BreakpointManager::setBreakpointCondition(Address addr, const std::string& condition) {
    auto* bp = getBreakpointMutable(addr);
    if (!bp) return false;
    bp->condition = condition;
    return true;
}

bool BreakpointManager::setBreakpointIgnoreCount(Address addr, uint32_t ignoreCount) {
    auto* bp = getBreakpointMutable(addr);
    if (!bp) return false;
    bp->ignoreCount = ignoreCount;
    return true;
}

bool BreakpointManager::setBreakpointLogOnly(Address addr, bool isLogOnly, const std::string& format) {
    auto* bp = getBreakpointMutable(addr);
    if (!bp) return false;
    bp->isLogOnly = isLogOnly;
    bp->logFormat = format;
    return true;
}

bool BreakpointManager::setBreakpointScript(Address addr, const std::string& code, const std::string& language) {
    auto* bp = getBreakpointMutable(addr);
    if (!bp) return false;
    bp->scriptCode = code;
    bp->scriptLanguage = language.empty() ? "python" : language;
    return true;
}

std::vector<Breakpoint> BreakpointManager::allBreakpoints(bool include_internal) const {
    std::vector<Breakpoint> result;
    result.reserve(breakpoints_.size());
    for (const auto& [_, bp] : breakpoints_) {
        if (!include_internal && bp.isInternal) {
            continue;
        }
        result.push_back(bp);
    }
    return result;
}

bool BreakpointManager::addPendingBreakpoint(const std::string& symbol, const std::string& condition,
                                             const std::string& scriptCode, const std::string& scriptLang,
                                             bool isLogOnly, const std::string& logFormat) {
    if (symbol.empty()) return false;
    for (const auto& pb : pendingBreakpoints_) {
        if (pb.symbol == symbol) {
            return false;
        }
    }
    pendingBreakpoints_.push_back(PendingBreakpoint{
        .symbol = symbol,
        .enabled = true,
        .condition = condition,
        .scriptCode = scriptCode,
        .scriptLanguage = scriptLang.empty() ? "python" : scriptLang,
        .isLogOnly = isLogOnly,
        .logFormat = logFormat
    });
    return true;
}

bool BreakpointManager::removePendingBreakpoint(const std::string& symbol) {
    auto it = std::remove_if(pendingBreakpoints_.begin(), pendingBreakpoints_.end(),
        [&](const PendingBreakpoint& pb) { return pb.symbol == symbol; });
    if (it != pendingBreakpoints_.end()) {
        pendingBreakpoints_.erase(it, pendingBreakpoints_.end());
        return true;
    }
    return false;
}

void BreakpointManager::clear() {
    for (auto& [_, bp] : breakpoints_) {
        if (bp.type != BreakpointType::Software) {
            if (bp.hardwareSlot >= 0 && clearHwBp_) {
                clearHwBp_(bp.hardwareSlot);
            }
        } else {
            if (bp.enabled) {
                writeMem_(bp.address, &bp.originalByte, 1);
            }
        }
    }
    slotOccupied_.fill(false);
    breakpoints_.clear();
    pendingBreakpoints_.clear();
    pendingReenableAddr_.reset();
}

bool BreakpointManager::prepareStepOver(Address addr) {
    auto it = breakpoints_.find(addr.value());
    if (it != breakpoints_.end() && it->second.enabled && it->second.type == BreakpointType::Software) {
        // Temporarily put original byte back so single step executes original instruction
        if (!writeMem_(addr, &it->second.originalByte, 1)) {
            return false;
        }
        it->second.enabled = false;
        pendingReenableAddr_ = addr;
        return true;
    }
    return false;
}

bool BreakpointManager::finishStepOver() {
    if (pendingReenableAddr_.has_value()) {
        Address addr = *pendingReenableAddr_;
        pendingReenableAddr_.reset();

        auto it = breakpoints_.find(addr.value());
        if (it != breakpoints_.end() && !it->second.enabled && it->second.type == BreakpointType::Software) {
            constexpr uint8_t int3_opcode = 0xCC;
            writeMem_(addr, &int3_opcode, 1);
            it->second.enabled = true;
            return true;
        }
    }
    return false;
}

} // namespace edb_next
