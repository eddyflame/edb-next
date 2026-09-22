#include "LuaScriptEngine.hpp"
#include "ScriptApiBridge.hpp"
#include "LuaTypeBinding.hpp"
#include "LogManager.hpp"
#include <sstream>
#include <iostream>

namespace edb_next {

static const char* kLuaEngineRegistryKey = "__edb_lua_engine__";
static thread_local LuaScriptEngine* t_activeLuaEngine = nullptr;

struct LuaEngineScope {
    LuaScriptEngine* prev_;
    explicit LuaEngineScope(LuaScriptEngine* eng) : prev_(t_activeLuaEngine) {
        t_activeLuaEngine = eng;
    }
    ~LuaEngineScope() {
        t_activeLuaEngine = prev_;
    }
};

namespace {

inline LuaScriptEngine* getEngine(lua_State* L) noexcept {
    if (!L) return t_activeLuaEngine;
    lua_getfield(L, LUA_REGISTRYINDEX, kLuaEngineRegistryKey);
    void* ptr = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return ptr ? static_cast<LuaScriptEngine*>(ptr) : t_activeLuaEngine;
}

inline DebugSession* getSession(lua_State* L) noexcept {
    auto* eng = getEngine(L);
    return eng ? eng->session() : nullptr;
}

int lua_custom_print(lua_State* L) {
    int n = lua_gettop(L);
    std::ostringstream oss;
    for (int i = 1; i <= n; ++i) {
        size_t len = 0;
        const char* s = luaL_tolstring(L, i, &len);
        if (i > 1) oss << "\t";
        if (s) oss << std::string(s, len);
        lua_pop(L, 1); // pop string created by luaL_tolstring
    }
    oss << "\n";
    auto* eng = getEngine(L);
    if (eng) {
        eng->appendOutput(oss.str());
    }
    return 0;
}

} // namespace

LuaScriptEngine::LuaScriptEngine() = default;

LuaScriptEngine::~LuaScriptEngine() {
    shutdown();
}

DebugSession* LuaScriptEngine::activeSession() noexcept {
    if (t_activeLuaEngine) {
        return t_activeLuaEngine->session_;
    }
    return nullptr;
}

void LuaScriptEngine::setSession(DebugSession* session) {
    session_ = session;
}

void LuaScriptEngine::appendOutput(const std::string& text) {
    capturedOutput_ += text;
}

void LuaScriptEngine::registerEdbModule() {
    if (!L_) return;

    // Create global 'edb' table
    lua_newtable(L_);

    auto reg = [this](const char* name, lua_CFunction fn) {
        lua_pushcfunction(L_, fn);
        lua_setfield(L_, -2, name);
    };

    reg("read_memory", [](lua_State* L) {
        using namespace lua_binding;
        return LuaFunctionDispatcher<std::vector<uint8_t>, uint64_t, size_t>::dispatch(L, [L](uint64_t a, size_t sz) {
            return ScriptApiBridge::readMemory(getSession(L), a, sz);
        });
    });

    reg("write_memory", [](lua_State* L) {
        using namespace lua_binding;
        return LuaFunctionDispatcher<bool, uint64_t, std::string_view>::dispatch(L, [L](uint64_t a, std::string_view b) {
            return ScriptApiBridge::writeMemory(getSession(L), a, b.data(), b.size());
        });
    });

    reg("get_reg", [](lua_State* L) {
        using namespace lua_binding;
        return LuaFunctionDispatcher<std::optional<uint64_t>, std::string>::dispatch(L, [L](const std::string& n) {
            return ScriptApiBridge::getReg(getSession(L), n);
        });
    });

    reg("set_reg", [](lua_State* L) {
        using namespace lua_binding;
        return LuaFunctionDispatcher<bool, std::string, uint64_t>::dispatch(L, [L](const std::string& n, uint64_t v) {
            return ScriptApiBridge::setReg(getSession(L), n, v);
        });
    });

    reg("get_regs", [](lua_State* L) {
        using namespace lua_binding;
        return LuaFunctionDispatcher<std::vector<std::pair<std::string, uint64_t>>>::dispatch(L, [L]() {
            return ScriptApiBridge::getRegs(getSession(L));
        });
    });

    reg("set_breakpoint", [](lua_State* L) {
        using namespace lua_binding;
        return LuaFunctionDispatcher<bool, uint64_t, std::optional<std::string>>::dispatch(L, [L](uint64_t a, std::optional<std::string> sym) {
            return ScriptApiBridge::setBreakpoint(getSession(L), a, sym.value_or(""));
        });
    });

    reg("remove_breakpoint", [](lua_State* L) {
        using namespace lua_binding;
        return LuaFunctionDispatcher<bool, uint64_t>::dispatch(L, [L](uint64_t a) {
            return ScriptApiBridge::removeBreakpoint(getSession(L), a);
        });
    });

    reg("step_into", [](lua_State* L) {
        using namespace lua_binding;
        return LuaFunctionDispatcher<void>::dispatch(L, [L]() {
            ScriptApiBridge::stepInto(getSession(L));
        });
    });

    reg("step_over", [](lua_State* L) {
        using namespace lua_binding;
        return LuaFunctionDispatcher<void>::dispatch(L, [L]() {
            ScriptApiBridge::stepOver(getSession(L));
        });
    });

    reg("step_source", [](lua_State* L) {
        using namespace lua_binding;
        return LuaFunctionDispatcher<void>::dispatch(L, [L]() {
            ScriptApiBridge::stepSource(getSession(L));
        });
    });

    reg("resume", [](lua_State* L) {
        using namespace lua_binding;
        return LuaFunctionDispatcher<void>::dispatch(L, [L]() {
            ScriptApiBridge::resume(getSession(L));
        });
    });

    reg("pause", [](lua_State* L) {
        using namespace lua_binding;
        return LuaFunctionDispatcher<void>::dispatch(L, [L]() {
            ScriptApiBridge::pause(getSession(L));
        });
    });

    reg("resolve_symbol", [](lua_State* L) {
        using namespace lua_binding;
        return LuaFunctionDispatcher<std::optional<uint64_t>, std::string>::dispatch(L, [L](const std::string& n) {
            return ScriptApiBridge::resolveSymbol(getSession(L), n);
        });
    });

    reg("eval", [](lua_State* L) {
        using namespace lua_binding;
        return LuaFunctionDispatcher<std::optional<uint64_t>, std::string>::dispatch(L, [L](const std::string& expr) {
            return ScriptApiBridge::eval(getSession(L), expr);
        });
    });

    reg("pid", [](lua_State* L) {
        using namespace lua_binding;
        return LuaFunctionDispatcher<int>::dispatch(L, [L]() {
            return ScriptApiBridge::pid(getSession(L));
        });
    });

    reg("tid", [](lua_State* L) {
        using namespace lua_binding;
        return LuaFunctionDispatcher<int>::dispatch(L, [L]() {
            return ScriptApiBridge::tid(getSession(L));
        });
    });

    reg("state", [](lua_State* L) {
        using namespace lua_binding;
        return LuaFunctionDispatcher<std::string>::dispatch(L, [L]() {
            return ScriptApiBridge::state(getSession(L));
        });
    });

    reg("log", [](lua_State* L) {
        using namespace lua_binding;
        return LuaFunctionDispatcher<void, std::string>::dispatch(L, [](const std::string& msg) {
            ScriptApiBridge::log("Lua", msg);
        });
    });

    lua_setglobal(L_, "edb");

    // Override global print
    lua_pushcfunction(L_, lua_custom_print);
    lua_setglobal(L_, "print");
}

bool LuaScriptEngine::initialize(DebugSession* session) {
    session_ = session;
    if (L_) {
        lua_close(L_);
        L_ = nullptr;
    }

    L_ = luaL_newstate();
    if (!L_) {
        return false;
    }

    // Associate this LuaScriptEngine instance with L_ via registry
    lua_pushlightuserdata(L_, this);
    lua_setfield(L_, LUA_REGISTRYINDEX, kLuaEngineRegistryKey);

    luaL_openlibs(L_);
    registerEdbModule();
    return true;
}

void LuaScriptEngine::shutdown() {
    if (t_activeLuaEngine == this) {
        t_activeLuaEngine = nullptr;
    }
    if (L_) {
        lua_close(L_);
        L_ = nullptr;
    }
}

ScriptResult LuaScriptEngine::executeString(const std::string& code) {
    if (!L_) {
        if (!initialize(session_)) {
            return {false, "", "Failed to initialize Lua 5.4 state."};
        }
    }

    LuaEngineScope scope(this);
    capturedOutput_.clear();

    int loadStatus = luaL_loadstring(L_, code.c_str());
    if (loadStatus != LUA_OK) {
        const char* err = lua_tostring(L_, -1);
        std::string errStr = err ? err : "Unknown compilation error";
        lua_pop(L_, 1);
        return {false, capturedOutput_, errStr};
    }

    int pcallStatus = lua_pcall(L_, 0, LUA_MULTRET, 0);
    if (pcallStatus != LUA_OK) {
        const char* err = lua_tostring(L_, -1);
        std::string errStr = err ? err : "Runtime error";
        lua_pop(L_, 1);
        return {false, capturedOutput_, errStr};
    }

    return {true, capturedOutput_, ""};
}

ScriptResult LuaScriptEngine::executeFile(const std::string& filepath) {
    if (!L_) {
        if (!initialize(session_)) {
            return {false, "", "Failed to initialize Lua 5.4 state."};
        }
    }

    LuaEngineScope scope(this);
    capturedOutput_.clear();

    int loadStatus = luaL_loadfile(L_, filepath.c_str());
    if (loadStatus != LUA_OK) {
        const char* err = lua_tostring(L_, -1);
        std::string errStr = err ? err : "Failed to load script file";
        lua_pop(L_, 1);
        return {false, capturedOutput_, errStr};
    }

    int pcallStatus = lua_pcall(L_, 0, LUA_MULTRET, 0);
    if (pcallStatus != LUA_OK) {
        const char* err = lua_tostring(L_, -1);
        std::string errStr = err ? err : "Runtime error";
        lua_pop(L_, 1);
        return {false, capturedOutput_, errStr};
    }

    return {true, capturedOutput_, ""};
}

bool LuaScriptEngine::executeHook(const std::string& code) {
    if (!L_) {
        if (!initialize(session_)) {
            return true;
        }
    }

    LuaEngineScope scope(this);
    capturedOutput_.clear();

    std::string wrapped = "local function __edb_bp_hook__()\n" + code + "\nend\nreturn __edb_bp_hook__()";
    int loadStatus = luaL_loadstring(L_, wrapped.c_str());
    if (loadStatus != LUA_OK) {
        lua_pop(L_, 1);
        // Fallback to direct loadstring
        loadStatus = luaL_loadstring(L_, code.c_str());
        if (loadStatus != LUA_OK) {
            const char* err = lua_tostring(L_, -1);
            if (err) LogManager::instance().error("Lua Hook Compile Error", err);
            lua_pop(L_, 1);
            return true;
        }
    }

    int pcallStatus = lua_pcall(L_, 0, 1, 0);
    if (pcallStatus != LUA_OK) {
        const char* err = lua_tostring(L_, -1);
        if (err) LogManager::instance().error("Lua Hook Runtime Error", err);
        lua_pop(L_, 1);
        return true;
    }

    bool shouldPause = true;
    if (lua_isboolean(L_, -1) && !lua_toboolean(L_, -1)) {
        shouldPause = false;
    }
    lua_pop(L_, 1);
    return shouldPause;
}

} // namespace edb_next
