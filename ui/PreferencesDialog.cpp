#include "PreferencesDialog.hpp"
#include "ConfigurationManager.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QFontDialog>
#include <QHeaderView>
#include <QDialogButtonBox>

namespace edb_next {

PreferencesDialog::PreferencesDialog(QWidget* parent) : QDialog(parent) {
    setupUi();
    loadFromConfig();
}

void PreferencesDialog::setupUi() {
    setWindowTitle("Preferences & Options - edb-next");
    resize(680, 520);

    auto* root_layout = new QVBoxLayout(this);
    tabWidget_ = new QTabWidget(this);

    // ==========================================
    // 1. General Tab
    // ==========================================
    auto* tab_general = new QWidget(tabWidget_);
    auto* gen_layout = new QVBoxLayout(tab_general);

    auto* grp_close = new QGroupBox("Process Close Behavior", tab_general);
    auto* close_layout = new QVBoxLayout(grp_close);
    rdoClosePrompt_ = new QRadioButton("Prompt user for action (Detach, Terminate, Cancel)", grp_close);
    rdoCloseDetach_ = new QRadioButton("Always detach process on exit", grp_close);
    rdoCloseTerminate_ = new QRadioButton("Always terminate process on exit", grp_close);
    close_layout->addWidget(rdoClosePrompt_);
    close_layout->addWidget(rdoCloseDetach_);
    close_layout->addWidget(rdoCloseTerminate_);
    gen_layout->addWidget(grp_close);

    auto* grp_window = new QGroupBox("Window & Workspace", tab_general);
    auto* win_layout = new QVBoxLayout(grp_window);
    chkRestoreGeometry_ = new QCheckBox("Restore window position, size and layout on startup", grp_window);
    win_layout->addWidget(chkRestoreGeometry_);

    auto* sess_layout = new QHBoxLayout();
    sess_layout->addWidget(new QLabel("Default Session Directory:"));
    txtSessionDir_ = new QLineEdit(grp_window);
    auto* btn_sess_browse = new QPushButton("Browse...", grp_window);
    connect(btn_sess_browse, &QPushButton::clicked, this, &PreferencesDialog::onBrowseSessionDir);
    sess_layout->addWidget(txtSessionDir_);
    sess_layout->addWidget(btn_sess_browse);
    win_layout->addLayout(sess_layout);

    gen_layout->addWidget(grp_window);
    gen_layout->addStretch();
    tabWidget_->addTab(tab_general, "General");

    // ==========================================
    // 2. Appearance Tab
    // ==========================================
    auto* tab_appear = new QWidget(tabWidget_);
    auto* app_layout = new QVBoxLayout(tab_appear);

    auto* grp_theme = new QGroupBox("Theme & Color Palette", tab_appear);
    auto* theme_layout = new QHBoxLayout(grp_theme);
    theme_layout->addWidget(new QLabel("UI Theme:"));
    cmbTheme_ = new QComboBox(grp_theme);
    cmbTheme_->addItem("Dark [Built-in]");
    cmbTheme_->addItem("Light [Built-in]");
    cmbTheme_->addItem("System Default");
    theme_layout->addWidget(cmbTheme_);
    app_layout->addWidget(grp_theme);

    auto* grp_fonts = new QGroupBox("Display Typography (Fonts)", tab_appear);
    auto* fonts_layout = new QGridLayout(grp_fonts);

    fonts_layout->addWidget(new QLabel("Disassembly View:"), 0, 0);
    btnDisasmFont_ = new QPushButton(grp_fonts);
    connect(btnDisasmFont_, &QPushButton::clicked, this, &PreferencesDialog::onDisasmFontChoose);
    fonts_layout->addWidget(btnDisasmFont_, 0, 1);

    fonts_layout->addWidget(new QLabel("Register View:"), 1, 0);
    btnRegFont_ = new QPushButton(grp_fonts);
    connect(btnRegFont_, &QPushButton::clicked, this, &PreferencesDialog::onRegFontChoose);
    fonts_layout->addWidget(btnRegFont_, 1, 1);

    fonts_layout->addWidget(new QLabel("Call Stack View:"), 2, 0);
    btnStackFont_ = new QPushButton(grp_fonts);
    connect(btnStackFont_, &QPushButton::clicked, this, &PreferencesDialog::onStackFontChoose);
    fonts_layout->addWidget(btnStackFont_, 2, 1);

    fonts_layout->addWidget(new QLabel("Memory Hex Dump:"), 3, 0);
    btnHexFont_ = new QPushButton(grp_fonts);
    connect(btnHexFont_, &QPushButton::clicked, this, &PreferencesDialog::onHexFontChoose);
    fonts_layout->addWidget(btnHexFont_, 3, 1);

    app_layout->addWidget(grp_fonts);

    auto* grp_visual = new QGroupBox("Visual Enhancements", tab_appear);
    auto* vis_layout = new QVBoxLayout(grp_visual);
    chkAddressColon_ = new QCheckBox("Show colon separator in hex address (e.g. 00007fff:f7fe4540)", grp_visual);
    chkJumpArrows_ = new QCheckBox("Draw jump routing arrows in disassembly view", grp_visual);
    chkHighlightRegs_ = new QCheckBox("Highlight changed registers upon pause/step", grp_visual);
    vis_layout->addWidget(chkAddressColon_);
    vis_layout->addWidget(chkJumpArrows_);
    vis_layout->addWidget(chkHighlightRegs_);
    app_layout->addWidget(grp_visual);

    app_layout->addStretch();
    tabWidget_->addTab(tab_appear, "Appearance");

    // ==========================================
    // 3. Debug Engine Tab
    // ==========================================
    auto* tab_engine = new QWidget(tabWidget_);
    auto* eng_layout = new QVBoxLayout(tab_engine);

    auto* grp_security = new QGroupBox("Process Startup Flags", tab_engine);
    auto* sec_layout = new QVBoxLayout(grp_security);
    chkDisableASLR_ = new QCheckBox("Disable ASLR (Address Space Layout Randomization via ADDR_NO_RANDOMIZE)", grp_security);
    chkDisableLazyBinding_ = new QCheckBox("Disable Lazy Binding (Set LD_BIND_NOW=1 to resolve PLT at launch)", grp_security);
    sec_layout->addWidget(chkDisableASLR_);
    sec_layout->addWidget(chkDisableLazyBinding_);
    eng_layout->addWidget(grp_security);

    auto* grp_bp = new QGroupBox("Initial Breakpoint Position", tab_engine);
    auto* bp_layout = new QVBoxLayout(grp_bp);
    rdoBpEntry_ = new QRadioButton("Break at ELF Entry Point (_start / interpreter)", grp_bp);
    rdoBpMain_ = new QRadioButton("Break at Main Symbol (main function)", grp_bp);
    rdoBpNone_ = new QRadioButton("No initial breakpoint (run immediately)", grp_bp);
    bp_layout->addWidget(rdoBpEntry_);
    bp_layout->addWidget(rdoBpMain_);
    bp_layout->addWidget(rdoBpNone_);
    eng_layout->addWidget(grp_bp);

    auto* grp_probes = new QGroupBox("Probes & Breakpoints", tab_engine);
    auto* probe_layout = new QGridLayout(grp_probes);
    chkBreakOnLibLoad_ = new QCheckBox("Break on dynamic library load/unload (_dl_debug_state probe)", grp_probes);
    probe_layout->addWidget(chkBreakOnLibLoad_, 0, 0, 1, 2);

    probe_layout->addWidget(new QLabel("Default Breakpoint Mode:"), 1, 0);
    cmbDefaultBpType_ = new QComboBox(grp_probes);
    cmbDefaultBpType_->addItem("Software Breakpoint (0xCC / int3)");
    cmbDefaultBpType_->addItem("Hardware Execution Breakpoint (DR0~DR3)");
    probe_layout->addWidget(cmbDefaultBpType_, 1, 1);
    eng_layout->addWidget(grp_probes);

    auto* grp_tty = new QGroupBox("External Terminal / PTY Redirection", tab_engine);
    auto* tty_layout = new QVBoxLayout(grp_tty);
    chkPtyTerminal_ = new QCheckBox("Run debuggee in separate pseudo-terminal for I/O", grp_tty);
    auto* tty_cmd_layout = new QHBoxLayout();
    tty_cmd_layout->addWidget(new QLabel("Terminal Command:"));
    txtPtyCommand_ = new QLineEdit(grp_tty);
    tty_cmd_layout->addWidget(txtPtyCommand_);
    tty_layout->addWidget(chkPtyTerminal_);
    tty_layout->addLayout(tty_cmd_layout);
    eng_layout->addWidget(grp_tty);

    eng_layout->addStretch();
    tabWidget_->addTab(tab_engine, "Debug Engine");

    // ==========================================
    // 4. Disassembly Tab
    // ==========================================
    auto* tab_disasm = new QWidget(tabWidget_);
    auto* dis_layout = new QVBoxLayout(tab_disasm);

    auto* grp_syntax = new QGroupBox("Assembly Syntax", tab_disasm);
    auto* syn_layout = new QHBoxLayout(grp_syntax);
    rdoSyntaxIntel_ = new QRadioButton("Intel Syntax (mov eax, [rbp - 4])", grp_syntax);
    rdoSyntaxATT_ = new QRadioButton("AT&T Syntax (movl -4(%rbp), %eax)", grp_syntax);
    syn_layout->addWidget(rdoSyntaxIntel_);
    syn_layout->addWidget(rdoSyntaxATT_);
    dis_layout->addWidget(grp_syntax);

    auto* grp_fmt = new QGroupBox("Formatting & Annotations", tab_disasm);
    auto* fmt_layout = new QVBoxLayout(grp_fmt);
    chkUppercase_ = new QCheckBox("Uppercase instruction mnemonics (MOV, CALL, RET)", grp_fmt);
    chkSymbolicAddrs_ = new QCheckBox("Display symbolic function names instead of raw addresses", grp_fmt);
    chkSimplifyRip_ = new QCheckBox("Simplify RIP-relative addressing targets", grp_fmt);
    chkShowBytesHex_ = new QCheckBox("Show instruction opcode bytes in hex column", grp_fmt);

    auto* tab_size_layout = new QHBoxLayout();
    tab_size_layout->addWidget(new QLabel("Tab spacing between mnemonic and operands:"));
    spnTabSize_ = new QSpinBox(grp_fmt);
    spnTabSize_->setRange(2, 16);
    tab_size_layout->addWidget(spnTabSize_);
    tab_size_layout->addStretch();

    fmt_layout->addWidget(chkUppercase_);
    fmt_layout->addWidget(chkSymbolicAddrs_);
    fmt_layout->addWidget(chkSimplifyRip_);
    fmt_layout->addWidget(chkShowBytesHex_);
    fmt_layout->addLayout(tab_size_layout);
    dis_layout->addWidget(grp_fmt);

    dis_layout->addStretch();
    tabWidget_->addTab(tab_disasm, "Disassembly");

    // ==========================================
    // 5. Signals & Exceptions Tab
    // ==========================================
    auto* tab_signals = new QWidget(tabWidget_);
    auto* sig_layout = new QVBoxLayout(tab_signals);

    auto* sig_header_layout = new QHBoxLayout();
    sig_header_layout->addWidget(new QLabel("Configure actions when target process receives POSIX signals:"));
    sig_header_layout->addStretch();
    btnResetSignals_ = new QPushButton("Reset to Defaults", tab_signals);
    connect(btnResetSignals_, &QPushButton::clicked, this, &PreferencesDialog::onResetSignalsClicked);
    sig_header_layout->addWidget(btnResetSignals_);
    sig_layout->addLayout(sig_header_layout);

    tableSignals_ = new QTableWidget(tab_signals);
    tableSignals_->setColumnCount(4);
    tableSignals_->setHorizontalHeaderLabels({"Signal # & Name", "Stop Debugger", "Pass to App", "Log Event"});
    tableSignals_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    tableSignals_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tableSignals_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tableSignals_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    sig_layout->addWidget(tableSignals_);

    tabWidget_->addTab(tab_signals, "Signals & Exceptions");

    // ==========================================
    // 6. Directories Tab
    // ==========================================
    auto* tab_dirs = new QWidget(tabWidget_);
    auto* dirs_layout = new QVBoxLayout(tab_dirs);

    auto* grp_dirs = new QGroupBox("Paths & Storage", tab_dirs);
    auto* d_grid = new QGridLayout(grp_dirs);

    d_grid->addWidget(new QLabel("Plugin Search Directory:"), 0, 0);
    txtPluginDir_ = new QLineEdit(grp_dirs);
    auto* btn_plugin_b = new QPushButton("Browse...", grp_dirs);
    connect(btn_plugin_b, &QPushButton::clicked, this, &PreferencesDialog::onBrowsePluginDir);
    d_grid->addWidget(txtPluginDir_, 0, 1);
    d_grid->addWidget(btn_plugin_b, 0, 2);

    d_grid->addWidget(new QLabel("Debug Symbols Path (/usr/lib/debug):"), 1, 0);
    txtSymbolDir_ = new QLineEdit(grp_dirs);
    auto* btn_sym_b = new QPushButton("Browse...", grp_dirs);
    connect(btn_sym_b, &QPushButton::clicked, this, &PreferencesDialog::onBrowseSymbolDir);
    d_grid->addWidget(txtSymbolDir_, 1, 1);
    d_grid->addWidget(btn_sym_b, 1, 2);

    d_grid->addWidget(new QLabel("User Scripts Directory:"), 2, 0);
    txtScriptDir_ = new QLineEdit(grp_dirs);
    auto* btn_script_b = new QPushButton("Browse...", grp_dirs);
    connect(btn_script_b, &QPushButton::clicked, this, &PreferencesDialog::onBrowseScriptDir);
    d_grid->addWidget(txtScriptDir_, 2, 1);
    d_grid->addWidget(btn_script_b, 2, 2);

    dirs_layout->addWidget(grp_dirs);
    dirs_layout->addStretch();
    tabWidget_->addTab(tab_dirs, "Directories");

    // ==========================================
    // 7. Plugins Options Tab
    // ==========================================
    pluginTabs_ = new QTabWidget(tabWidget_);
    tabWidget_->addTab(pluginTabs_, "Plugins");

    root_layout->addWidget(tabWidget_);

    // Dialog Button Box
    auto* btn_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    connect(btn_box->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &PreferencesDialog::onOkClicked);
    connect(btn_box->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &QDialog::reject);
    connect(btn_box->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &PreferencesDialog::onApplyClicked);
    root_layout->addWidget(btn_box);
}

void PreferencesDialog::loadFromConfig() {
    auto& cfg = ConfigurationManager::instance();

    // General
    rdoClosePrompt_->setChecked(cfg.general().closeBehavior == CloseBehavior::Prompt);
    rdoCloseDetach_->setChecked(cfg.general().closeBehavior == CloseBehavior::Detach);
    rdoCloseTerminate_->setChecked(cfg.general().closeBehavior == CloseBehavior::Terminate);
    chkRestoreGeometry_->setChecked(cfg.general().restoreWindowGeometry);
    txtSessionDir_->setText(cfg.general().sessionDir);

    // Appearance
    int theme_idx = cmbTheme_->findText(cfg.appearance().themeName);
    if (theme_idx >= 0) cmbTheme_->setCurrentIndex(theme_idx);
    fontDisasm_ = cfg.appearance().disasmFont;
    fontReg_ = cfg.appearance().registerFont;
    fontStack_ = cfg.appearance().stackFont;
    fontHex_ = cfg.appearance().hexDumpFont;
    btnDisasmFont_->setText(QString("%1, %2pt").arg(fontDisasm_.family()).arg(fontDisasm_.pointSize()));
    btnRegFont_->setText(QString("%1, %2pt").arg(fontReg_.family()).arg(fontReg_.pointSize()));
    btnStackFont_->setText(QString("%1, %2pt").arg(fontStack_.family()).arg(fontStack_.pointSize()));
    btnHexFont_->setText(QString("%1, %2pt").arg(fontHex_.family()).arg(fontHex_.pointSize()));
    chkAddressColon_->setChecked(cfg.appearance().showAddressColon);
    chkJumpArrows_->setChecked(cfg.appearance().showJumpArrows);
    chkHighlightRegs_->setChecked(cfg.appearance().highlightChangedRegisters);

    // Engine
    chkDisableASLR_->setChecked(cfg.engine().disableASLR);
    chkDisableLazyBinding_->setChecked(cfg.engine().disableLazyBinding);
    rdoBpEntry_->setChecked(cfg.engine().initialBreakpoint == InitialBreakpoint::EntryPoint);
    rdoBpMain_->setChecked(cfg.engine().initialBreakpoint == InitialBreakpoint::MainSymbol);
    rdoBpNone_->setChecked(cfg.engine().initialBreakpoint == InitialBreakpoint::None);
    chkBreakOnLibLoad_->setChecked(cfg.engine().breakOnLibraryLoad);
    chkPtyTerminal_->setChecked(cfg.engine().ptyTerminalEnabled);
    txtPtyCommand_->setText(cfg.engine().ptyTerminalCommand);
    cmbDefaultBpType_->setCurrentIndex(cfg.engine().defaultBpSlot);

    // Disassembly
    rdoSyntaxIntel_->setChecked(cfg.disasm().syntax == DisassemblySyntax::Intel);
    rdoSyntaxATT_->setChecked(cfg.disasm().syntax == DisassemblySyntax::ATT);
    chkUppercase_->setChecked(cfg.disasm().uppercaseMnemonics);
    chkSymbolicAddrs_->setChecked(cfg.disasm().showSymbolicAddresses);
    chkSimplifyRip_->setChecked(cfg.disasm().simplifyRipRelative);
    chkShowBytesHex_->setChecked(cfg.disasm().showBytesInHex);
    spnTabSize_->setValue(cfg.disasm().tabSize);

    // Directories
    txtPluginDir_->setText(cfg.directories().pluginDir);
    txtSymbolDir_->setText(cfg.directories().symbolSearchPath);
    txtScriptDir_->setText(cfg.directories().scriptDir);

    // Signals Table
    tableSignals_->setRowCount(0);
    const auto& sig_map = cfg.signalPolicies();
    int row = 0;
    for (auto it = sig_map.cbegin(); it != sig_map.cend(); ++it, ++row) {
        tableSignals_->insertRow(row);
        auto* name_item = new QTableWidgetItem(it->signalName);
        name_item->setFlags(name_item->flags() & ~Qt::ItemIsEditable);
        name_item->setData(Qt::UserRole, it.key());
        tableSignals_->setItem(row, 0, name_item);

        auto* chk_stop = new QTableWidgetItem();
        chk_stop->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        chk_stop->setCheckState(it->stopDebugger ? Qt::Checked : Qt::Unchecked);
        tableSignals_->setItem(row, 1, chk_stop);

        auto* chk_pass = new QTableWidgetItem();
        chk_pass->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        chk_pass->setCheckState(it->passToApp ? Qt::Checked : Qt::Unchecked);
        tableSignals_->setItem(row, 2, chk_pass);

        auto* chk_log = new QTableWidgetItem();
        chk_log->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        chk_log->setCheckState(it->logEvent ? Qt::Checked : Qt::Unchecked);
        tableSignals_->setItem(row, 3, chk_log);
    }
}

void PreferencesDialog::saveToConfig() {
    auto& cfg = ConfigurationManager::instance();

    // General
    if (rdoClosePrompt_->isChecked()) cfg.general().closeBehavior = CloseBehavior::Prompt;
    else if (rdoCloseDetach_->isChecked()) cfg.general().closeBehavior = CloseBehavior::Detach;
    else if (rdoCloseTerminate_->isChecked()) cfg.general().closeBehavior = CloseBehavior::Terminate;
    cfg.general().restoreWindowGeometry = chkRestoreGeometry_->isChecked();
    cfg.general().sessionDir = txtSessionDir_->text();

    // Appearance
    cfg.appearance().themeName = cmbTheme_->currentText();
    cfg.appearance().disasmFont = fontDisasm_;
    cfg.appearance().registerFont = fontReg_;
    cfg.appearance().stackFont = fontStack_;
    cfg.appearance().hexDumpFont = fontHex_;
    cfg.appearance().showAddressColon = chkAddressColon_->isChecked();
    cfg.appearance().showJumpArrows = chkJumpArrows_->isChecked();
    cfg.appearance().highlightChangedRegisters = chkHighlightRegs_->isChecked();

    // Engine
    cfg.engine().disableASLR = chkDisableASLR_->isChecked();
    cfg.engine().disableLazyBinding = chkDisableLazyBinding_->isChecked();
    if (rdoBpEntry_->isChecked()) cfg.engine().initialBreakpoint = InitialBreakpoint::EntryPoint;
    else if (rdoBpMain_->isChecked()) cfg.engine().initialBreakpoint = InitialBreakpoint::MainSymbol;
    else if (rdoBpNone_->isChecked()) cfg.engine().initialBreakpoint = InitialBreakpoint::None;
    cfg.engine().breakOnLibraryLoad = chkBreakOnLibLoad_->isChecked();
    cfg.engine().ptyTerminalEnabled = chkPtyTerminal_->isChecked();
    cfg.engine().ptyTerminalCommand = txtPtyCommand_->text();
    cfg.engine().defaultBpSlot = cmbDefaultBpType_->currentIndex();

    // Disassembly
    cfg.disasm().syntax = rdoSyntaxIntel_->isChecked() ? DisassemblySyntax::Intel : DisassemblySyntax::ATT;
    cfg.disasm().uppercaseMnemonics = chkUppercase_->isChecked();
    cfg.disasm().showSymbolicAddresses = chkSymbolicAddrs_->isChecked();
    cfg.disasm().simplifyRipRelative = chkSimplifyRip_->isChecked();
    cfg.disasm().showBytesInHex = chkShowBytesHex_->isChecked();
    cfg.disasm().tabSize = spnTabSize_->value();

    // Directories
    cfg.directories().pluginDir = txtPluginDir_->text();
    cfg.directories().symbolSearchPath = txtSymbolDir_->text();
    cfg.directories().scriptDir = txtScriptDir_->text();

    // Signals
    for (int r = 0; r < tableSignals_->rowCount(); ++r) {
        auto* name_item = tableSignals_->item(r, 0);
        if (!name_item) continue;
        int sig = name_item->data(Qt::UserRole).toInt();

        SignalPolicy pol;
        pol.signalNumber = sig;
        pol.signalName = name_item->text();
        pol.stopDebugger = (tableSignals_->item(r, 1)->checkState() == Qt::Checked);
        pol.passToApp = (tableSignals_->item(r, 2)->checkState() == Qt::Checked);
        pol.logEvent = (tableSignals_->item(r, 3)->checkState() == Qt::Checked);
        cfg.setSignalPolicy(sig, pol);
    }

    cfg.save();
}

void PreferencesDialog::addPluginOptionsPage(QWidget* page, const QString& title) {
    if (page && pluginTabs_) {
        pluginTabs_->addTab(page, title);
    }
}

void PreferencesDialog::onApplyClicked() {
    saveToConfig();
}

void PreferencesDialog::onOkClicked() {
    saveToConfig();
    accept();
}

void PreferencesDialog::onResetSignalsClicked() {
    auto& cfg = ConfigurationManager::instance();
    cfg.general(); // Touch instance
    // Re-trigger defaults load for signals
    cfg.signalPolicies().clear();
    cfg.save();
    loadFromConfig();
}

void PreferencesDialog::onBrowseSessionDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Session Directory", txtSessionDir_->text());
    if (!dir.isEmpty()) txtSessionDir_->setText(dir);
}

