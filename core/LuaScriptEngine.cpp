#include "LuaScriptEngine.hpp"
#include "DebugSession.hpp"
#include "ExpressionEvaluator.hpp"
#include "LogManager.hpp"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <algorithm>
#include <cctype>
#include <sstream>
#include <iostream>

namespace edb_next {

static LuaScriptEngine* s_currentLuaEngine = nullptr;

namespace {

std::string trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::optional<uint64_t> getRegVal(const std::string& regName, const RegisterContext& regs) {
    std::string name = toLower(trim(regName));
    if (name.empty()) return std::nullopt;
    if (name[0] == '$') name = name.substr(1);

    if (name == "rax") return regs.rax();
    if (name == "rbx") return regs.rbx();
    if (name == "rcx") return regs.rcx();
    if (name == "rdx") return regs.rdx();
    if (name == "rsi") return regs.rsi();
    if (name == "rdi") return regs.rdi();
    if (name == "rbp") return regs.rbp().value();
    if (name == "rsp") return regs.rsp().value();
    if (name == "r8") return regs.r8();
    if (name == "r9") return regs.r9();
    if (name == "r10") return regs.r10();
    if (name == "r11") return regs.r11();
    if (name == "r12") return regs.r12();
    if (name == "r13") return regs.r13();
    if (name == "r14") return regs.r14();
    if (name == "r15") return regs.r15();
    if (name == "rip") return regs.rip().value();
    if (name == "rflags" || name == "eflags") return regs.eflags();

    return std::nullopt;
}

bool setRegVal(const std::string& regName, uint64_t val, RegisterContext& regs) {
    std::string name = toLower(trim(regName));
    if (name.empty()) return false;
    if (name[0] == '$') name = name.substr(1);

    if (name == "rax") { regs.setRax(val); return true; }
    if (name == "rbx") { regs.setRbx(val); return true; }
    if (name == "rcx") { regs.setRcx(val); return true; }
    if (name == "rdx") { regs.setRdx(val); return true; }
    if (name == "rsi") { regs.setRsi(val); return true; }
    if (name == "rdi") { regs.setRdi(val); return true; }
    if (name == "rbp") { regs.setRbp(Address(val)); return true; }
    if (name == "rsp") { regs.setRsp(Address(val)); return true; }
    if (name == "r8")  { regs.setR8(val); return true; }
    if (name == "r9")  { regs.setR9(val); return true; }
    if (name == "r10") { regs.setR10(val); return true; }
    if (name == "r11") { regs.setR11(val); return true; }
    if (name == "r12") { regs.setR12(val); return true; }
    if (name == "r13") { regs.setR13(val); return true; }
    if (name == "r14") { regs.setR14(val); return true; }
    if (name == "r15") { regs.setR15(val); return true; }
    if (name == "rip") { regs.setRip(Address(val)); return true; }

    return false;
}

int lua_read_memory(lua_State* L) {
    lua_Integer addr = luaL_checkinteger(L, 1);
    lua_Integer size = luaL_checkinteger(L, 2);

    auto* session = LuaScriptEngine::activeSession();
    if (!session || size <= 0) {
        lua_pushlstring(L, "", 0);
        return 1;
    }

    auto bytes = session->readMemory(Address(static_cast<uint64_t>(addr)), static_cast<size_t>(size));
    lua_pushlstring(L, reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return 1;
}

int lua_write_memory(lua_State* L) {
    lua_Integer addr = luaL_checkinteger(L, 1);
    size_t len = 0;
    const char* data = luaL_checklstring(L, 2, &len);

    auto* session = LuaScriptEngine::activeSession();
    if (!session || len == 0 || !data) {
        lua_pushboolean(L, 0);
        return 1;
    }

    bool ok = session->writeMemory(Address(static_cast<uint64_t>(addr)), data, len);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int lua_get_reg(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    auto* session = LuaScriptEngine::activeSession();
    if (!session) {
        lua_pushnil(L);
        return 1;
    }

    auto val = getRegVal(name, session->registers());
    if (val) {
        lua_pushinteger(L, static_cast<lua_Integer>(*val));
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int lua_set_reg(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    lua_Integer val = luaL_checkinteger(L, 2);

    auto* session = LuaScriptEngine::activeSession();
    if (!session) {
        lua_pushboolean(L, 0);
        return 1;
    }

    RegisterContext regs = session->registers();
    if (setRegVal(name, static_cast<uint64_t>(val), regs)) {
        bool ok = session->setRegisters(regs);
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }
    lua_pushboolean(L, 0);
    return 1;
}

int lua_get_regs(lua_State* L) {
    auto* session = LuaScriptEngine::activeSession();
    lua_newtable(L);
    if (!session) {
        return 1;
    }

    const auto& regs = session->registers();
    auto list = regs.toList();
    for (const auto& item : list) {
        std::string lowerName = toLower(item.name);
        lua_pushstring(L, lowerName.c_str());
        lua_pushinteger(L, static_cast<lua_Integer>(item.value));
        lua_settable(L, -3);
    }
    return 1;
}

int lua_set_breakpoint(lua_State* L) {
    lua_Integer addr = luaL_checkinteger(L, 1);
    const char* symbol = lua_isstring(L, 2) ? lua_tostring(L, 2) : "";

    auto* session = LuaScriptEngine::activeSession();
    if (!session) {
        lua_pushboolean(L, 0);
        return 1;
    }

    bool ok = session->addBreakpoint(Address(static_cast<uint64_t>(addr)), symbol ? symbol : "");
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int lua_remove_breakpoint(lua_State* L) {
    lua_Integer addr = luaL_checkinteger(L, 1);

    auto* session = LuaScriptEngine::activeSession();
    if (!session) {
        lua_pushboolean(L, 0);
        return 1;
    }

    bool ok = session->removeBreakpoint(Address(static_cast<uint64_t>(addr)));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int lua_step_into(lua_State* /*L*/) {
    auto* session = LuaScriptEngine::activeSession();
    if (session) {
        session->stepInto();
    }
    return 0;
}

int lua_step_over(lua_State* /*L*/) {
    auto* session = LuaScriptEngine::activeSession();
    if (session) {
        session->stepOver();
    }
    return 0;
}

int lua_step_source(lua_State* /*L*/) {
    auto* session = LuaScriptEngine::activeSession();
    if (session) {
        session->stepSourceOver();
    }
    return 0;
}

int lua_resume(lua_State* /*L*/) {
    auto* session = LuaScriptEngine::activeSession();
    if (session) {
        session->resume();
    }
    return 0;
}

int lua_pause(lua_State* /*L*/) {
    auto* session = LuaScriptEngine::activeSession();
    if (session) {
        session->pause();
    }
    return 0;
}

int lua_resolve_symbol(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    auto* session = LuaScriptEngine::activeSession();
    if (!session) {
        lua_pushnil(L);
        return 1;
    }

    auto addr = session->resolveSymbol(name);
    if (addr) {
        lua_pushinteger(L, static_cast<lua_Integer>(addr->value()));
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int lua_eval(lua_State* L) {
    const char* expr = luaL_checkstring(L, 1);
    auto* session = LuaScriptEngine::activeSession();
    if (!session) {
        lua_pushnil(L);
        return 1;
    }

    auto val = ExpressionEvaluator::evaluate(expr, session->registers());
    if (val) {
        lua_pushinteger(L, static_cast<lua_Integer>(*val));
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int lua_pid(lua_State* L) {
    auto* session = LuaScriptEngine::activeSession();
    if (session) {
        lua_pushinteger(L, session->pid());
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

int lua_tid(lua_State* L) {
    auto* session = LuaScriptEngine::activeSession();
    if (session) {
        lua_pushinteger(L, session->activeTid());
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

int lua_state(lua_State* L) {
    auto* session = LuaScriptEngine::activeSession();
    if (!session) {
        lua_pushstring(L, "None");
        return 1;
    }
    switch (session->state()) {
        case SessionState::Running: lua_pushstring(L, "Running"); break;
        case SessionState::Paused: lua_pushstring(L, "Paused"); break;
        case SessionState::Stopped: lua_pushstring(L, "Stopped"); break;
        case SessionState::Terminated: lua_pushstring(L, "Terminated"); break;
        default: lua_pushstring(L, "Unknown"); break;
    }
    return 1;
}

int lua_log(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    LogManager::instance().log(LogLevel::Info, "Lua", msg ? msg : "");
    return 0;
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
    if (s_currentLuaEngine) {
        s_currentLuaEngine->appendOutput(oss.str());
    }
    return 0;
}

} // namespace

LuaScriptEngine::LuaScriptEngine() = default;

LuaScriptEngine::~LuaScriptEngine() {
    shutdown();
}

DebugSession* LuaScriptEngine::activeSession() noexcept {
    if (s_currentLuaEngine) {
        return s_currentLuaEngine->session_;
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

    auto registerFunc = [this](const char* name, lua_CFunction func) {
        lua_pushcfunction(L_, func);
        lua_setfield(L_, -2, name);
    };

    registerFunc("read_memory", lua_read_memory);
    registerFunc("write_memory", lua_write_memory);
    registerFunc("get_reg", lua_get_reg);
    registerFunc("set_reg", lua_set_reg);
    registerFunc("get_regs", lua_get_regs);
    registerFunc("set_breakpoint", lua_set_breakpoint);
    registerFunc("remove_breakpoint", lua_remove_breakpoint);
    registerFunc("step_into", lua_step_into);
    registerFunc("step_over", lua_step_over);
    registerFunc("step_source", lua_step_source);
    registerFunc("resume", lua_resume);
    registerFunc("pause", lua_pause);
    registerFunc("resolve_symbol", lua_resolve_symbol);
    registerFunc("eval", lua_eval);
    registerFunc("pid", lua_pid);
    registerFunc("tid", lua_tid);
    registerFunc("state", lua_state);
    registerFunc("log", lua_log);

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

    luaL_openlibs(L_);
    registerEdbModule();
    return true;
}

void LuaScriptEngine::shutdown() {
    if (s_currentLuaEngine == this) {
        s_currentLuaEngine = nullptr;
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

    s_currentLuaEngine = this;
    capturedOutput_.clear();

    // luaL_dostring is a macro: luaL_loadstring(L, s) || lua_pcall(L, 0, LUA_MULTRET, 0)
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

    s_currentLuaEngine = this;
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

    s_currentLuaEngine = this;
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
