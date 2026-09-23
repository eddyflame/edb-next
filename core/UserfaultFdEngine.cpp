#include "UserfaultFdEngine.hpp"
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <linux/userfaultfd.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <format>
#include <cstring>
#include <iostream>

namespace edb_next {

UserfaultFdEngine::UserfaultFdEngine() = default;

UserfaultFdEngine::~UserfaultFdEngine() {
    shutdown();
}

bool UserfaultFdEngine::isSystemSupported() noexcept {
    if (::geteuid() == 0) {
        return true;
    }
    std::ifstream file("/proc/sys/vm/unprivileged_userfaultfd");
    if (file.is_open()) {
        int val = 0;
        if (file >> val && val == 1) {
            return true;
        }
    }
    return false;
}

bool UserfaultFdEngine::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (isInitialized_) {
        return true;
    }

    int fd = static_cast<int>(::syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK));
    if (fd < 0) {
        // Fall back gracefully to simulation sandbox mode for unprivileged environments
        isSimulated_ = true;
        isInitialized_ = true;
        return true;
    }

    struct uffdio_api api{};
    api.api = UFFD_API;
    api.features = UFFD_FEATURE_PAGEFAULT_FLAG_WP;

    if (::ioctl(fd, UFFDIO_API, &api) < 0) {
        ::close(fd);
        isSimulated_ = true;
        isInitialized_ = true;
        return true;
    }

    uffd_ = UniqueFd(fd);
    isInitialized_ = true;
    isSimulated_ = false;
    return true;
}

void UserfaultFdEngine::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    watchedRanges_.clear();
    simulatedEvents_.clear();
    uffd_.reset();
    isInitialized_ = false;
    isSimulated_ = false;
}

bool UserfaultFdEngine::registerRange(Address addr, size_t size, UffdFaultMode mode, const std::string& comment) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isInitialized_) {
        return false;
    }

    // Align to 4KB page boundary
    uint64_t start = addr.value() & ~0xFFFULL;
    size_t aligned_size = (size + 0xFFFULL) & ~0xFFFULL;
    if (aligned_size == 0) aligned_size = 4096;

    if (!isSimulated_ && uffd_.isValid()) {
        struct uffdio_register uffd_reg{};
        uffd_reg.range.start = start;
        uffd_reg.range.len = aligned_size;
        uffd_reg.mode = 0;
        if (static_cast<uint8_t>(mode) & static_cast<uint8_t>(UffdFaultMode::Missing)) {
            uffd_reg.mode |= UFFDIO_REGISTER_MODE_MISSING;
        }
        if (static_cast<uint8_t>(mode) & static_cast<uint8_t>(UffdFaultMode::WriteProtect)) {
            uffd_reg.mode |= UFFDIO_REGISTER_MODE_WP;
        }

        if (::ioctl(uffd_.get(), UFFDIO_REGISTER, &uffd_reg) < 0) {
            // If kernel rejects real registration, mark as simulated region
            isSimulated_ = true;
        }
    }

    GuardedMemoryRegion region{
        .baseAddress = Address(start),
        .size = aligned_size,
        .mode = mode,
        .comment = comment,
        .hitCount = 0,
        .active = true
    };

    watchedRanges_[start] = region;
    return true;
}

bool UserfaultFdEngine::unregisterRange(Address addr) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t start = addr.value() & ~0xFFFULL;
    auto it = watchedRanges_.find(start);
    if (it == watchedRanges_.end()) {
        return false;
    }

    if (!isSimulated_ && uffd_.isValid()) {
        struct uffdio_range range{};
        range.start = start;
        range.len = it->second.size;
        ::ioctl(uffd_.get(), UFFDIO_UNREGISTER, &range);
    }

    watchedRanges_.erase(it);
    return true;
}

bool UserfaultFdEngine::isAddressWatched(Address addr) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t val = addr.value();
    for (const auto& [base, region] : watchedRanges_) {
        if (region.active && val >= base && val < (base + region.size)) {
            return true;
        }
    }
    return false;
}

