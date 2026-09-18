#include "CommandRegistry.hpp"
#include "DebugSession.hpp"
#include "ExpressionEvaluator.hpp"
#include "core/StateDumper.hpp"
#include "core/LogManager.hpp"
#include "core/MemoryScanner.hpp"
#include "core/TypeManager.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace edb_next {

CommandRegistry::CommandRegistry() {
    registerBuiltinCommands();
}

void CommandRegistry::registerCommand(CommandDescriptor desc) {
    std::string name = desc.name;
    for (const auto& alias : desc.aliases) {
        aliasMap_[alias] = name;
    }
    commands_[name] = std::move(desc);
}

void CommandRegistry::registerCommand(const std::string& name,
                                     std::function<void(const std::vector<std::string>&)> simpleHandler,
                                     const std::string& helpText,
                                     CommandCategory category) {
    CommandDescriptor desc;
    desc.name = name;
    desc.category = category;
    desc.syntax = name;
    desc.description = helpText.empty() ? name : helpText;
    desc.handler = [simpleHandler = std::move(simpleHandler)](const CommandContext& ctx) {
        if (simpleHandler) {
            simpleHandler(ctx.args);
        }
    };
    registerCommand(std::move(desc));
}

bool CommandRegistry::execute(const std::string& cmdLine, const CommandContext& baseCtx) {
    std::string trimmed = cmdLine;
    auto start = trimmed.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return true;
    auto end = trimmed.find_last_not_of(" \t\r\n");
    trimmed = trimmed.substr(start, end - start + 1);

    std::istringstream iss(trimmed);
    std::string cmd;
    iss >> cmd;
    if (cmd.empty()) return true;

    // Direct Script execution: py / :py / python, lua / :lua
    if (cmd == "py" || cmd == ":py" || cmd == "python" || cmd == "lua" || cmd == ":lua") {
        std::string lang = (cmd == "lua" || cmd == ":lua") ? "lua" : "python";
        size_t pos = trimmed.find(cmd);
        std::string scriptCode;
        if (pos != std::string::npos) {
            scriptCode = trimmed.substr(pos + cmd.length());
            auto s_pos = scriptCode.find_first_not_of(" \t");
            if (s_pos != std::string::npos) {
                scriptCode = scriptCode.substr(s_pos);
            } else {
                scriptCode.clear();
            }
        }
        if (scriptCode.empty()) {
            baseCtx.error(QString("Usage: %1 <code...>").arg(QString::fromStdString(cmd)));
            return false;
        }

        ScriptResult res;
        if (baseCtx.session) {
            res = baseCtx.session->scriptEngines().execute(lang, scriptCode);
        } else {
            ScriptEngineManager mgr;
            res = mgr.execute(lang, scriptCode);
        }

        if (!res.output.empty()) {
            baseCtx.log(QString::fromStdString(res.output), false);
        }
        if (!res.error.empty()) {
            baseCtx.error(QString::fromStdString(res.error));
        }
        if (res.output.empty() && res.error.empty()) {
            baseCtx.log(res.success ? "Script executed successfully" : "Script execution failed", !res.success);
        }
        return res.success;
    }

    std::vector<std::string> args;
    std::string arg;
    while (iss >> arg) {
        args.push_back(arg);
    }

    std::string resolvedCmd = cmd;
    auto aliasIt = aliasMap_.find(cmd);
    if (aliasIt != aliasMap_.end()) {
        resolvedCmd = aliasIt->second;
    }

    auto cmdIt = commands_.find(resolvedCmd);
    if (cmdIt != commands_.end()) {
        try {
            CommandContext ctx = baseCtx;
            ctx.args = std::move(args);
            ctx.rawCommandLine = trimmed;
            cmdIt->second.handler(ctx);
            return true;
        } catch (const std::exception& ex) {
            baseCtx.error(QString("Error executing '%1': %2").arg(QString::fromStdString(cmd), ex.what()));
            return false;
        }
    }

    baseCtx.error(QString("Unknown command '%1'. Type 'help' for available commands.").arg(QString::fromStdString(cmd)));
    return false;
}

std::vector<std::string> CommandRegistry::complete(const std::string& prefix) const {
    std::vector<std::string> matches;
    for (const auto& [name, desc] : commands_) {
        if (name.rfind(prefix, 0) == 0) {
            matches.push_back(name);
        }
    }
    for (const auto& [alias, target] : aliasMap_) {
        if (alias.rfind(prefix, 0) == 0) {
            matches.push_back(alias);
        }
    }
    std::sort(matches.begin(), matches.end());
    matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
    return matches;
}

std::vector<CommandDescriptor> CommandRegistry::commandsByCategory(CommandCategory cat) const {
    std::vector<CommandDescriptor> result;
    for (const auto& [name, desc] : commands_) {
        if (desc.category == cat) {
            result.push_back(desc);
        }
    }
    return result;
}

std::optional<CommandDescriptor> CommandRegistry::findCommand(const std::string& name) const {
    auto it = commands_.find(name);
    if (it != commands_.end()) {
        return it->second;
    }
    auto aliasIt = aliasMap_.find(name);
    if (aliasIt != aliasMap_.end()) {
        auto targetIt = commands_.find(aliasIt->second);
        if (targetIt != commands_.end()) {
            return targetIt->second;
        }
    }
    return std::nullopt;
}

void CommandRegistry::registerBuiltinCommands() {
    registerBreakpointCommands();
    registerExecutionCommands();
    registerMemoryCommands();
    registerAnalysisCommands();
    registerProcessCommands();
    registerSystemCommands();
}

