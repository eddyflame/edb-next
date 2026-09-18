#include "OpcodeSearcherView.hpp"
#include "core/DebugSession.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QFontDatabase>
#include <QClipboard>
#include <QApplication>
#include <QMenu>
#include <QMessageBox>
#include <iomanip>
#include <sstream>

namespace edb_next {

OpcodeSearcherView::OpcodeSearcherView(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void OpcodeSearcherView::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(4);

    // Top control bar
    auto* topBar = new QHBoxLayout();
    topBar->setSpacing(6);

    auto* lblType = new QLabel("Search Pattern:", this);
    topBar->addWidget(lblType);

    typeCombo_ = new QComboBox(this);
    typeCombo_->addItem("JMP <reg> (e.g. jmp rax)", static_cast<int>(OpcodeSearchType::JmpReg));
    typeCombo_->addItem("CALL <reg> (e.g. call rbx)", static_cast<int>(OpcodeSearchType::CallReg));
    typeCombo_->addItem("PUSH <reg>; RET (Gadget)", static_cast<int>(OpcodeSearchType::PushRegRet));
    typeCombo_->addItem("POP <reg>; RET (Gadget)", static_cast<int>(OpcodeSearchType::PopRegRet));
    typeCombo_->addItem("Syscall / Interrupt (syscall, int3...)", static_cast<int>(OpcodeSearchType::InterruptOrSyscall));
    typeCombo_->addItem("Custom Instruction Query", static_cast<int>(OpcodeSearchType::CustomInstruction));
    connect(typeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpcodeSearcherView::onPatternTypeChanged);
    topBar->addWidget(typeCombo_);

    queryEdit_ = new QLineEdit(this);
    queryEdit_->setPlaceholderText("Enter custom instruction or operand filter (e.g. 'mov eax, 1' or 'rax')...");
    queryEdit_->setEnabled(false);
    connect(queryEdit_, &QLineEdit::returnPressed, this, &OpcodeSearcherView::onSearchClicked);
    topBar->addWidget(queryEdit_, 1);

    searchBtn_ = new QPushButton("Search Opcodes", this);
    searchBtn_->setStyleSheet("font-weight: bold; background-color: #2e7d32; color: white; padding: 4px 10px;");
    connect(searchBtn_, &QPushButton::clicked, this, &OpcodeSearcherView::onSearchClicked);
    topBar->addWidget(searchBtn_);

    mainLayout->addLayout(topBar);

    // Filter Bar
    auto* filterBar = new QHBoxLayout();
    auto* lblFilter = new QLabel("Filter Results:", this);
    filterEdit_ = new QLineEdit(this);
    filterEdit_->setPlaceholderText("Filter found instructions by mnemonic, operand, or address...");
    filterEdit_->setClearButtonEnabled(true);
    connect(filterEdit_, &QLineEdit::textChanged, this, &OpcodeSearcherView::onFilterChanged);
    filterBar->addWidget(lblFilter);
    filterBar->addWidget(filterEdit_, 1);
    mainLayout->addLayout(filterBar);

    // Results Table
    table_ = new QTableWidget(this);
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({"Address", "Instruction", "Opcode Bytes", "Module / Region"});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    table_->setShowGrid(false);

    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(9);
    table_->setFont(monoFont);

    connect(table_, &QTableWidget::cellDoubleClicked, this, &OpcodeSearcherView::onCellDoubleClicked);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QWidget::customContextMenuRequested, this, &OpcodeSearcherView::handleCustomContextMenu);

    mainLayout->addWidget(table_, 1);
}

void OpcodeSearcherView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
}

void OpcodeSearcherView::refresh() {
    dirty_ = false;
    // Retain existing results
}

void OpcodeSearcherView::onPatternTypeChanged(int index) {
    auto type = static_cast<OpcodeSearchType>(typeCombo_->itemData(index).toInt());
    queryEdit_->setEnabled(type == OpcodeSearchType::CustomInstruction);
    if (type == OpcodeSearchType::CustomInstruction) {
        queryEdit_->setFocus();
    }
}

