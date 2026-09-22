#include "ScriptApiBridge.hpp"
#include "DebugSession.hpp"
#include "ExpressionEvaluator.hpp"
#include "LogManager.hpp"
#include <algorithm>
#include <cctype>

namespace edb_next {

std::string ScriptApiBridge::normalizeRegisterName(std::string name) {
    auto start = name.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = name.find_last_not_of(" \t\r\n");
    name = name.substr(start, end - start + 1);

    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (!name.empty() && name[0] == '$') {
        name = name.substr(1);
    }
    return name;
}

std::optional<uint64_t> ScriptApiBridge::getReg(DebugSession* session, const std::string& regName) {
    if (!session) return std::nullopt;

    std::string name = normalizeRegisterName(regName);
    if (name.empty()) return std::nullopt;

    const auto& regs = session->registers();
    if (name == "rax") return regs.rax();
    if (name == "rbx") return regs.rbx();
    if (name == "rcx") return regs.rcx();
    if (name == "rdx") return regs.rdx();
    if (name == "rsi") return regs.rsi();
    if (name == "rdi") return regs.rdi();
    if (name == "rbp") return regs.rbp().value();
    if (name == "rsp") return regs.rsp().value();
    if (name == "r8")  return regs.r8();
    if (name == "r9")  return regs.r9();
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

bool ScriptApiBridge::setReg(DebugSession* session, const std::string& regName, uint64_t val) {
    if (!session) return false;

    std::string name = normalizeRegisterName(regName);
    if (name.empty()) return false;

    RegisterContext regs = session->registers();
    bool matched = true;

    if (name == "rax") regs.setRax(val);
    else if (name == "rbx") regs.setRbx(val);
    else if (name == "rcx") regs.setRcx(val);
    else if (name == "rdx") regs.setRdx(val);
    else if (name == "rsi") regs.setRsi(val);
    else if (name == "rdi") regs.setRdi(val);
    else if (name == "rbp") regs.setRbp(Address(val));
    else if (name == "rsp") regs.setRsp(Address(val));
    else if (name == "r8")  regs.setR8(val);
    else if (name == "r9")  regs.setR9(val);
    else if (name == "r10") regs.setR10(val);
    else if (name == "r11") regs.setR11(val);
    else if (name == "r12") regs.setR12(val);
    else if (name == "r13") regs.setR13(val);
    else if (name == "r14") regs.setR14(val);
    else if (name == "r15") regs.setR15(val);
    else if (name == "rip") regs.setRip(Address(val));
    else matched = false;

    if (matched) {
        return session->setRegisters(regs);
    }
    return false;
}

std::vector<std::pair<std::string, uint64_t>> ScriptApiBridge::getRegs(DebugSession* session) {
    std::vector<std::pair<std::string, uint64_t>> result;
    if (!session) return result;

    const auto& regs = session->registers();
    auto list = regs.toList();
    result.reserve(list.size());

    for (const auto& item : list) {
        std::string lowerName = item.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        result.emplace_back(std::move(lowerName), item.value);
    }
    return result;
}

std::vector<uint8_t> ScriptApiBridge::readMemory(DebugSession* session, uint64_t addr, size_t size) {
    if (!session || size == 0) return {};
    return session->readMemory(Address(addr), size);
}

bool ScriptApiBridge::writeMemory(DebugSession* session, uint64_t addr, const void* data, size_t size) {
    if (!session || size == 0 || !data) return false;
    return session->writeMemory(Address(addr), data, size);
}

bool ScriptApiBridge::setBreakpoint(DebugSession* session, uint64_t addr, const std::string& symbol) {
    if (!session) return false;
    return session->addBreakpoint(Address(addr), symbol);
}

bool ScriptApiBridge::removeBreakpoint(DebugSession* session, uint64_t addr) {
    if (!session) return false;
    return session->removeBreakpoint(Address(addr));
}

void ScriptApiBridge::stepInto(DebugSession* session) {
    if (session) session->stepInto();
}

void ScriptApiBridge::stepOver(DebugSession* session) {
    if (session) session->stepOver();
}

void ScriptApiBridge::stepSource(DebugSession* session) {
    if (session) session->stepSourceOver();
}

void ScriptApiBridge::resume(DebugSession* session) {
    if (session) session->resume();
}

void ScriptApiBridge::pause(DebugSession* session) {
    if (session) session->pause();
}

std::optional<uint64_t> ScriptApiBridge::resolveSymbol(DebugSession* session, const std::string& name) {
    if (!session) return std::nullopt;
    auto addr = session->resolveSymbol(name);
    if (addr) return addr->value();
    return std::nullopt;
}

std::optional<uint64_t> ScriptApiBridge::eval(DebugSession* session, const std::string& expr) {
    if (!session) return std::nullopt;
    return ExpressionEvaluator::evaluate(expr, session->registers());
}

int ScriptApiBridge::pid(DebugSession* session) noexcept {
    return session ? session->pid() : 0;
}

int ScriptApiBridge::tid(DebugSession* session) noexcept {
    return session ? session->activeTid() : 0;
}

std::string ScriptApiBridge::state(DebugSession* session) {
    if (!session) return "None";
    switch (session->state()) {
        case SessionState::Running: return "Running";
        case SessionState::Paused: return "Paused";
        case SessionState::Stopped: return "Stopped";
        case SessionState::Terminated: return "Terminated";
        default: return "Unknown";
    }
}

void ScriptApiBridge::log(const std::string& sender, const std::string& msg) {
    LogManager::instance().log(LogLevel::Info, sender, msg);
}

} // namespace edb_next
