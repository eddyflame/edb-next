#include "StringReferencesView.hpp"
#include "DebugSession.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFontDatabase>
#include <iomanip>
#include <sstream>

namespace edb_next {

StringReferencesView::StringReferencesView(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void StringReferencesView::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Top control bar
    auto* topLayout = new QHBoxLayout();
    topLayout->setSpacing(6);

    scanBtn_ = new QPushButton("Scan Memory for Strings", this);
    topLayout->addWidget(scanBtn_);

    auto* filterLabel = new QLabel("Filter:", this);
    topLayout->addWidget(filterLabel);

    filterEdit_ = new QLineEdit(this);
    filterEdit_->setPlaceholderText("Search string content or module...");
    filterEdit_->setClearButtonEnabled(true);
    topLayout->addWidget(filterEdit_, 1);

    statusLabel_ = new QLabel("Ready", this);
    topLayout->addWidget(statusLabel_);

    mainLayout->addLayout(topLayout);

    // Table view
    table_ = new QTableWidget(this);
    table_->setColumnCount(6);
    table_->setHorizontalHeaderLabels({"Address", "String Content", "Length", "Region / Module", "Refs", "First Reference"});

    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);

    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    table_->setShowGrid(true);

    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(9);
    table_->setFont(monoFont);

    mainLayout->addWidget(table_, 1);

    connect(scanBtn_, &QPushButton::clicked, this, &StringReferencesView::onScanClicked);
    connect(filterEdit_, &QLineEdit::textChanged, this, &StringReferencesView::onFilterChanged);
    connect(table_, &QTableWidget::cellDoubleClicked, this, &StringReferencesView::onCellDoubleClicked);
}

void StringReferencesView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
    cachedStrings_.clear();
    updateTableDisplay();
}

void StringReferencesView::refresh() {
    onScanClicked();
}

void StringReferencesView::onScanClicked() {
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) {
        cachedStrings_.clear();
        updateTableDisplay();
        statusLabel_->setText("No target active");
        return;
    }

    statusLabel_->setText("Scanning...");
    table_->setRowCount(0);

    cachedStrings_ = StringScanner::scan(*session, 4, 3000);
    statusLabel_->setText(QString("Found %1 strings").arg(cachedStrings_.size()));
    updateTableDisplay();
}

void StringReferencesView::onFilterChanged(const QString& /*filter*/) {
    updateTableDisplay();
}

void StringReferencesView::updateTableDisplay() {
    QString filter = filterEdit_->text().trimmed();

    table_->setRowCount(0);
    QFont monoFont = table_->font();

    for (const auto& item : cachedStrings_) {
        QString text = QString::fromStdString(item.text);
        QString region = QString::fromStdString(item.regionName);

        if (!filter.isEmpty() && !text.contains(filter, Qt::CaseInsensitive) && !region.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }

        int row = table_->rowCount();
        table_->insertRow(row);

        std::ostringstream addrOss;
        addrOss << "0x" << std::hex << std::setw(16) << std::setfill('0') << item.address.value();
        auto* itemAddr = new QTableWidgetItem(QString::fromStdString(addrOss.str()));
        itemAddr->setFont(monoFont);

        auto* itemText = new QTableWidgetItem(text);
        itemText->setFont(monoFont);

        auto* itemLen = new QTableWidgetItem(QString::number(item.length));
        auto* itemRegion = new QTableWidgetItem(region);

        auto* itemRefs = new QTableWidgetItem(QString::number(item.references.size()));

        QString firstRefStr = "-";
        if (!item.references.empty()) {
            std::ostringstream refOss;
            refOss << "0x" << std::hex << std::setw(16) << std::setfill('0') << item.references[0].value();
            firstRefStr = QString::fromStdString(refOss.str());
        }
        auto* itemFirstRef = new QTableWidgetItem(firstRefStr);
        itemFirstRef->setFont(monoFont);

        if (!item.references.empty()) {
            itemRefs->setForeground(QColor(100, 200, 100));
            itemFirstRef->setForeground(QColor(100, 200, 100));
        }

        table_->setItem(row, 0, itemAddr);
        table_->setItem(row, 1, itemText);
        table_->setItem(row, 2, itemLen);
        table_->setItem(row, 3, itemRegion);
        table_->setItem(row, 4, itemRefs);
        table_->setItem(row, 5, itemFirstRef);
    }
}

void StringReferencesView::onCellDoubleClicked(int row, int col) {
    if (row < 0 || row >= table_->rowCount()) return;

    if (col == 4 || col == 5) {
        // Double clicked reference -> Jump to disassembly
        QString refStr = table_->item(row, 5)->text();
        if (refStr != "-") {
            bool ok = false;
            uint64_t addrVal = refStr.toULongLong(&ok, 16);
            if (ok) {
                Q_EMIT jumpToDisassemblyRequested(Address(addrVal));
                return;
            }
        }
    }

    // Default: Jump to memory location of string
    QString addrStr = table_->item(row, 0)->text();
    bool ok = false;
    uint64_t addrVal = addrStr.toULongLong(&ok, 16);
    if (ok) {
        Q_EMIT jumpToMemoryRequested(Address(addrVal));
    }
}

} // namespace edb_next
