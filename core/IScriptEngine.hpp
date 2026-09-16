#pragma once

#include <string>
#include <vector>

namespace edb_next {

class DebugSession;

struct ScriptResult {
    bool success{false};
    std::string output;
    std::string error;

    [[nodiscard]] bool ok() const noexcept { return success && error.empty(); }
};

class IScriptEngine {
public:
    virtual ~IScriptEngine() = default;

    [[nodiscard]] virtual std::string engineName() const = 0;
    [[nodiscard]] virtual std::string language() const = 0;
    [[nodiscard]] virtual bool isInitialized() const = 0;

    virtual bool initialize(DebugSession* session) = 0;
    virtual void shutdown() = 0;
    virtual void setSession(DebugSession* session) = 0;

    virtual ScriptResult executeString(const std::string& code) = 0;
    virtual ScriptResult executeFile(const std::string& filepath) = 0;
    virtual bool executeHook(const std::string& code) = 0;
};

} // namespace edb_next
