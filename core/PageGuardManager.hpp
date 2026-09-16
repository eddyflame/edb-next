#pragma once

#include "Types.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>
#include <sys/mman.h>

namespace edb_next {

enum class PageGuardAccess : uint8_t {
    NoAccess = 0,     // PROT_NONE (traps Read, Write, and Execute)
    ReadOnly = 1,     // PROT_READ (traps Write; allows Read and Execute)
    ExecuteOnly = 2   // PROT_EXEC (traps Read and Write; allows Execute)
};

inline std::string pageGuardAccessToString(PageGuardAccess access) {
    switch (access) {
        case PageGuardAccess::NoAccess: return "NoAccess";
        case PageGuardAccess::ReadOnly: return "ReadOnly";
        case PageGuardAccess::ExecuteOnly: return "ExecuteOnly";
    }
    return "NoAccess";
}

inline PageGuardAccess pageGuardAccessFromString(const std::string& str) {
    if (str == "ReadOnly" || str == "ro" || str == "w" || str == "write") {
        return PageGuardAccess::ReadOnly;
    }
    if (str == "ExecuteOnly" || str == "xo" || str == "x" || str == "exec") {
        return PageGuardAccess::ExecuteOnly;
    }
    return PageGuardAccess::NoAccess;
}

struct PageGuardInfo {
    Address address{0};         // Target watched address
    size_t size{1};             // Target watched size
    Address pageBase{0};        // 4096-aligned page base
    size_t pageSize{4096};      // Page span (multiple of 4096)
    int originalProt{PROT_READ | PROT_WRITE}; // Original memory permissions
    int guardedProt{PROT_NONE}; // Restricted permissions applied to page
    PageGuardAccess access{PageGuardAccess::NoAccess};
    bool enabled{true};
    bool isOneShot{false};
    uint32_t hitCount{0};
    std::string condition;
    std::string scriptCode;
    std::string scriptLanguage{"python"};
    std::string comment;
    bool isTemporarilyUnprotected{false};
};

class PageGuardManager {
public:
    using MprotectFunc = std::function<bool(Address, size_t, int)>;

    explicit PageGuardManager(MprotectFunc mprotect_func = nullptr);

    void setMprotectFunc(MprotectFunc func) { mprotectFunc_ = std::move(func); }

    bool addGuard(Address addr, size_t size, PageGuardAccess access, int originalProt, const std::string& comment = "");
    bool removeGuard(Address addr);
    bool enableGuard(Address addr);
    bool disableGuard(Address addr);
    bool toggleGuard(Address addr);
    void clear();

    [[nodiscard]] bool hasGuard(Address addr) const;
    [[nodiscard]] const PageGuardInfo* getGuard(Address addr) const;
    PageGuardInfo* getGuardMutable(Address addr);

    // Checks if any active guard's page covers this fault address
    [[nodiscard]] PageGuardInfo* findGuardForFault(Address faultAddr);
    [[nodiscard]] const PageGuardInfo* findGuardForFault(Address faultAddr) const;

    // Checks if an address falls within any guarded page
    [[nodiscard]] bool isPageGuarded(Address addr) const;

    // Checks if an address falls directly inside user's specific watched range
    [[nodiscard]] bool isAddressWatched(Address addr) const;

    // Temporarily lift protection for single-stepping
    bool temporarilyUnprotect(PageGuardInfo* guard);

    // Re-apply guarded protection after single-stepping
    bool reprotect(PageGuardInfo* guard);

    [[nodiscard]] std::vector<PageGuardInfo> allGuards() const;

    // Page calculation utilities (4KB alignment)
    static Address alignToPage(Address addr) noexcept {
        return Address(addr.value() & ~0xFFFULL);
    }

    static size_t calculatePageSpan(Address addr, size_t size) noexcept {
        uint64_t start = addr.value();
        uint64_t end = start + (size > 0 ? size : 1);
        uint64_t pageStart = start & ~0xFFFULL;
        uint64_t pageEnd = (end + 0xFFFULL) & ~0xFFFULL;
        return static_cast<size_t>(pageEnd - pageStart);
    }

private:
    MprotectFunc mprotectFunc_;
    std::unordered_map<uint64_t, PageGuardInfo> guards_;
};

} // namespace edb_next
