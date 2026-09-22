#pragma once

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <functional>
#include <stdexcept>
#include <cstdint>

namespace edb_next::lua_binding {

// Type traits for extracting typed arguments from Lua stack
template <typename T>
struct ArgReader;

template <>
struct ArgReader<int> {
    static int read(lua_State* L, int idx) {
        return static_cast<int>(luaL_checkinteger(L, idx));
    }
};

template <>
struct ArgReader<uint64_t> {
    static uint64_t read(lua_State* L, int idx) {
        return static_cast<uint64_t>(luaL_checkinteger(L, idx));
    }
};

template <>
struct ArgReader<std::string> {
    static std::string read(lua_State* L, int idx) {
        if (idx > lua_gettop(L) || lua_isnil(L, idx)) return "";
        size_t len = 0;
        const char* s = luaL_checklstring(L, idx, &len);
        return s ? std::string(s, len) : std::string();
    }
};

template <>
struct ArgReader<std::string_view> {
    static std::string_view read(lua_State* L, int idx) {
        if (idx > lua_gettop(L) || lua_isnil(L, idx)) return "";
        size_t len = 0;
        const char* s = luaL_checklstring(L, idx, &len);
        return s ? std::string_view(s, len) : std::string_view();
    }
};

template <>
struct ArgReader<bool> {
    static bool read(lua_State* L, int idx) {
        return lua_toboolean(L, idx) != 0;
    }
};

template <typename T>
struct ArgReader<std::optional<T>> {
    static std::optional<T> read(lua_State* L, int idx) {
        if (idx > lua_gettop(L) || lua_isnil(L, idx)) {
            return std::nullopt;
        }
        return ArgReader<T>::read(L, idx);
    }
};

// Type traits for pushing return values to Lua stack
inline void pushVal(lua_State* /*L*/) {}

inline void pushVal(lua_State* L, bool v) {
    lua_pushboolean(L, v ? 1 : 0);
}

inline void pushVal(lua_State* L, int v) {
    lua_pushinteger(L, static_cast<lua_Integer>(v));
}

inline void pushVal(lua_State* L, uint64_t v) {
    lua_pushinteger(L, static_cast<lua_Integer>(v));
}

inline void pushVal(lua_State* L, const std::string& v) {
    lua_pushlstring(L, v.data(), v.size());
}

inline void pushVal(lua_State* L, std::string_view v) {
    lua_pushlstring(L, v.data(), v.size());
}

inline void pushVal(lua_State* L, const char* v) {
    if (v) lua_pushstring(L, v);
    else lua_pushnil(L);
}

template <typename T>
inline void pushVal(lua_State* L, const std::optional<T>& v) {
    if (v.has_value()) {
        pushVal(L, *v);
    } else {
        lua_pushnil(L);
    }
}

inline void pushVal(lua_State* L, const std::vector<uint8_t>& bytes) {
    lua_pushlstring(L, reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

inline void pushVal(lua_State* L, const std::vector<std::pair<std::string, uint64_t>>& items) {
    lua_newtable(L);
    for (const auto& [k, v] : items) {
        lua_pushlstring(L, k.data(), k.size());
        lua_pushinteger(L, static_cast<lua_Integer>(v));
        lua_settable(L, -3);
    }
}

template <typename F, typename Tuple, size_t... Is>
auto invokeHelper(F&& f, Tuple&& args, std::index_sequence<Is...>) {
    return std::invoke(std::forward<F>(f), std::get<Is>(std::forward<Tuple>(args))...);
}

template <typename Ret, typename... Args>
class LuaFunctionDispatcher {
private:
    template <typename Func, size_t... Is>
    static int dispatchImpl(lua_State* L, Func&& func, std::index_sequence<Is...>) {
        std::tuple<std::decay_t<Args>...> argsTuple{
            ArgReader<std::decay_t<Args>>::read(L, static_cast<int>(Is + 1))...
        };

        if constexpr (std::is_void_v<Ret>) {
            invokeHelper(std::forward<Func>(func), std::move(argsTuple), std::index_sequence_for<Args...>{});
            return 0;
        } else {
            auto ret = invokeHelper(std::forward<Func>(func), std::move(argsTuple), std::index_sequence_for<Args...>{});
            pushVal(L, ret);
            return 1;
        }
    }

public:
    template <typename Func>
    static int dispatch(lua_State* L, Func&& func) {
        try {
            return dispatchImpl(L, std::forward<Func>(func), std::index_sequence_for<Args...>{});
        } catch (const std::exception& e) {
            return luaL_error(L, "C++ exception in Lua binding: %s", e.what());
        } catch (...) {
            return luaL_error(L, "Unknown C++ exception in Lua binding");
        }
    }
};

} // namespace edb_next::lua_binding
