#pragma once

#include "Types.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include <cstdint>

namespace edb_next {

struct UprobeDef {
    std::string binaryPath;
    uint64_t offset{0};
    std::string name;
    bool isReturn{false};
    bool isAttached{false};
};

struct UprobeEvent {
    std::string probeName;
    uint64_t timestampNs{0};
    Pid pid{0};
    Tid tid{0};
    uint64_t ip{0};
    std::vector<uint64_t> args;
    uint64_t returnValue{0};
};

class EbpfHookEngine {
public:
    EbpfHookEngine();
    ~EbpfHookEngine();

    // Disable copy, allow move
    EbpfHookEngine(const EbpfHookEngine&) = delete;
    EbpfHookEngine& operator=(const EbpfHookEngine&) = delete;
    EbpfHookEngine(EbpfHookEngine&&) noexcept;
    EbpfHookEngine& operator=(EbpfHookEngine&&) noexcept;

    [[nodiscard]] static bool isKernelTracingSupported() noexcept;
    [[nodiscard]] bool isHardwareBpfAvailable() const noexcept;

    Result<void> attachUprobe(const std::string& binaryPath, uint64_t offset,
                              const std::string& name, bool isReturn = false);
    Result<void> detachUprobe(const std::string& name);
    void detachAll();

    [[nodiscard]] const std::vector<UprobeDef>& attachedProbes() const noexcept { return probes_; }
    [[nodiscard]] std::vector<UprobeEvent> pollEvents();
    void recordSimulatedEvent(const UprobeEvent& ev);
    [[nodiscard]] size_t eventCount() const noexcept { return events_.size(); }
    void clearEvents();

private:
    std::vector<UprobeDef> probes_;
    std::vector<UprobeEvent> events_;
    bool kernelSupported_{false};
};

} // namespace edb_next
