#include "SourceView.hpp"
#include "SourceFileManager.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QShortcut>
#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QFontDatabase>

namespace edb_next {

SourceView::SourceView(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void SourceView::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(2);

    // Toolbar
    auto* toolbar = new QWidget(this);
    toolbar->setStyleSheet("background-color: #1f232a; border-bottom: 1px solid #333; padding: 2px;");
    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(4, 2, 4, 2);
    tbLayout->setSpacing(6);

    auto* lbl = new QLabel("Source File:", toolbar);
    lbl->setStyleSheet("color: #aaa; font-weight: bold;");
    tbLayout->addWidget(lbl);

    fileCombo_ = new QComboBox(toolbar);
    fileCombo_->setMinimumWidth(260);
    fileCombo_->setStyleSheet(
        "QComboBox { background-color: #2b303c; color: #e0e0e0; border: 1px solid #444; border-radius: 3px; padding: 2px 6px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background-color: #2b303c; color: #e0e0e0; selection-background-color: #3e4451; }"
    );
    connect(fileCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SourceView::onFileComboIndexChanged);
    tbLayout->addWidget(fileCombo_, 1);

    followRipCheck_ = new QCheckBox("Follow RIP", toolbar);
    followRipCheck_->setChecked(true);
    followRipCheck_->setStyleSheet("color: #80cbc4;");
    tbLayout->addWidget(followRipCheck_);

    stepOverBtn_ = new QPushButton("Step Over (Line)", toolbar);
    stepOverBtn_->setStyleSheet("background-color: #2a3d4f; color: #82d2f5; border: 1px solid #3b5773; padding: 3px 8px; border-radius: 3px;");
    connect(stepOverBtn_, &QPushButton::clicked, this, &SourceView::stepOverLine);
    tbLayout->addWidget(stepOverBtn_);

    stepIntoBtn_ = new QPushButton("Step Into (Line)", toolbar);
    stepIntoBtn_->setStyleSheet("background-color: #2a3d4f; color: #82d2f5; border: 1px solid #3b5773; padding: 3px 8px; border-radius: 3px;");
    connect(stepIntoBtn_, &QPushButton::clicked, this, &SourceView::stepIntoLine);
    tbLayout->addWidget(stepIntoBtn_);

    gotoLineInput_ = new QLineEdit(toolbar);
    gotoLineInput_->setPlaceholderText("Line #");
    gotoLineInput_->setMaximumWidth(60);
    gotoLineInput_->setStyleSheet("background-color: #2b303c; color: #fff; border: 1px solid #444; border-radius: 3px; padding: 2px;");
    connect(gotoLineInput_, &QLineEdit::returnPressed, this, &SourceView::onGotoLineTriggered);
    tbLayout->addWidget(gotoLineInput_);

    auto* btnGo = new QPushButton("Go", toolbar);
    btnGo->setStyleSheet("background-color: #3b4252; color: #eceff4; padding: 2px 8px; border-radius: 3px;");
    connect(btnGo, &QPushButton::clicked, this, &SourceView::onGotoLineTriggered);
    tbLayout->addWidget(btnGo);

    mainLayout->addWidget(toolbar);

    // Code Table
    codeTable_ = new QTableWidget(this);
    codeTable_->setColumnCount(3);
    codeTable_->setHorizontalHeaderLabels({"Mark", "Line", "Code"});
    codeTable_->verticalHeader()->setVisible(false);
    codeTable_->verticalHeader()->setDefaultSectionSize(20);
    codeTable_->setShowGrid(false);
    codeTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    codeTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    codeTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    QFont monoFont("Monospace", 9);
    monoFont.setStyleHint(QFont::Monospace);
    codeTable_->setFont(monoFont);

    codeTable_->setColumnWidth(0, 36);
    codeTable_->setColumnWidth(1, 55);
    codeTable_->horizontalHeader()->setStretchLastSection(true);

    codeTable_->setStyleSheet(
        "QTableWidget { background-color: #181b20; color: #abb2bf; border: none; }"
        "QTableWidget::item:selected { background-color: #2c313c; color: #ffffff; }"
        "QHeaderView::section { background-color: #1f232a; color: #5c6370; border: none; padding: 2px; }"
    );

    codeTable_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(codeTable_, &QTableWidget::customContextMenuRequested, this, &SourceView::onCustomContextMenu);
    connect(codeTable_, &QTableWidget::cellDoubleClicked, this, &SourceView::onCellDoubleClicked);

    mainLayout->addWidget(codeTable_, 1);

    // Shortcuts
    auto* sc_f2 = new QShortcut(QKeySequence("F2"), this);
    connect(sc_f2, &QShortcut::activated, this, &SourceView::toggleBreakpointAtCurrentLine);

    auto* sc_f4 = new QShortcut(QKeySequence("F4"), this);
    connect(sc_f4, &QShortcut::activated, this, &SourceView::runToCursor);
}

void SourceView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
    refresh();