void PreferencesDialog::onBrowsePluginDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Plugin Directory", txtPluginDir_->text());
    if (!dir.isEmpty()) txtPluginDir_->setText(dir);
}

void PreferencesDialog::onBrowseSymbolDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Symbol Directory", txtSymbolDir_->text());
    if (!dir.isEmpty()) txtSymbolDir_->setText(dir);
}

void PreferencesDialog::onBrowseScriptDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Script Directory", txtScriptDir_->text());
    if (!dir.isEmpty()) txtScriptDir_->setText(dir);
}

void PreferencesDialog::onDisasmFontChoose() {
    bool ok = false;
    QFont f = QFontDialog::getFont(&ok, fontDisasm_, this, "Select Disassembly Font");
    if (ok) {
        fontDisasm_ = f;
        btnDisasmFont_->setText(QString("%1, %2pt").arg(f.family()).arg(f.pointSize()));
    }
}

void PreferencesDialog::onRegFontChoose() {
    bool ok = false;
    QFont f = QFontDialog::getFont(&ok, fontReg_, this, "Select Register Font");
    if (ok) {
        fontReg_ = f;
        btnRegFont_->setText(QString("%1, %2pt").arg(f.family()).arg(f.pointSize()));
    }
}

void PreferencesDialog::onStackFontChoose() {
    bool ok = false;
    QFont f = QFontDialog::getFont(&ok, fontStack_, this, "Select Stack Font");
    if (ok) {
        fontStack_ = f;
        btnStackFont_->setText(QString("%1, %2pt").arg(f.family()).arg(f.pointSize()));
    }
}

void PreferencesDialog::onHexFontChoose() {
    bool ok = false;
    QFont f = QFontDialog::getFont(&ok, fontHex_, this, "Select Hex Dump Font");
    if (ok) {
        fontHex_ = f;
        btnHexFont_->setText(QString("%1, %2pt").arg(f.family()).arg(f.pointSize()));
    }
}

} // namespace edb_next
