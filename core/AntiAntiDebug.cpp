#include "AntiAntiDebug.hpp"
#include <sstream>
#include <format>
#include <algorithm>
#include <sys/syscall.h>
#include <sys/ptrace.h>
#include <sys/prctl.h>

namespace edb_next {

AntiAntiDebugEngine::AntiAntiDebugEngine() = default;

std::string AntiAntiDebugEngine::sanitizeProcStatus(const std::string& originalContent) {
    std::istringstream iss(originalContent);
    std::ostringstream oss;
    std::string line;

    while (std::getline(iss, line)) {
        if (line.starts_with("TracerPid:")) {
            oss << "TracerPid:\t0\n";
        } else {
            oss << line << "\n";
        }
    }

    return oss.str();
}

uint64_t AntiAntiDebugEngine::filterRdtscCycles(uint64_t realCycles) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!config_.smoothRdtscTiming) {
        return realCycles;
    }

    if (lastRealTsc_ == 0) {
        lastRealTsc_ = realCycles;
        lastVirtualTsc_ = realCycles;
        return realCycles;
    }

    uint64_t real_delta = (realCycles >= lastRealTsc_) ? (realCycles - lastRealTsc_) : 100;
    uint64_t synthetic_delta = std::clamp<uint64_t>(real_delta, 15, config_.maxSyntheticRdtscDelta);

    lastVirtualTsc_ += synthetic_delta;
    lastRealTsc_ = realCycles;

    if (real_delta > 100'000) {
        AntiDebugDetection det{
            .type = "RDTSC Timing Analysis",
            .triggerAddress = Address(0),
            .timestamp = std::chrono::system_clock::now(),
            .details = std::format("Detected single-step delay of {} cycles; smoothed to {} cycles.",
                                   real_delta, synthetic_delta)
        };
        detections_.push_back(std::move(det));
    }

    return lastVirtualTsc_;
}

bool AntiAntiDebugEngine::inspectSyscall(Address rip, uint64_t sysNo, uint64_t arg1, uint64_t arg2) {
    std::lock_guard<std::mutex> lock(mutex_);

#ifdef __NR_ptrace
    if (sysNo == __NR_ptrace && arg1 == PTRACE_TRACEME && config_.interceptPtraceTraceme) {
        AntiDebugDetection det{
            .type = "PTRACE_TRACEME Check",
            .triggerAddress = rip,
            .timestamp = std::chrono::system_clock::now(),
            .details = "Intercepted and neutralized self-ptrace anti-debugging probe."
        };
        detections_.push_back(std::move(det));
        return true;
    }
#endif

#ifdef __NR_prctl
    if (sysNo == __NR_prctl && arg1 == PR_SET_DUMPABLE && arg2 == 0 && config_.interceptPrctlDumpable) {
        AntiDebugDetection det{
            .type = "PR_SET_DUMPABLE Check",
            .triggerAddress = rip,
            .timestamp = std::chrono::system_clock::now(),
            .details = "Intercepted PR_SET_DUMPABLE(0) anti-dumping tamper attempt."
        };
        detections_.push_back(std::move(det));
        return true;
    }
#endif

    return false;
}

void AntiAntiDebugEngine::recordDetection(const AntiDebugDetection& detection) {
    std::lock_guard<std::mutex> lock(mutex_);
    detections_.push_back(detection);
}

std::vector<AntiDebugDetection> AntiAntiDebugEngine::allDetections() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return detections_;
}

size_t AntiAntiDebugEngine::detectionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return detections_.size();
}

void AntiAntiDebugEngine::clearDetections() {
    std::lock_guard<std::mutex> lock(mutex_);
    detections_.clear();
    lastRealTsc_ = 0;
    lastVirtualTsc_ = 0;
}

} // namespace edb_next