    if (auto sess = session_.lock()) {
        connect(sess.get(), &DebugSession::sourceLocationChanged, this, &SourceView::onSourceLocationChanged, Qt::UniqueConnection);
        connect(sess.get(), &DebugSession::breakpointsUpdated, this, &SourceView::updateBreakpointsAndRip, Qt::UniqueConnection);
        connect(sess.get(), &DebugSession::registersUpdated, this, &SourceView::onRegistersUpdated, Qt::UniqueConnection);
    }
}

void SourceView::onRegistersUpdated() {
    if (auto s = session_.lock()) {
        if (auto loc = s->currentSourceLocation()) {
            onSourceLocationChanged(*loc);
        } else {
            currentRipLine_ = -1;
            updateBreakpointsAndRip();
        }
    }
}

void SourceView::refresh() {
    auto session = session_.lock();
    fileCombo_->blockSignals(true);
    fileCombo_->clear();

    if (!session || !session->hasDebugInfo()) {
        fileCombo_->addItem("<No DWARF Debug Symbols Loaded>");
        fileCombo_->blockSignals(false);
        codeTable_->setRowCount(0);
        currentFilePath_.clear();
        currentRipLine_ = -1;
        return;
    }

    const auto& files = session->dwarfParser().allSourceFiles();
    for (const auto& f : files) {
        fileCombo_->addItem(QString::fromStdString(f));
    }
    fileCombo_->blockSignals(false);

    // Check if current RIP maps to a source line
    if (auto curLoc = session->currentSourceLocation()) {
        onSourceLocationChanged(*curLoc);
    } else if (!files.empty()) {
        loadFile(files.front());
    }
}

void SourceView::loadFile(const std::string& filePath) {
    currentFilePath_ = filePath;
    const auto& lines = SourceFileManager::instance().getFileLines(filePath);

    // Line 0 is empty dummy in 1-based indexing
    size_t count = (lines.empty()) ? 0 : (lines.size() - 1);
    codeTable_->setRowCount(static_cast<int>(count));

    for (size_t i = 1; i <= count; ++i) {
        int r = static_cast<int>(i - 1);

        auto* item_mark = new QTableWidgetItem("");
        item_mark->setTextAlignment(Qt::AlignCenter);

        auto* item_line = new QTableWidgetItem(QString::number(i));
        item_line->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        item_line->setForeground(QColor(92, 99, 112)); // dark grey line numbers

        auto* item_code = new QTableWidgetItem(QString::fromStdString(lines[i]));

        codeTable_->setItem(r, 0, item_mark);
        codeTable_->setItem(r, 1, item_line);
        codeTable_->setItem(r, 2, item_code);
    }

    updateBreakpointsAndRip();
}

void SourceView::updateBreakpointsAndRip() {
    auto session = session_.lock();
    if (!session || currentFilePath_.empty()) return;

    int rows = codeTable_->rowCount();
    for (int r = 0; r < rows; ++r) {
        int line = r + 1;
        bool hasBp = session->hasSourceBreakpoint(currentFilePath_, line);
        bool isRip = (line == currentRipLine_);

        auto* item_mark = codeTable_->item(r, 0);
        auto* item_line = codeTable_->item(r, 1);
        auto* item_code = codeTable_->item(r, 2);
        if (!item_mark || !item_line || !item_code) continue;

        QString markText;
        if (hasBp && isRip) {
            markText = "●➔";
        } else if (hasBp) {
            markText = "●";
        } else if (isRip) {
            markText = "➔";
        }

        item_mark->setText(markText);

        if (hasBp && isRip) {
            item_mark->setForeground(QColor(255, 100, 150));
            QColor bp_rip_bg(140, 40, 70, 110);
            item_mark->setBackground(bp_rip_bg);
            item_line->setBackground(bp_rip_bg);
            item_code->setBackground(bp_rip_bg);
            QFont f = item_code->font();
            f.setBold(true);
            item_code->setFont(f);
        } else if (hasBp) {
            item_mark->setForeground(QColor(255, 80, 80));
            QColor bp_bg(120, 30, 30, 80);
            item_mark->setBackground(bp_bg);
            item_line->setBackground(bp_bg);
            item_code->setBackground(bp_bg);
            QFont f = item_code->font();
            f.setBold(false);
            item_code->setFont(f);
        } else if (isRip) {
            item_mark->setForeground(QColor(80, 220, 140));
            QColor rip_bg(30, 80, 140, 100);
            item_mark->setBackground(rip_bg);
            item_line->setBackground(rip_bg);
            item_code->setBackground(rip_bg);
            QFont f = item_code->font();
            f.setBold(true);
            item_code->setFont(f);
        } else {
            item_mark->setBackground(QBrush());
            item_line->setBackground(QBrush());
            item_code->setBackground(QBrush());
            QFont f = item_code->font();
            f.setBold(false);
            item_code->setFont(f);
        }
    }
}

