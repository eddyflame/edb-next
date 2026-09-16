#include "BreakpointManagerView.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QFontDatabase>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QColor>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QDialogButtonBox>
#include <QLabel>
#include <QDialog>

namespace edb_next {

BreakpointManagerView::BreakpointManagerView(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void BreakpointManagerView::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(4);

    // Top action bar
    auto* top_bar = new QHBoxLayout();
    top_bar->setContentsMargins(2, 2, 2, 2);

    auto* btn_add = new QPushButton("+ Add Breakpoint", this);
    connect(btn_add, &QPushButton::clicked, this, &BreakpointManagerView::onAddBreakpointClicked);

    auto* btn_toggle = new QPushButton("Toggle", this);
    connect(btn_toggle, &QPushButton::clicked, this, &BreakpointManagerView::onToggleBreakpointClicked);

    auto* btn_del = new QPushButton("Delete", this);
    connect(btn_del, &QPushButton::clicked, this, &BreakpointManagerView::onDeleteBreakpointClicked);

    auto* btn_cond = new QPushButton("Edit Condition / Log...", this);
    connect(btn_cond, &QPushButton::clicked, this, &BreakpointManagerView::onEditConditionClicked);

    auto* btn_script = new QPushButton("Edit Script Action...", this);
    connect(btn_script, &QPushButton::clicked, this, &BreakpointManagerView::onEditScriptActionClicked);

    top_bar->addWidget(btn_add);
    top_bar->addWidget(btn_toggle);
    top_bar->addWidget(btn_del);
    top_bar->addWidget(btn_cond);
    top_bar->addWidget(btn_script);
    top_bar->addStretch();
    layout->addLayout(top_bar);

    // Table
    table_ = new QTableWidget(this);
    table_->setColumnCount(8);
    table_->setHorizontalHeaderLabels({"State", "Address", "Symbol / Label", "Type", "Hits", "Condition", "Log Format", "Script Action"});

    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Interactive);
    table_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Interactive);
    table_->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Interactive);

    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->verticalHeader()->setVisible(false);
    table_->setShowGrid(false);

    QFont mono_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono_font.setPointSize(9);
    table_->setFont(mono_font);

    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QTableWidget::customContextMenuRequested, this, &BreakpointManagerView::handleContextMenu);
    connect(table_, &QTableWidget::cellDoubleClicked, this, &BreakpointManagerView::handleCellDoubleClicked);
    connect(table_, &QTableWidget::itemChanged, this, &BreakpointManagerView::handleItemChanged);

    layout->addWidget(table_);
}

void BreakpointManagerView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
    refresh();
}