void CommandRegistry::registerBreakpointCommands() {
    registerCommand(CommandDescriptor{
        .name = "bp",
        .aliases = {},
        .category = CommandCategory::Breakpoint,
        .syntax = "bp <addr/symbol>",
        .description = "Set software breakpoint (int3)",
        .handler = [](const CommandContext& ctx) {
            if (ctx.args.empty()) {
                ctx.error("Usage: bp <addr/symbol>");
                return;
            }
            Address addr = ctx.parseAddress(ctx.args[0]);
            if (addr.isNull()) {
                if (ctx.session) {
                    const auto& token = ctx.args[0];
                    bool looksLikeSymbol = (!token.empty() && (std::isalpha(token[0]) || token[0] == '_'));
                    if (looksLikeSymbol) {
                        bool ok = ctx.session->addPendingBreakpoint(token);
                        if (ok) {
                            ctx.log(QString("Symbol '%1' not yet resolved. Added as Pending Breakpoint (auto-binds upon module load).").arg(QString::fromStdString(token)), false);
                            return;
                        }
                    }
                }
                ctx.error("Failed to resolve address for: " + QString::fromStdString(ctx.args[0]));
                return;
            }
            if (ctx.session) {
                bool ok = ctx.session->addBreakpoint(addr);
                ctx.log(QString("Breakpoint set at %1: %2").arg(addr.toQString(), ok ? "Success" : "Failed"), !ok);
            }
        }
    });

    registerCommand(CommandDescriptor{
        .name = "bpp",
        .aliases = {},
        .category = CommandCategory::Breakpoint,
        .syntax = "bpp <symbol>",
        .description = "Set deferred/pending breakpoint for dynamic library",
        .handler = [](const CommandContext& ctx) {
            if (ctx.args.empty()) {
                ctx.error("Usage: bpp <symbol>");
                return;
            }
            if (!ctx.session) {
                ctx.error("No active debug session");
                return;
            }
            bool ok = ctx.session->addPendingBreakpoint(ctx.args[0]);
            ctx.log(QString("Pending breakpoint for symbol '%1': %2").arg(QString::fromStdString(ctx.args[0]), ok ? "Added (waiting for module load)" : "Already exists or error"), !ok);
        }
    });

    registerCommand(CommandDescriptor{
        .name = "bph",
        .aliases = {},
        .category = CommandCategory::Breakpoint,
        .syntax = "bph <addr/symbol> [x|w|rw]",
        .description = "Set hardware breakpoint/watchpoint (DR0~DR3)",
        .handler = [](const CommandContext& ctx) {
            if (ctx.args.empty()) {
                ctx.error("Usage: bph <addr/symbol> [x|w|rw]");
                return;
            }
            Address addr = ctx.parseAddress(ctx.args[0]);
            if (addr.isNull()) {
                ctx.error("Failed to resolve address for: " + QString::fromStdString(ctx.args[0]));
                return;
            }
            HardwareBpType type = HardwareBpType::Execute;
            if (ctx.args.size() > 1) {
                if (ctx.args[1] == "w") type = HardwareBpType::Write;
                else if (ctx.args[1] == "rw" || ctx.args[1] == "r") type = HardwareBpType::ReadWrite;
            }
            if (ctx.session) {
                bool ok = ctx.session->addHardwareBreakpoint(addr, type);
                ctx.log(QString("Hardware breakpoint set at %1: %2").arg(addr.toQString(), ok ? "Success" : "Failed"), !ok);
            }
        }
    });

    registerCommand(CommandDescriptor{
        .name = "bc",
        .aliases = {},
        .category = CommandCategory::Breakpoint,
        .syntax = "bc <addr/symbol>",
        .description = "Clear/remove breakpoint",
        .handler = [](const CommandContext& ctx) {
            if (ctx.args.empty()) {
                ctx.error("Usage: bc <addr/symbol>");
                return;
            }
            Address addr = ctx.parseAddress(ctx.args[0]);
            if (ctx.session) {
                bool ok = ctx.session->removeBreakpoint(addr);
                ctx.log(QString("Breakpoint cleared at %1: %2").arg(addr.toQString(), ok ? "Success" : "Failed"), !ok);
            }
        }
    });

    registerCommand(CommandDescriptor{
        .name = "be",
        .aliases = {},
        .category = CommandCategory::Breakpoint,
        .syntax = "be <addr>",
        .description = "Enable breakpoint",
        .handler = [](const CommandContext& ctx) {
            if (ctx.args.empty()) { ctx.error("Usage: be <addr>"); return; }
            Address addr = ctx.parseAddress(ctx.args[0]);
            if (ctx.session) ctx.session->enableBreakpoint(addr);
            ctx.log(QString("Enabled breakpoint at %1").arg(addr.toQString()), false);
        }
    });

    registerCommand(CommandDescriptor{
        .name = "bd",
        .aliases = {},
        .category = CommandCategory::Breakpoint,
        .syntax = "bd <addr>",
        .description = "Disable breakpoint",
        .handler = [](const CommandContext& ctx) {
            if (ctx.args.empty()) { ctx.error("Usage: bd <addr>"); return; }
            Address addr = ctx.parseAddress(ctx.args[0]);
            if (ctx.session) ctx.session->disableBreakpoint(addr);
            ctx.log(QString("Disabled breakpoint at %1").arg(addr.toQString()), false);
        }
    });

    registerCommand(CommandDescriptor{
        .name = "pageguard",
        .aliases = {"guard"},
        .category = CommandCategory::Breakpoint,
        .syntax = "pageguard <addr/symbol> [size] [none|ro|xo]",
        .description = "Set Page-Guard memory protection breakpoint",
        .handler = [](const CommandContext& ctx) {
            if (ctx.args.empty()) {
                ctx.error("Usage: pageguard <addr/symbol> [size] [none|ro|xo]");
                return;
            }
            Address addr = ctx.parseAddress(ctx.args[0]);
            if (addr.isNull()) {
                ctx.error("Failed to resolve address for: " + QString::fromStdString(ctx.args[0]));
                return;
            }
            size_t size = 1;
            if (ctx.args.size() > 1) {
                size = std::strtoul(ctx.args[1].c_str(), nullptr, 0);
                if (size == 0) size = 1;
            }
            PageGuardAccess access = PageGuardAccess::NoAccess;
            if (ctx.args.size() > 2) {
                access = pageGuardAccessFromString(ctx.args[2]);
            }
            if (ctx.session) {
                bool ok = ctx.session->addPageGuard(addr, size, access);
                ctx.log(QString("Page-Guard set at %1 (size %2, type %3): %4")
                            .arg(addr.toQString())
                            .arg(size)
                            .arg(QString::fromStdString(pageGuardAccessToString(access)))
                            .arg(ok ? "Success" : "Failed"), !ok);
            }
        }
    });

    registerCommand(CommandDescriptor{
        .name = "unpageguard",
        .aliases = {"unguard"},
        .category = CommandCategory::Breakpoint,
        .syntax = "unpageguard <addr/symbol>",
        .description = "Remove Page-Guard breakpoint",
        .handler = [](const CommandContext& ctx) {
            if (ctx.args.empty()) {
                ctx.error("Usage: unpageguard <addr/symbol>");
                return;
            }
            Address addr = ctx.parseAddress(ctx.args[0]);
            if (ctx.session) {
                bool ok = ctx.session->removePageGuard(addr);
                ctx.log(QString("Page-Guard removed at %1: %2")
                            .arg(addr.toQString())
                            .arg(ok ? "Success" : "Failed"), !ok);
            }
        }
    });

    registerCommand(CommandDescriptor{
        .name = "pageguards",
        .aliases = {"guards"},
        .category = CommandCategory::Breakpoint,
        .syntax = "pageguards",
        .description = "List all active Page-Guard breakpoints",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.session) return;
            auto guards = ctx.session->pageGuardManager().allGuards();
            if (guards.empty()) {
                ctx.log("No active Page-Guard breakpoints.", false);
                return;
            }
            QString out = QString("Active Page-Guards (%1):\n").arg(guards.size());
            for (const auto& g : guards) {
                QString line = QString("  [Address: %1 - %2] Page: %3 (Size: %4) Type: %5 Hits: %6 Status: %7")
                                   .arg(g.address.toQString())
                                   .arg((g.address + g.size).toQString())
                                   .arg(g.pageBase.toQString())
                                   .arg(g.pageSize)
                                   .arg(QString::fromStdString(pageGuardAccessToString(g.access)))
                                   .arg(g.hitCount)
                                   .arg(g.enabled ? "Enabled" : "Disabled");
                ctx.log(line, false);
            }
        }
    });
}

