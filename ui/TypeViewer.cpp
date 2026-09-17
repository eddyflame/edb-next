#include "TypeViewer.hpp"
#include "core/DebugSession.hpp"
#include "core/ExpressionEvaluator.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QClipboard>
#include <QGuiApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QFont>
#include <sstream>
#include <iomanip>

namespace edb_next {

TypeViewer::TypeViewer(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

void TypeViewer::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
    updateStructList();
    refresh();
}

void TypeViewer::setInspectAddress(Address addr) {
    addressEdit_->setText(QString::fromStdString(addr.toHex()));
    onInspectClicked();
}

void TypeViewer::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Top control bar
    auto* topLayout = new QHBoxLayout();
    topLayout->setContentsMargins(2, 2, 2, 2);
    topLayout->setSpacing(6);

    auto* structLabel = new QLabel("Struct:", this);
    structLabel->setStyleSheet("font-weight: bold;");
    structCombo_ = new QComboBox(this);
    structCombo_->setMinimumWidth(180);
    connect(structCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TypeViewer::onStructSelected);

    defineBtn_ = new QPushButton("➕ Define Struct...", this);
    defineBtn_->setStyleSheet("QPushButton { background-color: #1e3a5f; color: #93c5fd; font-weight: bold; padding: 4px 10px; border-radius: 3px; } QPushButton:hover { background-color: #2563eb; color: white; }");
    connect(defineBtn_, &QPushButton::clicked, this, &TypeViewer::onDefineStructClicked);

    auto* addrLabel = new QLabel("Address:", this);
    addrLabel->setStyleSheet("font-weight: bold;");
    addressEdit_ = new QLineEdit(this);
    addressEdit_->setPlaceholderText("e.g. rsp, rbp - 0x20, 0x7fffffffd7d0");
    addressEdit_->setMinimumWidth(200);
    connect(addressEdit_, &QLineEdit::returnPressed, this, &TypeViewer::onInspectClicked);

    inspectBtn_ = new QPushButton("🔬 Inspect", this);
    inspectBtn_->setStyleSheet("QPushButton { background-color: #14532d; color: #bbf7d0; font-weight: bold; padding: 4px 12px; border-radius: 3px; } QPushButton:hover { background-color: #16a34a; color: white; }");
    connect(inspectBtn_, &QPushButton::clicked, this, &TypeViewer::onInspectClicked);

    refreshBtn_ = new QPushButton("🔄 Refresh", this);
    refreshBtn_->setStyleSheet("QPushButton { background-color: #334155; color: #f1f5f9; padding: 4px 10px; border-radius: 3px; } QPushButton:hover { background-color: #475569; }");
    connect(refreshBtn_, &QPushButton::clicked, this, &TypeViewer::refresh);

    sizeLabel_ = new QLabel(this);
    sizeLabel_->setStyleSheet("color: #94a3b8; font-style: italic; margin-left: 10px;");

    topLayout->addWidget(structLabel);
    topLayout->addWidget(structCombo_);
    topLayout->addWidget(defineBtn_);
    topLayout->addSpacing(10);
    topLayout->addWidget(addrLabel);
    topLayout->addWidget(addressEdit_);
    topLayout->addWidget(inspectBtn_);
    topLayout->addWidget(refreshBtn_);
    topLayout->addWidget(sizeLabel_, 1);

    mainLayout->addLayout(topLayout);

    // Fields Table
    fieldTable_ = new QTableWidget(this);
    fieldTable_->setColumnCount(6);
    fieldTable_->setHorizontalHeaderLabels({"Offset", "Field Name", "Type", "Size", "Raw Hex", "Value / Dereference"});
    fieldTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    fieldTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    fieldTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    fieldTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    fieldTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    fieldTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    fieldTable_->verticalHeader()->setVisible(false);
    fieldTable_->verticalHeader()->setDefaultSectionSize(22);
    fieldTable_->setAlternatingRowColors(true);
    fieldTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    fieldTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    fieldTable_->setContextMenuPolicy(Qt::CustomContextMenu);
    fieldTable_->setStyleSheet(
        "QTableWidget { background-color: #1e1e1e; alternate-background-color: #252526; color: #d4d4d4; gridline-color: #333333; selection-background-color: #264f78; }"
        "QHeaderView::section { background-color: #2d2d2d; color: #cccccc; padding: 3px; border: 1px solid #3c3c3c; font-weight: bold; }"
    );