void BreakpointManagerView::refresh() {
    auto session = session_.lock();
    if (!session) {
        currentBreakpoints_.clear();
        table_->setRowCount(0);
        return;
    }

    currentBreakpoints_ = session->breakpoints();

    isUpdatingTable_ = true;
    table_->setRowCount(static_cast<int>(currentBreakpoints_.size()));

    for (int r = 0; r < static_cast<int>(currentBreakpoints_.size()); ++r) {
        const auto& bp = currentBreakpoints_[r];

        // 0: State (Checkbox)
        auto* item_state = new QTableWidgetItem(bp.enabled ? "Enabled" : "Disabled");
        item_state->setCheckState(bp.enabled ? Qt::Checked : Qt::Unchecked);
        if (bp.enabled) {
            item_state->setForeground(QColor(80, 200, 120));
        } else {
            item_state->setForeground(QColor(140, 140, 140));
        }

        // 1: Address
        auto* item_addr = new QTableWidgetItem(QString::fromStdString(bp.address.toHex()));
        item_addr->setForeground(QColor(100, 180, 240));

        // 2: Symbol
        QString sym_str = QString::fromStdString(bp.symbol);
        if (sym_str.isEmpty()) {
            if (auto resolved = session->symbols().findNearestSymbol(bp.address)) {
                sym_str = QString::fromStdString(resolved->first.name);
            }
        }
        auto* item_sym = new QTableWidgetItem(sym_str.isEmpty() ? "-" : sym_str);

        // 3: Type
        QString type_str;
        switch (bp.type) {
            case BreakpointType::Software:
                type_str = "Software (INT 3)";
                break;
            case BreakpointType::HardwareExecute:
                type_str = QString("HW Exec (DR%1)").arg(bp.hardwareSlot);
                break;
            case BreakpointType::HardwareWrite:
                type_str = QString("HW Write Watch (DR%1)").arg(bp.hardwareSlot);
                break;
            case BreakpointType::HardwareReadWrite:
                type_str = QString("HW Access Watch (DR%1)").arg(bp.hardwareSlot);
                break;
        }
        auto* item_type = new QTableWidgetItem(type_str);
        if (bp.type != BreakpointType::Software) {
            item_type->setForeground(QColor(230, 180, 80));
        }

        // 4: Hit Count
        auto* item_hits = new QTableWidgetItem(QString::number(bp.hitCount));
        item_hits->setTextAlignment(Qt::AlignCenter);

        // 5: Condition
        auto* item_cond = new QTableWidgetItem(QString::fromStdString(bp.condition));
        item_cond->setForeground(QColor(152, 195, 121));

        // 6: Log Format / Ignore
        QString logStr;
        if (bp.isLogOnly) {
            logStr = "[LOG] " + QString::fromStdString(bp.logFormat);
        } else if (bp.ignoreCount > 0) {
            logStr = QString("Ignore next %1 hits").arg(bp.ignoreCount);
        }
        auto* item_log = new QTableWidgetItem(logStr);
        item_log->setForeground(QColor(230, 200, 100));

        // 7: Script Action
        QString scriptSummary;
        if (!bp.scriptCode.empty()) {
            int lineCount = QString::fromStdString(bp.scriptCode).split('\n').size();
            QString langUpper = QString::fromStdString(bp.scriptLanguage).toUpper();
            scriptSummary = QString("[%1] %2 line(s)").arg(langUpper).arg(lineCount);
        }
        auto* item_script = new QTableWidgetItem(scriptSummary);
        if (!bp.scriptCode.empty()) {
            item_script->setForeground(QColor(198, 120, 221));
            item_script->setToolTip(QString::fromStdString(bp.scriptCode));
        }

        table_->setItem(r, 0, item_state);
        table_->setItem(r, 1, item_addr);
        table_->setItem(r, 2, item_sym);
        table_->setItem(r, 3, item_type);
        table_->setItem(r, 4, item_hits);
        table_->setItem(r, 5, item_cond);
        table_->setItem(r, 6, item_log);
        table_->setItem(r, 7, item_script);
    }

    // Populate Pending Breakpoints
    const auto& pending = session->pendingBreakpoints();
    table_->setRowCount(static_cast<int>(currentBreakpoints_.size() + pending.size()));

    for (size_t i = 0; i < pending.size(); ++i) {
        int r = static_cast<int>(currentBreakpoints_.size() + i);
        const auto& pb = pending[i];

        auto* item_state = new QTableWidgetItem(pb.enabled ? "Pending" : "Disabled");
        item_state->setCheckState(pb.enabled ? Qt::Checked : Qt::Unchecked);
        item_state->setForeground(QColor(230, 180, 80));

        auto* item_addr = new QTableWidgetItem("[Pending]");
        item_addr->setForeground(QColor(100, 200, 220));

        auto* item_sym = new QTableWidgetItem(QString::fromStdString(pb.symbol));
        item_sym->setForeground(QColor(152, 195, 121));

        auto* item_type = new QTableWidgetItem("Deferred (Pending)");
        item_type->setForeground(QColor(180, 180, 180));

        auto* item_hits = new QTableWidgetItem("0");
        item_hits->setTextAlignment(Qt::AlignCenter);

        auto* item_cond = new QTableWidgetItem(QString::fromStdString(pb.condition));
        auto* item_log = new QTableWidgetItem(pb.isLogOnly ? QString::fromStdString(pb.logFormat) : QString(""));

        QString scriptSummary;
        if (!pb.scriptCode.empty()) {
            scriptSummary = QString("[%1]").arg(QString::fromStdString(pb.scriptLanguage).toUpper());
        }
        auto* item_script = new QTableWidgetItem(scriptSummary);

        table_->setItem(r, 0, item_state);
        table_->setItem(r, 1, item_addr);
        table_->setItem(r, 2, item_sym);
        table_->setItem(r, 3, item_type);
        table_->setItem(r, 4, item_hits);
        table_->setItem(r, 5, item_cond);
        table_->setItem(r, 6, item_log);
        table_->setItem(r, 7, item_script);
    }

    isUpdatingTable_ = false;
}