void CommandRegistry::registerExecutionCommands() {
    registerCommand(CommandDescriptor{
        .name = "run",
        .aliases = {"g"},
        .category = CommandCategory::Execution,
        .syntax = "run [g]",
        .description = "Resume target execution",
        .handler = [](const CommandContext& ctx) {
            if (ctx.session) ctx.session->resume();
        }
    });

    registerCommand(CommandDescriptor{
        .name = "pause",
        .aliases = {},
        .category = CommandCategory::Execution,
        .syntax = "pause",
        .description = "Interrupt and pause target",
        .handler = [](const CommandContext& ctx) {
            if (ctx.session) ctx.session->pause();
        }
    });

    registerCommand(CommandDescriptor{
        .name = "step",
        .aliases = {"s", "sti"},
        .category = CommandCategory::Execution,
        .syntax = "step [s, sti]",
        .description = "Step into single instruction",
        .handler = [](const CommandContext& ctx) {
            if (ctx.session) ctx.session->stepInto();
        }
    });

    registerCommand(CommandDescriptor{
        .name = "stepo",
        .aliases = {"so", "sto"},
        .category = CommandCategory::Execution,
        .syntax = "stepo [so, sto]",
        .description = "Step over call/instruction",
        .handler = [](const CommandContext& ctx) {
            if (ctx.session) ctx.session->stepOver();
        }
    });

    registerCommand(CommandDescriptor{
        .name = "ret",
        .aliases = {"rtr"},
        .category = CommandCategory::Execution,
        .syntax = "ret [rtr]",
        .description = "Step out / Run until return",
        .handler = [](const CommandContext& ctx) {
            if (ctx.session) ctx.session->stepOut();
        }
    });

    registerCommand(CommandDescriptor{
        .name = "trace",
        .aliases = {},
        .category = CommandCategory::Execution,
        .syntax = "trace [into|over] [count] [stopCondition]",
        .description = "Auto-trace instructions until count or condition",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.session) return;
            bool stepOver = false;
            size_t count = 100;
            std::string cond;
            size_t argIdx = 0;
            if (argIdx < ctx.args.size() && (ctx.args[argIdx] == "over" || ctx.args[argIdx] == "into")) {
                stepOver = (ctx.args[argIdx] == "over");
                argIdx++;
            }
            if (argIdx < ctx.args.size()) {
                count = std::strtoul(ctx.args[argIdx].c_str(), nullptr, 0);
                argIdx++;
            }
            while (argIdx < ctx.args.size()) {
                cond += ctx.args[argIdx++] + " ";
            }
            auto res = ctx.session->autoTrace(stepOver, count, cond);
            ctx.log(QString("AutoTrace: %1").arg(QString::fromStdString(res.message)), false);
        }
    });
}

void CommandRegistry::registerMemoryCommands() {
    registerCommand(CommandDescriptor{
        .name = "d",
        .aliases = {},
        .category = CommandCategory::Memory,
        .syntax = "d <addr/expr>",
        .description = "Navigate memory dump to address",
        .handler = [](const CommandContext& ctx) {
            if (ctx.args.empty()) { ctx.error("Usage: d <addr/expr>"); return; }
            Address addr = ctx.parseAddress(ctx.args[0]);
            if (!addr.isNull()) {
                if (ctx.jumpToMemory) ctx.jumpToMemory(addr);
                ctx.log(QString("Dump navigated to: %1").arg(addr.toQString()), false);
            }
        }
    });

    registerCommand(CommandDescriptor{
        .name = "patch",
        .aliases = {},
        .category = CommandCategory::Memory,
        .syntax = "patch <addr> <hexbytes e.g. 9090 or 90 90>",
        .description = "Write raw hex bytes directly into memory",
        .handler = [](const CommandContext& ctx) {
            if (ctx.args.size() < 2) { ctx.error("Usage: patch <addr> <hexbytes e.g. 9090 or 90 90>"); return; }
            Address addr = ctx.parseAddress(ctx.args[0]);
            std::string hexStr;
            for (size_t i = 1; i < ctx.args.size(); ++i) hexStr += ctx.args[i];
            if (hexStr.size() % 2 != 0) { ctx.error("Hex bytes must have even number of digits"); return; }
            std::vector<uint8_t> bytes;
            for (size_t i = 0; i < hexStr.size(); i += 2) {
                bytes.push_back(static_cast<uint8_t>(std::strtoul(hexStr.substr(i, 2).c_str(), nullptr, 16)));
            }
            if (ctx.session && !bytes.empty()) {
                bool ok = ctx.session->writeMemory(addr, bytes.data(), bytes.size());
                ctx.log(QString("Patched %1 bytes at %2: %3").arg(bytes.size()).arg(addr.toQString()).arg(ok ? "Success" : "Failed"), !ok);
            }
        }
    });

    registerCommand(CommandDescriptor{
        .name = "mprotect",
        .aliases = {},
        .category = CommandCategory::Memory,
        .syntax = "mprotect <addr> <size> <r|w|x|rwx>",
        .description = "Change target page protection (rwx)",
        .handler = [](const CommandContext& ctx) {
            if (ctx.args.size() < 3) { ctx.error("Usage: mprotect <addr> <size> <r|w|x|rwx>"); return; }
            Address addr = ctx.parseAddress(ctx.args[0]);
            size_t sz = std::strtoul(ctx.args[1].c_str(), nullptr, 0);
            int prot = 0;
            for (char c : ctx.args[2]) {
                if (c == 'r' || c == 'R') prot |= 1;
                if (c == 'w' || c == 'W') prot |= 2;
                if (c == 'x' || c == 'X') prot |= 4;
            }
            if (ctx.session) {
                bool ok = ctx.session->changeMemoryProtection(addr, sz, prot);
                ctx.log(QString("mprotect %1 (size %2, prot %3): %4")
                            .arg(addr.toQString())
                            .arg(sz)
                            .arg(prot)
                            .arg(ok ? "Success" : "Failed"), !ok);
            }
        }
    });

    registerCommand(CommandDescriptor{
        .name = "alloc",
        .aliases = {},
        .category = CommandCategory::Memory,
        .syntax = "alloc <size> [perms]",
        .description = "Allocate memory in target process via mmap",
        .handler = [](const CommandContext& ctx) {
            if (ctx.args.empty()) { ctx.error("Usage: alloc <size> [rwx]"); return; }
            size_t sz = std::strtoul(ctx.args[0].c_str(), nullptr, 0);
            int prot = 7; // RWX
            if (ctx.args.size() > 1) {
                prot = 0;
                for (char c : ctx.args[1]) {
                    if (c == 'r' || c == 'R') prot |= 1;
                    if (c == 'w' || c == 'W') prot |= 2;
                    if (c == 'x' || c == 'X') prot |= 4;
                }
            }
            if (ctx.session) {
                auto addr = ctx.session->allocateMemory(sz, prot);
                if (addr.has_value()) {
                    ctx.log(QString("Allocated %1 bytes at %2").arg(sz).arg(addr->toQString()), false);
                } else {
                    ctx.error("alloc failed");
                }
            }
        }
    });

    registerCommand(CommandDescriptor{
        .name = "free",
        .aliases = {},
        .category = CommandCategory::Memory,
        .syntax = "free <addr> <size>",
        .description = "Unmap memory in target process via munmap",
        .handler = [](const CommandContext& ctx) {
            if (ctx.args.size() < 2) { ctx.error("Usage: free <addr> <size>"); return; }
            Address addr = ctx.parseAddress(ctx.args[0]);
            size_t sz = std::strtoul(ctx.args[1].c_str(), nullptr, 0);
            if (ctx.session) {
                bool ok = ctx.session->freeMemory(addr, sz);
                ctx.log(QString("free %1 (size %2): %3").arg(addr.toQString()).arg(sz).arg(ok ? "Success" : "Failed"), !ok);
            }
        }
    });
}

