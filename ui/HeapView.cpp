#include "HeapView.hpp"
#include "DebugSession.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFontDatabase>
#include <iomanip>
#include <sstream>

namespace edb_next {

HeapView::HeapView(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void HeapView::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    auto* topLayout = new QHBoxLayout();
    topLayout->setSpacing(6);

    analyzeBtn_ = new QPushButton("Analyze Heap Chunks", this);
    topLayout->addWidget(analyzeBtn_);

    auto* filterLabel = new QLabel("Filter:", this);
    topLayout->addWidget(filterLabel);

    filterEdit_ = new QLineEdit(this);
    filterEdit_->setPlaceholderText("Filter by address or state...");
    filterEdit_->setClearButtonEnabled(true);
    topLayout->addWidget(filterEdit_, 1);

    statusLabel_ = new QLabel("Ready", this);
    topLayout->addWidget(statusLabel_);

    mainLayout->addLayout(topLayout);

    table_ = new QTableWidget(this);
    table_->setColumnCount(6);
    table_->setHorizontalHeaderLabels({"Chunk Address", "User Data Address", "Size (Bytes)", "Prev Size", "Flags (A|M|P)", "State"});

    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);

    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    table_->setShowGrid(true);

    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(9);
    table_->setFont(monoFont);

    mainLayout->addWidget(table_, 1);

    connect(analyzeBtn_, &QPushButton::clicked, this, &HeapView::onAnalyzeClicked);
    connect(filterEdit_, &QLineEdit::textChanged, this, &HeapView::onFilterChanged);
    connect(table_, &QTableWidget::cellDoubleClicked, this, &HeapView::onCellDoubleClicked);
}

void HeapView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
    cachedChunks_.clear();
    updateTableDisplay();
}

void HeapView::refresh() {
    onAnalyzeClicked();
}

void HeapView::onAnalyzeClicked() {
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) {
        cachedChunks_.clear();
        updateTableDisplay();
        statusLabel_->setText("No target active");
        return;
    }

    cachedChunks_ = HeapAnalyzer::analyze(*session, 2000);
    statusLabel_->setText(QString("Found %1 chunks").arg(cachedChunks_.size()));
    updateTableDisplay();
}

void HeapView::onFilterChanged(const QString& /*filter*/) {
    updateTableDisplay();
}

void HeapView::updateTableDisplay() {
    QString filter = filterEdit_->text().trimmed();

    table_->setRowCount(0);
    QFont monoFont = table_->font();

    for (const auto& chunk : cachedChunks_) {
        std::ostringstream chunkOss, userOss, prevOss, sizeOss;
        chunkOss << "0x" << std::hex << std::setw(16) << std::setfill('0') << chunk.chunkAddress.value();
        userOss << "0x" << std::hex << std::setw(16) << std::setfill('0') << chunk.userAddress.value();
        prevOss << "0x" << std::hex << chunk.prevSize;
        sizeOss << "0x" << std::hex << chunk.actualSize << " (" << std::dec << chunk.actualSize << ")";

        QString flagsStr = QString("%1 | %2 | %3")
            .arg(chunk.flagNonMainArena ? "A" : "-")
            .arg(chunk.flagIsMmapped ? "M" : "-")
            .arg(chunk.flagPrevInUse ? "P" : "-");

        QString statusStr = QString::fromStdString(chunk.status);
        QString chunkAddrStr = QString::fromStdString(chunkOss.str());

        if (!filter.isEmpty() && !chunkAddrStr.contains(filter, Qt::CaseInsensitive) &&
            !statusStr.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }

        int row = table_->rowCount();
        table_->insertRow(row);

        auto* itemChunk = new QTableWidgetItem(chunkAddrStr);
        itemChunk->setFont(monoFont);

        auto* itemUser = new QTableWidgetItem(QString::fromStdString(userOss.str()));
        itemUser->setFont(monoFont);

        auto* itemSize = new QTableWidgetItem(QString::fromStdString(sizeOss.str()));
        itemSize->setFont(monoFont);

        auto* itemPrev = new QTableWidgetItem(QString::fromStdString(prevOss.str()));
        itemPrev->setFont(monoFont);

        auto* itemFlags = new QTableWidgetItem(flagsStr);
        itemFlags->setFont(monoFont);

        auto* itemStatus = new QTableWidgetItem(statusStr);
        itemStatus->setFont(monoFont);

        if (chunk.status == "Allocated") {
            itemStatus->setForeground(QColor(100, 220, 120));
        } else if (chunk.status == "Free") {
            itemStatus->setForeground(QColor(100, 180, 255));
        } else if (chunk.isTopChunk) {
            itemStatus->setForeground(QColor(240, 180, 60));
        }

        table_->setItem(row, 0, itemChunk);
        table_->setItem(row, 1, itemUser);
        table_->setItem(row, 2, itemSize);
        table_->setItem(row, 3, itemPrev);
        table_->setItem(row, 4, itemFlags);
        table_->setItem(row, 5, itemStatus);
    }
}

void HeapView::onCellDoubleClicked(int row, int /*col*/) {
    if (row < 0 || row >= table_->rowCount()) return;

    QString userAddrStr = table_->item(row, 1)->text();
    bool ok = false;
    uint64_t addrVal = userAddrStr.toULongLong(&ok, 16);
    if (ok) {
        Q_EMIT jumpToMemoryRequested(Address(addrVal));
    }
}

} // namespace edb_next
