#pragma once

#include "SessionManager.hpp"
#include "SessionTabWidget.hpp"
#include "CommandBarView.hpp"
#include "IUIPlugin.hpp"
#include "core/PluginManager.hpp"
#include "core/PatchManager.hpp"
#include <QMainWindow>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QLabel>
#include <QAction>
#include <QMenu>
#include <memory>
#include <vector>
#include <functional>

namespace edb_next {

class MainWindow : public QMainWindow, public IUIPluginContext {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // IPluginContext implementation
    [[nodiscard]] SessionManager& sessionManager() override { return sessionMgr_; }
    [[nodiscard]] std::shared_ptr<DebugSession> activeSession() override { return sessionMgr_.activeSession(); }
    void addDockWidget(QDockWidget* dock, Qt::DockWidgetArea area = Qt::BottomDockWidgetArea) override;
    void logMessage(const QString& msg) override;
    void registerDebugEventListener(std::function<void(const DebugEvent&)> callback) override;
    void registerSessionStateListener(std::function<void(SessionState)> callback) override;
    void registerCommand(const std::string& cmd,
                         std::function<void(const std::vector<std::string>&)> handler,
                         const std::string& helpText = "") override;

    [[nodiscard]] PluginManager& pluginManager() noexcept { return pluginMgr_; }
    [[nodiscard]] PatchManager& patchManager() noexcept { return patchMgr_; }

    void toggleMaximized();

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private Q_SLOTS:
    // File
    void onOpenTargetTriggered();
    void onAttachTriggered();
    void onDetachTriggered();
    void onRestartTriggered();
    void onNewSessionTriggered();
    void onTargetArgumentsTriggered();
    void onSaveDatabaseTriggered();
    void onLoadDatabaseTriggered();
    void onRecentFileTriggered(const QString& path);
    void updateRecentFilesMenu();

    // Debug
    void onResumeTriggered();
    void onResumePassSignalTriggered();
    void onStepIntoTriggered();
    void onStepIntoPassSignalTriggered();
    void onStepOverTriggered();
    void onStepOverPassSignalTriggered();
    void onStepOutTriggered();
    void onRunUntilReturnTriggered();
    void onPauseTriggered();
    void onTerminateTriggered();
    void onDumpCpuStateTriggered();

    // Options & Tools
    void onPreferencesTriggered();
    void onPatchManagerTriggered();
    void onPluginManagerTriggered();
    void onResetLayoutTriggered();
    void onScriptConsoleTriggered();

    // Help
    void onShortcutsCheatsheetTriggered();
    void onAboutTriggered();

    // Tabs & Sessions
    void onCloseTabRequested(int index);
    void onCurrentTabChanged(int index);
    void onSelectBottomTabTriggered(int index);

    void onSessionCreated(std::shared_ptr<DebugSession> session);
    void onSessionClosed(const std::string& id);
    void onActiveSessionChanged(std::shared_ptr<DebugSession> session);
    void onSessionEventOccurred(const DebugEvent& event);
    void onSessionStateChanged(SessionState state);

private:
    void setupUi();
    void setupActions();
    void setupMenusAndToolbars();
    void updateUiForActiveSession();
    SessionTabWidget* currentSessionTabWidget() const;

    SessionManager sessionMgr_;
    PluginManager pluginMgr_;
    PatchManager patchMgr_;

    std::vector<std::function<void(const DebugEvent&)>> debugEventListeners_;
    std::vector<std::function<void(SessionState)>> sessionStateListeners_;

    QTabWidget* tabWidget_{nullptr};
    QPlainTextEdit* logConsole_{nullptr};
    CommandBarView* cmdBar_{nullptr};

    QLabel* statusSessionLabel_{nullptr};
    QLabel* statusPidLabel_{nullptr};
    QLabel* statusStateLabel_{nullptr};

    // Menus
    QMenu* menuFile_{nullptr};
    QMenu* menuRecentFiles_{nullptr};
    QMenu* menuView_{nullptr};
    QMenu* menuDebug_{nullptr};
    QMenu* menuPlugins_{nullptr};
    QMenu* menuOptions_{nullptr};
    QMenu* menuHelp_{nullptr};

    // Actions
    QAction* actOpen_{nullptr};
    QAction* actAttach_{nullptr};
    QAction* actDetach_{nullptr};
    QAction* actTargetArgs_{nullptr};
    QAction* actRestart_{nullptr};
    QAction* actNewSession_{nullptr};
    QAction* actSaveDatabase_{nullptr};
    QAction* actLoadDatabase_{nullptr};

    QAction* actResume_{nullptr};
    QAction* actResumePassSig_{nullptr};
    QAction* actStepInto_{nullptr};
    QAction* actStepIntoPassSig_{nullptr};
    QAction* actStepOver_{nullptr};
    QAction* actStepOverPassSig_{nullptr};
    QAction* actStepOut_{nullptr};
    QAction* actRunUntilReturn_{nullptr};
    QAction* actOrigin_{nullptr};
    QAction* actPause_{nullptr};
    QAction* actTerminate_{nullptr};
    QAction* actDumpState_{nullptr};

    QAction* actPreferences_{nullptr};
    QAction* actPatchManager_{nullptr};
    QAction* actPluginManager_{nullptr};
    QAction* actResetLayout_{nullptr};

    QAction* actToggleStack_{nullptr};
    QAction* actToggleConsole_{nullptr};

    QAction* actShortcuts_{nullptr};
    QAction* actAbout_{nullptr};
    QAction* actAboutQt_{nullptr};

    QDockWidget* dockEventConsole_{nullptr};
};

} // namespace edb_next
