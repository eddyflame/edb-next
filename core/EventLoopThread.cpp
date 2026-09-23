#include "EventLoopThread.hpp"
#include "IDebugBackend.hpp"
#include "BreakpointManager.hpp"
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <iostream>

namespace edb_next {

static int sys_pidfd_open(pid_t pid, unsigned int flags) {
#ifdef SYS_pidfd_open
    return static_cast<int>(::syscall(SYS_pidfd_open, pid, flags));
#else
    errno = ENOSYS;
    return -1;
#endif
}

EventLoopThread::EventLoopThread(IDebugBackend& engine, BreakpointManager& bp_mgr, QObject* parent)
    : QThread(parent), engine_(engine), bpMgr_(bp_mgr)
{
    qRegisterMetaType<edb_next::DebugEvent>("edb_next::DebugEvent");
}

EventLoopThread::~EventLoopThread() {
    stopLoop();
    cleanupEpoll();
}

void EventLoopThread::startLoop() {
    if (!running_.load()) {
        if (engine_.isAttached()) {
            initEpoll(engine_.pid());
        }
        running_.store(true);
        start();
    }
}

void EventLoopThread::setSuspended(bool s) {
    if (!isRunning() || !running_.load()) return;
    std::unique_lock<std::mutex> lock(suspendMutex_);
    if (s) {
        suspendRequested_.store(true);
        notifyWake();
        suspendCv_.wait(lock, [this] {
            return isSuspended_.load() || !running_.load();
        });
    } else {
        suspendRequested_.store(false);
        notifyWake();
        suspendCv_.notify_all();
        suspendCv_.wait(lock, [this] {
            return !isSuspended_.load() || !running_.load();
        });
    }
}

void EventLoopThread::stopLoop() {
    if (running_.load()) {
        {
            std::lock_guard<std::mutex> lock(suspendMutex_);
            running_.store(false);
            suspendRequested_.store(false);
            suspendCv_.notify_all();
        }
        notifyWake();
        disconnect();
        wait(500);
        if (isRunning()) {
            terminate();
            wait();
        }
    }
    cleanupEpoll();
}

void EventLoopThread::addDetachedChild(Pid pid) {
    std::lock_guard<std::mutex> lock(childMutex_);
    detachedChildren_.insert(pid);
}

void EventLoopThread::initEpoll(pid_t target) {
    cleanupEpoll();
    int wfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    int efd = ::epoll_create1(EPOLL_CLOEXEC);
    if (efd >= 0 && wfd >= 0) {
        struct epoll_event ev_wake{};
        ev_wake.events = EPOLLIN;
        ev_wake.data.fd = wfd;
        ::epoll_ctl(efd, EPOLL_CTL_ADD, wfd, &ev_wake);
    }
    wakeFd_.store(wfd);
    epollFd_.store(efd);

    if (target > 0) {
        int pfd = sys_pidfd_open(target, 0);
        if (pfd >= 0 && efd >= 0) {
            struct epoll_event ev_pid{};
            ev_pid.events = EPOLLIN;
            ev_pid.data.fd = pfd;
            ::epoll_ctl(efd, EPOLL_CTL_ADD, pfd, &ev_pid);
        }
        pidFd_.store(pfd);
    }
}

void EventLoopThread::cleanupEpoll() {
    int efd = epollFd_.exchange(-1);
    if (efd >= 0) {
        ::close(efd);
    }
    int wfd = wakeFd_.exchange(-1);
    if (wfd >= 0) {
        ::close(wfd);
    }
    int pfd = pidFd_.exchange(-1);
    if (pfd >= 0) {
        ::close(pfd);
    }
}

void EventLoopThread::notifyWake() {
    int wfd = wakeFd_.load();
    if (wfd >= 0) {
        uint64_t val = 1;
        ssize_t written = ::write(wfd, &val, sizeof(val));
        (void)written;
    }
}

void EventLoopThread::run() {
    pid_t current_target = (epollFd_.load() >= 0 && engine_.isAttached()) ? engine_.pid() : 0;

    while (running_.load()) {
        if (suspendRequested_.load()) {
            {
                std::unique_lock<std::mutex> lock(suspendMutex_);
                isSuspended_.store(true);
                suspendCv_.notify_all();
                suspendCv_.wait(lock, [this] {
                    return !suspendRequested_.load() || !running_.load();
                });
                isSuspended_.store(false);
                suspendCv_.notify_all();
            }
            continue;
        }

        if (!engine_.isAttached()) {
            if (epollFd_.load() >= 0) cleanupEpoll();
            current_target = 0;
            msleep(5);
            continue;
        }

        pid_t target = engine_.pid();
        if (target != current_target || epollFd_.load() < 0) {
            initEpoll(target);
            current_target = target;
        }

        // Event-driven kernel wait: sleep with 0% CPU until kernel signals on pidfd or wakeFd
        int efd = epollFd_.load();
        int wfd = wakeFd_.load();
        int pfd = pidFd_.load();
        if (efd >= 0) {
            struct epoll_event events[4];
            int timeout_ms = (pfd >= 0) ? 5 : 2;
            int nfds = ::epoll_wait(efd, events, 4, timeout_ms);
            if (!running_.load()) break;

            for (int i = 0; i < nfds; ++i) {
                if (events[i].data.fd == wfd) {
                    uint64_t val = 0;
                    ssize_t r = ::read(wfd, &val, sizeof(val));
                    (void)r;
                }
            }
        }

        if (!running_.load()) break;

        // Drain status events
        while (running_.load()) {
            int status = 0;
            pid_t target_pid = engine_.pid();
            if (target_pid <= 0) break;
            pid_t pgid = ::getpgid(target_pid);
            pid_t wait_target = (pgid > 0) ? -pgid : target_pid;
            Pid waited_pid = ::waitpid(wait_target, &status, __WALL | WNOHANG);

            if (!running_.load()) break;

            if (waited_pid == 0) {
                // Done draining events; wait for next epoll trigger
                break;
            }

            if (waited_pid < 0) {
                if (errno != ECHILD && errno != EINTR) {
                    std::cerr << "[WAITPID-ERR] waitpid returned " << waited_pid << ", errno=" << errno << " (" << strerror(errno) << ")" << std::endl;
                }
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
                    cleanupEpoll();
                    running_.store(false);
                    return;
                }
                break;
            }

            if (waited_pid != engine_.pid()) {
                {
                    std::lock_guard<std::mutex> lock(childMutex_);
                    if (detachedChildren_.contains(waited_pid)) {
                        // Detached child event; ignore and clean up on exit
                        if (WIFEXITED(status) || WIFSIGNALED(status)) {
                            detachedChildren_.erase(waited_pid);
                        }
                        continue;
                    }
                }
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
                cleanupEpoll();
                running_.store(false);
                return;
            }
        }
    }
    cleanupEpoll();
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
