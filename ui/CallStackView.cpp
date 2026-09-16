#include "CallStackView.hpp"
#include <QHeaderView>
#include <QFontDatabase>
#include <QMenu>
#include <QColor>

namespace edb_next {

CallStackView::CallStackView(QWidget* parent) : QTableWidget(parent) {
    setupUi();
}

void CallStackView::setupUi() {
    setColumnCount(5);
    setHorizontalHeaderLabels({"Frame", "Instruction Pointer", "Function / Symbol", "Module", "Frame Base (RBP)"});

    horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    verticalHeader()->setVisible(false);
    setShowGrid(false);

    QFont mono_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono_font.setPointSize(9);
    setFont(mono_font);

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QTableWidget::customContextMenuRequested, this, &CallStackView::handleContextMenu);
    connect(this, &QTableWidget::cellDoubleClicked, this, &CallStackView::handleCellDoubleClicked);
}

void CallStackView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
    refresh();
}

void CallStackView::refresh() {
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) {
        currentFrames_.clear();
        setRowCount(0);
        return;
    }

    currentFrames_ = session->callStack();
    setRowCount(static_cast<int>(currentFrames_.size()));

    for (int r = 0; r < static_cast<int>(currentFrames_.size()); ++r) {
        const auto& frame = currentFrames_[r];

        // 0: Frame #
        auto* item_idx = new QTableWidgetItem(QString("#%1").arg(frame.frameIndex));
        item_idx->setTextAlignment(Qt::AlignCenter);

        // 1: Instruction Pointer
        auto* item_ip = new QTableWidgetItem(QString::fromStdString(frame.ip.toHex()));
        item_ip->setForeground(QColor(100, 180, 240));

        // 2: Function / Symbol
        auto* item_sym = new QTableWidgetItem(QString::fromStdString(frame.functionSymbol));
        if (r == 0) {
            item_sym->setForeground(QColor(80, 220, 140)); // Top of stack highlighted in green
            QFont bold_font = font();
            bold_font.setBold(true);
            item_sym->setFont(bold_font);
        } else {
            item_sym->setForeground(QColor(220, 220, 220));
        }

        // 3: Module
        auto* item_mod = new QTableWidgetItem(QString::fromStdString(frame.moduleName));
        item_mod->setForeground(QColor(160, 160, 160));

        // 4: Frame Base (RBP)
        auto* item_bp = new QTableWidgetItem(QString::fromStdString(frame.frameBase.toHex()));
        item_bp->setForeground(QColor(120, 120, 120));

        setItem(r, 0, item_idx);
        setItem(r, 1, item_ip);
        setItem(r, 2, item_sym);
        setItem(r, 3, item_mod);
        setItem(r, 4, item_bp);
    }
}

void CallStackView::handleCellDoubleClicked(int row, int col) {
    Q_UNUSED(col);
    if (row >= 0 && row < static_cast<int>(currentFrames_.size())) {
        Q_EMIT jumpToAddressRequested(currentFrames_[row].ip);
    }
}

void CallStackView::handleContextMenu(const QPoint& pos) {
    int row = rowAt(pos.y());
    QMenu menu(this);

    if (row >= 0 && row < static_cast<int>(currentFrames_.size())) {
        Address ip = currentFrames_[row].ip;
        menu.addAction("Goto Instruction in Disassembly", this, [this, ip]() {
            Q_EMIT jumpToAddressRequested(ip);
        });
        menu.addSeparator();
    }

    menu.addAction("Refresh Call Stack", this, &CallStackView::refresh);
    menu.exec(mapToGlobal(pos));
}

} // namespace edb_next