void BreakpointManagerView::handleCellDoubleClicked(int row, int col) {
    if (row >= 0 && row < static_cast<int>(currentBreakpoints_.size())) {
        if (col == 5 || col == 6) {
            onEditConditionClicked();
        } else if (col == 7) {
            onEditScriptActionClicked();
        } else {
            Q_EMIT jumpToAddressRequested(currentBreakpoints_[row].address);
        }
    }
}

void BreakpointManagerView::handleItemChanged(QTableWidgetItem* item) {
    if (isUpdatingTable_ || !item || item->column() != 0) return;

    int row = item->row();
    if (row >= 0 && row < static_cast<int>(currentBreakpoints_.size())) {
        auto session = session_.lock();
        if (!session) return;

        Address addr = currentBreakpoints_[row].address;
        if (item->checkState() == Qt::Checked) {
            session->enableBreakpoint(addr);
        } else {
            session->disableBreakpoint(addr);
        }
        refresh();
    }
}

void BreakpointManagerView::handleContextMenu(const QPoint& pos) {
    int row = table_->rowAt(pos.y());
    QMenu menu(this);

    if (row >= 0 && row < static_cast<int>(currentBreakpoints_.size())) {
        const auto& bp = currentBreakpoints_[row];
        menu.addAction(bp.enabled ? "Disable Breakpoint" : "Enable Breakpoint", this, &BreakpointManagerView::onToggleBreakpointClicked);
        menu.addAction("Edit Condition & Log...", this, &BreakpointManagerView::onEditConditionClicked);
        menu.addAction("Edit Script Action...", this, &BreakpointManagerView::onEditScriptActionClicked);
        menu.addAction("Delete Breakpoint", this, &BreakpointManagerView::onDeleteBreakpointClicked);
        menu.addAction("Goto Address in Disassembly", this, [this, bp]() {
            Q_EMIT jumpToAddressRequested(bp.address);
        });
        menu.addSeparator();
    }

    menu.addAction("Add Software Breakpoint...", this, &BreakpointManagerView::onAddBreakpointClicked);
    menu.addAction("Refresh", this, &BreakpointManagerView::refresh);
    menu.exec(table_->mapToGlobal(pos));
}

void BreakpointManagerView::onEditConditionClicked() {
    int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(currentBreakpoints_.size())) return;
    const auto& bp = currentBreakpoints_[row];
    auto session = session_.lock();
    if (!session) return;

    bool ok = false;
    QString cond = QInputDialog::getText(
        this,
        "Breakpoint Condition",
        "Enter condition expression (e.g. rax == 0, rdi > 10, [rsp] != 0):",
        QLineEdit::Normal,
        QString::fromStdString(bp.condition),
        &ok
    );
    if (!ok) return;

    int ignoreCount = QInputDialog::getInt(
        this,
        "Ignore Count",
        "Number of hits to ignore before stopping (0 = stop on first hit):",
        static_cast<int>(bp.ignoreCount),
        0,
        1000000,
        1,
        &ok
    );
    if (!ok) return;

    QString logFmt = QInputDialog::getText(
        this,
        "Log Breakpoint (Optional)",
        "Enter log message format (if non-empty, will not pause process, e.g. 'secret hit {rdi}'):",
        QLineEdit::Normal,
        QString::fromStdString(bp.logFormat),
        &ok
    );
    if (!ok) return;

    session->setBreakpointCondition(bp.address, cond.trimmed().toStdString());
    session->setBreakpointIgnoreCount(bp.address, static_cast<uint32_t>(ignoreCount));
    session->setBreakpointLogOnly(bp.address, !logFmt.trimmed().isEmpty(), logFmt.trimmed().toStdString());
    refresh();
}