    connect(fieldTable_, &QTableWidget::cellDoubleClicked, this, &TypeViewer::onTableDoubleClicked);
    connect(fieldTable_, &QTableWidget::customContextMenuRequested, this, &TypeViewer::onTableContextMenu);

    mainLayout->addWidget(fieldTable_, 1);
}

void TypeViewer::updateStructList() {
    structCombo_->blockSignals(true);
    structCombo_->clear();
    if (session_) {
        auto names = session_->typeManager().structNames();
        for (const auto& name : names) {
            structCombo_->addItem(QString::fromStdString(name));
        }
    }
    structCombo_->blockSignals(false);
    onStructSelected(structCombo_->currentIndex());
}

Address TypeViewer::parseAddressInput() const {
    if (!session_) return Address(0);
    QString text = addressEdit_->text().trimmed();
    if (text.isEmpty()) {
        return session_->registers().rsp();
    }

    std::string token = text.toStdString();
    // Try expression evaluation (e.g. rsp, rbp - 0x20)
    auto res = ExpressionEvaluator::evaluate(token, session_->registers(), nullptr);
    if (res.has_value()) {
        return Address(*res);
    }

    // Try hex or decimal
    uint64_t val = 0;
    if (token.rfind("0x", 0) == 0 || token.rfind("0X", 0) == 0) {
        val = std::strtoull(token.c_str(), nullptr, 16);
    } else {
        val = std::strtoull(token.c_str(), nullptr, 0);
    }
    return Address(val);
}

void TypeViewer::onStructSelected(int) {
    if (!session_) return;
    QString cur = structCombo_->currentText();
    if (cur.isEmpty()) {
        sizeLabel_->setText("");
        return;
    }
    const auto* def = session_->typeManager().findStruct(cur.toStdString());
    if (def) {
        sizeLabel_->setText(QString("Total Size: %1 bytes (0x%2) | Alignment: %3 bytes | Fields: %4")
                                .arg(def->totalSize)
                                .arg(def->totalSize, 0, 16)
                                .arg(def->alignment)
                                .arg(def->fields.size()));
    }
}

void TypeViewer::onInspectClicked() {
    if (!session_) return;
    QString structName = structCombo_->currentText();
    if (structName.isEmpty()) return;

    Address addr = parseAddressInput();
    if (addr.isNull()) {
        statusTip();
    }

    currentEvaluation_ = session_->typeManager().evaluate(structName.toStdString(), addr, session_->engine());
    refresh();
}

