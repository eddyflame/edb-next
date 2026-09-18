#pragma once

#include "SessionManager.hpp"
#include "DebugSession.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <QString>

namespace edb_next {

class IPluginContext {
public:
    virtual ~IPluginContext() = default;

    // Engine & Session Access
    [[nodiscard]] virtual SessionManager& sessionManager() = 0;
    [[nodiscard]] virtual std::shared_ptr<DebugSession> activeSession() = 0;

    // Output & Log
    virtual void logMessage(const QString& msg) = 0;

    // Debug Event Listeners
    virtual void registerDebugEventListener(std::function<void(const DebugEvent&)> callback) = 0;
    virtual void registerSessionStateListener(std::function<void(SessionState)> callback) = 0;

    // Command Line CLI Command Registration (for Command Bar)
    virtual void registerCommand(const std::string& cmd,
                                std::function<void(const std::vector<std::string>&)> handler,
                                const std::string& helpText = "") = 0;
};

} // namespace edb_next
