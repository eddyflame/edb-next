#pragma once

#include "IScriptEngine.hpp"
#include <string>

struct lua_State;

namespace edb_next {

class LuaScriptEngine : public IScriptEngine {
public:
    LuaScriptEngine();
    ~LuaScriptEngine() override;

    [[nodiscard]] std::string engineName() const override { return "Lua 5.4"; }
    [[nodiscard]] std::string language() const override { return "lua"; }
    [[nodiscard]] bool isInitialized() const override { return L_ != nullptr; }

    bool initialize(DebugSession* session) override;
    void shutdown() override;
    void setSession(DebugSession* session) override;

    ScriptResult executeString(const std::string& code) override;
    ScriptResult executeFile(const std::string& filepath) override;
    bool executeHook(const std::string& code) override;

    static DebugSession* activeSession() noexcept;
    void appendOutput(const std::string& text);

private:
    void registerEdbModule();

    lua_State* L_{nullptr};
    DebugSession* session_{nullptr};
    std::string capturedOutput_;
};

} // namespace edb_next
