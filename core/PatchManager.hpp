#pragma once

#include "Types.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <optional>
#include <QObject>

namespace edb_next {

class DebugSession;

struct MemoryPatch {
    Address address{0};
    std::vector<uint8_t> originalBytes;
    std::vector<uint8_t> patchedBytes;
    std::string comment;
    bool isApplied{true};
};

class PatchManager : public QObject {
    Q_OBJECT

public:
    explicit PatchManager(QObject* parent = nullptr);
    ~PatchManager() override = default;

    void addPatch(Address addr, const std::vector<uint8_t>& origBytes, const std::vector<uint8_t>& newBytes, const std::string& comment = "");
    bool revertPatch(size_t index, DebugSession* session);
    bool reapplyPatch(size_t index, DebugSession* session);
    void clearAll(DebugSession* session = nullptr);

    [[nodiscard]] const std::vector<MemoryPatch>& patches() const noexcept { return patches_; }
    [[nodiscard]] size_t patchCount() const noexcept { return patches_.size(); }

    // Export all active patches to a patched ELF binary file on disk
    bool patchFileToDisk(const std::string& inputBinaryPath, const std::string& outputBinaryPath, std::string& errorMsg, Address runtimeBase = Address(0));

Q_SIGNALS:
    void patchesUpdated();

private:
    std::vector<MemoryPatch> patches_;
};

} // namespace edb_next
