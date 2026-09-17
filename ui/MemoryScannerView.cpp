#include "MemoryScannerView.hpp"
#include "core/DebugSession.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QMenu>
#include <QClipboard>
#include <QGuiApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QColor>

namespace edb_next {

MemoryScannerView::MemoryScannerView(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

void MemoryScannerView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
    updateButtons();
    refreshResults();
}

void MemoryScannerView::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Controls container
    auto* controlWidget = new QWidget(this);
    auto* gridLayout = new QGridLayout(controlWidget);
    gridLayout->setContentsMargins(2, 2, 2, 2);
    gridLayout->setHorizontalSpacing(6);
    gridLayout->setVerticalSpacing(4);

    // Row 0: Inputs
    auto* valLabel = new QLabel("Value:", this);
    valueEdit_ = new QLineEdit(this);
    valueEdit_->setPlaceholderText("Enter value to search (e.g. 100, 0x1337, 3.14, 'str', 48 89 ?? 55)");
    connect(valueEdit_, &QLineEdit::returnPressed, this, [this]() {
        if (session_ && session_->memoryScanner().hasSearched()) {
            onNextScanClicked();
        } else {
            onFirstScanClicked();
        }
    });

    auto* deltaLabel = new QLabel("Delta (+/-):", this);
    deltaEdit_ = new QLineEdit(this);
    deltaEdit_->setPlaceholderText("Offset/Delta (e.g. 10, -5)");
    deltaEdit_->setMaximumWidth(120);
    deltaLabel->setVisible(false);
    deltaEdit_->setVisible(false);

    auto* typeLabel = new QLabel("Data Type:", this);
    dataTypeCombo_ = new QComboBox(this);
    dataTypeCombo_->addItem("Int32 (4 Bytes)", static_cast<int>(ScanDataType::Int32));
    dataTypeCombo_->addItem("Int64 (8 Bytes)", static_cast<int>(ScanDataType::Int64));
    dataTypeCombo_->addItem("Int16 (2 Bytes)", static_cast<int>(ScanDataType::Int16));
    dataTypeCombo_->addItem("Int8 (1 Byte)", static_cast<int>(ScanDataType::Int8));
    dataTypeCombo_->addItem("Float (Single)", static_cast<int>(ScanDataType::Float));
    dataTypeCombo_->addItem("Double", static_cast<int>(ScanDataType::Double));
    dataTypeCombo_->addItem("String (Text)", static_cast<int>(ScanDataType::String));
    dataTypeCombo_->addItem("Hex Bytes (ByteArray)", static_cast<int>(ScanDataType::ByteArray));
    connect(dataTypeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MemoryScannerView::onDataTypeChanged);

    auto* compareLabel = new QLabel("Scan Type:", this);
    compareTypeCombo_ = new QComboBox(this);
    compareTypeCombo_->addItem("Exact Value", static_cast<int>(ScanCompareType::ExactValue));
    compareTypeCombo_->addItem("Increased Value", static_cast<int>(ScanCompareType::IncreasedValue));
    compareTypeCombo_->addItem("Decreased Value", static_cast<int>(ScanCompareType::DecreasedValue));
    compareTypeCombo_->addItem("Changed Value", static_cast<int>(ScanCompareType::ChangedValue));
    compareTypeCombo_->addItem("Unchanged Value", static_cast<int>(ScanCompareType::UnchangedValue));
    compareTypeCombo_->addItem("Increased By...", static_cast<int>(ScanCompareType::IncreasedBy));
    compareTypeCombo_->addItem("Decreased By...", static_cast<int>(ScanCompareType::DecreasedBy));
    compareTypeCombo_->addItem("Unknown Initial Value", static_cast<int>(ScanCompareType::UnknownInitialValue));
    connect(compareTypeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MemoryScannerView::onCompareTypeChanged);

    gridLayout->addWidget(valLabel, 0, 0);
    gridLayout->addWidget(valueEdit_, 0, 1);
    gridLayout->addWidget(deltaLabel, 0, 2);
    gridLayout->addWidget(deltaEdit_, 0, 3);
    gridLayout->addWidget(typeLabel, 0, 4);
    gridLayout->addWidget(dataTypeCombo_, 0, 5);
    gridLayout->addWidget(compareLabel, 0, 6);
    gridLayout->addWidget(compareTypeCombo_, 0, 7);

    // Row 1: Actions & Scope
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(6);

    firstScanBtn_ = new QPushButton("🔍 First Scan", this);
    firstScanBtn_->setStyleSheet("QPushButton { background-color: #1a446c; color: #e0f2fe; font-weight: bold; padding: 4px 10px; border-radius: 3px; } QPushButton:hover { background-color: #0284c7; }");
    connect(firstScanBtn_, &QPushButton::clicked, this, &MemoryScannerView::onFirstScanClicked);

