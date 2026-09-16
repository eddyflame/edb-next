#include "TraceView.hpp"
#include "DebugSession.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>

namespace edb_next {

TraceView::TraceView(TraceEngine& traceEngine, QWidget* parent)
    : QWidget(parent), traceEngine_(traceEngine)
{
    setupUi();
    connect(&traceEngine_, &TraceEngine::traceUpdated, this, &TraceView::onRefreshTable);
}

void TraceView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
    refresh();
}

void TraceView::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* top_bar1 = new QHBoxLayout();
    chkEnableHit_ = new QCheckBox("Record Hit Coverage", this);
    chkEnableHit_->setChecked(traceEngine_.isHitTraceEnabled());
    connect(chkEnableHit_, &QCheckBox::toggled, this, [this](bool val) {
        traceEngine_.setHitTraceEnabled(val);
    });

    chkEnableRun_ = new QCheckBox("Record Run Trace", this);
    chkEnableRun_->setChecked(traceEngine_.isRunTraceEnabled());
    connect(chkEnableRun_, &QCheckBox::toggled, this, [this](bool val) {
        traceEngine_.setRunTraceEnabled(val);
    });

    lblCoverageStats_ = new QLabel("Coverage: 0 unique instructions executed", this);
    lblCoverageStats_->setStyleSheet("font-weight: bold; color: #81c784;");

    btnClearRun_ = new QPushButton("Clear Run Trace", this);
    connect(btnClearRun_, &QPushButton::clicked, this, &TraceView::onClearRunTrace);

    btnClearHit_ = new QPushButton("Clear Coverage", this);
    connect(btnClearHit_, &QPushButton::clicked, this, &TraceView::onClearHitTrace);

    top_bar1->addWidget(chkEnableHit_);
    top_bar1->addWidget(chkEnableRun_);
    top_bar1->addSpacing(10);
    top_bar1->addWidget(lblCoverageStats_);
    top_bar1->addStretch();
    top_bar1->addWidget(btnClearRun_);
    top_bar1->addWidget(btnClearHit_);

    auto* top_bar2 = new QHBoxLayout();
    btnStepBack_ = new QPushButton("< Step Back", this);
    connect(btnStepBack_, &QPushButton::clicked, this, &TraceView::onStepBack);

    btnStepForward_ = new QPushButton("Step Forward >", this);
    connect(btnStepForward_, &QPushButton::clicked, this, &TraceView::onStepForward);

    btnTraceInto_ = new QPushButton("Auto Trace Into...", this);
    connect(btnTraceInto_, &QPushButton::clicked, this, [this]() { onAutoTracePrompt(false); });

    btnTraceOver_ = new QPushButton("Auto Trace Over...", this);
    connect(btnTraceOver_, &QPushButton::clicked, this, [this]() { onAutoTracePrompt(true); });

    top_bar2->addWidget(btnStepBack_);
    top_bar2->addWidget(btnStepForward_);
    top_bar2->addWidget(btnTraceInto_);
    top_bar2->addWidget(btnTraceOver_);
    top_bar2->addStretch();

    layout->addLayout(top_bar1);
    layout->addLayout(top_bar2);

    tableTrace_ = new QTableWidget(this);
    tableTrace_->setColumnCount(4);
    tableTrace_->setHorizontalHeaderLabels({"Step #", "Instruction Address", "Disassembly", "Registers Changed"});
    tableTrace_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tableTrace_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tableTrace_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tableTrace_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    tableTrace_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableTrace_->setFont(QFont("Monospace", 9));
    connect(tableTrace_, &QTableWidget::cellDoubleClicked, this, &TraceView::onRowDoubleClicked);

    layout->addWidget(tableTrace_);
}

void TraceView::refresh() {
    onRefreshTable();
}

void TraceView::onRefreshTable() {
    lblCoverageStats_->setText(QString("Coverage: %1 unique instructions executed")
        .arg(traceEngine_.hitCount()));

    const auto& frames = traceEngine_.traceFrames();
    tableTrace_->setRowCount(static_cast<int>(frames.size()));

    for (size_t i = 0; i < frames.size(); ++i) {
        const auto& f = frames[i];
        int row = static_cast<int>(i);

        auto* step_item = new QTableWidgetItem(QString::number(f.stepIndex));
        tableTrace_->setItem(row, 0, step_item);

        auto* addr_item = new QTableWidgetItem(QString::fromStdString(f.address.toHex()));
        tableTrace_->setItem(row, 1, addr_item);

        auto* insn_item = new QTableWidgetItem(QString::fromStdString(f.mnemonic + " " + f.operands));
        tableTrace_->setItem(row, 2, insn_item);

        QString changedStr;
        for (size_t c = 0; c < f.changedRegs.size(); ++c) {
            if (c > 0) changedStr += ", ";
            changedStr += QString::fromStdString(f.changedRegs[c]);
        }
        auto* delta_item = new QTableWidgetItem(changedStr);
        delta_item->setForeground(QColor(255, 183, 77));
        tableTrace_->setItem(row, 3, delta_item);
    }
}

void TraceView::onClearRunTrace() {
    traceEngine_.clearRunTrace();
}

void TraceView::onClearHitTrace() {
    traceEngine_.clearHitTrace();
}

void TraceView::onRowDoubleClicked(int row, int) {
    const auto& frames = traceEngine_.traceFrames();
    if (row >= 0 && row < static_cast<int>(frames.size())) {
        Q_EMIT jumpToDisassemblyRequested(frames[row].address);
    }
}

void TraceView::onAutoTracePrompt(bool stepOver) {
    if (!session_ || session_->state() != SessionState::Paused) {
        QMessageBox::warning(this, "Trace Error", "Target must be paused to run auto-trace.");
        return;
    }

    bool ok = false;
    int steps = QInputDialog::getInt(this, "Auto Trace", "Max Steps to execute:", 100, 1, 10000, 1, &ok);
    if (!ok) return;

    QString cond = QInputDialog::getText(this, "Stop Condition (Optional)",
        "Enter expression to stop when true (e.g. 'rax == 0' or 'rip == 0x...'):",
        QLineEdit::Normal, "", &ok);
    if (!ok) return;

    auto res = session_->autoTrace(stepOver, steps, cond.trimmed().toStdString());
    QMessageBox::information(this, "Trace Result", QString::fromStdString(res.message));
}

void TraceView::onStepBack() {
    if (traceEngine_.stepBack()) {
        int idx = static_cast<int>(traceEngine_.currentFrameIndex());
        tableTrace_->selectRow(idx);
        tableTrace_->scrollToItem(tableTrace_->item(idx, 0));
        if (auto f = traceEngine_.currentFrame()) {
            Q_EMIT jumpToDisassemblyRequested(f->address);
        }
    }
}

void TraceView::onStepForward() {
    if (traceEngine_.stepForward()) {
        int idx = static_cast<int>(traceEngine_.currentFrameIndex());
        tableTrace_->selectRow(idx);
        tableTrace_->scrollToItem(tableTrace_->item(idx, 0));
        if (auto f = traceEngine_.currentFrame()) {
            Q_EMIT jumpToDisassemblyRequested(f->address);
        }
    }
}

} // namespace edb_next
