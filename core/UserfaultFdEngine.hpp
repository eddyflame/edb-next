#pragma once

#include "Types.hpp"
#include "UniqueFd.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <mutex>

namespace edb_next {

enum class UffdFaultMode : uint8_t {
    Missing = 1 << 0,       // Triggers when page is missing / unmapped
    WriteProtect = 1 << 1,  // Triggers when page is written (stealth write watchpoint)
    All = Missing | WriteProtect
};

struct UffdFaultEvent {
    Address faultAddress{0};
    bool isWrite{false};
    bool isWriteProtected{false};
    uint64_t threadId{0};
    std::chrono::system_clock::time_point timestamp;
    std::string details;
};

struct GuardedMemoryRegion {
    Address baseAddress{0};
    size_t size{4096};
    UffdFaultMode mode{UffdFaultMode::WriteProtect};
    std::string comment;
    uint32_t hitCount{0};
    bool active{true};
};

class UserfaultFdEngine {
public:
    UserfaultFdEngine();
    ~UserfaultFdEngine();

    // Check if Linux kernel and environment allows unprivileged or privileged userfaultfd
    [[nodiscard]] static bool isSystemSupported() noexcept;

    // Initialize userfaultfd descriptor with non-blocking flag and handshake UFFD_API
    bool initialize();
    void shutdown();

    [[nodiscard]] bool isInitialized() const noexcept { return isInitialized_; }
    [[nodiscard]] bool isSimulated() const noexcept { return isSimulated_; }
    [[nodiscard]] int fd() const noexcept { return uffd_.get(); }

    // Register a memory range for userfaultfd handling
    bool registerRange(Address addr, size_t size, UffdFaultMode mode = UffdFaultMode::WriteProtect, const std::string& comment = "");
    bool unregisterRange(Address addr);

    [[nodiscard]] bool isAddressWatched(Address addr) const noexcept;
    [[nodiscard]] std::optional<GuardedMemoryRegion> findWatchedRegion(Address addr) const;
    [[nodiscard]] std::vector<GuardedMemoryRegion> allWatchedRanges() const;

    // Poll for pending userfaultfd fault events (returns non-empty vector if faults arrived)
    [[nodiscard]] std::vector<UffdFaultEvent> pollEvents(int timeoutMs = 0);

    // Resolve fault by providing a zero-page or acknowledging write-protect
    bool resolveFault(Address faultAddr, void* copySource = nullptr, size_t copySize = 0);

    // Testing and unprivileged simulation support
    void simulateFault(const UffdFaultEvent& ev);
    void clearSimulationQueue();

private:
    UniqueFd uffd_;
    bool isInitialized_{false};
    bool isSimulated_{false};
    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, GuardedMemoryRegion> watchedRanges_;
    std::vector<UffdFaultEvent> simulatedEvents_;
};

} // namespace edb_next