    nextScanBtn_ = new QPushButton("⚡ Next Scan", this);
    nextScanBtn_->setStyleSheet("QPushButton { background-color: #14532d; color: #bbf7d0; font-weight: bold; padding: 4px 10px; border-radius: 3px; } QPushButton:hover { background-color: #16a34a; }");
    nextScanBtn_->setEnabled(false);
    connect(nextScanBtn_, &QPushButton::clicked, this, &MemoryScannerView::onNextScanClicked);

    refreshBtn_ = new QPushButton("🔄 Refresh", this);
    refreshBtn_->setStyleSheet("QPushButton { background-color: #334155; color: #f1f5f9; padding: 4px 8px; border-radius: 3px; } QPushButton:hover { background-color: #475569; }");
    connect(refreshBtn_, &QPushButton::clicked, this, &MemoryScannerView::onRefreshClicked);

    resetBtn_ = new QPushButton("🗑 Reset", this);
    resetBtn_->setStyleSheet("QPushButton { background-color: #7f1d1d; color: #fecaca; padding: 4px 8px; border-radius: 3px; } QPushButton:hover { background-color: #b91c1c; }");
    connect(resetBtn_, &QPushButton::clicked, this, &MemoryScannerView::onResetClicked);

    writableOnlyCheck_ = new QCheckBox("Writable Memory Only (rw-p)", this);
    writableOnlyCheck_->setChecked(true);
    writableOnlyCheck_->setToolTip("Focus scanning on heaps, stacks, and data sections. Drastically accelerates scan time.");

    auto* alignLabel = new QLabel("Align:", this);
    alignCombo_ = new QComboBox(this);
    alignCombo_->addItem("4 Bytes (Default)", 4);
    alignCombo_->addItem("1 Byte (Unaligned)", 1);
    alignCombo_->addItem("2 Bytes", 2);
    alignCombo_->addItem("8 Bytes", 8);

    statusLabel_ = new QLabel("Ready to scan", this);
    statusLabel_->setStyleSheet("color: #94a3b8; font-style: italic; margin-left: 10px;");

    btnLayout->addWidget(firstScanBtn_);
    btnLayout->addWidget(nextScanBtn_);
    btnLayout->addWidget(refreshBtn_);
    btnLayout->addWidget(resetBtn_);
    btnLayout->addSpacing(10);
    btnLayout->addWidget(writableOnlyCheck_);
    btnLayout->addSpacing(6);
    btnLayout->addWidget(alignLabel);
    btnLayout->addWidget(alignCombo_);
    btnLayout->addWidget(statusLabel_, 1);

    gridLayout->addLayout(btnLayout, 1, 0, 1, 8);
    mainLayout->addWidget(controlWidget);

    // Results Table
    resultsTable_ = new QTableWidget(this);
    resultsTable_->setColumnCount(5);
    resultsTable_->setHorizontalHeaderLabels({"Address", "Type", "Previous Value", "Current Value", "Delta"});
    resultsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    resultsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    resultsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    resultsTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    resultsTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    resultsTable_->verticalHeader()->setVisible(false);
    resultsTable_->verticalHeader()->setDefaultSectionSize(22);
    resultsTable_->setAlternatingRowColors(true);
    resultsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    resultsTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    resultsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultsTable_->setContextMenuPolicy(Qt::CustomContextMenu);
    resultsTable_->setStyleSheet(
        "QTableWidget { background-color: #1e1e1e; alternate-background-color: #252526; color: #d4d4d4; gridline-color: #333333; selection-background-color: #264f78; }"
        "QHeaderView::section { background-color: #2d2d2d; color: #cccccc; padding: 3px; border: 1px solid #3c3c3c; font-weight: bold; }"
    );

    connect(resultsTable_, &QTableWidget::cellDoubleClicked, this, &MemoryScannerView::onTableDoubleClicked);
    connect(resultsTable_, &QTableWidget::customContextMenuRequested, this, &MemoryScannerView::onTableContextMenu);

    mainLayout->addWidget(resultsTable_, 1);
}

void MemoryScannerView::updateButtons() {
    if (!session_) {
        firstScanBtn_->setEnabled(false);
        nextScanBtn_->setEnabled(false);
        refreshBtn_->setEnabled(false);
        resetBtn_->setEnabled(false);
        return;
    }
    firstScanBtn_->setEnabled(true);
    resetBtn_->setEnabled(true);
    bool hasSearched = session_->memoryScanner().hasSearched();
    nextScanBtn_->setEnabled(hasSearched && session_->memoryScanner().resultCount() > 0);
    refreshBtn_->setEnabled(hasSearched && session_->memoryScanner().resultCount() > 0);
}

