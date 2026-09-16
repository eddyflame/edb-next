#include "SessionManager.hpp"

namespace edb_next {

SessionManager::SessionManager(QObject* parent) : QObject(parent) {}

SessionManager::~SessionManager() {
    for (auto& [id, sess] : sessions_) {
        sess->terminate();
    }
    sessions_.clear();
}

std::shared_ptr<DebugSession> SessionManager::createSession(const std::string& name) {
    std::string id = "session_" + std::to_string(nextSessionIndex_++);
    std::string session_name = name.empty() ? ("Session " + std::to_string(nextSessionIndex_ - 1)) : name;

    // Pass nullptr as QObject parent because DebugSession lifetime is managed by std::shared_ptr
    auto session = std::make_shared<DebugSession>(id, session_name, nullptr);
    sessions_[id] = session;

    if (activeSessionId_.empty()) {
        activeSessionId_ = id;
    }

    Q_EMIT sessionCreated(session);
    return session;
}

std::shared_ptr<DebugSession> SessionManager::createChildSession(
    std::shared_ptr<DebugSession> parentSession, Pid childPid)
{
    if (!parentSession || childPid <= 0) return nullptr;

    std::string id = "session_" + std::to_string(nextSessionIndex_++);
    std::string session_name = "Child [PID: " + std::to_string(childPid) + "]";

    auto childSession = std::make_shared<DebugSession>(id, session_name, nullptr);
    sessions_[id] = childSession;

    childSession->initAsChild(parentSession, childPid);

    Q_EMIT sessionCreated(childSession);
    return childSession;
}

void SessionManager::closeSession(const std::string& id) {
    auto it = sessions_.find(id);
    if (it != sessions_.end()) {
        it->second->terminate();
        sessions_.erase(it);
        Q_EMIT sessionClosed(id);

        if (activeSessionId_ == id) {
            if (!sessions_.empty()) {
                activeSessionId_ = sessions_.begin()->first;
                Q_EMIT activeSessionChanged(sessions_.begin()->second);
            } else {
                activeSessionId_.clear();
                Q_EMIT activeSessionChanged(nullptr);
            }
        }
    }
}

std::shared_ptr<DebugSession> SessionManager::session(const std::string& id) const {
    auto it = sessions_.find(id);
    if (it != sessions_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<DebugSession> SessionManager::activeSession() const {
    if (activeSessionId_.empty()) return nullptr;
    return session(activeSessionId_);
}

void SessionManager::setActiveSession(const std::string& id) {
    if (sessions_.contains(id) && activeSessionId_ != id) {
        activeSessionId_ = id;
        Q_EMIT activeSessionChanged(sessions_[id]);
    }
}

std::vector<std::shared_ptr<DebugSession>> SessionManager::allSessions() const {
    std::vector<std::shared_ptr<DebugSession>> list;
    list.reserve(sessions_.size());
    for (const auto& [_, sess] : sessions_) {
        list.push_back(sess);
    }
    return list;
}

} // namespace edb_next
