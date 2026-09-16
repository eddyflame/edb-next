#pragma once

#include "IScriptEngine.hpp"
#include <memory>

namespace edb_next {

class PythonScriptEngine : public IScriptEngine {
public:
    PythonScriptEngine();
    ~PythonScriptEngine() override;

    [[nodiscard]] std::string engineName() const override { return "Python 3"; }
    [[nodiscard]] std::string language() const override { return "python"; }
    [[nodiscard]] bool isInitialized() const override { return initialized_; }

    bool initialize(DebugSession* session) override;
    void shutdown() override;
    void setSession(DebugSession* session) override;

    ScriptResult executeString(const std::string& code) override;
    ScriptResult executeFile(const std::string& filepath) override;
    bool executeHook(const std::string& code) override;

    static DebugSession* activeSession() noexcept;

private:
    bool initialized_{false};
    DebugSession* session_{nullptr};
};

} // namespace edb_next
