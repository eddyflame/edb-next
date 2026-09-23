#include "EbpfHookEngine.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include <fstream>
#include <chrono>

namespace edb_next {

EbpfHookEngine::EbpfHookEngine()
    : kernelSupported_(isKernelTracingSupported()) {}

EbpfHookEngine::~EbpfHookEngine() {
    detachAll();
}

EbpfHookEngine::EbpfHookEngine(EbpfHookEngine&&) noexcept = default;
EbpfHookEngine& EbpfHookEngine::operator=(EbpfHookEngine&&) noexcept = default;

bool EbpfHookEngine::isKernelTracingSupported() noexcept {
    if (::access("/sys/kernel/tracing/uprobe_events", W_OK) == 0) return true;
    if (::access("/sys/kernel/debug/tracing/uprobe_events", W_OK) == 0) return true;
    return false;
}

bool EbpfHookEngine::isHardwareBpfAvailable() const noexcept {
    return kernelSupported_;
}

Result<void> EbpfHookEngine::attachUprobe(const std::string& binaryPath, uint64_t offset,
                                         const std::string& name, bool isReturn) {
    for (const auto& p : probes_) {
        if (p.name == name) {
            return Result<void>::Err("Probe with name '" + name + "' already exists");
        }
    }

    if (kernelSupported_) {
        std::string tracingPath = "/sys/kernel/tracing/uprobe_events";
        if (::access(tracingPath.c_str(), W_OK) != 0) {
            tracingPath = "/sys/kernel/debug/tracing/uprobe_events";
        }

        std::ofstream uprobeFile(tracingPath, std::ios::app);
        if (uprobeFile.is_open()) {
            std::ostringstream cmd;
            cmd << (isReturn ? "r:uprobes/" : "p:uprobes/") << name << " " << binaryPath << ":0x" << std::hex << offset;
            uprobeFile << cmd.str() << "\n";
            uprobeFile.close();

            // Enable probe
            std::string enablePath = "/sys/kernel/tracing/events/uprobes/" + name + "/enable";
            std::ofstream enableFile(enablePath);
            if (enableFile.is_open()) {
                enableFile << "1\n";
                enableFile.close();
            }
        }
    }

    UprobeDef def;
    def.binaryPath = binaryPath;
    def.offset = offset;
    def.name = name;
    def.isReturn = isReturn;
    def.isAttached = true;
    probes_.push_back(std::move(def));

    return Result<void>::Ok();
}

Result<void> EbpfHookEngine::detachUprobe(const std::string& name) {
    auto it = std::find_if(probes_.begin(), probes_.end(), [&](const UprobeDef& p) {
        return p.name == name;
    });

    if (it == probes_.end()) {
        return Result<void>::Err("Probe '" + name + "' not found");
    }

    if (kernelSupported_) {
        std::string enablePath = "/sys/kernel/tracing/events/uprobes/" + name + "/enable";
        std::ofstream enableFile(enablePath);
        if (enableFile.is_open()) {
            enableFile << "0\n";
            enableFile.close();
        }

        std::string tracingPath = "/sys/kernel/tracing/uprobe_events";
        if (::access(tracingPath.c_str(), W_OK) != 0) {
            tracingPath = "/sys/kernel/debug/tracing/uprobe_events";
        }
        std::ofstream uprobeFile(tracingPath, std::ios::app);
        if (uprobeFile.is_open()) {
            uprobeFile << "-:uprobes/" << name << "\n";
            uprobeFile.close();
        }
    }

    probes_.erase(it);
    return Result<void>::Ok();
}

void EbpfHookEngine::detachAll() {
    auto copy = probes_;
    for (const auto& p : copy) {
        detachUprobe(p.name);
    }
    probes_.clear();
}

std::vector<UprobeEvent> EbpfHookEngine::pollEvents() {
    auto result = std::move(events_);
    events_.clear();
    return result;
}

void EbpfHookEngine::recordSimulatedEvent(const UprobeEvent& ev) {
    events_.push_back(ev);
}

void EbpfHookEngine::clearEvents() {
    events_.clear();
}

} // namespace edb_next