ScanOptions MemoryScannerView::collectOptions() const {
    ScanOptions opt;
    opt.dataType = static_cast<ScanDataType>(dataTypeCombo_->currentData().toInt());
    opt.compareType = static_cast<ScanCompareType>(compareTypeCombo_->currentData().toInt());
    opt.valueStr = valueEdit_->text().toStdString();
    opt.deltaStr = deltaEdit_->text().toStdString();
    opt.writableOnly = writableOnlyCheck_->isChecked();
    opt.alignment = static_cast<size_t>(alignCombo_->currentData().toInt());
    return opt;
}

void MemoryScannerView::onFirstScanClicked() {
    if (!session_) return;
    auto opt = collectOptions();
    if (opt.compareType == ScanCompareType::ExactValue && opt.valueStr.empty()) {
        QMessageBox::warning(this, "Memory Scanner", "Please specify a value to search for First Scan.");
        return;
    }

    size_t count = session_->firstMemoryScan(opt);
    updateButtons();
    refreshResults();
    statusLabel_->setText(QString("Pass 1: Found %1 results").arg(count));
}

void MemoryScannerView::onNextScanClicked() {
    if (!session_) return;
    auto opt = collectOptions();
    size_t count = session_->nextMemoryScan(opt);
    updateButtons();
    refreshResults();
    statusLabel_->setText(QString("Pass %1: Found %2 results (converged)")
                              .arg(session_->memoryScanner().scanPass())
                              .arg(count));
}

void MemoryScannerView::onRefreshClicked() {
    if (!session_) return;
    session_->refreshMemoryScan();
    refreshResults();
    statusLabel_->setText(QString("Refreshed %1 candidate values from live memory")
                              .arg(session_->memoryScanner().resultCount()));
}

void MemoryScannerView::onResetClicked() {
    if (!session_) return;
    session_->resetMemoryScan();
    updateButtons();
    refreshResults();
    statusLabel_->setText("Scanner reset. Ready for new search.");
}

void MemoryScannerView::onDataTypeChanged(int) {
    auto type = static_cast<ScanDataType>(dataTypeCombo_->currentData().toInt());
    if (type == ScanDataType::String || type == ScanDataType::ByteArray) {
        alignCombo_->setCurrentIndex(1); // 1 byte
    } else if (type == ScanDataType::Int64 || type == ScanDataType::Double) {
        alignCombo_->setCurrentIndex(3); // 8 bytes
    } else {
        alignCombo_->setCurrentIndex(0); // 4 bytes
    }
}

void MemoryScannerView::onCompareTypeChanged(int) {
    auto comp = static_cast<ScanCompareType>(compareTypeCombo_->currentData().toInt());
    bool needDelta = (comp == ScanCompareType::IncreasedBy || comp == ScanCompareType::DecreasedBy);
    deltaEdit_->setVisible(needDelta);
    if (parentWidget()) parentWidget()->layout()->activate();
}

void MemoryScannerView::refreshResults() {
    resultsTable_->setRowCount(0);
    if (!session_) return;

    const auto& results = session_->memoryScanner().results();
    auto type = session_->memoryScanner().activeOptions().dataType;

    size_t displayLimit = std::min<size_t>(results.size(), 2000);
    resultsTable_->setRowCount(static_cast<int>(displayLimit));

    for (size_t i = 0; i < displayLimit; ++i) {
        const auto& item = results[i];
        // Col 0: Address
        auto* addrItem = new QTableWidgetItem(item.address.toQString());
        addrItem->setForeground(QColor(100, 200, 255));
        addrItem->setFont(QFont("Monospace"));
        resultsTable_->setItem(static_cast<int>(i), 0, addrItem);

        // Col 1: Type
        auto* typeItem = new QTableWidgetItem(QString::fromStdString(scanDataTypeToString(type)));
        resultsTable_->setItem(static_cast<int>(i), 1, typeItem);

        // Col 2: Previous Value
        auto* prevItem = new QTableWidgetItem(QString::fromStdString(item.formatPreviousValue(type)));
        prevItem->setForeground(QColor(160, 160, 160));
        resultsTable_->setItem(static_cast<int>(i), 2, prevItem);

        // Col 3: Current Value
        auto* curItem = new QTableWidgetItem(QString::fromStdString(item.formatCurrentValue(type)));
        curItem->setForeground(QColor(240, 240, 240));
        curItem->setFont(QFont("Monospace"));
        resultsTable_->setItem(static_cast<int>(i), 3, curItem);

        // Col 4: Delta
        std::string deltaStr = item.formatDelta(type);
        auto* deltaItem = new QTableWidgetItem(QString::fromStdString(deltaStr));
        if (deltaStr.rfind("+", 0) == 0) {
            deltaItem->setForeground(QColor(74, 222, 128)); // Light green
        } else if (deltaStr.rfind("-", 0) == 0 && deltaStr != "-") {
            deltaItem->setForeground(QColor(248, 113, 113)); // Light red
        } else {
            deltaItem->setForeground(QColor(148, 163, 184));
        }
        resultsTable_->setItem(static_cast<int>(i), 4, deltaItem);
    }
}