void CommandRegistry::registerAnalysisCommands() {
    registerCommand(CommandDescriptor{
        .name = "u",
        .aliases = {},
        .category = CommandCategory::Analysis,
        .syntax = "u <addr/symbol>",
        .description = "Navigate disassembly view to address",
        .handler = [](const CommandContext& ctx) {
            if (ctx.args.empty()) { ctx.error("Usage: u <addr/symbol>"); return; }
            Address addr = ctx.parseAddress(ctx.args[0]);
            if (!addr.isNull()) {
                if (ctx.jumpToDisassembly) ctx.jumpToDisassembly(addr);
                ctx.log(QString("Disassembly navigated to: %1").arg(addr.toQString()), false);
            }
        }
    });

    registerCommand(CommandDescriptor{
        .name = "eval",
        .aliases = {},
        .category = CommandCategory::Analysis,
        .syntax = "eval <expression>",
        .description = "Evaluate arithmetic/register expression",
        .handler = [](const CommandContext& ctx) {
            if (ctx.args.empty()) { ctx.error("Usage: eval <expression>"); return; }
            std::string expr;
            for (const auto& a : ctx.args) expr += a + " ";
            if (!ctx.session) return;
            auto res = ExpressionEvaluator::evaluate(expr, ctx.session->registers(), nullptr);
            if (res.has_value()) {
                ctx.log(QString("Result: %1 (Dec: %2)").arg(Address(*res).toQString(), QString::number(*res)), false);
            } else {
                ctx.error("Evaluation failed for: " + QString::fromStdString(expr));
            }
        }
    });

    registerCommand(CommandDescriptor{
        .name = "lbl",
        .aliases = {"label"},
        .category = CommandCategory::Analysis,
        .syntax = "lbl [addr] [name]",
        .description = "List, show, set, or remove user labels",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.session) return;
            if (ctx.args.empty()) {
                const auto& all = ctx.session->annotations().allLabels();
                if (all.empty()) {
                    ctx.log("No user labels defined.", false);
                    return;
                }
                QString out = QString("User Defined Labels (%1):\n").arg(all.size());
                for (const auto& [addrVal, lbl] : all) {
                    out += QString("  %1 -> %2\n").arg(Address(addrVal).toQString(), QString::fromStdString(lbl));
                }
                ctx.log(out.trimmed(), false);
                return;
            }
            Address addr = ctx.parseAddress(ctx.args[0]);
            if (addr.isNull()) {
                ctx.error("Invalid address/symbol: " + QString::fromStdString(ctx.args[0]));
                return;
            }
            if (ctx.args.size() == 1) {
                std::string cur = ctx.session->annotations().getLabel(addr);
                if (cur.empty()) {
                    ctx.log(QString("No label set at %1").arg(addr.toQString()), false);
                } else {
                    ctx.log(QString("Label at %1: %2").arg(addr.toQString(), QString::fromStdString(cur)), false);
                }
                return;
            }
            std::string labelName = ctx.args[1];
            if (labelName == "-" || labelName == "none" || labelName == "del") {
                ctx.session->annotations().removeLabel(addr);
                ctx.log(QString("Label removed from %1").arg(addr.toQString()), false);
            } else {
                ctx.session->annotations().setLabel(addr, labelName);
                ctx.log(QString("Label set at %1 -> %2").arg(addr.toQString(), QString::fromStdString(labelName)), false);
            }
        }
    });

    registerCommand(CommandDescriptor{
        .name = "structs",
        .aliases = {},
        .category = CommandCategory::Analysis,
        .syntax = "structs",
        .description = "List all registered struct types",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.session) {
                ctx.error("No active session.");
                return;
            }
            const auto& typeMgr = ctx.session->typeManager();
            auto names = typeMgr.structNames();
            if (names.empty()) {
                ctx.log("No registered structs.", false);
                return;
            }
            QString out = QString("Registered Structs (%1):\n").arg(names.size());
            for (const auto& name : names) {
                const auto* def = typeMgr.findStruct(name);
                if (def) {
                    out += QString("  struct %1 (size: %2 bytes, align: %3, fields: %4)\n")
                               .arg(QString::fromStdString(name))
                               .arg(def->totalSize)
                               .arg(def->alignment)
                               .arg(def->fields.size());
                }
            }
            ctx.log(out, false);
        }
    });

    registerCommand(CommandDescriptor{
        .name = "struct",
        .aliases = {},
        .category = CommandCategory::Analysis,
        .syntax = "struct <name> <addr_or_expr>",
        .description = "Evaluate and print struct fields at memory address",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.session) {
                ctx.error("No active session.");
                return;
            }
            if (ctx.args.size() < 2) {
                ctx.error("Usage: struct <name> <addr_or_expr>");
                return;
            }
            std::string sname = ctx.args[0];
            std::string expr = ctx.args[1];
            auto res = ExpressionEvaluator::evaluate(expr, ctx.session->registers(), nullptr);
            if (!res.has_value()) {
                ctx.error(QString("Failed to evaluate address expression: %1").arg(QString::fromStdString(expr)));
                return;
            }
            Address baseAddr(*res);
            auto evalOpt = ctx.session->typeManager().evaluate(sname, baseAddr, ctx.session->engine());
            if (!evalOpt.has_value()) {
                ctx.error(QString("Failed to evaluate struct '%1' at %2 (struct not found or read memory failed)").arg(QString::fromStdString(sname), baseAddr.toQString()));
                return;
            }
            const auto& eval = *evalOpt;
            QString out = QString("struct %1 @ %2 (size %3):\n")
                              .arg(QString::fromStdString(eval.structName))
                              .arg(eval.baseAddress.toQString())
                              .arg(eval.totalSize);
            for (const auto& f : eval.fields) {
                QString hexStr;
                for (auto b : f.rawBytes) {
                    hexStr += QString("%1 ").arg(b, 2, 16, QChar('0'));
                }
                out += QString("  +0x%1 : %2 %3 [%4] = %5\n")
                           .arg(f.offset, 3, 16, QChar('0'))
                           .arg(QString::fromStdString(f.typeName), -12)
                           .arg(QString::fromStdString(f.name), -16)
                           .arg(hexStr.trimmed(), -18)
                           .arg(QString::fromStdString(f.formattedValue));
            }
            ctx.log(out, false);
        }
    });

    registerCommand(CommandDescriptor{
        .name = "defstruct",
        .aliases = {},
        .category = CommandCategory::Analysis,
        .syntax = "defstruct <c_code...>",
        .description = "Define and register a new C struct type",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.session) {
                ctx.error("No active session.");
                return;
            }
            if (ctx.args.empty()) {
                ctx.error("Usage: defstruct <C struct declaration>");
                return;
            }
            std::string cCode;
            for (size_t i = 0; i < ctx.args.size(); ++i) {
                if (i > 0) cCode += " ";
                cCode += ctx.args[i];
            }
            std::string err;
            bool ok = ctx.session->typeManager().parseAndRegister(cCode, &err);
            if (ok) {
                ctx.log("Struct registered successfully.", false);
            } else {
                ctx.error(QString("Error parsing struct: %1").arg(QString::fromStdString(err)));
            }
        }
    });

    registerCommand(CommandDescriptor{
        .name = "scan",
        .aliases = {},
        .category = CommandCategory::Analysis,
        .syntax = "scan <value|unknown> [type]",
        .description = "Initiate first memory scan pass (CheatEngine style)",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.session) {
                ctx.error("No active session.");
                return;
            }
            if (ctx.args.empty()) {
                ctx.error("Usage: scan <value|unknown> [type=int32|int64|int16|int8|float|double|str|hex]");
                return;
            }
            ScanOptions opt;
            opt.writableOnly = true;
            opt.alignment = 4;

            std::string val = ctx.args[0];
            std::string typeStr = (ctx.args.size() > 1) ? ctx.args[1] : "int32";

            if (val == "unknown" || val == "?") {
                opt.compareType = ScanCompareType::UnknownInitialValue;
                if (ctx.args.size() > 1) opt.dataType = stringToScanDataType(ctx.args[1]);
            } else {
                opt.compareType = ScanCompareType::ExactValue;
                opt.valueStr = val;
                opt.dataType = stringToScanDataType(typeStr);
            }

            if (opt.dataType == ScanDataType::String || opt.dataType == ScanDataType::ByteArray) {
                opt.alignment = 1;
            } else if (opt.dataType == ScanDataType::Int64 || opt.dataType == ScanDataType::Double) {
                opt.alignment = 8;
            }

            size_t count = ctx.session->firstMemoryScan(opt);
            ctx.log(QString("[Scan] Pass 1 complete. Found %1 candidate addresses for '%2' (%3).")
                        .arg(count)
                        .arg(QString::fromStdString(val))
                        .arg(QString::fromStdString(scanDataTypeToString(opt.dataType))), false);
        }
    });

    registerCommand(CommandDescriptor{
        .name = "nextscan",
        .aliases = {},
        .category = CommandCategory::Analysis,
        .syntax = "nextscan <compare> [val]",
        .description = "Next differential scan pass (e.g. 'nextscan >', 'nextscan 105', 'nextscan + 10')",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.session) {
                ctx.error("No active session.");
                return;
            }
            if (ctx.args.empty()) {
                ctx.error("Usage: nextscan <exact|inc|dec|diff|same|+delta|-delta> [val/delta]");
                return;
            }
            if (!ctx.session->memoryScanner().hasSearched()) {
                ctx.error("No active scan in progress. Run 'scan <value>' first.");
                return;
            }

            ScanOptions opt = ctx.session->memoryScanner().activeOptions();
            std::string compStr = ctx.args[0];

            if (compStr == "exact" || compStr == "==" || compStr == "=") {
                if (ctx.args.size() < 2) {
                    ctx.error("Usage: nextscan exact <new_value>");
                    return;
                }
                opt.compareType = ScanCompareType::ExactValue;
                opt.valueStr = ctx.args[1];
            } else if (compStr == "inc" || compStr == ">" || compStr == "increased") {
                opt.compareType = ScanCompareType::IncreasedValue;
            } else if (compStr == "dec" || compStr == "<" || compStr == "decreased") {
                opt.compareType = ScanCompareType::DecreasedValue;
            } else if (compStr == "diff" || compStr == "!=" || compStr == "changed") {
                opt.compareType = ScanCompareType::ChangedValue;
            } else if (compStr == "same" || compStr == "unchanged") {
                opt.compareType = ScanCompareType::UnchangedValue;
            } else if (compStr == "+" || compStr == "increasedby") {
                if (ctx.args.size() < 2) { ctx.error("Usage: nextscan + <delta>"); return; }
                opt.compareType = ScanCompareType::IncreasedBy;
                opt.deltaStr = ctx.args[1];
            } else if (compStr == "-" || compStr == "decreasedby") {
                if (ctx.args.size() < 2) { ctx.error("Usage: nextscan - <delta>"); return; }
                opt.compareType = ScanCompareType::DecreasedBy;
                opt.deltaStr = ctx.args[1];
            } else {
                opt.compareType = ScanCompareType::ExactValue;
                opt.valueStr = compStr;
            }

            size_t count = ctx.session->nextMemoryScan(opt);
            ctx.log(QString("[Scan] Pass %1 complete. Converged to %2 candidate addresses.")
                        .arg(ctx.session->memoryScanner().scanPass())
                        .arg(count), false);
        }
    });

    registerCommand(CommandDescriptor{
        .name = "scanresults",
        .aliases = {},
        .category = CommandCategory::Analysis,
        .syntax = "scanresults [limit]",
        .description = "Display top candidate addresses from current scan",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.session) {
                ctx.error("No active session.");
                return;
            }
            const auto& res = ctx.session->memoryScanner().results();
            if (res.empty()) {
                ctx.log("No memory scan results available.", false);
                return;
            }
            size_t limit = 10;
            if (!ctx.args.empty()) {
                try { limit = std::stoul(ctx.args[0]); } catch (...) {}
            }
            limit = std::min<size_t>(limit, res.size());

            auto type = ctx.session->memoryScanner().activeOptions().dataType;
            QString out = QString("=== Memory Scanner Results (Top %1 of %2) ===\n").arg(limit).arg(res.size());
            for (size_t i = 0; i < limit; ++i) {
                out += QString("  [%1] %2 | Type: %3 | Prev: %4 | Cur: %5 | Delta: %6\n")
                           .arg(i + 1, 2)
                           .arg(res[i].address.toQString())
                           .arg(QString::fromStdString(scanDataTypeToString(type)))
                           .arg(QString::fromStdString(res[i].formatPreviousValue(type)))
                           .arg(QString::fromStdString(res[i].formatCurrentValue(type)))
                           .arg(QString::fromStdString(res[i].formatDelta(type)));
            }
            ctx.log(out, false);
        }
    });

    registerCommand(CommandDescriptor{
        .name = "scanreset",
        .aliases = {},
        .category = CommandCategory::Analysis,
        .syntax = "scanreset",
        .description = "Reset memory scanner and clear candidate list",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.session) {
                ctx.error("No active session.");
                return;
            }
            ctx.session->resetMemoryScan();
            ctx.log("Memory scanner reset.", false);
        }
    });
}

