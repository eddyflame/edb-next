#include "CommandBarView.hpp"
#include "DebugSession.hpp"
#include "ExpressionEvaluator.hpp"
#include "core/StateDumper.hpp"
#include "core/LogManager.hpp"
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QCompleter>
#include <sstream>
#include <iomanip>

namespace edb_next {

CommandBarView::CommandBarView(QWidget* parent) : QWidget(parent) {
    setupUi();
    setupDefaultCommands();
}

void CommandBarView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
}

void CommandBarView::setupUi() {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(6);

    promptLabel_ = new QLabel("edb-next >", this);
    promptLabel_->setStyleSheet("font-weight: bold; color: #42a5f5; font-family: Monospace; font-size: 10pt;");
    layout->addWidget(promptLabel_);

    cmdInput_ = new QLineEdit(this);
    cmdInput_->setPlaceholderText("Enter command (e.g. 'bp main', 'r rax = 0x42', 'd rsp', 'step', 'help')...");
    cmdInput_->setFont(QFont("Monospace", 9));
    cmdInput_->installEventFilter(this);
    connect(cmdInput_, &QLineEdit::returnPressed, this, &CommandBarView::onReturnPressed);
    layout->addWidget(cmdInput_, 1);
}

void CommandBarView::registerCommand(const std::string& cmd,
                                     std::function<void(const std::vector<std::string>&)> handler,
                                     const std::string& helpText)
{
    commands_[cmd] = CommandEntry{handler, helpText};
}

