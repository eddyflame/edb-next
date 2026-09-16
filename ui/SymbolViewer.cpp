#include "SymbolViewer.hpp"
#include "DebugSession.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFontDatabase>
#include <iomanip>
#include <sstream>

namespace edb_next {

namespace {

QString symbolTypeToString(uint8_t type) {
    switch (type) {
        case 0: return "NOTYPE";
        case 1: return "OBJECT";
        case 2: return "FUNC";
        case 3: return "SECTION";
        case 4: return "FILE";
        case 6: return "TLS";
        case 10: return "IFUNC";
        default: return QString::number(type);
    }
}

QString symbolBindingToString(uint8_t binding) {
    switch (binding) {
        case 0: return "LOCAL";
        case 1: return "GLOBAL";
        case 2: return "WEAK";
        default: return QString::number(binding);
    }
}

} // namespace

SymbolViewer::SymbolViewer(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void SymbolViewer::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    auto* topLayout = new QHBoxLayout();
    topLayout->setSpacing(6);

    auto* filterLabel = new QLabel("Filter Symbols:", this);
    topLayout->addWidget(filterLabel);

    filterEdit_ = new QLineEdit(this);
    filterEdit_->setPlaceholderText("Search symbol name or address...");
    filterEdit_->setClearButtonEnabled(true);
    topLayout->addWidget(filterEdit_, 1);

    auto* refreshBtn = new QPushButton("Refresh", this);
    topLayout->addWidget(refreshBtn);

    countLabel_ = new QLabel("0 symbols", this);
    topLayout->addWidget(countLabel_);

    mainLayout->addLayout(topLayout);

    table_ = new QTableWidget(this);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({"Address", "Symbol Name", "Size", "Type", "Binding"});

    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    table_->setShowGrid(true);

    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(9);
    table_->setFont(monoFont);

    mainLayout->addWidget(table_, 1);

    connect(filterEdit_, &QLineEdit::textChanged, this, &SymbolViewer::onFilterChanged);
    connect(refreshBtn, &QPushButton::clicked, this, &SymbolViewer::refresh);
    connect(table_, &QTableWidget::cellDoubleClicked, this, &SymbolViewer::onCellDoubleClicked);
}

void SymbolViewer::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
    refresh();
}

void SymbolViewer::refresh() {
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) {
        cachedSymbols_.clear();
        updateTableDisplay();
        countLabel_->setText("0 symbols");
        return;
    }

    cachedSymbols_ = session->symbols().allSymbols();
    countLabel_->setText(QString("%1 symbols").arg(cachedSymbols_.size()));
    updateTableDisplay();
}

void SymbolViewer::onFilterChanged(const QString& /*filter*/) {
    updateTableDisplay();
}

void SymbolViewer::updateTableDisplay() {
    QString filter = filterEdit_->text().trimmed();

    table_->setRowCount(0);
    QFont monoFont = table_->font();

    for (const auto& sym : cachedSymbols_) {
        QString name = QString::fromStdString(sym.name);
        if (name.isEmpty()) continue;

        std::ostringstream addrOss;
        addrOss << "0x" << std::hex << std::setw(16) << std::setfill('0') << sym.address.value();
        QString addrStr = QString::fromStdString(addrOss.str());

        if (!filter.isEmpty() && !name.contains(filter, Qt::CaseInsensitive) && !addrStr.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }

        int row = table_->rowCount();
        table_->insertRow(row);

        auto* itemAddr = new QTableWidgetItem(addrStr);
        itemAddr->setFont(monoFont);

        auto* itemName = new QTableWidgetItem(name);
        itemName->setFont(monoFont);

        if (sym.type == 2) { // STT_FUNC
            itemName->setForeground(QColor(100, 200, 255));
        } else if (sym.type == 1) { // STT_OBJECT
            itemName->setForeground(QColor(230, 180, 80));
        }

        auto* itemSize = new QTableWidgetItem(QString::number(sym.size));
        auto* itemType = new QTableWidgetItem(symbolTypeToString(sym.type));
        auto* itemBind = new QTableWidgetItem(symbolBindingToString(sym.binding));

        table_->setItem(row, 0, itemAddr);
        table_->setItem(row, 1, itemName);
        table_->setItem(row, 2, itemSize);
        table_->setItem(row, 3, itemType);
        table_->setItem(row, 4, itemBind);
    }
}

void SymbolViewer::onCellDoubleClicked(int row, int /*col*/) {
    if (row < 0 || row >= table_->rowCount()) return;

    QString addrStr = table_->item(row, 0)->text();
    bool ok = false;
    uint64_t addrVal = addrStr.toULongLong(&ok, 16);
    if (ok) {
        Q_EMIT jumpToAddressRequested(Address(addrVal));
    }
}

} // namespace edb_next
