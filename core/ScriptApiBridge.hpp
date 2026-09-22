#pragma once

#include "Types.hpp"
#include <string>
#include <vector>
#include <optional>
#include <utility>
#include <cstdint>

namespace edb_next {

class DebugSession;

class ScriptApiBridge {
public:
    static std::string normalizeRegisterName(std::string name);

    // Register access
    static std::optional<uint64_t> getReg(DebugSession* session, const std::string& regName);
    static bool setReg(DebugSession* session, const std::string& regName, uint64_t val);
    static std::vector<std::pair<std::string, uint64_t>> getRegs(DebugSession* session);

    // Memory access
    static std::vector<uint8_t> readMemory(DebugSession* session, uint64_t addr, size_t size);
    static bool writeMemory(DebugSession* session, uint64_t addr, const void* data, size_t size);
    static bool writeMemory(DebugSession* session, uint64_t addr, const std::string& bytes) {
        return writeMemory(session, addr, bytes.data(), bytes.size());
    }

    // Breakpoint control
    static bool setBreakpoint(DebugSession* session, uint64_t addr, const std::string& symbol = "");
    static bool removeBreakpoint(DebugSession* session, uint64_t addr);

    // Execution control
    static void stepInto(DebugSession* session);
    static void stepOver(DebugSession* session);
    static void stepSource(DebugSession* session);
    static void resume(DebugSession* session);
    static void pause(DebugSession* session);

    // Introspection & evaluation
    static std::optional<uint64_t> resolveSymbol(DebugSession* session, const std::string& name);
    static std::optional<uint64_t> eval(DebugSession* session, const std::string& expr);
    static int pid(DebugSession* session) noexcept;
    static int tid(DebugSession* session) noexcept;
    static std::string state(DebugSession* session);
    static void log(const std::string& sender, const std::string& msg);
};

} // namespace edb_next
