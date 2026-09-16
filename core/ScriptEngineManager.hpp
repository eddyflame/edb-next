#pragma once

#include "IScriptEngine.hpp"
#include "PythonScriptEngine.hpp"
#include "LuaScriptEngine.hpp"
#include <memory>
#include <string>
#include <vector>

namespace edb_next {

class DebugSession;

class ScriptEngineManager {
public:
    ScriptEngineManager();
    ~ScriptEngineManager();

    void initialize(DebugSession* session);
    void setSession(DebugSession* session);
    void shutdown();

    [[nodiscard]] IScriptEngine* engine(const std::string& lang);
    [[nodiscard]] IScriptEngine* activeEngine();
    [[nodiscard]] const std::string& activeLanguage() const noexcept { return activeLanguage_; }
    void setActiveLanguage(const std::string& lang);

    ScriptResult execute(const std::string& lang, const std::string& code);
    ScriptResult executeActive(const std::string& code);
    ScriptResult executeFile(const std::string& filepath);

    [[nodiscard]] std::vector<std::string> availableLanguages() const;

private:
    std::unique_ptr<PythonScriptEngine> pythonEngine_;
    std::unique_ptr<LuaScriptEngine> luaEngine_;
    std::string activeLanguage_{"python"};
    DebugSession* session_{nullptr};
};

} // namespace edb_next