void SourceView::scrollToLine(int line) {
    int r = line - 1;
    if (r >= 0 && r < codeTable_->rowCount()) {
        codeTable_->selectRow(r);
        codeTable_->scrollToItem(codeTable_->item(r, 1), QAbstractItemView::PositionAtCenter);
    }
}

void SourceView::onSourceLocationChanged(const edb_next::SourceLocation& loc) {
    if (!followRipCheck_->isChecked()) return;

    currentRipLine_ = loc.line;

    // Check if file is already loaded
    if (currentFilePath_ != loc.filePath) {
        int idx = fileCombo_->findText(QString::fromStdString(loc.filePath));
        if (idx >= 0) {
            fileCombo_->setCurrentIndex(idx);
        } else {
            // Try matching basename
            for (int i = 0; i < fileCombo_->count(); ++i) {
                if (fileCombo_->itemText(i).endsWith(QString::fromStdString(loc.fileName))) {
                    fileCombo_->setCurrentIndex(i);
                    break;
                }
            }
        }
        loadFile(loc.filePath);
    }

    updateBreakpointsAndRip();
    scrollToLine(loc.line);
}

void SourceView::onFileComboIndexChanged(int index) {
    if (index < 0) return;
    std::string path = fileCombo_->itemText(index).toStdString();
    if (!path.empty() && path != currentFilePath_) {
        loadFile(path);
    }
}

int SourceView::currentSelectedLine() const {
    int row = codeTable_->currentRow();
    return (row >= 0) ? (row + 1) : -1;
}

void SourceView::onCellDoubleClicked(int row, int col) {
    int line = row + 1;
    if (auto session = session_.lock()) {
        if (session->toggleSourceBreakpoint(currentFilePath_, line)) {
            updateBreakpointsAndRip();
            if (auto addr = session->resolveSourceLine(currentFilePath_, line)) {
                Q_EMIT breakpointToggled(*addr);
            }
        }
    }
}

void SourceView::toggleBreakpointAtCurrentLine() {
    int line = currentSelectedLine();
    if (line <= 0) return;
    if (auto session = session_.lock()) {
        if (session->toggleSourceBreakpoint(currentFilePath_, line)) {
            updateBreakpointsAndRip();
            if (auto addr = session->resolveSourceLine(currentFilePath_, line)) {
                Q_EMIT breakpointToggled(*addr);
            }
        }
    }
}

void SourceView::runToCursor() {
    int line = currentSelectedLine();
    if (line <= 0) return;
    if (auto session = session_.lock()) {
        if (auto addr = session->resolveSourceLine(currentFilePath_, line)) {
            session->runTo(*addr);
        }
    }
}

void SourceView::stepOverLine() {
    if (auto session = session_.lock()) {
        session->stepSourceOver();
    }
}

void SourceView::stepIntoLine() {
    if (auto session = session_.lock()) {
        session->stepSourceInto();
    }
}

void SourceView::onGotoLineTriggered() {
    bool ok = false;
    int line = gotoLineInput_->text().trimmed().toInt(&ok);
    if (ok && line > 0) {
        scrollToLine(line);
    }
}

void SourceView::onCustomContextMenu(const QPoint& pos) {
    int line = currentSelectedLine();
    if (line <= 0) return;

    auto session = session_.lock();
    if (!session) return;

    QMenu menu(this);
    bool hasBp = session->hasSourceBreakpoint(currentFilePath_, line);

    QAction* act_bp = menu.addAction(hasBp ? "Remove Breakpoint (F2)" : "Set Breakpoint (F2)");
    connect(act_bp, &QAction::triggered, this, &SourceView::toggleBreakpointAtCurrentLine);

    QAction* act_runto = menu.addAction("Run to Cursor (F4)");
    connect(act_runto, &QAction::triggered, this, &SourceView::runToCursor);

    auto addr = session->resolveSourceLine(currentFilePath_, line);
    if (addr) {
        QAction* act_disasm = menu.addAction("Jump to Disassembly (Alt+C)");
        connect(act_disasm, &QAction::triggered, this, [this, addr]() {
            Q_EMIT jumpToDisassemblyRequested(*addr);
        });
    }

    menu.addSeparator();

    int row = line - 1;
    auto* item_code = codeTable_->item(row, 2);
    if (item_code) {
        QAction* act_copy = menu.addAction("Copy Line Text");
        connect(act_copy, &QAction::triggered, this, [item_code]() {
            QApplication::clipboard()->setText(item_code->text());
        });
    }

    QAction* act_reload = menu.addAction("Reload Source File");
    connect(act_reload, &QAction::triggered, this, [this]() {
        SourceFileManager::instance().clear();
        loadFile(currentFilePath_);
    });

    menu.exec(codeTable_->mapToGlobal(pos));
}

} // namespace edb_next