void CommandRegistry::registerProcessCommands() {
    registerCommand(CommandDescriptor{
        .name = "modules",
        .aliases = {"libs", "solist"},
        .category = CommandCategory::Process,
        .syntax = "modules [libs, solist]",
        .description = "List all loaded shared libraries (_r_debug)",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.session) {
                ctx.error("No active debug session");
                return;
            }
            auto libs = ctx.session->loadedLibraries();
            if (libs.empty()) {
                ctx.log("No shared libraries loaded (or process is static)", false);
                return;
            }
            QString out = QString("=== Loaded Shared Libraries (%1) ===\n").arg(libs.size());
            out += QString("  %1  %2  %3  %4\n")
                       .arg("Base Address", -18)
                       .arg("Dynamic (l_ld)", -18)
                       .arg("Name", -24)
                       .arg("Path");
            out += "  --------------------------------------------------------------------------------------\n";
            for (const auto& lib : libs) {
                out += QString("  %1  %2  %3  %4\n")
                           .arg(lib.baseAddress.toQString(), -18)
                           .arg(lib.dynamicAddress.toQString(), -18)
                           .arg(QString::fromStdString(lib.name), -24)
                           .arg(QString::fromStdString(lib.path));
            }
            ctx.log(out, false);
        }
    });

    registerCommand(CommandDescriptor{
        .name = "catch",
        .aliases = {},
        .category = CommandCategory::Process,
        .syntax = "catch <load|dlopen|fork|vfork>",
        .description = "Toggle pause on runtime events",
        .handler = [](const CommandContext& ctx) {
            if (ctx.args.empty()) {
                ctx.error("Usage: catch <load|dlopen|fork|vfork>");
                return;
            }
            if (ctx.args[0] == "load" || ctx.args[0] == "dlopen" || ctx.args[0] == "unload") {
                if (ctx.session) {
                    bool newState = !ctx.session->stopOnLibraryEvents();
                    ctx.session->setStopOnLibraryEvents(newState);
                    ctx.log(QString("Catch shared library events (stop on load/unload): %1").arg(newState ? "ENABLED" : "DISABLED"), false);
                }
            } else if (ctx.args[0] == "fork" || ctx.args[0] == "vfork") {
                if (ctx.session) {
                    bool newState = !ctx.session->stopOnForkEvents();
                    ctx.session->setStopOnForkEvents(newState);
                    ctx.log(QString("Catch fork events (stop on fork/vfork): %1").arg(newState ? "ENABLED" : "DISABLED"), false);
                }
            } else {
                ctx.error("Unknown catch event: " + QString::fromStdString(ctx.args[0]));
            }
        }
    });

    registerCommand(CommandDescriptor{
        .name = "follow-fork",
        .aliases = {},
        .category = CommandCategory::Process,
        .syntax = "follow-fork [parent|child|both]",
        .description = "Set or query follow-fork mode",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.session) {
                ctx.error("No active session.");
                return;
            }
            if (ctx.args.empty()) {
                std::string mode_str = followForkModeToString(ctx.session->followForkMode());
                ctx.log(QString("Current follow-fork mode: %1").arg(mode_str.c_str()), false);
                return;
            }
            std::string mode = ctx.args[0];
            if (mode == "parent") {
                ctx.session->setFollowForkMode(FollowForkMode::Parent);
                ctx.log("Follow-fork mode set to: PARENT (child process runs detached)", false);
            } else if (mode == "child") {
                ctx.session->setFollowForkMode(FollowForkMode::Child);
                ctx.log("Follow-fork mode set to: CHILD (debugger follows child, detaches parent)", false);
            } else if (mode == "both") {
                ctx.session->setFollowForkMode(FollowForkMode::Both);
                ctx.log("Follow-fork mode set to: BOTH (child process gets dedicated session tab)", false);
            } else {
                ctx.error("Invalid mode. Usage: follow-fork <parent|child|both>");
            }
        }
    });

    registerCommand(CommandDescriptor{
        .name = "set",
        .aliases = {},
        .category = CommandCategory::Process,
        .syntax = "set follow-fork-mode <parent|child|both>",
        .description = "Configure follow-fork mode",
        .handler = [](const CommandContext& ctx) {
            if (ctx.args.size() >= 2 && (ctx.args[0] == "follow-fork-mode" || ctx.args[0] == "follow-fork" || ctx.args[0] == "fork")) {
                ctx.execute("follow-fork " + QString::fromStdString(ctx.args[1]));
                return;
            }
            ctx.error("Usage: set follow-fork-mode <parent|child|both>");
        }
    });

    registerCommand(CommandDescriptor{
        .name = "show",
        .aliases = {},
        .category = CommandCategory::Process,
        .syntax = "show follow-fork-mode",
        .description = "Display current follow-fork mode",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.args.empty() && (ctx.args[0] == "follow-fork-mode" || ctx.args[0] == "follow-fork" || ctx.args[0] == "fork")) {
                ctx.execute("follow-fork");
                return;
            }
            ctx.error("Usage: show follow-fork-mode");
        }
    });

    registerCommand(CommandDescriptor{
        .name = "inferiors",
        .aliases = {"processes"},
        .category = CommandCategory::Process,
        .syntax = "inferiors [processes]",
        .description = "Display current active inferior process",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.session) {
                ctx.error("No active session.");
                return;
            }
            QString out = QString("Active Inferior: [%1] PID: %2 Target: %3 (Follow-Fork: %4)")
                .arg(QString::fromStdString(ctx.session->name()))
                .arg(ctx.session->pid())
                .arg(QString::fromStdString(ctx.session->targetPath()))
                .arg(followForkModeToString(ctx.session->followForkMode()));
            ctx.log(out, false);
        }
    });

    registerCommand(CommandDescriptor{
        .name = "inferior",
        .aliases = {"process"},
        .category = CommandCategory::Process,
        .syntax = "inferior <id|pid> [process]",
        .description = "Switch active session/tab to specified inferior",
        .handler = [](const CommandContext& ctx) {
            if (ctx.args.empty()) {
                ctx.execute("inferiors");
                return;
            }
            if (ctx.switchSession) {
                ctx.switchSession(QString::fromStdString(ctx.args[0]));
            }
        }
    });

    registerCommand(CommandDescriptor{
        .name = "threads",
        .aliases = {},
        .category = CommandCategory::Process,
        .syntax = "threads",
        .description = "List all threads in the current target process",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.session) {
                ctx.error("No active session.");
                return;
            }
            auto threads = ctx.session->getThreads();
            QString out = QString("=== Threads (%1 total, active TID: %2) ===\n")
                .arg(threads.size()).arg(ctx.session->activeTid());
            for (const auto& t : threads) {
                out += QString("%1 [TID: %2] %3 (State: %4%5) RIP: %6 %7\n")
                    .arg(t.isActive ? "➔" : " ")
                    .arg(t.tid, 6)
                    .arg(QString::fromStdString(t.name), -16)
                    .arg(QString::fromStdString(t.state))
                    .arg(t.isFrozen ? ", ❄ FROZEN" : "")
                    .arg(t.rip.toQString())
                    .arg(QString::fromStdString(t.symbol));
            }
            ctx.log(out, false);
        }
    });

    registerCommand(CommandDescriptor{
        .name = "thread",
        .aliases = {},
        .category = CommandCategory::Process,
        .syntax = "thread [tid]",
        .description = "Switch to or display threads",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.session) {
                ctx.error("No active session.");
                return;
            }
            if (ctx.args.empty()) {
                ctx.execute("threads");
                return;
            }
            try {
                Tid tid = static_cast<Tid>(std::stol(ctx.args[0], nullptr, 0));
                if (ctx.session->switchThread(tid)) {
                    ctx.log(QString("Switched active thread to TID: %1").arg(tid), false);
                } else {
                    ctx.error(QString("Failed to switch to TID: %1").arg(tid));
                }
            } catch (...) {
                ctx.error("Usage: thread <tid>");
            }
        }
    });

    registerCommand(CommandDescriptor{
        .name = "freeze",
        .aliases = {},
        .category = CommandCategory::Process,
        .syntax = "freeze <tid|all>",
        .description = "Freeze a specific thread or all other threads",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.session) {
                ctx.error("No active session.");
                return;
            }
            if (ctx.args.empty()) {
                ctx.error("Usage: freeze <tid|all>");
                return;
            }
            if (ctx.args[0] == "all") {
                ctx.session->freezeAllOtherThreads();
                ctx.log(QString("Frozen all threads except active TID: %1").arg(ctx.session->activeTid()), false);
                return;
            }
            try {
                Tid tid = static_cast<Tid>(std::stol(ctx.args[0], nullptr, 0));
                if (ctx.session->freezeThread(tid)) {
                    ctx.log(QString("Thread %1 is now FROZEN (❄).").arg(tid), false);
                } else {
                    ctx.error(QString("Failed to freeze thread %1.").arg(tid));
                }
            } catch (...) {
                ctx.error("Usage: freeze <tid|all>");
            }
        }
    });

    registerCommand(CommandDescriptor{
        .name = "thaw",
        .aliases = {},
        .category = CommandCategory::Process,
        .syntax = "thaw [tid|all]",
        .description = "Thaw a frozen thread or all threads",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.session) {
                ctx.error("No active session.");
                return;
            }
            if (ctx.args.empty() || ctx.args[0] == "all") {
                ctx.session->thawAllThreads();
                ctx.log("All threads have been THAWED (🔥).", false);
                return;
            }
            try {
                Tid tid = static_cast<Tid>(std::stol(ctx.args[0], nullptr, 0));
                if (ctx.session->thawThread(tid)) {
                    ctx.log(QString("Thread %1 is now THAWED (🔥).").arg(tid), false);
                } else {
                    ctx.error(QString("Failed to thaw thread %1 (not found or not frozen).").arg(tid));
                }
            } catch (...) {
                ctx.error("Usage: thaw <tid|all>");
            }
        }
    });
}