void OpcodeSearcherView::onSearchClicked() {
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) {
        QMessageBox::warning(this, "Search Opcodes", "Target must be paused to scan opcodes.");
        return;
    }

    auto type = static_cast<OpcodeSearchType>(typeCombo_->currentData().toInt());
    std::string query = queryEdit_->text().trimmed().toStdString();

    allResults_ = OpcodeSearcher::search(*session, type, query);
    onFilterChanged(filterEdit_->text());
}

void OpcodeSearcherView::onFilterChanged(const QString& text) {
    displayedResults_.clear();
    QString filter = text.trimmed();

    for (const auto& r : allResults_) {
        if (filter.isEmpty()) {
            displayedResults_.push_back(r);
        } else {
            QString addr = r.address.toQString();
            QString insn = QString::fromStdString(r.mnemonic + " " + r.operands);
            QString mod = QString::fromStdString(r.moduleName);
            if (addr.contains(filter, Qt::CaseInsensitive) ||
                insn.contains(filter, Qt::CaseInsensitive) ||
                mod.contains(filter, Qt::CaseInsensitive)) {
                displayedResults_.push_back(r);
            }
        }
    }

    renderTable();
}

void OpcodeSearcherView::renderTable() {
    table_->setRowCount(static_cast<int>(displayedResults_.size()));

    for (int r = 0; r < static_cast<int>(displayedResults_.size()); ++r) {
        const auto& item = displayedResults_[r];

        // 0: Address
        auto* itemAddr = new QTableWidgetItem(item.address.toQString());
        itemAddr->setForeground(QColor(100, 180, 240));

        // 1: Instruction
        QString insnStr = QString::fromStdString(item.mnemonic + " " + item.operands).trimmed();
        auto* itemInsn = new QTableWidgetItem(insnStr);
        QFont boldFont = table_->font();
        boldFont.setBold(true);
        itemInsn->setFont(boldFont);

        // Color coding
        if (item.mnemonic == "jmp") itemInsn->setForeground(QColor(230, 100, 100));
        else if (item.mnemonic == "call") itemInsn->setForeground(QColor(240, 180, 70));
        else if (item.mnemonic == "push" || item.mnemonic == "pop") itemInsn->setForeground(QColor(140, 200, 140));
        else itemInsn->setForeground(QColor(220, 220, 220));

        // 2: Opcode Bytes
        std::ostringstream bytesOss;
        for (size_t b = 0; b < item.bytes.size(); ++b) {
            bytesOss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(item.bytes[b]);
            if (b + 1 < item.bytes.size()) bytesOss << " ";
        }
        auto* itemBytes = new QTableWidgetItem(QString::fromStdString(bytesOss.str()));
        itemBytes->setForeground(QColor(160, 160, 160));

        // 3: Module
        auto* itemMod = new QTableWidgetItem(QString::fromStdString(item.moduleName));
        itemMod->setForeground(QColor(130, 170, 200));

        table_->setItem(r, 0, itemAddr);
        table_->setItem(r, 1, itemInsn);
        table_->setItem(r, 2, itemBytes);
        table_->setItem(r, 3, itemMod);
    }
}

void OpcodeSearcherView::onCellDoubleClicked(int row, int /*col*/) {
    if (row >= 0 && row < static_cast<int>(displayedResults_.size())) {
        Q_EMIT jumpToDisassemblyRequested(displayedResults_[row].address);
    }
}

void OpcodeSearcherView::handleCustomContextMenu(const QPoint& pos) {
    int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(displayedResults_.size())) return;

    const auto& res = displayedResults_[row];

    QMenu menu(this);
    menu.addAction("Follow in Disassembly", [this, res]() {
        Q_EMIT jumpToDisassemblyRequested(res.address);
    });

    menu.addSeparator();
    menu.addAction("Copy Address", [res]() {
        QApplication::clipboard()->setText(res.address.toQString());
    });
    menu.addAction("Copy Instruction", [res]() {
        QApplication::clipboard()->setText(QString::fromStdString(res.mnemonic + " " + res.operands).trimmed());
    });
    menu.addAction("Copy Opcode Bytes", [res]() {
        std::ostringstream oss;
        for (size_t b = 0; b < res.bytes.size(); ++b) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(res.bytes[b]);
            if (b + 1 < res.bytes.size()) oss << " ";
        }
        QApplication::clipboard()->setText(QString::fromStdString(oss.str()));
    });

    menu.exec(table_->mapToGlobal(pos));
}

} // namespace edb_next
