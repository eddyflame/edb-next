#pragma once

#include "Types.hpp"
#include <QObject>
#include <QString>
#include <QFont>
#include <QMap>
#include <memory>

namespace edb_next {

enum class CloseBehavior {
    Detach = 0,
    Terminate = 1,
    Prompt = 2
};

enum class InitialBreakpoint {
    EntryPoint = 0,
    MainSymbol = 1,
    None = 2
};

enum class DisassemblySyntax {
    Intel = 0,
    ATT = 1
};

struct GeneralConfig {
    CloseBehavior closeBehavior{CloseBehavior::Prompt};
    bool restoreWindowGeometry{true};
    QString sessionDir;
};

struct AppearanceConfig {
    QString themeName{"Dark [Built-in]"};
    QFont disasmFont{"Monospace", 10};
    QFont registerFont{"Monospace", 9};
    QFont stackFont{"Monospace", 9};
    QFont hexDumpFont{"Monospace", 9};
    bool showAddressColon{true};
    bool showJumpArrows{true};
    bool highlightChangedRegisters{true};
};

struct EngineConfig {
    bool disableASLR{true};
    bool disableLazyBinding{true};
    InitialBreakpoint initialBreakpoint{InitialBreakpoint::EntryPoint};
    bool breakOnLibraryLoad{false};
    bool ptyTerminalEnabled{false};
    QString ptyTerminalCommand{"xterm -e"};
    int defaultBpSlot{0}; // 0: Software (int3), 1: Hardware (DRx)
};

struct DisassemblyConfig {
    DisassemblySyntax syntax{DisassemblySyntax::Intel};
    bool uppercaseMnemonics{false};
    bool showSymbolicAddresses{true};
    bool simplifyRipRelative{true};
    bool showBytesInHex{true};
    int tabSize{8};
};

struct SignalPolicy {
    int signalNumber{0};
    QString signalName;
    bool stopDebugger{true};
    bool passToApp{false};
    bool logEvent{true};
};

struct DirectoriesConfig {
    QString pluginDir;
    QString symbolSearchPath{"/usr/lib/debug"};
    QString scriptDir;
};

class ConfigurationManager : public QObject {
    Q_OBJECT

public:
    static ConfigurationManager& instance();

    void load();
    void save();

    [[nodiscard]] const GeneralConfig& general() const noexcept { return general_; }
    [[nodiscard]] const AppearanceConfig& appearance() const noexcept { return appearance_; }
    [[nodiscard]] const EngineConfig& engine() const noexcept { return engine_; }
    [[nodiscard]] const DisassemblyConfig& disasm() const noexcept { return disasm_; }
    [[nodiscard]] const QMap<int, SignalPolicy>& signalPolicies() const noexcept { return signals_; }
    [[nodiscard]] const DirectoriesConfig& directories() const noexcept { return directories_; }

    GeneralConfig& general() noexcept { return general_; }
    AppearanceConfig& appearance() noexcept { return appearance_; }
    EngineConfig& engine() noexcept { return engine_; }
    DisassemblyConfig& disasm() noexcept { return disasm_; }
    QMap<int, SignalPolicy>& signalPolicies() noexcept { return signals_; }
    DirectoriesConfig& directories() noexcept { return directories_; }

    [[nodiscard]] SignalPolicy signalPolicy(int sig) const;
    void setSignalPolicy(int sig, const SignalPolicy& policy);

    // Recent files management
    [[nodiscard]] QStringList recentFiles() const;
    void addRecentFile(const QString& filePath);
    void clearRecentFiles();

Q_SIGNALS:
    void configurationChanged();

private:
    explicit ConfigurationManager(QObject* parent = nullptr);
    ~ConfigurationManager() override = default;

    void initDefaultSignals();

    GeneralConfig general_;
    AppearanceConfig appearance_;
    EngineConfig engine_;
    DisassemblyConfig disasm_;
    QMap<int, SignalPolicy> signals_;
    DirectoriesConfig directories_;
    QStringList recentFiles_;
};

} // namespace edb_next