void TypeViewer::refresh() {
    fieldTable_->setRowCount(0);
    if (!session_ || !currentEvaluation_.has_value()) return;

    const auto& evaluated = *currentEvaluation_;
    fieldTable_->setRowCount(static_cast<int>(evaluated.fields.size()));

    QFont monoFont("Monospace");

    for (size_t i = 0; i < evaluated.fields.size(); ++i) {
        const auto& f = evaluated.fields[i];

        // Col 0: Offset (+0x00)
        std::ostringstream offOss;
        offOss << "+0x" << std::hex << std::setw(2) << std::setfill('0') << f.offset
               << " (" << std::dec << f.offset << ")";
        auto* offItem = new QTableWidgetItem(QString::fromStdString(offOss.str()));
        offItem->setForeground(QColor(148, 163, 184));
        offItem->setFont(monoFont);
        fieldTable_->setItem(static_cast<int>(i), 0, offItem);

        // Col 1: Field Name
        auto* nameItem = new QTableWidgetItem(QString::fromStdString(f.name));
        nameItem->setFont(QFont(font().family(), -1, QFont::Bold));
        nameItem->setForeground(QColor(241, 245, 249));
        fieldTable_->setItem(static_cast<int>(i), 1, nameItem);

        // Col 2: Type
        auto* typeItem = new QTableWidgetItem(QString::fromStdString(f.typeName));
        typeItem->setForeground(QColor(56, 189, 248)); // Cyan
        fieldTable_->setItem(static_cast<int>(i), 2, typeItem);

        // Col 3: Size
        auto* sizeItem = new QTableWidgetItem(QString("%1 B").arg(f.size));
        sizeItem->setForeground(QColor(156, 163, 175));
        fieldTable_->setItem(static_cast<int>(i), 3, sizeItem);

        // Col 4: Raw Hex
        std::ostringstream rawOss;
        for (size_t b = 0; b < f.rawBytes.size() && b < 8; ++b) {
            if (b > 0) rawOss << " ";
            rawOss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(f.rawBytes[b]);
        }
        if (f.rawBytes.size() > 8) rawOss << " ...";
        auto* rawItem = new QTableWidgetItem(QString::fromStdString(rawOss.str()));
        rawItem->setFont(monoFont);
        rawItem->setForeground(QColor(203, 213, 225));
        fieldTable_->setItem(static_cast<int>(i), 4, rawItem);

        // Col 5: Value / Dereference
        auto* valItem = new QTableWidgetItem(QString::fromStdString(f.formattedValue));
        valItem->setFont(monoFont);
        if (f.pointerTarget != 0) {
            valItem->setForeground(QColor(125, 211, 252)); // Light blue pointer
        } else {
            valItem->setForeground(QColor(253, 230, 138)); // Warm amber
        }
        fieldTable_->setItem(static_cast<int>(i), 5, valItem);
    }
}

void TypeViewer::onTableDoubleClicked(int row, int) {
    if (!currentEvaluation_.has_value() || row < 0 || row >= static_cast<int>(currentEvaluation_->fields.size())) return;
    const auto& f = currentEvaluation_->fields[row];

    if (f.pointerTarget != 0) {
        Q_EMIT jumpToMemoryRequested(Address(f.pointerTarget));
    } else {
        Address fieldAddr = currentEvaluation_->baseAddress + f.offset;
        Q_EMIT jumpToMemoryRequested(fieldAddr);
    }
}

void TypeViewer::onTableContextMenu(const QPoint& pos) {
    int row = fieldTable_->rowAt(pos.y());
    if (row < 0 || !currentEvaluation_.has_value() || row >= static_cast<int>(currentEvaluation_->fields.size())) return;

    const auto& f = currentEvaluation_->fields[row];
    Address fieldAddr = currentEvaluation_->baseAddress + f.offset;

    QMenu menu(this);
    auto* actFieldHex = menu.addAction(QString("Follow Field in Hex Dump (%1)").arg(QString::fromStdString(fieldAddr.toHex())));
    auto* actFieldDisasm = menu.addAction("Follow Field in Disassembly");

    QAction* actPtrHex = nullptr;
    if (f.pointerTarget != 0) {
        actPtrHex = menu.addAction(QString("Follow Pointer Target in Hex Dump (0x%1)").arg(f.pointerTarget, 0, 16));
    }

    menu.addSeparator();
    auto* actCopyAddr = menu.addAction("Copy Field Address");
    auto* actCopyVal = menu.addAction("Copy Formatted Value");
    menu.addSeparator();
    auto* actEdit = menu.addAction("Edit Field Value...");

    QAction* selected = menu.exec(fieldTable_->viewport()->mapToGlobal(pos));
    if (selected == actFieldHex) {
        Q_EMIT jumpToMemoryRequested(fieldAddr);
    } else if (selected == actFieldDisasm) {
        Q_EMIT jumpToDisassemblyRequested(fieldAddr);
    } else if (selected == actPtrHex && f.pointerTarget != 0) {
        Q_EMIT jumpToMemoryRequested(Address(f.pointerTarget));
    } else if (selected == actCopyAddr) {
        QGuiApplication::clipboard()->setText(QString::fromStdString(fieldAddr.toHex()));
    } else if (selected == actCopyVal) {
        QGuiApplication::clipboard()->setText(QString::fromStdString(f.formattedValue));
    } else if (selected == actEdit) {
        editSelectedField();
    }
}

