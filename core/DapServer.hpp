#pragma once

#include "Types.hpp"
#include "LinuxDebugEngine.hpp"
#include "BreakpointManager.hpp"
#include "RegisterContext.hpp"
#include "TimeTravelEngine.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <istream>
#include <ostream>

namespace edb_next {

/**
 * @brief DAP (Debug Adapter Protocol) Headless Server
 *
 * Implements Microsoft DAP JSON-RPC protocol over stdio or TCP,
 * allowing VS Code, Cursor, Neovim (nvim-dap), and Helix to use edb-next
 * as an ultra-fast reverse engineering debugger backend.
 */
class DapServer {
public:
    DapServer();
    explicit DapServer(std::shared_ptr<LinuxDebugEngine> engine);
    ~DapServer();

    // Disable copy, allow move
    DapServer(const DapServer&) = delete;
    DapServer& operator=(const DapServer&) = delete;
    DapServer(DapServer&&) noexcept;
    DapServer& operator=(DapServer&&) noexcept;

    void setEngine(std::shared_ptr<LinuxDebugEngine> engine);
    [[nodiscard]] std::shared_ptr<LinuxDebugEngine> engine() const noexcept { return engine_; }

    // Direct JSON-RPC message processing (headless / testable)
    [[nodiscard]] std::string handleMessage(const std::string& jsonText);

    // Stdio server loop (blocking until disconnect request or EOF)
    void run(std::istream& in, std::ostream& out);
    void stop();

    [[nodiscard]] bool isRunning() const noexcept { return isRunning_; }

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
    std::shared_ptr<LinuxDebugEngine> engine_;
    std::unique_ptr<BreakpointManager> bpMgr_;
    TimeTravelEngine timeTravel_;
    bool isRunning_{false};
};

} // namespace edb_next
