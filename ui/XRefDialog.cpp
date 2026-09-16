#include "XRefDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>

namespace edb_next {

XRefDialog::XRefDialog(Address targetAddr, const std::vector<CodeXRef>& xrefs, QWidget* parent)
    : QDialog(parent), targetAddr_(targetAddr), xrefs_(xrefs) {

    setWindowTitle(QString("Cross References (XREFS) to %1").arg(QString::fromStdString(targetAddr.toHex())));
    resize(700, 360);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto* headerLabel = new QLabel(
        QString("Found %1 reference(s) to address <b>%2</b>. Double-click an item to jump to code.")
            .arg(xrefs.size())
            .arg(QString::fromStdString(targetAddr.toHex())),
        this);
    layout->addWidget(headerLabel);

    table_ = new QTableWidget(this);
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({"Type", "Source Address", "Function", "Instruction"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    table_->setRowCount(static_cast<int>(xrefs_.size()));
    for (int r = 0; r < static_cast<int>(xrefs_.size()); ++r) {
        const auto& x = xrefs_[r];

        auto* itemType = new QTableWidgetItem(QString::fromStdString(x.type));
        auto* itemAddr = new QTableWidgetItem(QString::fromStdString(x.sourceAddress.toHex()));
        auto* itemFunc = new QTableWidgetItem(QString::fromStdString(x.sourceFunction));
        auto* itemInsn = new QTableWidgetItem(QString::fromStdString(x.mnemonic + " " + x.operands));

        itemType->setTextAlignment(Qt::AlignCenter);
        if (x.type == "CALL") {
            itemType->setForeground(QColor(80, 200, 120));
        } else if (x.type == "JUMP" || x.type == "COND_JUMP") {
            itemType->setForeground(QColor(240, 140, 60));
        } else {
            itemType->setForeground(QColor(70, 180, 220));
        }

        itemFunc->setForeground(QColor(180, 180, 220));

        table_->setItem(r, 0, itemType);
        table_->setItem(r, 1, itemAddr);
        table_->setItem(r, 2, itemFunc);
        table_->setItem(r, 3, itemInsn);
    }

    layout->addWidget(table_);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* btnClose = new QPushButton("Close", this);
    btnLayout->addWidget(btnClose);
    layout->addLayout(btnLayout);

    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
    connect(table_, &QTableWidget::cellDoubleClicked, this, &XRefDialog::onCellDoubleClicked);
}

void XRefDialog::onCellDoubleClicked(int row, int col) {
    Q_UNUSED(col);
    if (row < 0 || row >= static_cast<int>(xrefs_.size())) return;
    Q_EMIT jumpRequested(xrefs_[row].sourceAddress);
    accept();
}

} // namespace edb_next