void CommandRegistry::registerSystemCommands() {
    registerCommand(CommandDescriptor{
        .name = "r",
        .aliases = {},
        .category = CommandCategory::System,
        .syntax = "r [reg] [= val]",
        .description = "Show or set register values",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.session) return;
            const auto& regs = ctx.session->registers();
            if (ctx.args.empty()) {
                QString out = QString("RAX: %1  RBX: %2  RCX: %3  RDX: %4\n"
                                      "RSI: %5  RDI: %6  RBP: %7  RSP: %8\n"
                                      "R8:  %9  R9:  %10 R10: %11 R11: %12\n"
                                      "R12: %13 R13: %14 R14: %15 R15: %16\n"
                                      "RIP: %17")
                    .arg(Address(regs.rax()).toQString())
                    .arg(Address(regs.rbx()).toQString())
                    .arg(Address(regs.rcx()).toQString())
                    .arg(Address(regs.rdx()).toQString())
                    .arg(Address(regs.rsi()).toQString())
                    .arg(Address(regs.rdi()).toQString())
                    .arg(regs.rbp().toQString())
                    .arg(regs.rsp().toQString())
                    .arg(Address(regs.r8()).toQString())
                    .arg(Address(regs.r9()).toQString())
                    .arg(Address(regs.r10()).toQString())
                    .arg(Address(regs.r11()).toQString())
                    .arg(Address(regs.r12()).toQString())
                    .arg(Address(regs.r13()).toQString())
                    .arg(Address(regs.r14()).toQString())
                    .arg(Address(regs.r15()).toQString())
                    .arg(regs.rip().toQString());
                ctx.log(out, false);
                return;
            }

            std::string regName = ctx.args[0];
            if (ctx.args.size() >= 3 && ctx.args[1] == "=") {
                Address val = ctx.parseAddress(ctx.args[2]);
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
                    ctx.session->setRegisters(newRegs);
                    ctx.log(QString("%1 set to %2").arg(QString::fromStdString(regName), val.toQString()), false);
                } else {
                    ctx.error("Unsupported register: " + QString::fromStdString(regName));
                }
            } else {
                auto val = ExpressionEvaluator::evaluate(regName, regs, nullptr);
                if (val.has_value()) {
                    ctx.log(QString("%1 = %2 (Dec: %3)").arg(QString::fromStdString(regName), Address(*val).toQString(), QString::number(*val)), false);
                } else {
                    ctx.error("Unknown register: " + QString::fromStdString(regName));
                }
            }
        }
    });

    registerCommand(CommandDescriptor{
        .name = "origin",
        .aliases = {},
        .category = CommandCategory::System,
        .syntax = "origin [addr/symbol]",
        .description = "Jump to current RIP, or set RIP if address given",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.session) return;
            if (ctx.args.empty()) {
                Address rip = ctx.session->registers().rip();
                if (ctx.jumpToDisassembly) ctx.jumpToDisassembly(rip);
                ctx.log("Disassembly centered on current RIP: " + rip.toQString(), false);
                return;
            }
            Address addr = ctx.parseAddress(ctx.args[0]);
            if (!addr.isNull()) {
                ctx.session->setInstructionPointer(addr);
                ctx.log("RIP set to: " + addr.toQString(), false);
            }
        }
    });

    registerCommand(CommandDescriptor{
        .name = "rip",
        .aliases = {},
        .category = CommandCategory::System,
        .syntax = "rip",
        .description = "Center disassembly on current RIP",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.session) return;
            Address rip = ctx.session->registers().rip();
            if (ctx.jumpToDisassembly) ctx.jumpToDisassembly(rip);
            ctx.log("Disassembly centered on current RIP: " + rip.toQString(), false);
        }
    });

    registerCommand(CommandDescriptor{
        .name = "setrip",
        .aliases = {},
        .category = CommandCategory::System,
        .syntax = "setrip <addr/symbol>",
        .description = "Force set RIP register to target address",
        .handler = [](const CommandContext& ctx) {
            if (ctx.args.empty()) { ctx.error("Usage: setrip <addr/symbol>"); return; }
            Address addr = ctx.parseAddress(ctx.args[0]);
            if (!addr.isNull() && ctx.session) {
                ctx.session->setInstructionPointer(addr);
                ctx.log("RIP set to: " + addr.toQString(), false);
            }
        }
    });

    registerCommand(CommandDescriptor{
        .name = "dumpstate",
        .aliases = {},
        .category = CommandCategory::System,
        .syntax = "dumpstate",
        .description = "Dump full CPU machine state, registers, disassembly and stack",
        .handler = [](const CommandContext& ctx) {
            if (!ctx.session) return;
            std::string dump = StateDumper::dumpState(*ctx.session);
            ctx.log(QString::fromStdString(dump), false);
        }
    });

    registerCommand(CommandDescriptor{
        .name = "cls",
        .aliases = {"clear"},
        .category = CommandCategory::System,
        .syntax = "cls [clear]",
        .description = "Clear log console",
        .handler = [](const CommandContext&) {
            LogManager::instance().clear();
        }
    });

    registerCommand(CommandDescriptor{
        .name = "py",
        .aliases = {":py", "python"},
        .category = CommandCategory::System,
        .syntax = "py <code...>",
        .description = "Execute Python 3 script statement or expression",
        .handler = [](const CommandContext&) {}
    });

    registerCommand(CommandDescriptor{
        .name = "lua",
        .aliases = {":lua"},
        .category = CommandCategory::System,
        .syntax = "lua <code...>",
        .description = "Execute Lua 5.4 script statement or expression",
        .handler = [](const CommandContext&) {}
    });

    registerCommand(CommandDescriptor{
        .name = "help",
        .aliases = {"?"},
        .category = CommandCategory::System,
        .syntax = "help [command]",
        .description = "Show available commands organized by category or detailed command help",
        .handler = [this](const CommandContext& ctx) {
            if (!ctx.args.empty()) {
                std::string target = ctx.args[0];
                auto opt = findCommand(target);
                if (opt.has_value()) {
                    QString out = QString("Command: %1\nCategory: %2\nSyntax: %3\nDescription: %4\n")
                        .arg(QString::fromStdString(opt->name))
                        .arg(commandCategoryToString(opt->category))
                        .arg(QString::fromStdString(opt->syntax.empty() ? opt->name : opt->syntax))
                        .arg(QString::fromStdString(opt->description));
                    if (!opt->aliases.empty()) {
                        QString aliasList;
                        for (const auto& a : opt->aliases) {
                            if (!aliasList.isEmpty()) aliasList += ", ";
                            aliasList += QString::fromStdString(a);
                        }
                        out += "Aliases: " + aliasList + "\n";
                    }
                    ctx.log(out.trimmed(), false);
                    return;
                }
            }

            QString out = "=== edb-next Command Bar Help ===\n";
            static const std::vector<CommandCategory> categories = {
                CommandCategory::Execution,
                CommandCategory::Breakpoint,
                CommandCategory::Memory,
                CommandCategory::Analysis,
                CommandCategory::Process,
                CommandCategory::System,
                CommandCategory::Plugin
            };
            for (auto cat : categories) {
                auto cmds = commandsByCategory(cat);
                if (cmds.empty()) continue;
                out += QString("\n[%1 Commands]\n").arg(commandCategoryToString(cat));
                for (const auto& desc : cmds) {
                    QString label = QString::fromStdString(desc.name);
                    if (!desc.aliases.empty()) {
                        QString aliasStr;
                        for (const auto& a : desc.aliases) {
                            if (!aliasStr.isEmpty()) aliasStr += ", ";
                            aliasStr += QString::fromStdString(a);
                        }
                        label += QString(" (%1)").arg(aliasStr);
                    }
                    out += QString("  %-20s : %2\n").arg(label, QString::fromStdString(desc.description));
                }
            }
            ctx.log(out, false);
        }
    });
}

} // namespace edb_next
