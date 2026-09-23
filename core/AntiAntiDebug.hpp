#pragma once

#include "Types.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include <chrono>

namespace edb_next {

struct AntiDebugDetection {
    std::string type;
    Address triggerAddress{0};
    std::chrono::system_clock::time_point timestamp;
    std::string details;
};

struct AntiAntiDebugConfig {
    bool spoofTracerPid{true};
    bool smoothRdtscTiming{true};
    bool interceptPtraceTraceme{true};
    bool interceptPrctlDumpable{true};
    uint64_t maxSyntheticRdtscDelta{450}; // Max simulated TSC cycles per step
};

class AntiAntiDebugEngine {
public:
    AntiAntiDebugEngine();
    ~AntiAntiDebugEngine() = default;

    [[nodiscard]] const AntiAntiDebugConfig& config() const noexcept { return config_; }
    void setConfig(const AntiAntiDebugConfig& cfg) noexcept { config_ = cfg; }

    // Spoof /proc/[pid]/status content: rewrites "TracerPid:\t<num>" to "TracerPid:\t0"
    [[nodiscard]] static std::string sanitizeProcStatus(const std::string& originalContent);

    // Filter and smooth RDTSC values to prevent timing-based debugger detection
    [[nodiscard]] uint64_t filterRdtscCycles(uint64_t realCycles);

    // Inspect syscall arguments to detect and neutralize anti-debug checks
    bool inspectSyscall(Address rip, uint64_t sysNo, uint64_t arg1, uint64_t arg2);

    void recordDetection(const AntiDebugDetection& detection);
    [[nodiscard]] std::vector<AntiDebugDetection> allDetections() const;
    [[nodiscard]] size_t detectionCount() const;
    void clearDetections();

private:
    AntiAntiDebugConfig config_;
    mutable std::mutex mutex_;
    std::vector<AntiDebugDetection> detections_;
    uint64_t lastVirtualTsc_{0};
    uint64_t lastRealTsc_{0};
};

} // namespace edb_next
