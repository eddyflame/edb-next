#pragma once

#include "Types.hpp"
#include <string>
#include <vector>
#include <memory>

namespace edb_next {

class DebugSession;
class PatchManager;

struct DatabaseBreakpointData {
    uint64_t address{0};
    std::string type;
    std::string condition;
    std::string logFormat;
    uint32_t ignoreCount{0};
    std::string scriptCode;
    std::string scriptLanguage{"python"};
};

struct DatabasePatchData {
    uint64_t address{0};
    std::string originalHex;
    std::string patchedHex;
};

struct DatabasePageGuardData {
    uint64_t address{0};
    size_t size{1};
    std::string access{"NoAccess"};
    std::string comment;
    std::string condition;
    std::string scriptCode;
    std::string scriptLanguage{"python"};
};

struct DatabaseProject {
    std::string binaryPath;
    std::string notes;
    std::vector<std::pair<uint64_t, std::string>> comments;
    std::vector<std::pair<uint64_t, std::string>> labels;
    std::vector<uint64_t> bookmarks;
    std::vector<DatabaseBreakpointData> breakpoints;
    std::vector<DatabasePageGuardData> pageGuards;
    std::vector<std::string> watches;
    std::vector<DatabasePatchData> patches;
};

class DatabaseManager {
public:
    static DatabaseManager& instance();

    bool saveToFile(const std::string& filepath, const DatabaseProject& project);
    bool loadFromFile(const std::string& filepath, DatabaseProject& project);

    // Auto-save database path calculation based on binary path
    [[nodiscard]] std::string defaultDatabasePath(const std::string& binaryPath) const;

    // High-level session sync
    bool exportSession(std::shared_ptr<DebugSession> session,
                       const PatchManager* patchMgr,
                       const std::string& notes,
                       const std::vector<std::string>& watches,
                       const std::string& filepath);

    bool importSession(std::shared_ptr<DebugSession> session,
                       PatchManager* patchMgr,
                       std::string& outNotes,
                       std::vector<std::string>& outWatches,
                       const std::string& filepath);

private:
    DatabaseManager() = default;
    ~DatabaseManager() = default;
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
};

} // namespace edb_next