void BreakpointManagerView::onAddBreakpointClicked() {
    auto session = session_.lock();
    if (!session) return;

    bool ok = false;
    QString text = QInputDialog::getText(
        this,
        "Add Breakpoint",
        "Enter address (hex) or symbol name (e.g. main):",
        QLineEdit::Normal,
        "",
        &ok
    );

    if (!ok || text.trimmed().isEmpty()) return;
    std::string query = text.trimmed().toStdString();

    Address target_addr(0);
    if (auto sym = session->resolveSymbol(query)) {
        target_addr = *sym;
    } else {
        bool conv = false;
        uint64_t val = text.toULongLong(&conv, 16);
        if (conv) target_addr = Address(val);
    }

    if (!target_addr.isNull()) {
        session->addBreakpoint(target_addr, query);
        refresh();
        Q_EMIT jumpToAddressRequested(target_addr);
    } else {
        QMessageBox::warning(this, "Error", "Could not resolve address or symbol.");
    }
}

void BreakpointManagerView::onToggleBreakpointClicked() {
    int row = table_->currentRow();
    if (row >= 0 && row < static_cast<int>(currentBreakpoints_.size())) {
        auto session = session_.lock();
        if (!session) return;
        Address addr = currentBreakpoints_[row].address;
        if (currentBreakpoints_[row].enabled) {
            session->disableBreakpoint(addr);
        } else {
            session->enableBreakpoint(addr);
        }
        refresh();
    }
}

void BreakpointManagerView::onDeleteBreakpointClicked() {
    int row = table_->currentRow();
    if (row >= 0 && row < static_cast<int>(currentBreakpoints_.size())) {
        auto session = session_.lock();
        if (!session) return;
        session->removeBreakpoint(currentBreakpoints_[row].address);
        refresh();
    }
}

void BreakpointManagerView::onEditScriptActionClicked() {
    int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(currentBreakpoints_.size())) {
        QMessageBox::information(this, "Script Action", "Please select a breakpoint row first.");
        return;
    }

    auto session = session_.lock();
    if (!session) return;

    const auto& bp = currentBreakpoints_[row];

    QDialog dlg(this);
    dlg.setWindowTitle(QString("Breakpoint Script Action - %1").arg(QString::fromStdString(bp.address.toHex())));
    dlg.resize(550, 420);

    auto* layout = new QVBoxLayout(&dlg);

    // Language selector
    auto* langLayout = new QHBoxLayout();
    auto* langLabel = new QLabel("Script Language:", &dlg);
    auto* langCombo = new QComboBox(&dlg);
    langCombo->addItem("Python 3", "python");
    langCombo->addItem("Lua 5.4", "lua");
    if (bp.scriptLanguage == "lua") {
        langCombo->setCurrentIndex(1);
    } else {
        langCombo->setCurrentIndex(0);
    }
    langLayout->addWidget(langLabel);
    langLayout->addWidget(langCombo);
    langLayout->addStretch();
    layout->addLayout(langLayout);

    // Hint / Help
    auto* tipLabel = new QLabel(
        "Hint: Return false (e.g. 'return False' in Python, 'return false' in Lua) to silently step over without halting execution.\n"
        "Returning True, None, or having no return statement will pause the target normally.",
        &dlg);
    tipLabel->setStyleSheet("color: #abb2bf; font-size: 11px; padding: 6px; background: #21252b; border: 1px solid #3e4451; border-radius: 4px;");
    tipLabel->setWordWrap(true);
    layout->addWidget(tipLabel);

    // Script editor
    auto* editor = new QPlainTextEdit(&dlg);
    QFont mono_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono_font.setPointSize(10);
    editor->setFont(mono_font);
    editor->setPlainText(QString::fromStdString(bp.scriptCode));
    editor->setPlaceholderText("# Python example:\n# if edb.get_reg('rdi') == 7:\n#     edb.set_reg('rax', 42)\n#     return False\n# return True");
    layout->addWidget(editor);

    // Button box
    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    auto* btnClear = btnBox->addButton("Clear Script", QDialogButtonBox::ResetRole);
    connect(btnClear, &QPushButton::clicked, editor, &QPlainTextEdit::clear);
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(btnBox);

    if (dlg.exec() == QDialog::Accepted) {
        std::string code = editor->toPlainText().toStdString();
        std::string lang = langCombo->currentData().toString().toStdString();
        session->setBreakpointScript(bp.address, code, lang);
        refresh();
    }
}

} // namespace edb_next
