#pragma once

#include "DebugSession.hpp"
#include <QObject>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

namespace edb_next {

class SessionManager : public QObject {
    Q_OBJECT

public:
    explicit SessionManager(QObject* parent = nullptr);
    ~SessionManager() override;

    std::shared_ptr<DebugSession> createSession(const std::string& name);
    std::shared_ptr<DebugSession> createChildSession(std::shared_ptr<DebugSession> parentSession, Pid childPid);
    void closeSession(const std::string& id);
    [[nodiscard]] std::shared_ptr<DebugSession> session(const std::string& id) const;
    [[nodiscard]] std::shared_ptr<DebugSession> activeSession() const;
    void setActiveSession(const std::string& id);

    [[nodiscard]] std::vector<std::shared_ptr<DebugSession>> allSessions() const;
    [[nodiscard]] size_t sessionCount() const noexcept { return sessions_.size(); }

Q_SIGNALS:
    void sessionCreated(std::shared_ptr<edb_next::DebugSession> session);
    void sessionClosed(const std::string& id);
    void activeSessionChanged(std::shared_ptr<edb_next::DebugSession> session);

private:
    std::unordered_map<std::string, std::shared_ptr<DebugSession>> sessions_;
    std::string activeSessionId_;
    uint64_t nextSessionIndex_{1};
};

} // namespace edb_next
