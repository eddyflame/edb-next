#include "EventLoopThread.hpp"
#include "LinuxDebugEngine.hpp"
#include "BreakpointManager.hpp"
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <cerrno>
#include <iostream>

namespace edb_next {

EventLoopThread::EventLoopThread(LinuxDebugEngine& engine, BreakpointManager& bp_mgr, QObject* parent)
    : QThread(parent), engine_(engine), bpMgr_(bp_mgr)
{
    qRegisterMetaType<edb_next::DebugEvent>("edb_next::DebugEvent");
}

EventLoopThread::~EventLoopThread() {
    stopLoop();
}

void EventLoopThread::startLoop() {
    if (!running_.load()) {
        running_.store(true);
        start();
    }
}

void EventLoopThread::stopLoop() {
    if (running_.load()) {
        running_.store(false);
        disconnect();
        wait(500);
        if (isRunning()) {
            terminate();
            wait();
        }
    }
}

void EventLoopThread::addDetachedChild(Pid pid) {
    std::lock_guard<std::mutex> lock(childMutex_);
    detachedChildren_.insert(pid);
}

void EventLoopThread::run() {
    while (running_.load()) {
        if (!engine_.isAttached() || suspended_.load()) {
            msleep(5);
            continue;
        }

        int status = 0;
        pid_t target = engine_.pid();
        pid_t pgid = ::getpgid(target);
        pid_t wait_target = (pgid > 0) ? -pgid : target;
        Pid waited_pid = ::waitpid(wait_target, &status, __WALL | WNOHANG);

        if (!running_.load()) {
            break;
        }

        if (suspended_.load()) {
            msleep(2);
            continue;
        }

        if (waited_pid == 0) {
            msleep(2);
            continue;
        }

        if (waited_pid < 0) {
            std::cerr << "[WAITPID-ERR] waitpid returned " << waited_pid << ", errno=" << errno << " (" << strerror(errno) << ")" << std::endl;
            if (errno == ECHILD) {
                // Child has terminated
                DebugEvent ev{
                    .pid = engine_.pid(),
                    .tid = engine_.mainTid(),
                    .reason = StopReason::ProcessExit,
                    .exitCode = 0,
                    .message = "Target process finished (ECHILD)"
                };
                Q_EMIT eventReceived(ev);
                break;
            }
            msleep(10);
            continue;
        }

        if (waited_pid != engine_.pid()) {
            std::string task_path = "/proc/" + std::to_string(engine_.pid()) + "/task/" + std::to_string(waited_pid);
            if (::access(task_path.c_str(), F_OK) != 0) {
                // Not a thread of our engine's main target. Could be child process initial stop or exit.
                DebugEvent ev;
                ev.pid = waited_pid;
                ev.tid = waited_pid;
                ev.childPid = waited_pid;
                ev.reason = StopReason::ThreadCreated;
                ev.message = "Child process initial stop";
                Q_EMIT eventReceived(ev);
                continue;
            }
        }

        if (WIFSTOPPED(status)) {
            int sig = WSTOPSIG(status);
            int ptrace_event = (status >> 16);

            if (ptrace_event == PTRACE_EVENT_FORK || ptrace_event == PTRACE_EVENT_VFORK) {
                DebugEvent ev;
                ev.pid = engine_.pid();
                ev.tid = waited_pid;
                ev.reason = StopReason::ProcessForked;
                ev.message = (ptrace_event == PTRACE_EVENT_VFORK) ? "vfork" : "fork";
                Q_EMIT eventReceived(ev);
                continue;
            }

            if (ptrace_event == PTRACE_EVENT_CLONE) {
                DebugEvent ev;
                ev.pid = engine_.pid();
                ev.tid = waited_pid;
                ev.reason = StopReason::ThreadCreated;
                ev.message = "Clone event";
                Q_EMIT eventReceived(ev);
                continue;
            }
            if (sig == SIGSTOP && waited_pid != engine_.pid()) {
                DebugEvent ev;
                ev.pid = engine_.pid();
                ev.tid = waited_pid;
                ev.reason = StopReason::ThreadCreated;
                ev.message = "New thread initial stop";
                Q_EMIT eventReceived(ev);
                continue;
            }
        }

        // Process status
        DebugEvent ev = processWaitStatus(status, waited_pid);
        if (ev.reason == StopReason::None) {
            continue;
        }

        Q_EMIT eventReceived(ev);

        if (ev.reason == StopReason::ProcessExit) {
            break;
        }
    }
    running_.store(false);
}

DebugEvent EventLoopThread::processWaitStatus(int status, Pid pid) {
    DebugEvent ev;
    ev.pid = engine_.pid();
    ev.tid = pid;

    if (WIFEXITED(status)) {
        if (pid == engine_.pid()) {
            ev.reason = StopReason::ProcessExit;
            ev.exitCode = WEXITSTATUS(status);
        } else {
            ev.reason = StopReason::None;
        }
        return ev;
    }

    if (WIFSIGNALED(status)) {
        if (pid == engine_.pid()) {
            ev.reason = StopReason::ProcessExit;
            ev.signal = WTERMSIG(status);
            ev.exitCode = 128 + WTERMSIG(status);
        } else {
            ev.reason = StopReason::None;
        }
        return ev;
    }

    if (WIFSTOPPED(status)) {
        int sig = WSTOPSIG(status);
        ev.signal = sig;
        ev.reason = (sig == SIGTRAP) ? StopReason::Breakpoint : StopReason::Signal;
        return ev;
    }

    ev.reason = StopReason::Error;
    ev.message = "Unhandled waitpid status: " + std::to_string(status);
    return ev;
}

} // namespace edb_next
