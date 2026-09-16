#pragma once

#include "Types.hpp"
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <memory>

namespace edb_next {

struct SharedLibraryInfo {
    std::string name;             // e.g. "libm.so.6"
    std::string path;             // full path e.g. "/usr/lib/x86_64-linux-gnu/libm.so.6"
    Address baseAddress{0};       // l_addr (load bias / memory base)
    Address dynamicAddress{0};    // l_ld (dynamic section pointer)
    size_t size{0};               // approximate mapped size
    bool symbolsLoaded{false};
};

enum class LinkerState : uint32_t {
    Consistent = 0,   // RT_CONSISTENT: mapping change is complete
    Add = 1,          // RT_ADD: adding a library mapping
    Delete = 2        // RT_DELETE: removing a library mapping
};

// 64-bit explicit layout for Linux struct r_debug
struct TargetRDebug64 {
    int32_t r_version;
    int32_t pad0;
    uint64_t r_map;
    uint64_t r_brk;
    uint32_t r_state;
    uint32_t pad1;
    uint64_t r_ldbase;
};

// 64-bit explicit layout for Linux struct link_map
struct TargetLinkMap64 {
    uint64_t l_addr;
    uint64_t l_name;
    uint64_t l_ld;
    uint64_t l_next;
    uint64_t l_prev;
};

struct LibraryDiffResult {
    std::vector<SharedLibraryInfo> added;
    std::vector<SharedLibraryInfo> removed;
};

class RendezvousManager {
public:
    using ReadMemFunc = std::function<bool(Address, void*, size_t)>;

    RendezvousManager() = default;
    explicit RendezvousManager(ReadMemFunc read_mem);

    void setReadMemFunc(ReadMemFunc read_mem) noexcept { readMem_ = std::move(read_mem); }

    // Initialize discovery of _r_debug and r_brk (_dl_debug_state)
    bool initialize(Pid pid, Address mainExeBase, const std::string& mainExePath);

    // Refresh debug state from target memory (struct r_debug)
    bool updateDebugState();

    // Enumerate currently loaded libraries from link_map chain
    std::vector<SharedLibraryInfo> enumerateLibraries();

    // Check for changes (added/removed) against previously cached libraries
    LibraryDiffResult detectChanges();

    [[nodiscard]] Address rDebugAddr() const noexcept { return rDebugAddr_; }
    [[nodiscard]] Address rBrkAddr() const noexcept { return rBrkAddr_; }
    [[nodiscard]] LinkerState currentState() const noexcept { return state_; }
    [[nodiscard]] const std::vector<SharedLibraryInfo>& loadedLibraries() const noexcept { return libraries_; }
    [[nodiscard]] bool isInitialized() const noexcept { return rBrkAddr_.value() != 0; }

    void clear();

private:
    ReadMemFunc readMem_;
    Pid pid_{0};
    Address mainExeBase_{0};
    std::string mainExePath_;

    Address rDebugAddr_{0};
    Address rBrkAddr_{0};
    LinkerState state_{LinkerState::Consistent};
    std::vector<SharedLibraryInfo> libraries_;

    bool readStringFromTarget(Address addr, std::string& outStr, size_t maxLen = 512);
    Address findRDebugFromDynamic(Address dynamicAddr);
};

} // namespace edb_next
