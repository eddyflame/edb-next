#include "MainWindow.hpp"
#include "PreferencesDialog.hpp"
#include "LaunchArgumentsDialog.hpp"
#include "PatchManagerDialog.hpp"
#include "PluginManagerDialog.hpp"
#include "core/ConfigurationManager.hpp"
#include "core/DatabaseManager.hpp"
#include "core/LogManager.hpp"
#include "core/StateDumper.hpp"
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QDateTime>
#include <QApplication>
#include <QClipboard>
#include <QShortcut>

namespace edb_next {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      pluginMgr_(this, this),
      patchMgr_(this)
{
    setupUi();
    setupActions();
    setupMenusAndToolbars();

    connect(&sessionMgr_, &SessionManager::sessionCreated, this, &MainWindow::onSessionCreated);
    connect(&sessionMgr_, &SessionManager::sessionClosed, this, &MainWindow::onSessionClosed);
    connect(&sessionMgr_, &SessionManager::activeSessionChanged, this, &MainWindow::onActiveSessionChanged);

    // Load plugins from configured directory
    QString pdir = ConfigurationManager::instance().directories().pluginDir;
    pluginMgr_.loadPluginsFromDirectory(pdir);

    // Mount loaded plugins into Plugins menu
    for (const auto& p : pluginMgr_.loadedPlugins()) {
        if (p.instance) {
            if (QMenu* m = p.instance->createMenu(this)) {
                menuPlugins_->addMenu(m);
            }
        }
    }

    // Create default initial session
    sessionMgr_.createSession("Main Target");
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent* event) {
    auto behavior = ConfigurationManager::instance().general().closeBehavior;

    bool has_active_process = false;
    for (const auto& sess : sessionMgr_.allSessions()) {
        if (sess->pid() > 0) {
            has_active_process = true;
            break;
        }
    }

    if (has_active_process && behavior == CloseBehavior::Prompt) {
        auto res = QMessageBox::question(
            this,
            "Exit Debugger",
            "There are still active debuggee processes.\n\n"
            "Would you like to terminate all processes before exiting?\n"
            "(Click 'No' to detach and keep them running)",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::Yes
        );

        if (res == QMessageBox::Cancel) {
            event->ignore();
            return;
        }

        if (res == QMessageBox::No) {
            behavior = CloseBehavior::Detach;
        } else {
            behavior = CloseBehavior::Terminate;
        }
    }

    for (auto& sess : sessionMgr_.allSessions()) {
        sess->disconnect();
        if (behavior == CloseBehavior::Detach) {
            sess->detach();
        } else {
            sess->terminate();
        }
    }

    QMainWindow::closeEvent(event);
}

void MainWindow::setupUi() {
    setWindowTitle("edb-next: Next-Gen Linux x86-64 Debugger");
    setMinimumSize(640, 400);
    resize(1360, 860);

    setWindowFlags(Qt::Window | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

    // Central widget wrapper with TabWidget and CommandBarView at bottom
    auto* center_container = new QWidget(this);
    auto* center_layout = new QVBoxLayout(center_container);
    center_layout->setContentsMargins(0, 0, 0, 0);
    center_layout->setSpacing(2);

    tabWidget_ = new QTabWidget(center_container);
    tabWidget_->setTabsClosable(true);
    tabWidget_->setMovable(true);
    tabWidget_->setUsesScrollButtons(true);
    connect(tabWidget_, &QTabWidget::tabCloseRequested, this, &MainWindow::onCloseTabRequested);
    connect(tabWidget_, &QTabWidget::currentChanged, this, &MainWindow::onCurrentTabChanged);
    center_layout->addWidget(tabWidget_, 1);

    // Bottom x64dbg-style CLI Command Bar
    cmdBar_ = new CommandBarView(center_container);
    connect(cmdBar_, &CommandBarView::outputLogged, this, [this](const QString& msg, bool isErr) {
        logMessage((isErr ? "[CMD ERR] " : "[CMD] ") + msg);
    });
    connect(cmdBar_, &CommandBarView::jumpToDisassemblyRequested, this, [this](Address addr) {
        if (auto* tab = currentSessionTabWidget()) {
            tab->disasmView()->gotoAddress(addr);
        }
    });
    connect(cmdBar_, &CommandBarView::jumpToMemoryRequested, this, [this](Address addr) {
        if (auto* tab = currentSessionTabWidget()) {
            tab->memoryDumpView()->setBaseAddress(addr);
            tab->selectBottomTab(0);
        }
    });
    center_layout->addWidget(cmdBar_);

    setCentralWidget(center_container);

    // Bottom Log / Output Dock
    dockEventConsole_ = new QDockWidget("Debugger Event Console", this);
    dockEventConsole_->setObjectName("dockEventConsole");
    dockEventConsole_->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);

    logConsole_ = new QPlainTextEdit(dockEventConsole_);
    logConsole_->setReadOnly(true);
    QFont mono_font("Monospace", 9);
    logConsole_->setFont(mono_font);
    dockEventConsole_->setWidget(logConsole_);

    QMainWindow::addDockWidget(Qt::BottomDockWidgetArea, dockEventConsole_);
    // Hidden by default to maximize central 4-quadrant debugging area (Disasm/Regs/Dump/Stack)
    dockEventConsole_->hide();

    // Status bar labels
    statusSessionLabel_ = new QLabel("Session: None", this);
    statusPidLabel_ = new QLabel("PID: -", this);
    statusStateLabel_ = new QLabel("State: Stopped", this);

    statusBar()->addWidget(statusSessionLabel_, 1);
    statusBar()->addWidget(statusPidLabel_, 1);
    statusBar()->addWidget(statusStateLabel_, 1);

    logMessage("edb-next initialized with clean-slate microkernel architecture.");
}

void MainWindow::setupActions() {
    // File Actions
    actOpen_ = new QAction("&Open Executable...", this);
    actOpen_->setShortcuts({QKeySequence::Open, QKeySequence("F3")});
    connect(actOpen_, &QAction::triggered, this, &MainWindow::onOpenTargetTriggered);

    actAttach_ = new QAction("&Attach to PID...", this);
    actAttach_->setShortcut(QKeySequence("Shift+F3"));
    connect(actAttach_, &QAction::triggered, this, &MainWindow::onAttachTriggered);

    actDetach_ = new QAction("&Detach", this);
    connect(actDetach_, &QAction::triggered, this, &MainWindow::onDetachTriggered);

    actTargetArgs_ = new QAction("Target Arguments & &Working Directory...", this);
    connect(actTargetArgs_, &QAction::triggered, this, &MainWindow::onTargetArgumentsTriggered);

    actRestart_ = new QAction("↺ &Restart Target", this);
    actRestart_->setShortcut(QKeySequence("Ctrl+F2"));
    connect(actRestart_, &QAction::triggered, this, &MainWindow::onRestartTriggered);

    actNewSession_ = new QAction("&New Session Tab", this);
    actNewSession_->setShortcut(QKeySequence("Ctrl+T"));
    connect(actNewSession_, &QAction::triggered, this, &MainWindow::onNewSessionTriggered);

    actSaveDatabase_ = new QAction("💾 &Save Project Database...", this);
    actSaveDatabase_->setShortcut(QKeySequence("Ctrl+S"));
    connect(actSaveDatabase_, &QAction::triggered, this, &MainWindow::onSaveDatabaseTriggered);

    actLoadDatabase_ = new QAction("📂 &Load Project Database...", this);
    actLoadDatabase_->setShortcut(QKeySequence("Ctrl+Shift+S"));
    connect(actLoadDatabase_, &QAction::triggered, this, &MainWindow::onLoadDatabaseTriggered);

    // Debug Actions
    actResume_ = new QAction("▶ &Run / Continue", this);
    actResume_->setShortcut(QKeySequence("F9"));
    connect(actResume_, &QAction::triggered, this, &MainWindow::onResumeTriggered);

    actResumePassSig_ = new QAction("Run (&Pass Signal to Application)", this);
    actResumePassSig_->setShortcut(QKeySequence("Shift+F9"));
    connect(actResumePassSig_, &QAction::triggered, this, &MainWindow::onResumePassSignalTriggered);

    actPause_ = new QAction("⏸ &Pause", this);
    actPause_->setShortcut(QKeySequence("F12"));
    connect(actPause_, &QAction::triggered, this, &MainWindow::onPauseTriggered);

    actStepInto_ = new QAction("↷ &Step Into", this);
    actStepInto_->setShortcut(QKeySequence("F7"));
    connect(actStepInto_, &QAction::triggered, this, &MainWindow::onStepIntoTriggered);

    actStepIntoPassSig_ = new QAction("Step Into (Pass &Signal to Application)", this);
    actStepIntoPassSig_->setShortcut(QKeySequence("Shift+F7"));
    connect(actStepIntoPassSig_, &QAction::triggered, this, &MainWindow::onStepIntoPassSignalTriggered);

    actStepOver_ = new QAction("⤼ Step &Over", this);
    actStepOver_->setShortcut(QKeySequence("F8"));
    connect(actStepOver_, &QAction::triggered, this, &MainWindow::onStepOverTriggered);

    actStepOverPassSig_ = new QAction("Step Over (Pass Signal to &Application)", this);
    actStepOverPassSig_->setShortcut(QKeySequence("Shift+F8"));
    connect(actStepOverPassSig_, &QAction::triggered, this, &MainWindow::onStepOverPassSignalTriggered);

    actStepOut_ = new QAction("⤸ Step O&ut", this);
    actStepOut_->setShortcut(QKeySequence("Shift+F11"));
    connect(actStepOut_, &QAction::triggered, this, &MainWindow::onStepOutTriggered);

    actRunUntilReturn_ = new QAction("Run &Until Return", this);
    actRunUntilReturn_->setShortcut(QKeySequence("Ctrl+F9"));
    connect(actRunUntilReturn_, &QAction::triggered, this, &MainWindow::onRunUntilReturnTriggered);

    actTerminate_ = new QAction("⏹ &Terminate", this);
    actTerminate_->setShortcut(QKeySequence("Shift+F5"));
    connect(actTerminate_, &QAction::triggered, this, &MainWindow::onTerminateTriggered);

    actDumpState_ = new QAction("📋 &Dump CPU State (DumpState)", this);
    actDumpState_->setShortcut(QKeySequence("Ctrl+D"));
    connect(actDumpState_, &QAction::triggered, this, &MainWindow::onDumpCpuStateTriggered);

    // Options & Tools
    actPreferences_ = new QAction("&Preferences...", this);
    actPreferences_->setShortcut(QKeySequence("Ctrl+,"));
    connect(actPreferences_, &QAction::triggered, this, &MainWindow::onPreferencesTriggered);

    actPatchManager_ = new QAction("💾 &Patch Manager...", this);
    actPatchManager_->setShortcut(QKeySequence("Ctrl+P"));
    connect(actPatchManager_, &QAction::triggered, this, &MainWindow::onPatchManagerTriggered);

    actPluginManager_ = new QAction("🔌 &Plugin Manager...", this);
    connect(actPluginManager_, &QAction::triggered, this, &MainWindow::onPluginManagerTriggered);

    actResetLayout_ = new QAction("Reset &UI Layout", this);
    connect(actResetLayout_, &QAction::triggered, this, &MainWindow::onResetLayoutTriggered);

    actToggleStack_ = new QAction("Toggle &Stack View", this);
    actToggleStack_->setShortcut(QKeySequence("Shift+S"));
    connect(actToggleStack_, &QAction::triggered, this, [this]{
        if (auto* tab = currentSessionTabWidget()) {
            tab->toggleStackView();
        }
    });

    actToggleConsole_ = new QAction("Toggle Event Console &Dock", this);
    actToggleConsole_->setShortcut(QKeySequence("Alt+0"));
    connect(actToggleConsole_, &QAction::triggered, this, [this]{
        if (dockEventConsole_) {
            dockEventConsole_->setVisible(!dockEventConsole_->isVisible());
        }
    });

    // Help Actions
    actShortcuts_ = new QAction("&Shortcuts Cheatsheet", this);
    connect(actShortcuts_, &QAction::triggered, this, &MainWindow::onShortcutsCheatsheetTriggered);

    actAbout_ = new QAction("&About edb-next", this);
    connect(actAbout_, &QAction::triggered, this, &MainWindow::onAboutTriggered);

    actAboutQt_ = new QAction("About &Qt", this);
    connect(actAboutQt_, &QAction::triggered, qApp, &QApplication::aboutQt);
}

void MainWindow::setupMenusAndToolbars() {
    // 1. File Menu
    menuFile_ = menuBar()->addMenu("&File");
    menuFile_->addAction(actOpen_);
    menuFile_->addAction(actAttach_);
    menuFile_->addAction(actDetach_);
    menuFile_->addSeparator();

    menuRecentFiles_ = menuFile_->addMenu("Recent &Files");
    updateRecentFilesMenu();

    menuFile_->addAction(actTargetArgs_);
    menuFile_->addAction(actRestart_);
    menuFile_->addAction(actNewSession_);
    menuFile_->addSeparator();
    menuFile_->addAction(actSaveDatabase_);
    menuFile_->addAction(actLoadDatabase_);
    menuFile_->addSeparator();
    auto* act_quit = menuFile_->addAction("E&xit");
    act_quit->setShortcut(QKeySequence("Alt+X"));
    connect(act_quit, &QAction::triggered, this, &QWidget::close);

    // 2. View Menu
    menuView_ = menuBar()->addMenu("&View");
    auto* act_tab_dump = menuView_->addAction("Memory &Dump");
    act_tab_dump->setShortcut(QKeySequence("Ctrl+1"));
    connect(act_tab_dump, &QAction::triggered, this, [this]{ onSelectBottomTabTriggered(0); });

    auto* act_tab_callstack = menuView_->addAction("&Call Stack Frames");
    act_tab_callstack->setShortcut(QKeySequence("Ctrl+2"));
    connect(act_tab_callstack, &QAction::triggered, this, [this]{ onSelectBottomTabTriggered(1); });

    auto* act_tab_bp = menuView_->addAction("&Breakpoints");
    act_tab_bp->setShortcut(QKeySequence("Ctrl+3"));
    connect(act_tab_bp, &QAction::triggered, this, [this]{ onSelectBottomTabTriggered(2); });

    auto* act_tab_maps = menuView_->addAction("Memory &Regions (/proc/maps)");
    act_tab_maps->setShortcut(QKeySequence("Ctrl+4"));
    connect(act_tab_maps, &QAction::triggered, this, [this]{ onSelectBottomTabTriggered(3); });

    auto* act_tab_str = menuView_->addAction("String &References");
    act_tab_str->setShortcut(QKeySequence("Ctrl+5"));
    connect(act_tab_str, &QAction::triggered, this, [this]{ onSelectBottomTabTriggered(4); });

    auto* act_tab_sym = menuView_->addAction("Symbol &Viewer");
    act_tab_sym->setShortcut(QKeySequence("Ctrl+6"));
    connect(act_tab_sym, &QAction::triggered, this, [this]{ onSelectBottomTabTriggered(5); });

    auto* act_tab_heap = menuView_->addAction("&Glibc Heap Analyzer");
    act_tab_heap->setShortcut(QKeySequence("Ctrl+7"));
    connect(act_tab_heap, &QAction::triggered, this, [this]{ onSelectBottomTabTriggered(6); });

    auto* act_tab_proc = menuView_->addAction("&Process Properties");
    act_tab_proc->setShortcut(QKeySequence("Ctrl+8"));
    connect(act_tab_proc, &QAction::triggered, this, [this]{ onSelectBottomTabTriggered(7); });

    auto* act_tab_threads = menuView_->addAction("&Threads");
    connect(act_tab_threads, &QAction::triggered, this, [this]{ onSelectBottomTabTriggered(8); });

    auto* act_tab_rop = menuView_->addAction("&ROP Tool");
    connect(act_tab_rop, &QAction::triggered, this, [this]{ onSelectBottomTabTriggered(9); });

    auto* act_tab_watches = menuView_->addAction("&Watch Expressions");
    act_tab_watches->setShortcut(QKeySequence("Ctrl+W"));
    connect(act_tab_watches, &QAction::triggered, this, [this]{ onSelectBottomTabTriggered(10); });

    auto* act_tab_trace = menuView_->addAction("&Trace & Coverage");
    connect(act_tab_trace, &QAction::triggered, this, [this]{ onSelectBottomTabTriggered(11); });

    auto* act_tab_cfg = menuView_->addAction("Control Flow &Graph (CFG)");
    act_tab_cfg->setShortcut(QKeySequence("G"));
    connect(act_tab_cfg, &QAction::triggered, this, [this]{
        onSelectBottomTabTriggered(12);
        if (auto* tab = currentSessionTabWidget()) {
            tab->cfgGraphView()->onRefreshCurrent();
        }
    });

    auto* act_tab_notes = menuView_->addAction("&Notes (Scratchpad)");
    act_tab_notes->setShortcut(QKeySequence("Ctrl+N"));
    connect(act_tab_notes, &QAction::triggered, this, [this]{ onSelectBottomTabTriggered(13); });

    auto* act_tab_log = menuView_->addAction("Debug &Log");
    act_tab_log->setShortcut(QKeySequence("Ctrl+L"));
    connect(act_tab_log, &QAction::triggered, this, [this]{ onSelectBottomTabTriggered(14); });

    auto* act_tab_bininfo = menuView_->addAction("&Binary Info (Headers & Sections)");
    connect(act_tab_bininfo, &QAction::triggered, this, [this]{ onSelectBottomTabTriggered(15); });

    auto* act_tab_intermod = menuView_->addAction("&Intermodular Calls");
    connect(act_tab_intermod, &QAction::triggered, this, [this]{ onSelectBottomTabTriggered(16); });

    auto* act_tab_opcodes = menuView_->addAction("&Opcode Search");
    connect(act_tab_opcodes, &QAction::triggered, this, [this]{ onSelectBottomTabTriggered(17); });

    auto* act_tab_script = menuView_->addAction("🐍 &Script Console (Python/Lua)");
    act_tab_script->setShortcut(QKeySequence("Alt+P"));
    connect(act_tab_script, &QAction::triggered, this, &MainWindow::onScriptConsoleTriggered);

    menuView_->addSeparator();
    auto* act_cpu = menuView_->addAction("Focus CPU / Disassembly");
    act_cpu->setShortcut(QKeySequence("Alt+C"));
    connect(act_cpu, &QAction::triggered, this, [this] {
        if (auto* tab = currentSessionTabWidget()) {
            tab->showDisassemblyView();
            tab->disasmView()->setFocus();
        }
    });

    auto* act_src = menuView_->addAction("Focus Source Code");
    act_src->setShortcut(QKeySequence("Alt+S"));
    connect(act_src, &QAction::triggered, this, [this] {
        if (auto* tab = currentSessionTabWidget()) {
            tab->showSourceView();
            tab->sourceView()->setFocus();
        }
    });

    // x64dbg-aligned global muscle memory shortcuts
    auto* sc_dump_alt = new QShortcut(QKeySequence("Alt+D"), this);
    connect(sc_dump_alt, &QShortcut::activated, this, [this]{ onSelectBottomTabTriggered(0); });

    auto* sc_call_alt = new QShortcut(QKeySequence("Alt+K"), this);
    connect(sc_call_alt, &QShortcut::activated, this, [this]{ onSelectBottomTabTriggered(1); });

    auto* sc_bp_alt = new QShortcut(QKeySequence("Alt+B"), this);
    connect(sc_bp_alt, &QShortcut::activated, this, [this]{ onSelectBottomTabTriggered(2); });

    auto* sc_maps_alt = new QShortcut(QKeySequence("Alt+M"), this);
    connect(sc_maps_alt, &QShortcut::activated, this, [this]{ onSelectBottomTabTriggered(3); });

    auto* sc_sym_alt = new QShortcut(QKeySequence("Alt+E"), this);
    connect(sc_sym_alt, &QShortcut::activated, this, [this]{ onSelectBottomTabTriggered(5); });

    auto* sc_log_alt = new QShortcut(QKeySequence("Alt+L"), this);
    connect(sc_log_alt, &QShortcut::activated, this, [this]{ onSelectBottomTabTriggered(14); });

    menuView_->addSeparator();
    menuView_->addAction(actToggleStack_);
    menuView_->addAction(actToggleConsole_);
    menuView_->addSeparator();
    menuView_->addAction(actResetLayout_);

    // 3. Debug Menu
    menuDebug_ = menuBar()->addMenu("&Debug");
    menuDebug_->addAction(actResume_);
    menuDebug_->addAction(actPause_);
    menuDebug_->addAction(actRestart_);
    menuDebug_->addSeparator();
    menuDebug_->addAction(actStepInto_);
    menuDebug_->addAction(actStepOver_);
    menuDebug_->addAction(actStepOut_);
    menuDebug_->addAction(actRunUntilReturn_);
    menuDebug_->addSeparator();
    menuDebug_->addAction(actResumePassSig_);
    menuDebug_->addAction(actStepIntoPassSig_);
    menuDebug_->addAction(actStepOverPassSig_);
    menuDebug_->addSeparator();
    menuDebug_->addAction(actTerminate_);
    menuDebug_->addSeparator();
    menuDebug_->addAction(actDumpState_);

    // 4. Plugins Menu
    menuPlugins_ = menuBar()->addMenu("&Plugins");
    menuPlugins_->addAction(actPluginManager_);
    menuPlugins_->addSeparator();

    // 5. Options Menu
    menuOptions_ = menuBar()->addMenu("&Options");
    menuOptions_->addAction(actPreferences_);
    menuOptions_->addAction(actPatchManager_);
    menuOptions_->addAction(actTargetArgs_);
    menuOptions_->addSeparator();
    menuOptions_->addAction(actResetLayout_);

    // 6. Help Menu
    menuHelp_ = menuBar()->addMenu("&Help");
    menuHelp_->addAction(actShortcuts_);
    menuHelp_->addSeparator();
    menuHelp_->addAction(actAboutQt_);
    menuHelp_->addAction(actAbout_);

    // ToolBar
    auto* toolbar = addToolBar("Main Debug Toolbar");
    toolbar->setMovable(false);
    toolbar->addAction(actOpen_);
    toolbar->addAction(actNewSession_);
    toolbar->addSeparator();
    toolbar->addAction(actResume_);
    toolbar->addAction(actPause_);
    toolbar->addAction(actStepInto_);
    toolbar->addAction(actStepOver_);
    toolbar->addAction(actStepOut_);
    toolbar->addAction(actRunUntilReturn_);
    toolbar->addAction(actRestart_);
    toolbar->addAction(actTerminate_);
    toolbar->addSeparator();
    toolbar->addAction(actToggleStack_);
    toolbar->addAction(actPatchManager_);
    toolbar->addAction(actPreferences_);
}

void MainWindow::updateRecentFilesMenu() {
    if (!menuRecentFiles_) return;
    menuRecentFiles_->clear();

    const auto list = ConfigurationManager::instance().recentFiles();
    if (list.isEmpty()) {
        auto* empty_act = menuRecentFiles_->addAction("No recent files");
        empty_act->setEnabled(false);
        return;
    }

    for (const auto& path : list) {
        auto* act = menuRecentFiles_->addAction(path);
        connect(act, &QAction::triggered, this, [this, path]{
            onRecentFileTriggered(path);
        });
    }

    menuRecentFiles_->addSeparator();
    auto* clear_act = menuRecentFiles_->addAction("Clear History");
    connect(clear_act, &QAction::triggered, this, []{
        ConfigurationManager::instance().clearRecentFiles();
    });
}

void MainWindow::onSaveDatabaseTriggered() {
    auto session = sessionMgr_.activeSession();
    if (!session || session->targetPath().empty()) {
        QMessageBox::warning(this, "Save Database", "No active target binary in session to save database for.");
        return;
    }

    QString default_path = QString::fromStdString(DatabaseManager::instance().defaultDatabasePath(session->targetPath()));
    QString path = QFileDialog::getSaveFileName(this, "Save Project Database", default_path, "EDB Database Files (*.edb_db);;JSON Files (*.json)");
    if (path.isEmpty()) return;

    QString notes;
    std::vector<std::string> watches;
    if (auto* tab = currentSessionTabWidget()) {
        notes = tab->notesView()->notesText();
        watches = tab->watchView()->watchExpressions();
    }

    bool ok = DatabaseManager::instance().exportSession(session, &patchMgr_, notes.toStdString(), watches, path.toStdString());
    if (ok) {
        logMessage("Project database saved successfully to: " + path);
        QMessageBox::information(this, "Save Database", "Project database saved successfully:\n" + path);
    } else {
        QMessageBox::critical(this, "Save Error", "Failed to save project database.");
    }
}

void MainWindow::onLoadDatabaseTriggered() {
    auto session = sessionMgr_.activeSession();
    if (!session) {
        QMessageBox::warning(this, "Load Database", "Please create or select an active session first.");
        return;
    }

    QString default_path = QString::fromStdString(DatabaseManager::instance().defaultDatabasePath(session->targetPath()));
    QString path = QFileDialog::getOpenFileName(this, "Load Project Database", default_path, "EDB Database Files (*.edb_db);;JSON Files (*.json)");
    if (path.isEmpty()) return;

    std::string notes;
    std::vector<std::string> watches;
    bool ok = DatabaseManager::instance().importSession(session, &patchMgr_, notes, watches, path.toStdString());
    if (ok) {
        if (auto* tab = currentSessionTabWidget()) {
            tab->notesView()->setNotesText(QString::fromStdString(notes));
            tab->watchView()->clearWatches();
            for (const auto& w : watches) {
                tab->watchView()->addWatchExpression(QString::fromStdString(w));
            }
            tab->refreshAll();
        }
        logMessage("Project database loaded successfully from: " + path);
        QMessageBox::information(this, "Load Database", "Project database restored successfully.");
    } else {
        QMessageBox::critical(this, "Load Error", "Failed to load project database. File may be corrupt or invalid.");
    }
}

void MainWindow::onRecentFileTriggered(const QString& path) {
    auto session = sessionMgr_.activeSession();
    if (!session) {
        session = sessionMgr_.createSession("Target");
    }

    logMessage(QString("Launching binary from recent: %1").arg(path));
    bool ok = session->launch(path.toStdString(), {});
    if (!ok) {
        QMessageBox::critical(this, "Launch Error", "Failed to launch target binary via ptrace.");
    } else {
        ConfigurationManager::instance().addRecentFile(path);
        updateRecentFilesMenu();

        std::string db_file = DatabaseManager::instance().defaultDatabasePath(path.toStdString());
        if (QFile::exists(QString::fromStdString(db_file))) {
            std::string notes;
            std::vector<std::string> watches;
            if (DatabaseManager::instance().importSession(session, &patchMgr_, notes, watches, db_file)) {
                if (auto* cur_tab = currentSessionTabWidget()) {
                    cur_tab->notesView()->setNotesText(QString::fromStdString(notes));
                    for (const auto& w : watches) {
                        cur_tab->watchView()->addWatchExpression(QString::fromStdString(w));
                    }
                    cur_tab->refreshAll();
                }
                logMessage("Auto-loaded project database from: " + QString::fromStdString(db_file));
            }
        }
    }
}

SessionTabWidget* MainWindow::currentSessionTabWidget() const {
    int idx = tabWidget_->currentIndex();
    if (idx >= 0) {
        return qobject_cast<SessionTabWidget*>(tabWidget_->widget(idx));
    }
    return nullptr;
}

void MainWindow::onSelectBottomTabTriggered(int index) {
    if (auto* tab = currentSessionTabWidget()) {
        tab->selectBottomTab(index);
    }
}

void MainWindow::onResetLayoutTriggered() {
    resize(1360, 860);
    logMessage("UI layout reset to default dimensions.");
}

void MainWindow::onScriptConsoleTriggered() {
    if (auto* tab = currentSessionTabWidget()) {
        if (tab->bottomTabs() && tab->scriptConsoleView()) {
            tab->bottomTabs()->setCurrentWidget(tab->scriptConsoleView());
            tab->scriptConsoleView()->setFocus();
        }
    }
}

void MainWindow::onPreferencesTriggered() {
    PreferencesDialog dlg(this);

    // Mount plugin options pages
    for (const auto& p : pluginMgr_.loadedPlugins()) {
        if (p.instance) {
            if (QWidget* page = p.instance->createOptionsPage(&dlg)) {
                dlg.addPluginOptionsPage(page, QString::fromStdString(p.metadata.name));
            }
        }
    }

    if (dlg.exec() == QDialog::Accepted) {
        logMessage("Preferences updated and applied.");
        if (auto* tab = currentSessionTabWidget()) {
            tab->refreshAll();
        }
    }
}

void MainWindow::onTargetArgumentsTriggered() {
    auto session = sessionMgr_.activeSession();
    QString curBin;
    if (session && session->pid() > 0) {
        char exe_buf[PATH_MAX] = {0};
        ssize_t len = ::readlink(("/proc/" + std::to_string(session->pid()) + "/exe").c_str(), exe_buf, sizeof(exe_buf) - 1);
        if (len > 0) curBin = QString::fromUtf8(exe_buf);
    }

    LaunchArgumentsDialog dlg(curBin, {}, QDir::currentPath(), this);
    if (dlg.exec() == QDialog::Accepted) {
        QString bin = dlg.binaryPath();
        if (!bin.isEmpty()) {
            if (!session) {
                session = sessionMgr_.createSession("Target");
            }
            if (!dlg.workingDirectory().isEmpty()) {
                QDir::setCurrent(dlg.workingDirectory());
            }
            logMessage(QString("Launching %1 with custom arguments...").arg(bin));
            bool ok = session->launch(bin.toStdString(), dlg.arguments());
            if (!ok) {
                QMessageBox::critical(this, "Launch Error", "Failed to launch target program.");
            } else {
                ConfigurationManager::instance().addRecentFile(bin);
                updateRecentFilesMenu();
            }
        }
    }
}

void MainWindow::onPatchManagerTriggered() {
    PatchManagerDialog dlg(patchMgr_, sessionMgr_.activeSession(), this);
    dlg.exec();
}

void MainWindow::onPluginManagerTriggered() {
    PluginManagerDialog dlg(pluginMgr_, this);
    dlg.exec();
}

void MainWindow::onShortcutsCheatsheetTriggered() {
    QMessageBox::information(
        this,
        "edb-next Shortcuts Cheatsheet",
        "=== Debugger Execution Control ===\n"
        "  F9         : Continue / Run\n"
        "  Shift+F9   : Run (Pass Signal to Application)\n"
        "  F12        : Pause / Break Execution\n"
        "  F7         : Step Into Single Instruction\n"
        "  Shift+F7   : Step Into (Pass Signal to Application)\n"
        "  F8         : Step Over (Calls / Jumps)\n"
        "  Shift+F8   : Step Over (Pass Signal to Application)\n"
        "  Shift+F11  : Step Out of Current Function\n"
        "  Ctrl+F9    : Run Until Return\n"
        "  F4         : Run to Cursor / Selection\n"
        "  Ctrl+F2    : Restart Debug Target\n"
        "  Shift+F5   : Terminate Debuggee\n\n"
        "=== Views, Tools & Navigation ===\n"
        "  F3 / Ctrl+O: Open Executable\n"
        "  Shift+F3   : Attach to Process PID\n"
        "  Ctrl+P     : Patch Manager (Disk ELF Patching)\n"
        "  Ctrl+,     : Preferences & Options\n"
        "  Ctrl+T     : New Debug Session Tab\n"
        "  G          : Control Flow Graph (CFG)\n"
        "  Ctrl+1..9  : Direct Bottom Workstation Tabs\n"
        "  Space      : Inline GNU Assembler with NOP padding\n"
        "  Ctrl+E     : Dynamic Hex Machine Code Hot Patch\n"
        "  ;          : Comment Instruction Line\n"
        "  Ctrl+B     : Bookmark Line\n"
        "  X          : Interactive Code Cross References (XRef)"
    );
}

void MainWindow::onAboutTriggered() {
    QMessageBox::about(
        this,
        "About edb-next",
        "<h3>edb-next: Next-Gen Linux x86-64 Debugger</h3>"
        "<p>Version 1.0.0 (Clean-Slate Architecture)</p>"
        "<p>edb-next is built to achieve and surpass the technical capabilities of "
        "<b>edb-debugger</b> and <b>x64dbg</b> on modern Linux platforms.</p>"
        "<p><b>Key Pillars:</b></p>"
        "<ul>"
        "  <li>Modern C++20 Microkernel Core with Concurrent Multi-Session Debugging</li>"
        "  <li>Interactive Basic Block Control Flow Graph (CFG) & Branch Prediction</li>"
        "  <li>Hit Trace Code Coverage & Run Trace Execution Frame Recording</li>"
        "  <li>x64dbg-Style Bottom Command Bar with Autocompletion</li>"
        "  <li>Centralized Patch Manager with Direct ELF File Disk Output</li>"
        "  <li>Modern Decoupled Plugin System with Extensible Context Gateway</li>"
        "</ul>"
    );
}

void MainWindow::onSessionCreated(std::shared_ptr<DebugSession> session) {
    auto* tab_widget = new SessionTabWidget(session, tabWidget_);
    int idx = tabWidget_->addTab(tab_widget, QString::fromStdString(session->name()));
    tabWidget_->setCurrentIndex(idx);

    connect(session.get(), &DebugSession::eventOccurred, this, &MainWindow::onSessionEventOccurred);
    connect(session.get(), &DebugSession::stateChanged, this, &MainWindow::onSessionStateChanged);
    connect(tab_widget, &SessionTabWidget::requestSaveDatabase, this, &MainWindow::onSaveDatabaseTriggered);
    connect(tab_widget->multiDumpWidget(), &MultiDumpWidget::patchCreated, this, [this](Address addr, const std::vector<uint8_t>& oldB, const std::vector<uint8_t>& newB, const QString& c) {
        patchMgr_.addPatch(addr, oldB, newB, c.toStdString());
    });

    cmdBar_->setSession(session);

    logMessage(QString("Session [%1] created.").arg(QString::fromStdString(session->name())));
}

void MainWindow::onSessionClosed(const std::string& id) {
    logMessage(QString("Session [%1] closed.").arg(QString::fromStdString(id)));
}

void MainWindow::onActiveSessionChanged(std::shared_ptr<DebugSession> session) {
    cmdBar_->setSession(session);
    updateUiForActiveSession();
}

void MainWindow::onCurrentTabChanged(int index) {
    if (index >= 0) {
        auto* tab = qobject_cast<SessionTabWidget*>(tabWidget_->widget(index));
        if (tab && tab->session()) {
            sessionMgr_.setActiveSession(tab->session()->id());
            cmdBar_->setSession(tab->session());
        }
    }
}

void MainWindow::onCloseTabRequested(int index) {
    if (index >= 0) {
        auto* tab = qobject_cast<SessionTabWidget*>(tabWidget_->widget(index));
        if (tab && tab->session()) {
            std::string id = tab->session()->id();
            tabWidget_->removeTab(index);
            tab->deleteLater();
            sessionMgr_.closeSession(id);
        }
    }
}

void MainWindow::onNewSessionTriggered() {
    sessionMgr_.createSession("");
}

void MainWindow::onOpenTargetTriggered() {
    auto session = sessionMgr_.activeSession();
    if (!session) {
        session = sessionMgr_.createSession("Target");
    }

    QString file_path = QFileDialog::getOpenFileName(
        this,
        "Select Executable to Debug",
        QString(),
        "All Files (*)"
    );

    if (file_path.isEmpty()) return;

    logMessage(QString("Launching binary: %1").arg(file_path));
    bool ok = session->launch(file_path.toStdString(), {});
    if (!ok) {
        QMessageBox::critical(this, "Launch Error", "Failed to launch target program via ptrace.");
    } else {
        ConfigurationManager::instance().addRecentFile(file_path);
        updateRecentFilesMenu();

        std::string db_file = DatabaseManager::instance().defaultDatabasePath(file_path.toStdString());
        if (QFile::exists(QString::fromStdString(db_file))) {
            std::string notes;
            std::vector<std::string> watches;
            if (DatabaseManager::instance().importSession(session, &patchMgr_, notes, watches, db_file)) {
                if (auto* cur_tab = currentSessionTabWidget()) {
                    cur_tab->notesView()->setNotesText(QString::fromStdString(notes));
                    for (const auto& w : watches) {
                        cur_tab->watchView()->addWatchExpression(QString::fromStdString(w));
                    }
                    cur_tab->refreshAll();
                }
                logMessage("Auto-loaded project database from: " + QString::fromStdString(db_file));
            }
        }
    }
}

void MainWindow::onAttachTriggered() {
    auto session = sessionMgr_.activeSession();
    if (!session) {
        session = sessionMgr_.createSession("Attach Target");
    }

    bool ok = false;
    int pid = QInputDialog::getInt(this, "Attach to Process", "Target PID:", 1, 1, 999999, 1, &ok);
    if (!ok) return;

    logMessage(QString("Attaching to PID %1...").arg(pid));
    bool attached = session->attach(pid);
    if (!attached) {
        QMessageBox::critical(this, "Attach Error", QString("Failed to attach to PID %1").arg(pid));
    }
}

void MainWindow::onDetachTriggered() {
    if (auto s = sessionMgr_.activeSession()) {
        logMessage(QString("Detaching from process PID %1...").arg(s->pid()));
        s->detach();
        updateUiForActiveSession();
    }
}

void MainWindow::onResumeTriggered() {
    if (auto s = sessionMgr_.activeSession()) {
        logMessage("Resuming execution (Continue)...");
        s->resume(false);
    }
}

void MainWindow::onResumePassSignalTriggered() {
    if (auto s = sessionMgr_.activeSession()) {
        logMessage(QString("Resuming execution (Passing Signal %1)...").arg(s->lastSignal()));
        s->resume(true);
    }
}

void MainWindow::onStepIntoTriggered() {
    if (auto s = sessionMgr_.activeSession()) {
        s->stepInto(false);
    }
}

void MainWindow::onStepIntoPassSignalTriggered() {
    if (auto s = sessionMgr_.activeSession()) {
        logMessage(QString("Step Into (Passing Signal %1)...").arg(s->lastSignal()));
        s->stepInto(true);
    }
}

void MainWindow::onStepOverTriggered() {
    if (auto s = sessionMgr_.activeSession()) {
        s->stepOver(false);
    }
}

void MainWindow::onStepOverPassSignalTriggered() {
    if (auto s = sessionMgr_.activeSession()) {
        logMessage(QString("Step Over (Passing Signal %1)...").arg(s->lastSignal()));
        s->stepOver(true);
    }
}

void MainWindow::onStepOutTriggered() {
    if (auto s = sessionMgr_.activeSession()) {
        s->stepOut();
    }
}

void MainWindow::onRunUntilReturnTriggered() {
    if (auto s = sessionMgr_.activeSession()) {
        s->runUntilReturn();
    }
}

void MainWindow::onRestartTriggered() {
    if (auto s = sessionMgr_.activeSession()) {
        logMessage("Restarting debug target...");
        s->restart();
    }
}

void MainWindow::onPauseTriggered() {
    if (auto s = sessionMgr_.activeSession()) {
        s->pause();
    }
}

void MainWindow::onTerminateTriggered() {
    if (auto s = sessionMgr_.activeSession()) {
        s->terminate();
    }
}

void MainWindow::onDumpCpuStateTriggered() {
    auto session = sessionMgr_.activeSession();
    if (!session || session->state() == SessionState::Stopped) {
        QMessageBox::warning(this, "Dump State", "Target process must be running or paused to dump CPU state.");
        return;
    }
    std::string dump = StateDumper::dumpState(*session);
    LogManager::instance().info("StateDump", dump);
    QApplication::clipboard()->setText(QString::fromStdString(dump));
    statusBar()->showMessage("CPU State dumped to log and copied to clipboard.", 4000);
}

void MainWindow::onSessionEventOccurred(const DebugEvent& event) {
    logMessage(QString::fromStdString(event.describe()));
    updateUiForActiveSession();

    // Broadcast to plugin event listeners
    for (auto& listener : debugEventListeners_) {
        try {
            listener(event);
        } catch (...) {}
    }
}

void MainWindow::onSessionStateChanged(SessionState state) {
    updateUiForActiveSession();

    for (auto& listener : sessionStateListeners_) {
        try {
            listener(state);
        } catch (...) {}
    }
}

void MainWindow::updateUiForActiveSession() {
    auto session = sessionMgr_.activeSession();
    if (!session) {
        statusSessionLabel_->setText("Session: None");
        statusPidLabel_->setText("PID: -");
        statusStateLabel_->setText("State: -");
        actResume_->setEnabled(false);
        actResumePassSig_->setEnabled(false);
        actStepInto_->setEnabled(false);
        actStepIntoPassSig_->setEnabled(false);
        actStepOver_->setEnabled(false);
        actStepOverPassSig_->setEnabled(false);
        actStepOut_->setEnabled(false);
        actRunUntilReturn_->setEnabled(false);
        actRestart_->setEnabled(false);
        actPause_->setEnabled(false);
        actTerminate_->setEnabled(false);
        actDetach_->setEnabled(false);
        return;
    }

    statusSessionLabel_->setText("Session: " + QString::fromStdString(session->name()));
    statusPidLabel_->setText(session->pid() > 0 ? QString("PID: %1").arg(session->pid()) : "PID: -");

    QString state_str;
    switch (session->state()) {
        case SessionState::Stopped: state_str = "Stopped"; break;
        case SessionState::Running: state_str = "Running"; break;
        case SessionState::Paused: state_str = "Paused"; break;
        case SessionState::Terminated: state_str = "Terminated"; break;
    }
    statusStateLabel_->setText("State: " + state_str);

    bool is_paused = (session->state() == SessionState::Paused);
    bool is_running = (session->state() == SessionState::Running);
    bool has_target = (session->pid() > 0 || session->state() != SessionState::Stopped);
    bool has_signal = (session->lastSignal() > 0);

    actResume_->setEnabled(is_paused);
    actResumePassSig_->setEnabled(is_paused && has_signal);
    actStepInto_->setEnabled(is_paused);
    actStepIntoPassSig_->setEnabled(is_paused && has_signal);
    actStepOver_->setEnabled(is_paused);
    actStepOverPassSig_->setEnabled(is_paused && has_signal);
    actStepOut_->setEnabled(is_paused);
    actRunUntilReturn_->setEnabled(is_paused);
    actRestart_->setEnabled(has_target);
    actPause_->setEnabled(is_running);
    actTerminate_->setEnabled(is_paused || is_running);
    actDetach_->setEnabled(has_target);
}

void MainWindow::logMessage(const QString& msg) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    logConsole_->appendPlainText(QString("[%1] %2").arg(timestamp, msg));
    LogManager::instance().info("System", msg.toStdString());
}

// IPluginContext implementation
void MainWindow::addDockWidget(QDockWidget* dock, Qt::DockWidgetArea area) {
    if (dock) {
        QMainWindow::addDockWidget(area, dock);
    }
}

void MainWindow::registerDebugEventListener(std::function<void(const DebugEvent&)> callback) {
    debugEventListeners_.push_back(std::move(callback));
}

void MainWindow::registerSessionStateListener(std::function<void(SessionState)> callback) {
    sessionStateListeners_.push_back(std::move(callback));
}

void MainWindow::registerCommand(const std::string& cmd,
                                 std::function<void(const std::vector<std::string>&)> handler,
                                 const std::string& helpText)
{
    if (cmdBar_) {
        cmdBar_->registerCommand(cmd, handler, helpText);
    }
}

} // namespace edb_next