bool CommandBarView::eventFilter(QObject* obj, QEvent* event) {
    if (obj == cmdInput_ && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Up) {
            if (!history_.isEmpty() && historyIndex_ > 0) {
                historyIndex_--;
                cmdInput_->setText(history_[historyIndex_]);
            } else if (!history_.isEmpty() && historyIndex_ == -1) {
                historyIndex_ = history_.size() - 1;
                cmdInput_->setText(history_[historyIndex_]);
            }
            return true;
        } else if (keyEvent->key() == Qt::Key_Down) {
            if (!history_.isEmpty() && historyIndex_ < history_.size() - 1 && historyIndex_ >= 0) {
                historyIndex_++;
                cmdInput_->setText(history_[historyIndex_]);
            } else {
                historyIndex_ = -1;
                cmdInput_->clear();
            }
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void CommandBarView::onReturnPressed() {
    QString raw = cmdInput_->text().trimmed();
    if (raw.isEmpty()) return;

    cmdInput_->clear();
    history_.append(raw);
    historyIndex_ = -1;

    executeCommand(raw);
}

void CommandBarView::executeCommand(const QString& line) {
    std::string str = line.toStdString();
    std::istringstream iss(str);
    std::string cmd;
    iss >> cmd;
    if (cmd.empty()) return;

    // Direct Script execution: py / :py / python, lua / :lua
    if (cmd == "py" || cmd == ":py" || cmd == "python" || cmd == "lua" || cmd == ":lua") {
        std::string lang = (cmd == "lua" || cmd == ":lua") ? "lua" : "python";
        size_t pos = str.find(cmd);
        std::string scriptCode;
        if (pos != std::string::npos) {
            scriptCode = str.substr(pos + cmd.length());
            auto s_pos = scriptCode.find_first_not_of(" \t");
            if (s_pos != std::string::npos) {
                scriptCode = scriptCode.substr(s_pos);
            } else {
                scriptCode.clear();
            }
        }
        if (scriptCode.empty()) {
            Q_EMIT outputLogged(QString("Usage: %1 <code...>").arg(QString::fromStdString(cmd)), true);
            return;
        }

        ScriptResult res;
        if (session_) {
            res = session_->scriptEngines().execute(lang, scriptCode);
        } else {
            ScriptEngineManager mgr;
            res = mgr.execute(lang, scriptCode);
        }

        if (!res.output.empty()) {
            Q_EMIT outputLogged(QString::fromStdString(res.output), false);
        }
        if (!res.error.empty()) {
            Q_EMIT outputLogged(QString::fromStdString(res.error), true);
        }
        if (res.output.empty() && res.error.empty()) {
            Q_EMIT outputLogged(res.success ? "Script executed successfully" : "Script execution failed", !res.success);
        }
        return;
    }

    std::vector<std::string> args;
    std::string arg;
    while (iss >> arg) {
        args.push_back(arg);
    }

    auto it = commands_.find(cmd);
    if (it != commands_.end()) {
        try {
            it->second.handler(args);
        } catch (const std::exception& ex) {
            Q_EMIT outputLogged(QString("Error executing '%1': %2").arg(QString::fromStdString(cmd), ex.what()), true);
        }
    } else {
        Q_EMIT outputLogged(QString("Unknown command '%1'. Type 'help' for available commands.").arg(QString::fromStdString(cmd)), true);
    }
}

Address CommandBarView::parseAddress(const std::string& token) {
    if (!session_) return Address(0);

    // Try resolving as symbol
    auto sym = session_->resolveSymbol(token);
    if (sym.has_value()) {
        return *sym;
    }

    // Try evaluating as expression (supports registers and arithmetic)
    auto res = ExpressionEvaluator::evaluate(token, session_->registers(), nullptr);
    if (res.has_value()) {
        return Address(*res);
    }

    // Try parsing hex or decimal
    uint64_t val = 0;
    if (token.rfind("0x", 0) == 0 || token.rfind("0X", 0) == 0) {
        val = std::strtoull(token.c_str(), nullptr, 16);
    } else {
        val = std::strtoull(token.c_str(), nullptr, 0);
    }
    return Address(val);
}

void CommandBarView::setupDefaultCommands() {
    // 1. Breakpoints: bp, bph, bc, be, bd
    registerCommand("bp", [this](const std::vector<std::string>& args) {
        if (args.empty()) {
            Q_EMIT outputLogged("Usage: bp <addr/symbol>", true);
            return;
        }
        Address addr = parseAddress(args[0]);
        if (addr.isNull()) {
            Q_EMIT outputLogged("Failed to resolve address for: " + QString::fromStdString(args[0]), true);
            return;
        }
        if (session_) {
            bool ok = session_->addBreakpoint(addr);
            Q_EMIT outputLogged(QString("Breakpoint set at %1: %2").arg(QString::fromStdString(addr.toHex()), ok ? "Success" : "Failed"), !ok);
        }
    }, "bp <addr/symbol> - Set software breakpoint (int3)");

    registerCommand("bph", [this](const std::vector<std::string>& args) {
        if (args.empty()) {
            Q_EMIT outputLogged("Usage: bph <addr/symbol> [x|w|rw]", true);
            return;
        }
        Address addr = parseAddress(args[0]);
        if (addr.isNull()) {
            Q_EMIT outputLogged("Failed to resolve address for: " + QString::fromStdString(args[0]), true);
            return;
        }
        HardwareBpType type = HardwareBpType::Execute;
        if (args.size() > 1) {
            if (args[1] == "w") type = HardwareBpType::Write;
            else if (args[1] == "rw" || args[1] == "r") type = HardwareBpType::ReadWrite;
        }
        if (session_) {
            bool ok = session_->addHardwareBreakpoint(addr, type);
            Q_EMIT outputLogged(QString("Hardware breakpoint set at %1: %2").arg(QString::fromStdString(addr.toHex()), ok ? "Success" : "Failed"), !ok);
        }
    }, "bph <addr/symbol> [x|w|rw] - Set hardware breakpoint/watchpoint (DR0~DR3)");

    registerCommand("bc", [this](const std::vector<std::string>& args) {
        if (args.empty()) {
            Q_EMIT outputLogged("Usage: bc <addr/symbol>", true);
            return;
        }
        Address addr = parseAddress(args[0]);
        if (session_) {
            bool ok = session_->removeBreakpoint(addr);
            Q_EMIT outputLogged(QString("Breakpoint cleared at %1: %2").arg(QString::fromStdString(addr.toHex()), ok ? "Success" : "Failed"), !ok);
        }
    }, "bc <addr/symbol> - Clear/remove breakpoint");

    registerCommand("be", [this](const std::vector<std::string>& args) {
        if (args.empty()) { Q_EMIT outputLogged("Usage: be <addr>", true); return; }
        Address addr = parseAddress(args[0]);
        if (session_) session_->enableBreakpoint(addr);
        Q_EMIT outputLogged(QString("Enabled breakpoint at %1").arg(QString::fromStdString(addr.toHex())), false);
    }, "be <addr> - Enable breakpoint");

    registerCommand("bd", [this](const std::vector<std::string>& args) {
        if (args.empty()) { Q_EMIT outputLogged("Usage: bd <addr>", true); return; }
        Address addr = parseAddress(args[0]);
        if (session_) session_->disableBreakpoint(addr);
        Q_EMIT outputLogged(QString("Disabled breakpoint at %1").arg(QString::fromStdString(addr.toHex())), false);
    }, "bd <addr> - Disable breakpoint");

    // 2. Navigation: d (dump), u (disassembly)
    registerCommand("d", [this](const std::vector<std::string>& args) {
        if (args.empty()) { Q_EMIT outputLogged("Usage: d <addr/expr>", true); return; }
        Address addr = parseAddress(args[0]);
        if (!addr.isNull()) {
            Q_EMIT jumpToMemoryRequested(addr);
            Q_EMIT outputLogged(QString("Dump navigated to: %1").arg(QString::fromStdString(addr.toHex())), false);
        }
    }, "d <addr/expr> - Navigate memory dump to address");

    registerCommand("u", [this](const std::vector<std::string>& args) {
        if (args.empty()) { Q_EMIT outputLogged("Usage: u <addr/symbol>", true); return; }
        Address addr = parseAddress(args[0]);
        if (!addr.isNull()) {
            Q_EMIT jumpToDisassemblyRequested(addr);
            Q_EMIT outputLogged(QString("Disassembly navigated to: %1").arg(QString::fromStdString(addr.toHex())), false);
        }
    }, "u <addr/symbol> - Navigate disassembly view to address");

    // 3. Execution Control: run, pause, step, stepo, ret
    registerCommand("run", [this](const std::vector<std::string>&) {
        if (session_) session_->resume();
    }, "run (or g) - Resume target execution");
    registerCommand("g", [this](const std::vector<std::string>& args) {
        executeCommand("run " + QString::fromStdString(args.empty() ? "" : args[0]));
    }, "Alias for run");

    registerCommand("pause", [this](const std::vector<std::string>&) {
        if (session_) session_->pause();
    }, "pause - Interrupt and pause target");

    registerCommand("step", [this](const std::vector<std::string>&) {
        if (session_) session_->stepInto();
    }, "step (or s) - Step into single instruction");
    registerCommand("s", [this](const std::vector<std::string>&) {
        if (session_) session_->stepInto();
    }, "Alias for step");

    registerCommand("stepo", [this](const std::vector<std::string>&) {
        if (session_) session_->stepOver();
    }, "stepo (or so) - Step over call/instruction");
    registerCommand("so", [this](const std::vector<std::string>&) {
        if (session_) session_->stepOver();
    }, "Alias for stepo");

    registerCommand("ret", [this](const std::vector<std::string>&) {
        if (session_) session_->stepOut();
    }, "ret - Step out / Run until return");

    // 4. Register inspection & setting: r [reg] [= val]
    registerCommand("r", [this](const std::vector<std::string>& args) {
        if (!session_) return;
        const auto& regs = session_->registers();
        if (args.empty()) {
            // Print all GPRs
            QString out = QString("RAX: %1  RBX: %2  RCX: %3  RDX: %4\n"
                                  "RSI: %5  RDI: %6  RBP: %7  RSP: %8\n"
                                  "R8:  %9  R9:  %10 R10: %11 R11: %12\n"
                                  "R12: %13 R13: %14 R14: %15 R15: %16\n"
                                  "RIP: %17")
                .arg(QString::fromStdString(Address(regs.rax()).toHex()))
                .arg(QString::fromStdString(Address(regs.rbx()).toHex()))
                .arg(QString::fromStdString(Address(regs.rcx()).toHex()))
                .arg(QString::fromStdString(Address(regs.rdx()).toHex()))
                .arg(QString::fromStdString(Address(regs.rsi()).toHex()))
                .arg(QString::fromStdString(Address(regs.rdi()).toHex()))
                .arg(QString::fromStdString(regs.rbp().toHex()))
                .arg(QString::fromStdString(regs.rsp().toHex()))
                .arg(QString::fromStdString(Address(regs.r8()).toHex()))
                .arg(QString::fromStdString(Address(regs.r9()).toHex()))
                .arg(QString::fromStdString(Address(regs.r10()).toHex()))
                .arg(QString::fromStdString(Address(regs.r11()).toHex()))
                .arg(QString::fromStdString(Address(regs.r12()).toHex()))
                .arg(QString::fromStdString(Address(regs.r13()).toHex()))
                .arg(QString::fromStdString(Address(regs.r14()).toHex()))
                .arg(QString::fromStdString(Address(regs.r15()).toHex()))
                .arg(QString::fromStdString(regs.rip().toHex()));
            Q_EMIT outputLogged(out, false);
            return;
        }

        std::string regName = args[0];
        if (args.size() >= 3 && args[1] == "=") {
            // Modify register
            Address val = this->parseAddress(args[2]);
            RegisterContext newRegs = regs;
            bool matched = true;
            if (regName == "rax") newRegs.setRax(val.value());
            else if (regName == "rbx") newRegs.setRbx(val.value());
            else if (regName == "rcx") newRegs.setRcx(val.value());
            else if (regName == "rdx") newRegs.setRdx(val.value());
            else if (regName == "rsi") newRegs.setRsi(val.value());
            else if (regName == "rdi") newRegs.setRdi(val.value());
            else if (regName == "rbp") newRegs.setRbp(val);
            else if (regName == "rsp") newRegs.setRsp(val);
            else if (regName == "rip") newRegs.setRip(val);
            else matched = false;

            if (matched) {
                session_->setRegisters(newRegs);
                Q_EMIT outputLogged(QString("%1 set to %2").arg(QString::fromStdString(regName), QString::fromStdString(val.toHex())), false);
            } else {
                Q_EMIT outputLogged("Unsupported register: " + QString::fromStdString(regName), true);
            }
        } else {
            // Query single register
            auto val = ExpressionEvaluator::evaluate(regName, regs, nullptr);
            if (val.has_value()) {
                Q_EMIT outputLogged(QString("%1 = %2 (Dec: %3)").arg(QString::fromStdString(regName), QString::fromStdString(Address(*val).toHex()), QString::number(*val)), false);
            } else {
                Q_EMIT outputLogged("Unknown register: " + QString::fromStdString(regName), true);
            }
        }
    }, "r [reg] [= val] - Show or set register values");

    // 5. Expression Evaluation: eval <expr>
    registerCommand("eval", [this](const std::vector<std::string>& args) {
        if (args.empty()) { Q_EMIT outputLogged("Usage: eval <expression>", true); return; }
        std::string expr;
        for (const auto& a : args) expr += a + " ";
        if (!session_) return;
        auto res = ExpressionEvaluator::evaluate(expr, session_->registers(), nullptr);
        if (res.has_value()) {
            Q_EMIT outputLogged(QString("Result: %1 (Dec: %2)").arg(QString::fromStdString(Address(*res).toHex()), QString::number(*res)), false);
        } else {
            Q_EMIT outputLogged("Evaluation failed for: " + QString::fromStdString(expr), true);
        }
    }, "eval <expr> - Evaluate arithmetic/register expression");

    // 6. Extended Commands: origin, mprotect, alloc, free, dumpstate, cls, patch, trace
    registerCommand("origin", [this](const std::vector<std::string>& args) {
        if (args.empty()) { Q_EMIT outputLogged("Usage: origin <addr/symbol>", true); return; }
        Address addr = parseAddress(args[0]);
        if (!addr.isNull() && session_) {
            session_->setInstructionPointer(addr);
            Q_EMIT outputLogged("RIP set to: " + QString::fromStdString(addr.toHex()), false);
        }
    }, "origin <addr/symbol> - Set RIP to address without executing");

    registerCommand("setrip", [this](const std::vector<std::string>& args) {
        executeCommand("origin " + QString::fromStdString(args.empty() ? "" : args[0]));
    }, "Alias for origin");

    registerCommand("mprotect", [this](const std::vector<std::string>& args) {
        if (args.size() < 3) { Q_EMIT outputLogged("Usage: mprotect <addr> <size> <r|w|x|rwx>", true); return; }
        Address addr = parseAddress(args[0]);
        size_t sz = std::strtoul(args[1].c_str(), nullptr, 0);
        int prot = 0;
        for (char c : args[2]) {
            if (c == 'r' || c == 'R') prot |= 1;
            if (c == 'w' || c == 'W') prot |= 2;
            if (c == 'x' || c == 'X') prot |= 4;
        }
        if (session_) {
            bool ok = session_->changeMemoryProtection(addr, sz, prot);
            Q_EMIT outputLogged(QString("mprotect %1 (size %2, prot %3): %4")
                                    .arg(QString::fromStdString(addr.toHex()))
                                    .arg(sz)
                                    .arg(prot)
                                    .arg(ok ? "Success" : "Failed"), !ok);
        }
    }, "mprotect <addr> <size> <perms> - Change target page protection (rwx)");

    registerCommand("alloc", [this](const std::vector<std::string>& args) {
        if (args.empty()) { Q_EMIT outputLogged("Usage: alloc <size> [rwx]", true); return; }
        size_t sz = std::strtoul(args[0].c_str(), nullptr, 0);
        int prot = 7; // RWX
        if (args.size() > 1) {
            prot = 0;
            for (char c : args[1]) {
                if (c == 'r' || c == 'R') prot |= 1;
                if (c == 'w' || c == 'W') prot |= 2;
                if (c == 'x' || c == 'X') prot |= 4;
            }
        }
        if (session_) {
            auto addr = session_->allocateMemory(sz, prot);
            if (addr.has_value()) {
                Q_EMIT outputLogged(QString("Allocated %1 bytes at %2").arg(sz).arg(QString::fromStdString(addr->toHex())), false);
            } else {
                Q_EMIT outputLogged("alloc failed", true);
            }
        }
    }, "alloc <size> [perms] - Allocate memory in target process via mmap");

    registerCommand("free", [this](const std::vector<std::string>& args) {
        if (args.size() < 2) { Q_EMIT outputLogged("Usage: free <addr> <size>", true); return; }
        Address addr = parseAddress(args[0]);
        size_t sz = std::strtoul(args[1].c_str(), nullptr, 0);
        if (session_) {
            bool ok = session_->freeMemory(addr, sz);
            Q_EMIT outputLogged(QString("free %1 (size %2): %3").arg(QString::fromStdString(addr.toHex())).arg(sz).arg(ok ? "Success" : "Failed"), !ok);
        }
    }, "free <addr> <size> - Unmap memory in target process via munmap");

    registerCommand("pageguard", [this](const std::vector<std::string>& args) {
        if (args.empty()) {
            Q_EMIT outputLogged("Usage: pageguard <addr/symbol> [size] [none|ro|xo]", true);
            return;
        }
        Address addr = parseAddress(args[0]);
        if (addr.isNull()) {
            Q_EMIT outputLogged("Failed to resolve address for: " + QString::fromStdString(args[0]), true);
            return;
        }
        size_t size = 1;
        if (args.size() > 1) {
            size = std::strtoul(args[1].c_str(), nullptr, 0);
            if (size == 0) size = 1;
        }
        PageGuardAccess access = PageGuardAccess::NoAccess;
        if (args.size() > 2) {
            access = pageGuardAccessFromString(args[2]);
        }
        if (session_) {
            bool ok = session_->addPageGuard(addr, size, access);
            Q_EMIT outputLogged(QString("Page-Guard set at %1 (size %2, type %3): %4")
                                    .arg(QString::fromStdString(addr.toHex()))
                                    .arg(size)
                                    .arg(QString::fromStdString(pageGuardAccessToString(access)))
                                    .arg(ok ? "Success" : "Failed"), !ok);
        }
    }, "pageguard <addr/symbol> [size] [none|ro|xo] - Set Page-Guard memory protection breakpoint");

    registerCommand("guard", [this](const std::vector<std::string>& args) {
        std::string cmd = "pageguard";
        for (const auto& a : args) cmd += " " + a;
        executeCommand(QString::fromStdString(cmd));
    }, "Alias for pageguard");

    registerCommand("unpageguard", [this](const std::vector<std::string>& args) {
        if (args.empty()) {
            Q_EMIT outputLogged("Usage: unpageguard <addr/symbol>", true);
            return;
        }
        Address addr = parseAddress(args[0]);
        if (session_) {
            bool ok = session_->removePageGuard(addr);
            Q_EMIT outputLogged(QString("Page-Guard removed at %1: %2")
                                    .arg(QString::fromStdString(addr.toHex()))
                                    .arg(ok ? "Success" : "Failed"), !ok);
        }
    }, "unpageguard <addr/symbol> - Remove Page-Guard breakpoint");

    registerCommand("unguard", [this](const std::vector<std::string>& args) {
        std::string cmd = "unpageguard";
        for (const auto& a : args) cmd += " " + a;
        executeCommand(QString::fromStdString(cmd));
    }, "Alias for unpageguard");

    registerCommand("pageguards", [this](const std::vector<std::string>&) {
        if (!session_) return;
        auto guards = session_->pageGuardManager().allGuards();
        if (guards.empty()) {
            Q_EMIT outputLogged("No active Page-Guard breakpoints.", false);
            return;
        }
        Q_EMIT outputLogged(QString("Active Page-Guards (%1):").arg(guards.size()), false);
        for (const auto& g : guards) {
            QString line = QString("  [Address: %1 - %2] Page: %3 (Size: %4) Type: %5 Hits: %6 Status: %7")
                               .arg(QString::fromStdString(g.address.toHex()))
                               .arg(QString::fromStdString((g.address + g.size).toHex()))
                               .arg(QString::fromStdString(g.pageBase.toHex()))
                               .arg(g.pageSize)
                               .arg(QString::fromStdString(pageGuardAccessToString(g.access)))
                               .arg(g.hitCount)
                               .arg(g.enabled ? "Enabled" : "Disabled");
            Q_EMIT outputLogged(line, false);
        }
    }, "pageguards - List all active Page-Guard breakpoints");

    registerCommand("guards", [this](const std::vector<std::string>&) {
        executeCommand("pageguards");
    }, "Alias for pageguards");

    registerCommand("dumpstate", [this](const std::vector<std::string>&) {
        if (!session_) return;
        std::string dump = StateDumper::dumpState(*session_);
        Q_EMIT outputLogged(QString::fromStdString(dump), false);
    }, "dumpstate - Dump full CPU machine state, registers, disassembly and stack");

    registerCommand("cls", [](const std::vector<std::string>&) {
        LogManager::instance().clear();
    }, "cls (or clear) - Clear log console");

    registerCommand("clear", [this](const std::vector<std::string>&) {
        executeCommand("cls");
    }, "Alias for cls");

    registerCommand("patch", [this](const std::vector<std::string>& args) {
        if (args.size() < 2) { Q_EMIT outputLogged("Usage: patch <addr> <hexbytes e.g. 9090 or 90 90>", true); return; }
        Address addr = parseAddress(args[0]);
        std::string hexStr;
        for (size_t i = 1; i < args.size(); ++i) hexStr += args[i];
        if (hexStr.size() % 2 != 0) { Q_EMIT outputLogged("Hex bytes must have even number of digits", true); return; }
        std::vector<uint8_t> bytes;
        for (size_t i = 0; i < hexStr.size(); i += 2) {
            bytes.push_back(static_cast<uint8_t>(std::strtoul(hexStr.substr(i, 2).c_str(), nullptr, 16)));
        }
        if (session_ && !bytes.empty()) {
            bool ok = session_->writeMemory(addr, bytes.data(), bytes.size());
            Q_EMIT outputLogged(QString("Patched %1 bytes at %2: %3").arg(bytes.size()).arg(QString::fromStdString(addr.toHex())).arg(ok ? "Success" : "Failed"), !ok);
        }
    }, "patch <addr> <hexbytes> - Write raw hex bytes directly into memory");

    registerCommand("trace", [this](const std::vector<std::string>& args) {
        if (!session_) return;
        bool stepOver = false;
        size_t count = 100;
        std::string cond;
        size_t argIdx = 0;
        if (argIdx < args.size() && (args[argIdx] == "over" || args[argIdx] == "into")) {
            stepOver = (args[argIdx] == "over");
            argIdx++;
        }
        if (argIdx < args.size()) {
            count = std::strtoul(args[argIdx].c_str(), nullptr, 0);
            argIdx++;
        }
        while (argIdx < args.size()) {
            cond += args[argIdx++] + " ";
        }
        auto res = session_->autoTrace(stepOver, count, cond);
        Q_EMIT outputLogged(QString("AutoTrace: %1").arg(QString::fromStdString(res.message)), false);
    }, "trace [into|over] [count] [stopCondition] - Auto-trace instructions until count or condition");

    // 7. Scripting: py, lua
    registerCommand("py", [](const std::vector<std::string>&) {}, "py <code...> - Execute Python 3 script statement or expression");
    registerCommand("lua", [](const std::vector<std::string>&) {}, "lua <code...> - Execute Lua 5.4 script statement or expression");

    // 8. Help: help
    registerCommand("help", [this](const std::vector<std::string>&) {
        QString out = "=== edb-next Command Bar Help ===\n";
        for (const auto& [name, entry] : commands_) {
            out += QString("  %-16s : %2\n").arg(QString::fromStdString(name), QString::fromStdString(entry.help));
        }
        Q_EMIT outputLogged(out, false);
    }, "help - Show available commands");
}

} // namespace edb_next
