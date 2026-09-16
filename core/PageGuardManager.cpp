#include "PageGuardManager.hpp"
#include <algorithm>

namespace edb_next {

PageGuardManager::PageGuardManager(MprotectFunc mprotect_func)
    : mprotectFunc_(std::move(mprotect_func)) {}

bool PageGuardManager::addGuard(Address addr, size_t size, PageGuardAccess access, int originalProt, const std::string& comment) {
    if (size == 0) size = 1;

    Address pageBase = alignToPage(addr);
    size_t pageSize = calculatePageSpan(addr, size);

    int guardedProt = PROT_NONE;
    switch (access) {
        case PageGuardAccess::NoAccess:
            guardedProt = PROT_NONE;
            break;
        case PageGuardAccess::ReadOnly:
            guardedProt = (originalProt & ~PROT_WRITE);
            if ((guardedProt & PROT_READ) == 0) {
                guardedProt |= PROT_READ;
            }
            break;
        case PageGuardAccess::ExecuteOnly:
            guardedProt = (originalProt & ~(PROT_READ | PROT_WRITE));
            if ((guardedProt & PROT_EXEC) == 0) {
                guardedProt |= PROT_EXEC;
            }
            break;
    }

    if (mprotectFunc_) {
        if (!mprotectFunc_(pageBase, pageSize, guardedProt)) {
            return false;
        }
    }

    PageGuardInfo info{
        .address = addr,
        .size = size,
        .pageBase = pageBase,
        .pageSize = pageSize,
        .originalProt = originalProt,
        .guardedProt = guardedProt,
        .access = access,
        .enabled = true,
        .isOneShot = false,
        .hitCount = 0,
        .condition = "",
        .scriptCode = "",
        .scriptLanguage = "python",
        .comment = comment,
        .isTemporarilyUnprotected = false
    };

    guards_[addr.value()] = info;
    return true;
}

bool PageGuardManager::removeGuard(Address addr) {
    auto it = guards_.find(addr.value());
    if (it == guards_.end()) {
        return false;
    }

    Address pageBase = it->second.pageBase;
    size_t pageSize = it->second.pageSize;
    int originalProt = it->second.originalProt;

    guards_.erase(it);

    // Check if other active guards still share this page
    bool otherActive = false;
    for (const auto& [_, g] : guards_) {
        if (g.enabled && g.pageBase == pageBase) {
            otherActive = true;
            break;
        }
    }

    if (!otherActive && mprotectFunc_) {
        mprotectFunc_(pageBase, pageSize, originalProt);
    }

    return true;
}

bool PageGuardManager::enableGuard(Address addr) {
    auto it = guards_.find(addr.value());
    if (it == guards_.end() || it->second.enabled) {
        return false;
    }

    if (mprotectFunc_) {
        if (!mprotectFunc_(it->second.pageBase, it->second.pageSize, it->second.guardedProt)) {
            return false;
        }
    }

    it->second.enabled = true;
    it->second.isTemporarilyUnprotected = false;
    return true;
}

bool PageGuardManager::disableGuard(Address addr) {
    auto it = guards_.find(addr.value());
    if (it == guards_.end() || !it->second.enabled) {
        return false;
    }

    // Check if other active guards still share this page
    bool otherActive = false;
    for (const auto& [a, g] : guards_) {
        if (a != addr.value() && g.enabled && g.pageBase == it->second.pageBase) {
            otherActive = true;
            break;
        }
    }

    if (!otherActive && mprotectFunc_) {
        mprotectFunc_(it->second.pageBase, it->second.pageSize, it->second.originalProt);
    }

    it->second.enabled = false;
    it->second.isTemporarilyUnprotected = false;
    return true;
}

bool PageGuardManager::toggleGuard(Address addr) {
    auto it = guards_.find(addr.value());
    if (it == guards_.end()) return false;
    return it->second.enabled ? disableGuard(addr) : enableGuard(addr);
}

void PageGuardManager::clear() {
    if (mprotectFunc_) {
        for (const auto& [_, g] : guards_) {
            if (g.enabled) {
                mprotectFunc_(g.pageBase, g.pageSize, g.originalProt);
            }
        }
    }
    guards_.clear();
}

bool PageGuardManager::hasGuard(Address addr) const {
    return guards_.find(addr.value()) != guards_.end();
}

const PageGuardInfo* PageGuardManager::getGuard(Address addr) const {
    auto it = guards_.find(addr.value());
    return it != guards_.end() ? &it->second : nullptr;
}

PageGuardInfo* PageGuardManager::getGuardMutable(Address addr) {
    auto it = guards_.find(addr.value());
    return it != guards_.end() ? &it->second : nullptr;
}

PageGuardInfo* PageGuardManager::findGuardForFault(Address faultAddr) {
    for (auto& [_, g] : guards_) {
        if (g.enabled) {
            if (faultAddr >= g.pageBase && faultAddr < g.pageBase + g.pageSize) {
                return &g;
            }
        }
    }
    return nullptr;
}

const PageGuardInfo* PageGuardManager::findGuardForFault(Address faultAddr) const {
    for (const auto& [_, g] : guards_) {
        if (g.enabled) {
            if (faultAddr >= g.pageBase && faultAddr < g.pageBase + g.pageSize) {
                return &g;
            }
        }
    }
    return nullptr;
}

bool PageGuardManager::isPageGuarded(Address addr) const {
    return findGuardForFault(addr) != nullptr;
}

bool PageGuardManager::isAddressWatched(Address addr) const {
    for (const auto& [_, g] : guards_) {
        if (g.enabled) {
            if (addr >= g.address && addr < g.address + g.size) {
                return true;
            }
        }
    }
    return false;
}

bool PageGuardManager::temporarilyUnprotect(PageGuardInfo* guard) {
    if (!guard || guard->isTemporarilyUnprotected) return false;
    if (mprotectFunc_) {
        if (!mprotectFunc_(guard->pageBase, guard->pageSize, guard->originalProt)) {
            return false;
        }
    }
    guard->isTemporarilyUnprotected = true;
    return true;
}

bool PageGuardManager::reprotect(PageGuardInfo* guard) {
    if (!guard || !guard->isTemporarilyUnprotected) return false;
    if (mprotectFunc_) {
        if (!mprotectFunc_(guard->pageBase, guard->pageSize, guard->guardedProt)) {
            return false;
        }
    }
    guard->isTemporarilyUnprotected = false;
    return true;
}

std::vector<PageGuardInfo> PageGuardManager::allGuards() const {
    std::vector<PageGuardInfo> result;
    result.reserve(guards_.size());
    for (const auto& [_, g] : guards_) {
        result.push_back(g);
    }
    std::sort(result.begin(), result.end(), [](const PageGuardInfo& a, const PageGuardInfo& b) {
        return a.address < b.address;
    });
    return result;
}

} // namespace edb_next