void TypeViewer::editSelectedField() {
    int row = fieldTable_->currentRow();
    if (row < 0 || !currentEvaluation_.has_value() || row >= static_cast<int>(currentEvaluation_->fields.size()) || !session_) return;

    const auto& f = currentEvaluation_->fields[row];
    Address fieldAddr = currentEvaluation_->baseAddress + f.offset;

    bool ok = false;
    QString newValStr = QInputDialog::getText(
        this, "Edit Field Value",
        QString("Enter new value for %1 (%2 at %3):")
            .arg(QString::fromStdString(f.name))
            .arg(QString::fromStdString(f.typeName))
            .arg(QString::fromStdString(fieldAddr.toHex())),
        QLineEdit::Normal, QString::fromStdString(f.formattedValue), &ok);

    if (!ok || newValStr.isEmpty()) return;

    std::vector<uint8_t> bytes(f.size, 0);
    std::string valStr = newValStr.toStdString();

    if (f.typeName.find("float") != std::string::npos) {
        float flt = newValStr.toFloat();
        std::memcpy(bytes.data(), &flt, sizeof(float));
    } else if (f.typeName.find("double") != std::string::npos) {
        double dbl = newValStr.toDouble();
        std::memcpy(bytes.data(), &dbl, sizeof(double));
    } else if (f.typeName.find("char[") != std::string::npos) {
        std::string s = newValStr.toStdString();
        size_t copyLen = std::min(s.size(), f.size - 1);
        std::memcpy(bytes.data(), s.data(), copyLen);
    } else {
        bool numOk = false;
        int64_t intVal = 0;
        if (newValStr.startsWith("0x", Qt::CaseInsensitive)) {
            intVal = static_cast<int64_t>(newValStr.toULongLong(&numOk, 16));
        } else {
            intVal = newValStr.toLongLong(&numOk, 10);
        }
        if (numOk) {
            std::memcpy(bytes.data(), &intVal, std::min<size_t>(f.size, sizeof(int64_t)));
        }
    }

    if (session_->writeMemory(fieldAddr, bytes.data(), bytes.size())) {
        onInspectClicked();
    } else {
        QMessageBox::critical(this, "Error", "Failed to write memory to target process!");
    }
}

void TypeViewer::onDefineStructClicked() {
    if (!session_) return;

    QDialog dlg(this);
    dlg.setWindowTitle("Define C Struct");
    dlg.resize(550, 420);

    auto* layout = new QVBoxLayout(&dlg);
    auto* infoLabel = new QLabel("Enter C struct declaration below. Natural AMD64 ABI alignment will be computed:", &dlg);
    infoLabel->setStyleSheet("color: #94a3b8;");

    auto* edit = new QPlainTextEdit(&dlg);
    edit->setFont(QFont("Monospace", 10));
    edit->setPlainText(
        "struct CustomTask {\n"
        "    int id;\n"
        "    char name[32];\n"
        "    double priority;\n"
        "    void* pNext;\n"
        "};"
    );

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    layout->addWidget(infoLabel);
    layout->addWidget(edit, 1);
    layout->addWidget(buttonBox);

    if (dlg.exec() == QDialog::Accepted) {
        std::string code = edit->toPlainText().toStdString();
        std::string err;
        if (session_->typeManager().parseAndRegister(code, &err)) {
            updateStructList();
            auto parsed = TypeManager::parseCStruct(code);
            if (parsed.has_value()) {
                structCombo_->setCurrentText(QString::fromStdString(parsed->name));
            }
        } else {
            QMessageBox::warning(this, "Parse Error", QString("Failed to parse struct:\n%1").arg(QString::fromStdString(err)));
        }
    }
}

} // namespace edb_next
