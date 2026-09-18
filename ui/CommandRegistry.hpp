#pragma once

#include "Types.hpp"
#include <QString>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <optional>

namespace edb_next {

class DebugSession;

/**
 * @brief Categorization for debugger command-line commands.
 */
enum class CommandCategory {
    Breakpoint,
    Execution,
    Memory,
    Analysis,
    Process,
    System,
    Plugin
};

[[nodiscard]] inline QString commandCategoryToString(CommandCategory cat) {
    switch (cat) {
        case CommandCategory::Breakpoint: return "Breakpoint";
        case CommandCategory::Execution:  return "Execution";
        case CommandCategory::Memory:     return "Memory";
        case CommandCategory::Analysis:   return "Analysis";
        case CommandCategory::Process:    return "Process";
        case CommandCategory::System:     return "System";
        case CommandCategory::Plugin:     return "Plugin";
    }
    return "Unknown";
}

/**
 * @brief Execution context passed to command handlers with decoupling callbacks.
 */
struct CommandContext {
    std::shared_ptr<DebugSession> session{nullptr};
    std::vector<std::string> args;
    std::string rawCommandLine;

    std::function<Address(const std::string&)> addressParser;
    std::function<void(Address)> jumpToDisassembly;
    std::function<void(Address)> jumpToMemory;
    std::function<void(const QString&)> switchSession;
    std::function<void(const QString& msg, bool isError)> outputLogger;
    std::function<void(const QString& cmdLine)> commandExecutor;

    void log(const QString& msg, bool isError = false) const {
        if (outputLogger) outputLogger(msg, isError);
    }

    void error(const QString& msg) const {
        if (outputLogger) outputLogger(msg, true);
    }

    [[nodiscard]] Address parseAddress(const std::string& token) const {
        if (addressParser) return addressParser(token);
        return Address(0);
    }

    void execute(const QString& cmdLine) const {
        if (commandExecutor) commandExecutor(cmdLine);
    }
};

using CommandHandler = std::function<void(const CommandContext& ctx)>;

/**
 * @brief Descriptor for a registered command.
 */
struct CommandDescriptor {
    std::string name;
    std::vector<std::string> aliases;
    CommandCategory category{CommandCategory::System};
    std::string syntax;
    std::string description;
    CommandHandler handler;
};

/**
 * @brief Centralized registry and dispatcher for debugger CLI commands.
 */
class CommandRegistry {
public:
    CommandRegistry();
    ~CommandRegistry() = default;

    // Registration APIs
    void registerCommand(CommandDescriptor desc);
    void registerCommand(const std::string& name,
                         std::function<void(const std::vector<std::string>&)> simpleHandler,
                         const std::string& helpText = "",
                         CommandCategory category = CommandCategory::Plugin);

    // Execution API
    bool execute(const std::string& cmdLine, const CommandContext& baseCtx);

    // Completion / Introspection APIs
    [[nodiscard]] std::vector<std::string> complete(const std::string& prefix) const;
    [[nodiscard]] const std::map<std::string, CommandDescriptor>& commands() const noexcept { return commands_; }
    [[nodiscard]] std::vector<CommandDescriptor> commandsByCategory(CommandCategory cat) const;
    [[nodiscard]] std::optional<CommandDescriptor> findCommand(const std::string& name) const;

    // Register all default debugger command sets
    void registerBuiltinCommands();

private:
    void registerBreakpointCommands();
    void registerExecutionCommands();
    void registerMemoryCommands();
    void registerAnalysisCommands();
    void registerProcessCommands();
    void registerSystemCommands();

    std::map<std::string, CommandDescriptor> commands_;
    std::map<std::string, std::string> aliasMap_;
};

} // namespace edb_next
