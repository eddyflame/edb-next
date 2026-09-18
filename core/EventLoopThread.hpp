#pragma once

#include "Types.hpp"
#include <QThread>
#include <atomic>
#include <functional>

#include <mutex>
#include <unordered_set>

namespace edb_next {

class IDebugBackend;
class BreakpointManager;

class EventLoopThread : public QThread {
    Q_OBJECT

public:
    using EventFilter = std::function<DebugEvent(int status, Pid pid)>;

    explicit EventLoopThread(IDebugBackend& engine, BreakpointManager& bp_mgr, QObject* parent = nullptr);
    ~EventLoopThread() override;

    void startLoop();
    void stopLoop();
    [[nodiscard]] bool isRunningLoop() const noexcept { return running_.load(); }
    void setSuspended(bool s);
    [[nodiscard]] bool isSuspended() const noexcept { return isSuspended_.load(); }

    void addDetachedChild(Pid pid);

Q_SIGNALS:
    void eventReceived(const edb_next::DebugEvent& event);

protected:
    void run() override;

private:
    DebugEvent processWaitStatus(int status, Pid pid);

    IDebugBackend& engine_;
    BreakpointManager& bpMgr_;
    std::atomic<bool> running_{false};
    std::atomic<bool> suspendRequested_{false};
    std::atomic<bool> isSuspended_{false};
    std::mutex suspendMutex_;
    std::condition_variable suspendCv_;

    std::mutex childMutex_;
    std::unordered_set<Pid> pendingForkChildren_;
    std::unordered_set<Pid> detachedChildren_;
};

} // namespace edb_next

Q_DECLARE_METATYPE(edb_next::DebugEvent)
