#include "RendezvousManager.hpp"
#include "ElfParser.hpp"
#include <elf.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace edb_next {

RendezvousManager::RendezvousManager(ReadMemFunc read_mem)
    : readMem_(std::move(read_mem)) {}

void RendezvousManager::clear() {
    pid_ = 0;
    mainExeBase_ = Address(0);
    mainExePath_.clear();
    rDebugAddr_ = Address(0);
    rBrkAddr_ = Address(0);
    state_ = LinkerState::Consistent;
    libraries_.clear();
}

bool RendezvousManager::readStringFromTarget(Address addr, std::string& outStr, size_t maxLen) {
    outStr.clear();
    if (addr.value() == 0 || !readMem_) return false;

    std::vector<char> buf(maxLen, 0);
    // Read in chunks or full maxLen
    if (!readMem_(addr, buf.data(), maxLen)) {
        return false;
    }

    for (size_t i = 0; i < maxLen; ++i) {
        if (buf[i] == '\0') {
            outStr.assign(buf.data(), i);
            return true;
        }
    }
    outStr.assign(buf.data(), maxLen);
    return true;
}

Address RendezvousManager::findRDebugFromDynamic(Address dynamicAddr) {
    if (dynamicAddr.value() == 0 || !readMem_) return Address(0);

    const size_t maxDyn = 128;
    std::vector<Elf64_Dyn> dynEntries(maxDyn);
    if (!readMem_(dynamicAddr, dynEntries.data(), maxDyn * sizeof(Elf64_Dyn))) {
        return Address(0);
    }

    for (const auto& dyn : dynEntries) {
        if (dyn.d_tag == DT_NULL) {
            break;
        }
        if (dyn.d_tag == DT_DEBUG && dyn.d_un.d_ptr != 0) {
            return Address(dyn.d_un.d_ptr);
        }
    }
    return Address(0);
}

bool RendezvousManager::initialize(Pid pid, Address mainExeBase, const std::string& mainExePath) {
    clear();
    pid_ = pid;
    mainExeBase_ = mainExeBase;
    mainExePath_ = mainExePath;

    if (pid <= 0 || !readMem_) return false;

    // Track 1: Try reading DT_DEBUG from the main executable's PT_DYNAMIC section
    if (!mainExePath.empty()) {
        ElfParser mainElf;
        if (mainElf.loadBinary(mainExePath, mainExeBase)) {
            for (const auto& ph : mainElf.programHeaders()) {
                if (ph.type == PT_DYNAMIC) {
                    Address dynAddr = ph.vaddr;
                    Address rdbg = findRDebugFromDynamic(dynAddr);
                    if (rdbg.value() != 0) {
                        rDebugAddr_ = rdbg;
                    }
                    break;
                }
            }
        }
    }

    // Track 2: If DT_DEBUG is not yet populated or rDebugAddr_ is 0,
    // locate ld-linux in /proc/<pid>/maps and parse dynamic symbols
    std::string ldPath;
    Address ldBase{0};
    std::ifstream mapsFile("/proc/" + std::to_string(pid) + "/maps");
    if (mapsFile.is_open()) {
        std::string line;
        while (std::getline(mapsFile, line)) {
            if (line.find("ld-linux") != std::string::npos || line.find("ld-2.") != std::string::npos) {
                std::istringstream iss(line);
                std::string addrRange, perms, offset, dev, inode, pathname;
                if (iss >> addrRange >> perms >> offset >> dev >> inode >> pathname) {
                    if (!pathname.empty() && pathname[0] == '/') {
                        ldPath = pathname;
                        auto dash = addrRange.find('-');
                        if (dash != std::string::npos) {
                            ldBase = Address(std::strtoull(addrRange.substr(0, dash).c_str(), nullptr, 16));
                        }
                        break;
                    }
                }
            }
        }
    }

    if (!ldPath.empty() && ldBase.value() != 0) {
        ElfParser ldElf;
        if (ldElf.loadBinary(ldPath, ldBase)) {
            if (rDebugAddr_.value() == 0) {
                if (auto sym = ldElf.findSymbolAddress("_r_debug")) {
                    rDebugAddr_ = *sym;
                }
            }
            if (rBrkAddr_.value() == 0) {
                if (auto sym = ldElf.findSymbolAddress("_dl_debug_state")) {
                    rBrkAddr_ = *sym;
                }
            }
        }
    }

    // Track 3: If rDebugAddr_ is known and rBrkAddr_ is still 0, read r_brk from struct r_debug
    if (rDebugAddr_.value() != 0 && rBrkAddr_.value() == 0) {
        TargetRDebug64 dbg{};
        if (readMem_(rDebugAddr_, &dbg, sizeof(dbg))) {
            if (dbg.r_brk != 0) {
                rBrkAddr_ = Address(dbg.r_brk);
            }
        }
    }

    // Cache initial loaded libraries
    if (rDebugAddr_.value() != 0) {
        libraries_ = enumerateLibraries();
    }

    return (rBrkAddr_.value() != 0 || rDebugAddr_.value() != 0);
}

