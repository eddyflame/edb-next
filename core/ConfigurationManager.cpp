#include "ConfigurationManager.hpp"
#include <QSettings>
#include <QDir>
#include <csignal>

namespace edb_next {

ConfigurationManager& ConfigurationManager::instance() {
    static ConfigurationManager inst;
    return inst;
}

ConfigurationManager::ConfigurationManager(QObject* parent) : QObject(parent) {
    general_.sessionDir = QDir::homePath() + "/.config/edb-next/sessions";
    directories_.pluginDir = QDir::homePath() + "/.config/edb-next/plugins";
    directories_.scriptDir = QDir::homePath() + "/.config/edb-next/scripts";

    initDefaultSignals();
    load();
}

void ConfigurationManager::initDefaultSignals() {
    struct SigDef {
        int num;
        const char* name;
        bool stop;
        bool pass;
        bool log;
    };

    static const SigDef defaults[] = {
        {SIGHUP,    "SIGHUP (Hangup)",                  true,  false, true},
        {SIGINT,    "SIGINT (Terminal Interrupt)",       true,  false, true},
        {SIGQUIT,   "SIGQUIT (Terminal Quit)",           true,  false, true},
        {SIGILL,    "SIGILL (Illegal Instruction)",      true,  false, true},
        {SIGTRAP,   "SIGTRAP (Trace/Breakpoint Trap)",  true,  false, true},
        {SIGABRT,   "SIGABRT (Process Abort)",           true,  false, true},
        {SIGBUS,    "SIGBUS (Bus Error)",                true,  false, true},
        {SIGFPE,    "SIGFPE (Floating Point Error)",    true,  false, true},
        {SIGKILL,   "SIGKILL (Kill)",                   false, true,  true},
        {SIGUSR1,   "SIGUSR1 (User Signal 1)",          false, true,  true},
        {SIGSEGV,   "SIGSEGV (Segmentation Fault)",     true,  false, true},
        {SIGUSR2,   "SIGUSR2 (User Signal 2)",          false, true,  true},
        {SIGPIPE,   "SIGPIPE (Broken Pipe)",             false, true,  true},
        {SIGALRM,   "SIGALRM (Alarm Clock)",            false, true,  false},
        {SIGTERM,   "SIGTERM (Termination)",            true,  false, true},
        {SIGCHLD,   "SIGCHLD (Child Status Changed)",    false, true,  false},
        {SIGCONT,   "SIGCONT (Continue)",               false, true,  false},
        {SIGSTOP,   "SIGSTOP (Stop)",                   true,  false, true},
        {SIGTSTP,   "SIGTSTP (Terminal Stop)",          true,  false, true},
        {SIGTTIN,   "SIGTTIN (Background Read)",        false, true,  false},
        {SIGTTOU,   "SIGTTOU (Background Write)",       false, true,  false},
        {SIGURG,    "SIGURG (Urgent Socket Condition)", false, true,  false},
        {SIGXCPU,   "SIGXCPU (CPU Limit Exceeded)",     true,  false, true},
        {SIGXFSZ,   "SIGXFSZ (File Size Exceeded)",     true,  false, true},
        {SIGVTALRM, "SIGVTALRM (Virtual Alarm)",        false, true,  false},
        {SIGPROF,   "SIGPROF (Profiling Timer)",        false, true,  false},
        {SIGWINCH,  "SIGWINCH (Window Size Change)",    false, true,  false},
        {SIGSYS,    "SIGSYS (Bad System Call)",         true,  false, true}
    };

    signals_.clear();
    for (const auto& d : defaults) {
        signals_[d.num] = SignalPolicy{
            .signalNumber = d.num,
            .signalName = QString::fromUtf8(d.name),
            .stopDebugger = d.stop,
            .passToApp = d.pass,
            .logEvent = d.log
        };
    }
}

void ConfigurationManager::load() {
    QSettings s("edb-next", "debugger");

    s.beginGroup("General");
    general_.closeBehavior = static_cast<CloseBehavior>(s.value("closeBehavior", static_cast<int>(general_.closeBehavior)).toInt());
    general_.restoreWindowGeometry = s.value("restoreWindowGeometry", general_.restoreWindowGeometry).toBool();
    general_.sessionDir = s.value("sessionDir", general_.sessionDir).toString();
    windowGeometry_ = s.value("windowGeometry").toByteArray();
    windowState_ = s.value("windowState").toByteArray();
    s.endGroup();

    s.beginGroup("Appearance");
    appearance_.themeName = s.value("themeName", appearance_.themeName).toString();
    if (s.contains("disasmFont")) appearance_.disasmFont.fromString(s.value("disasmFont").toString());
    if (s.contains("registerFont")) appearance_.registerFont.fromString(s.value("registerFont").toString());
    if (s.contains("stackFont")) appearance_.stackFont.fromString(s.value("stackFont").toString());
    if (s.contains("hexDumpFont")) appearance_.hexDumpFont.fromString(s.value("hexDumpFont").toString());
    appearance_.showAddressColon = s.value("showAddressColon", appearance_.showAddressColon).toBool();
    appearance_.showJumpArrows = s.value("showJumpArrows", appearance_.showJumpArrows).toBool();
    appearance_.highlightChangedRegisters = s.value("highlightChangedRegisters", appearance_.highlightChangedRegisters).toBool();
    s.endGroup();

    s.beginGroup("Engine");
    engine_.disableASLR = s.value("disableASLR", engine_.disableASLR).toBool();
    engine_.disableLazyBinding = s.value("disableLazyBinding", engine_.disableLazyBinding).toBool();
    engine_.initialBreakpoint = static_cast<InitialBreakpoint>(s.value("initialBreakpoint", static_cast<int>(engine_.initialBreakpoint)).toInt());
    engine_.breakOnLibraryLoad = s.value("breakOnLibraryLoad", engine_.breakOnLibraryLoad).toBool();
    engine_.ptyTerminalEnabled = s.value("ptyTerminalEnabled", engine_.ptyTerminalEnabled).toBool();
    engine_.ptyTerminalCommand = s.value("ptyTerminalCommand", engine_.ptyTerminalCommand).toString();
    engine_.defaultBpSlot = s.value("defaultBpSlot", engine_.defaultBpSlot).toInt();
    s.endGroup();

    s.beginGroup("Disassembly");
    disasm_.syntax = static_cast<DisassemblySyntax>(s.value("syntax", static_cast<int>(disasm_.syntax)).toInt());
    disasm_.engine = static_cast<DisassemblyEngine>(s.value("engine", static_cast<int>(disasm_.engine)).toInt());
    disasm_.uppercaseMnemonics = s.value("uppercaseMnemonics", disasm_.uppercaseMnemonics).toBool();
    disasm_.showSymbolicAddresses = s.value("showSymbolicAddresses", disasm_.showSymbolicAddresses).toBool();
    disasm_.simplifyRipRelative = s.value("simplifyRipRelative", disasm_.simplifyRipRelative).toBool();
    disasm_.showBytesInHex = s.value("showBytesInHex", disasm_.showBytesInHex).toBool();
    disasm_.tabSize = s.value("tabSize", disasm_.tabSize).toInt();
    s.endGroup();

    s.beginGroup("Directories");
    directories_.pluginDir = s.value("pluginDir", directories_.pluginDir).toString();
    directories_.symbolSearchPath = s.value("symbolSearchPath", directories_.symbolSearchPath).toString();
    directories_.scriptDir = s.value("scriptDir", directories_.scriptDir).toString();
    s.endGroup();

    s.beginGroup("RecentFiles");
    recentFiles_ = s.value("list").toStringList();
    s.endGroup();

    // Signal policies
    s.beginGroup("Signals");
    for (auto it = signals_.begin(); it != signals_.end(); ++it) {
        QString prefix = QString("sig_%1_").arg(it.key());
        if (s.contains(prefix + "stop")) {
            it->stopDebugger = s.value(prefix + "stop").toBool();
            it->passToApp = s.value(prefix + "pass").toBool();
            it->logEvent = s.value(prefix + "log").toBool();
        }
    }
    s.endGroup();
}

void ConfigurationManager::save() {
    QSettings s("edb-next", "debugger");

    s.beginGroup("General");
    s.setValue("closeBehavior", static_cast<int>(general_.closeBehavior));
    s.setValue("restoreWindowGeometry", general_.restoreWindowGeometry);
    s.setValue("sessionDir", general_.sessionDir);
    if (!windowGeometry_.isEmpty()) s.setValue("windowGeometry", windowGeometry_);
    if (!windowState_.isEmpty()) s.setValue("windowState", windowState_);
    s.endGroup();

    s.beginGroup("Appearance");
    s.setValue("themeName", appearance_.themeName);
    s.setValue("disasmFont", appearance_.disasmFont.toString());
    s.setValue("registerFont", appearance_.registerFont.toString());
    s.setValue("stackFont", appearance_.stackFont.toString());
    s.setValue("hexDumpFont", appearance_.hexDumpFont.toString());
    s.setValue("showAddressColon", appearance_.showAddressColon);
    s.setValue("showJumpArrows", appearance_.showJumpArrows);
    s.setValue("highlightChangedRegisters", appearance_.highlightChangedRegisters);
    s.endGroup();

    s.beginGroup("Engine");
    s.setValue("disableASLR", engine_.disableASLR);
    s.setValue("disableLazyBinding", engine_.disableLazyBinding);
    s.setValue("initialBreakpoint", static_cast<int>(engine_.initialBreakpoint));
    s.setValue("breakOnLibraryLoad", engine_.breakOnLibraryLoad);
    s.setValue("ptyTerminalEnabled", engine_.ptyTerminalEnabled);
    s.setValue("ptyTerminalCommand", engine_.ptyTerminalCommand);
    s.setValue("defaultBpSlot", engine_.defaultBpSlot);
    s.endGroup();

    s.beginGroup("Disassembly");
    s.setValue("syntax", static_cast<int>(disasm_.syntax));
    s.setValue("engine", static_cast<int>(disasm_.engine));
    s.setValue("uppercaseMnemonics", disasm_.uppercaseMnemonics);
    s.setValue("showSymbolicAddresses", disasm_.showSymbolicAddresses);
    s.setValue("simplifyRipRelative", disasm_.simplifyRipRelative);
    s.setValue("showBytesInHex", disasm_.showBytesInHex);
    s.setValue("tabSize", disasm_.tabSize);
    s.endGroup();

    s.beginGroup("Directories");
    s.setValue("pluginDir", directories_.pluginDir);
    s.setValue("symbolSearchPath", directories_.symbolSearchPath);
    s.setValue("scriptDir", directories_.scriptDir);
    s.endGroup();

    s.beginGroup("RecentFiles");
    s.setValue("list", recentFiles_);
    s.endGroup();

    s.beginGroup("Signals");
    for (auto it = signals_.cbegin(); it != signals_.cend(); ++it) {
        QString prefix = QString("sig_%1_").arg(it.key());
        s.setValue(prefix + "stop", it->stopDebugger);
        s.setValue(prefix + "pass", it->passToApp);
        s.setValue(prefix + "log", it->logEvent);
    }
    s.endGroup();

    Q_EMIT configurationChanged();
}

SignalPolicy ConfigurationManager::signalPolicy(int sig) const {
    auto it = signals_.find(sig);
    if (it != signals_.end()) {
        return it.value();
    }
    return SignalPolicy{
        .signalNumber = sig,
        .signalName = QString("Signal %1").arg(sig),
        .stopDebugger = true,
        .passToApp = false,
        .logEvent = true
    };
}

void ConfigurationManager::setSignalPolicy(int sig, const SignalPolicy& policy) {
    signals_[sig] = policy;
}

QStringList ConfigurationManager::recentFiles() const {
    return recentFiles_;
}

void ConfigurationManager::addRecentFile(const QString& filePath) {
    if (filePath.isEmpty()) return;
    recentFiles_.removeAll(filePath);
    recentFiles_.prepend(filePath);
    while (recentFiles_.size() > 10) {
        recentFiles_.removeLast();
    }
    save();
}

void ConfigurationManager::clearRecentFiles() {
    recentFiles_.clear();
    save();
}

} // namespace edb_next
