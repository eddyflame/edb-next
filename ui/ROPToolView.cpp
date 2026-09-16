#include "ROPToolView.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QClipboard>
#include <QApplication>

namespace edb_next {

ROPToolView::ROPToolView(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void ROPToolView::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Control bar
    auto* ctrlLayout = new QHBoxLayout();

    ctrlLayout->addWidget(new QLabel("Filter:", this));
    filterEdit_ = new QLineEdit(this);
    filterEdit_->setPlaceholderText("Filter by instruction, e.g. pop rdi, syscall...");
    filterEdit_->setClearButtonEnabled(true);
    ctrlLayout->addWidget(filterEdit_, 2);

    ctrlLayout->addWidget(new QLabel("Category:", this));
    categoryCombo_ = new QComboBox(this);
    categoryCombo_->addItems({"All Categories", "Stack (pop)", "Syscall", "Arithmetic", "Data Transfer", "Branch", "General"});
    ctrlLayout->addWidget(categoryCombo_, 1);

    ctrlLayout->addWidget(new QLabel("Max Insns:", this));
    spinMaxInsn_ = new QSpinBox(this);
    spinMaxInsn_->setRange(1, 6);
    spinMaxInsn_->setValue(4);
    ctrlLayout->addWidget(spinMaxInsn_);

    btnScan_ = new QPushButton("🔍 Scan ROP Gadgets", this);
    ctrlLayout->addWidget(btnScan_);

    statusLabel_ = new QLabel("Gadgets: 0", this);
    ctrlLayout->addWidget(statusLabel_);

    ctrlLayout->addStretch();
    mainLayout->addLayout(ctrlLayout);

    // Table
    table_ = new QTableWidget(this);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({"Address", "Disassembly", "Category", "Length (Bytes)", "Instruction Count"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);

    mainLayout->addWidget(table_);

    connect(btnScan_, &QPushButton::clicked, this, &ROPToolView::onScanClicked);
    connect(filterEdit_, &QLineEdit::textChanged, this, &ROPToolView::onFilterChanged);
    connect(categoryCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ROPToolView::onFilterChanged);
    connect(table_, &QTableWidget::cellDoubleClicked, this, &ROPToolView::onCellDoubleClicked);
    connect(table_, &QTableWidget::customContextMenuRequested, this, &ROPToolView::onContextMenu);
}

void ROPToolView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
}

void ROPToolView::onScanClicked() {
    scanGadgets();
}

void ROPToolView::scanGadgets() {
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) {
        statusLabel_->setText("Gadgets: 0 (No target active)");
        allGadgets_.clear();
        filteredGadgets_.clear();
        table_->setRowCount(0);
        return;
    }

    size_t maxLen = spinMaxInsn_->value();
    allGadgets_ = session->scanROP(maxLen, 1000, "");
    onFilterChanged();
}

void ROPToolView::onFilterChanged() {
    QString filter = filterEdit_->text().trimmed().toLower();
    QString cat = categoryCombo_->currentText();

    filteredGadgets_.clear();
    filteredGadgets_.reserve(allGadgets_.size());

    for (const auto& g : allGadgets_) {
        if (cat != "All Categories" && QString::fromStdString(g.category) != cat) {
            continue;
        }

        if (!filter.isEmpty()) {
            QString disasm = QString::fromStdString(g.disassembly).toLower();
            if (!disasm.contains(filter)) {
                continue;
            }
        }

        filteredGadgets_.push_back(g);
    }

    displayGadgets(filteredGadgets_);
}

void ROPToolView::displayGadgets(const std::vector<ROPGadget>& list) {
    table_->setRowCount(static_cast<int>(list.size()));
    statusLabel_->setText(QString("Gadgets: %1 / %2").arg(list.size()).arg(allGadgets_.size()));

    for (int r = 0; r < static_cast<int>(list.size()); ++r) {
        const auto& g = list[r];

        auto* itemAddr = new QTableWidgetItem(QString::fromStdString(g.address.toHex()));
        auto* itemDisasm = new QTableWidgetItem(QString::fromStdString(g.disassembly));
        auto* itemCat = new QTableWidgetItem(QString::fromStdString(g.category));
        auto* itemLen = new QTableWidgetItem(QString::number(g.length));
        auto* itemCount = new QTableWidgetItem(QString::number(g.insnCount));

        itemAddr->setForeground(QColor(80, 200, 120));
        itemDisasm->setForeground(QColor(230, 230, 230));
        itemCat->setForeground(QColor(70, 180, 220));
        itemLen->setTextAlignment(Qt::AlignCenter);
        itemCount->setTextAlignment(Qt::AlignCenter);

        table_->setItem(r, 0, itemAddr);
        table_->setItem(r, 1, itemDisasm);
        table_->setItem(r, 2, itemCat);
        table_->setItem(r, 3, itemLen);
        table_->setItem(r, 4, itemCount);
    }
}

void ROPToolView::onCellDoubleClicked(int row, int col) {
    Q_UNUSED(col);
    if (row < 0 || row >= static_cast<int>(filteredGadgets_.size())) return;
    Q_EMIT jumpToAddressRequested(filteredGadgets_[row].address);
}

void ROPToolView::onContextMenu(const QPoint& pos) {
    int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(filteredGadgets_.size())) return;
    const auto& g = filteredGadgets_[row];

    QMenu menu(this);
    auto* actJump = menu.addAction("Jump to Disassembly");
    menu.addSeparator();
    auto* actCopyAddr = menu.addAction("Copy Address (Hex)");
    auto* actCopyPayload = menu.addAction("Copy Python Payload (p64(0x...))");
    auto* actCopyDisasm = menu.addAction("Copy Disassembly");

    auto* selected = menu.exec(table_->viewport()->mapToGlobal(pos));
    if (selected == actJump) {
        Q_EMIT jumpToAddressRequested(g.address);
    } else if (selected == actCopyAddr) {
        QApplication::clipboard()->setText(QString::fromStdString(g.address.toHex()));
    } else if (selected == actCopyPayload) {
        QString p64 = QString("p64(%1)").arg(QString::fromStdString(g.address.toHex()));
        QApplication::clipboard()->setText(p64);
    } else if (selected == actCopyDisasm) {
        QApplication::clipboard()->setText(QString::fromStdString(g.disassembly));
    }
}

} // namespace edb_next