bool RendezvousManager::updateDebugState() {
    if (rDebugAddr_.value() == 0) {
        // Retry discovering rDebugAddr_ from PT_DYNAMIC if it wasn't populated earlier
        if (!mainExePath_.empty()) {
            ElfParser mainElf;
            if (mainElf.loadBinary(mainExePath_, mainExeBase_)) {
                for (const auto& ph : mainElf.programHeaders()) {
                    if (ph.type == PT_DYNAMIC) {
                        Address dynAddr = ph.vaddr;
                        Address rdbg = findRDebugFromDynamic(dynAddr);
                        if (rdbg.value() != 0) {
                            rDebugAddr_ = rdbg;
                        }
                        break;
                    }
                }
            }
        }
    }

    if (rDebugAddr_.value() == 0 || !readMem_) return false;

    TargetRDebug64 dbg{};
    if (!readMem_(rDebugAddr_, &dbg, sizeof(dbg))) {
        return false;
    }

    state_ = static_cast<LinkerState>(dbg.r_state);
    if (rBrkAddr_.value() == 0 && dbg.r_brk != 0) {
        rBrkAddr_ = Address(dbg.r_brk);
    }
    return true;
}

std::vector<SharedLibraryInfo> RendezvousManager::enumerateLibraries() {
    std::vector<SharedLibraryInfo> result;
    if (rDebugAddr_.value() == 0 || !readMem_) return result;

    TargetRDebug64 dbg{};
    if (!readMem_(rDebugAddr_, &dbg, sizeof(dbg))) {
        return result;
    }

    state_ = static_cast<LinkerState>(dbg.r_state);
    uint64_t cur = dbg.r_map;
    size_t count = 0;
    const size_t maxModules = 1024; // safety threshold against corrupted cyclic linked lists

    while (cur != 0 && count++ < maxModules) {
        TargetLinkMap64 mapNode{};
        if (!readMem_(Address(cur), &mapNode, sizeof(mapNode))) {
            break;
        }

        std::string libPath;
        if (mapNode.l_name != 0) {
            readStringFromTarget(Address(mapNode.l_name), libPath);
        }

        // If l_name is empty, this node represents the main executable
        if (libPath.empty()) {
            libPath = mainExePath_;
        }

        std::string libName = libPath;
        auto slash = libPath.find_last_of('/');
        if (slash != std::string::npos) {
            libName = libPath.substr(slash + 1);
        }

        SharedLibraryInfo info{
            .name = libName,
            .path = libPath,
            .baseAddress = Address(mapNode.l_addr),
            .dynamicAddress = Address(mapNode.l_ld),
            .size = 0,
            .symbolsLoaded = false
        };

        result.push_back(info);
        cur = mapNode.l_next;
    }

    return result;
}

LibraryDiffResult RendezvousManager::detectChanges() {
    LibraryDiffResult diff;
    auto current = enumerateLibraries();

    for (const auto& curLib : current) {
        bool found = false;
        for (const auto& oldLib : libraries_) {
            if (oldLib.path == curLib.path && oldLib.baseAddress == curLib.baseAddress) {
                found = true;
                break;
            }
        }
        if (!found) {
            diff.added.push_back(curLib);
        }
    }

    for (const auto& oldLib : libraries_) {
        bool found = false;
        for (const auto& curLib : current) {
            if (curLib.path == oldLib.path && curLib.baseAddress == oldLib.baseAddress) {
                found = true;
                break;
            }
        }
        if (!found) {
            diff.removed.push_back(oldLib);
        }
    }

    libraries_ = current;
    return diff;
}

} // namespace edb_next
