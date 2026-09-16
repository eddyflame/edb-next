#include "ScriptEngineManager.hpp"
#include <algorithm>
#include <cctype>

namespace edb_next {

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

} // namespace

ScriptEngineManager::ScriptEngineManager()
    : pythonEngine_(std::make_unique<PythonScriptEngine>())
    , luaEngine_(std::make_unique<LuaScriptEngine>())
    , activeLanguage_("python")
{
}

ScriptEngineManager::~ScriptEngineManager() {
    shutdown();
}

void ScriptEngineManager::initialize(DebugSession* session) {
    session_ = session;
    if (pythonEngine_) {
        pythonEngine_->initialize(session);
    }
    if (luaEngine_) {
        luaEngine_->initialize(session);
    }
}

void ScriptEngineManager::setSession(DebugSession* session) {
    session_ = session;
    if (pythonEngine_) {
        pythonEngine_->setSession(session);
    }
    if (luaEngine_) {
        luaEngine_->setSession(session);
    }
}

void ScriptEngineManager::shutdown() {
    if (pythonEngine_) {
        pythonEngine_->shutdown();
    }
    if (luaEngine_) {
        luaEngine_->shutdown();
    }
}

IScriptEngine* ScriptEngineManager::engine(const std::string& lang) {
    std::string l = toLower(lang);
    if (l == "python" || l == "py" || l == "python3") {
        return pythonEngine_.get();
    }
    if (l == "lua" || l == "lua5" || l == "lua5.4") {
        return luaEngine_.get();
    }
    return activeEngine();
}

IScriptEngine* ScriptEngineManager::activeEngine() {
    return engine(activeLanguage_);
}

void ScriptEngineManager::setActiveLanguage(const std::string& lang) {
    std::string l = toLower(lang);
    if (l == "lua" || l == "lua5" || l == "lua5.4") {
        activeLanguage_ = "lua";
    } else {
        activeLanguage_ = "python";
    }
}

ScriptResult ScriptEngineManager::execute(const std::string& lang, const std::string& code) {
    IScriptEngine* eng = engine(lang);
    if (!eng) {
        return {false, "", "No scripting engine available for language: " + lang};
    }
    return eng->executeString(code);
}

ScriptResult ScriptEngineManager::executeActive(const std::string& code) {
    return execute(activeLanguage_, code);
}

ScriptResult ScriptEngineManager::executeFile(const std::string& filepath) {
    std::string lowerPath = toLower(filepath);
    if (lowerPath.ends_with(".lua")) {
        return luaEngine_->executeFile(filepath);
    } else if (lowerPath.ends_with(".py")) {
        return pythonEngine_->executeFile(filepath);
    }
    return activeEngine()->executeFile(filepath);
}

std::vector<std::string> ScriptEngineManager::availableLanguages() const {
    return {"python", "lua"};
}

} // namespace edb_next
