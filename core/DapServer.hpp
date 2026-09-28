#pragma once

#include "Types.hpp"
#include "IDebugBackend.hpp"
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
    explicit DapServer(std::shared_ptr<IDebugBackend> backend);
    ~DapServer();

    // Disable copy, allow move
    DapServer(const DapServer&) = delete;
    DapServer& operator=(const DapServer&) = delete;
    DapServer(DapServer&&) noexcept;
    DapServer& operator=(DapServer&&) noexcept;

    void setBackend(std::shared_ptr<IDebugBackend> backend);
    [[nodiscard]] std::shared_ptr<IDebugBackend> backend() const noexcept { return backend_; }

    void setEngine(std::shared_ptr<IDebugBackend> engine) { setBackend(std::move(engine)); }
    [[nodiscard]] std::shared_ptr<IDebugBackend> engine() const noexcept { return backend_; }

    // Direct JSON-RPC message processing (headless / testable)
    [[nodiscard]] std::string handleMessage(const std::string& jsonText);

    // Stdio server loop (blocking until disconnect request or EOF)
    void run(std::istream& in, std::ostream& out);
    void stop();

    [[nodiscard]] bool isRunning() const noexcept { return isRunning_; }

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
    std::shared_ptr<IDebugBackend> backend_;
    std::unique_ptr<BreakpointManager> bpMgr_;
    TimeTravelEngine timeTravel_;
    bool isRunning_{false};
};

} // namespace edb_next