void MemoryScannerView::onTableDoubleClicked(int row, int) {
    if (row < 0 || row >= resultsTable_->rowCount()) return;
    QString addrStr = resultsTable_->item(row, 0)->text();
    bool ok = false;
    uint64_t val = addrStr.toULongLong(&ok, 16);
    if (ok) {
        Q_EMIT jumpToMemoryRequested(Address(val));
    }
}

void MemoryScannerView::onTableContextMenu(const QPoint& pos) {
    int row = resultsTable_->rowAt(pos.y());
    if (row < 0) return;

    QString addrStr = resultsTable_->item(row, 0)->text();
    bool ok = false;
    uint64_t val = addrStr.toULongLong(&ok, 16);
    if (!ok) return;
    Address addr(val);

    QMenu menu(this);
    auto* actHex = menu.addAction("Follow in Hex Dump");
    auto* actDisasm = menu.addAction("Follow in Disassembly");
    menu.addSeparator();
    auto* actCopy = menu.addAction("Copy Address");
    auto* actEdit = menu.addAction("Edit / Write Value...");

    QAction* selected = menu.exec(resultsTable_->viewport()->mapToGlobal(pos));
    if (selected == actHex) {
        Q_EMIT jumpToMemoryRequested(addr);
    } else if (selected == actDisasm) {
        Q_EMIT jumpToDisassemblyRequested(addr);
    } else if (selected == actCopy) {
        QGuiApplication::clipboard()->setText(addrStr);
    } else if (selected == actEdit) {
        editSelectedValue();
    }
}

void MemoryScannerView::editSelectedValue() {
    int row = resultsTable_->currentRow();
    if (row < 0 || !session_) return;
    QString addrStr = resultsTable_->item(row, 0)->text();
    bool ok = false;
    uint64_t val = addrStr.toULongLong(&ok, 16);
    if (!ok) return;
    Address addr(val);

    auto type = session_->memoryScanner().activeOptions().dataType;
    QString currentVal = resultsTable_->item(row, 3)->text();

    bool dialogOk = false;
    QString newValStr = QInputDialog::getText(
        this, "Write Memory Value",
        QString("Enter new value for address %1 (%2):").arg(addrStr).arg(QString::fromStdString(scanDataTypeToString(type))),
        QLineEdit::Normal, currentVal, &dialogOk);

    if (!dialogOk || newValStr.isEmpty()) return;

    size_t size = MemoryScanner::getDataTypeSize(type, newValStr.toStdString());
    std::vector<uint8_t> bytes(size, 0);

    if (type == ScanDataType::Float) {
        float f = newValStr.toFloat();
        std::memcpy(bytes.data(), &f, sizeof(float));
    } else if (type == ScanDataType::Double) {
        double d = newValStr.toDouble();
        std::memcpy(bytes.data(), &d, sizeof(double));
    } else if (type == ScanDataType::String) {
        std::string s = newValStr.toStdString();
        bytes.assign(s.begin(), s.end());
    } else if (type == ScanDataType::ByteArray) {
        auto pattern = PatternSearcher::parsePattern(newValStr.toStdString());
        bytes.resize(pattern.size());
        for (size_t i = 0; i < pattern.size(); ++i) bytes[i] = pattern[i].value;
    } else {
        bool numOk = false;
        int64_t intVal = 0;
        if (newValStr.startsWith("0x", Qt::CaseInsensitive)) {
            intVal = static_cast<int64_t>(newValStr.toULongLong(&numOk, 16));
        } else {
            intVal = newValStr.toLongLong(&numOk, 10);
        }
        if (numOk) {
            std::memcpy(bytes.data(), &intVal, std::min<size_t>(size, sizeof(int64_t)));
        }
    }

    if (session_->writeMemory(addr, bytes.data(), bytes.size())) {
        session_->refreshMemoryScan();
        refreshResults();
    } else {
        QMessageBox::critical(this, "Error", "Failed to write memory to target process!");
    }
}

} // namespace edb_next