std::optional<GuardedMemoryRegion> UserfaultFdEngine::findWatchedRegion(Address addr) const {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t val = addr.value();
    for (const auto& [base, region] : watchedRanges_) {
        if (region.active && val >= base && val < (base + region.size)) {
            return region;
        }
    }
    return std::nullopt;
}

std::vector<GuardedMemoryRegion> UserfaultFdEngine::allWatchedRanges() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<GuardedMemoryRegion> res;
    res.reserve(watchedRanges_.size());
    for (const auto& [_, r] : watchedRanges_) {
        res.push_back(r);
    }
    return res;
}

std::vector<UffdFaultEvent> UserfaultFdEngine::pollEvents(int timeoutMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<UffdFaultEvent> events;

    // First deliver any simulated events
    if (!simulatedEvents_.empty()) {
        events = std::move(simulatedEvents_);
        simulatedEvents_.clear();
        for (const auto& ev : events) {
            uint64_t base = ev.faultAddress.value() & ~0xFFFULL;
            auto it = watchedRanges_.find(base);
            if (it != watchedRanges_.end()) {
                it->second.hitCount++;
            }
        }
        return events;
    }

    if (!isInitialized_ || isSimulated_ || !uffd_.isValid()) {
        return events;
    }

    struct pollfd pfd{};
    pfd.fd = uffd_.get();
    pfd.events = POLLIN;

    int ret = ::poll(&pfd, 1, timeoutMs);
    if (ret <= 0 || !(pfd.revents & POLLIN)) {
        return events;
    }

    struct uffd_msg msg{};
    ssize_t n = ::read(uffd_.get(), &msg, sizeof(msg));
    while (n == sizeof(msg)) {
        if (msg.event == UFFD_EVENT_PAGEFAULT) {
            Address fault_addr(msg.arg.pagefault.address);
            bool is_write = (msg.arg.pagefault.flags & UFFD_PAGEFAULT_FLAG_WRITE) != 0;
            bool is_wp = (msg.arg.pagefault.flags & UFFD_PAGEFAULT_FLAG_WP) != 0;

            UffdFaultEvent ev{
                .faultAddress = fault_addr,
                .isWrite = is_write,
                .isWriteProtected = is_wp,
                .threadId = static_cast<uint64_t>(msg.arg.pagefault.feat.ptid),
                .timestamp = std::chrono::system_clock::now(),
                .details = std::format("Page fault at 0x{:016x} [{}]", fault_addr.value(), is_write ? "WRITE" : "READ")
            };

            uint64_t base = fault_addr.value() & ~0xFFFULL;
            auto it = watchedRanges_.find(base);
            if (it != watchedRanges_.end()) {
                it->second.hitCount++;
            }

            events.push_back(ev);
        }
        n = ::read(uffd_.get(), &msg, sizeof(msg));
    }

    return events;
}

bool UserfaultFdEngine::resolveFault(Address faultAddr, void* copySource, size_t copySize) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isInitialized_) {
        return false;
    }
    if (isSimulated_ || !uffd_.isValid()) {
        return true;
    }

    uint64_t page_base = faultAddr.value() & ~0xFFFULL;

    if (copySource != nullptr && copySize > 0) {
        struct uffdio_copy uffd_copy{};
        uffd_copy.dst = page_base;
        uffd_copy.src = reinterpret_cast<uint64_t>(copySource);
        uffd_copy.len = 4096;
        uffd_copy.mode = 0;
        return ::ioctl(uffd_.get(), UFFDIO_COPY, &uffd_copy) >= 0;
    } else {
        struct uffdio_zeropage uffd_zero{};
        uffd_zero.range.start = page_base;
        uffd_zero.range.len = 4096;
        uffd_zero.mode = 0;
        return ::ioctl(uffd_.get(), UFFDIO_ZEROPAGE, &uffd_zero) >= 0;
    }
}

void UserfaultFdEngine::simulateFault(const UffdFaultEvent& ev) {
    std::lock_guard<std::mutex> lock(mutex_);
    simulatedEvents_.push_back(ev);
}

void UserfaultFdEngine::clearSimulationQueue() {
    std::lock_guard<std::mutex> lock(mutex_);
    simulatedEvents_.clear();
}

} // namespace edb_next
