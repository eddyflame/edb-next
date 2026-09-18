#include "RegisterView.hpp"
#include "core/ConfigurationManager.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFontDatabase>
#include <QInputDialog>
#include <QColor>
#include <QLabel>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <QKeyEvent>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <bit>

namespace edb_next {

RegisterView::RegisterView(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void RegisterView::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(4);

    // Flags Bar
    auto* flagsLayout = new QHBoxLayout();
    flagsLayout->setSpacing(2);

    auto* flagsLabel = new QLabel("FLAGS:", this);
    QFont labelFont = flagsLabel->font();
    labelFont.setBold(true);
    flagsLabel->setFont(labelFont);
    flagsLayout->addWidget(flagsLabel);

    // Standard x86 EFLAGS: CF(0), PF(2), AF(4), ZF(6), SF(7), TF(8), IF(9), DF(10), OF(11)
    const std::vector<std::pair<int, QString>> flags = {
        {0, "CF"},
        {2, "PF"},
        {4, "AF"},
        {6, "ZF"},
        {7, "SF"},
        {8, "TF"},
        {9, "IF"},
        {10, "DF"},
        {11, "OF"}
    };

    for (const auto& [bit, name] : flags) {
        auto* btn = new QPushButton(name + " 0", this);
        btn->setMaximumWidth(46);
        btn->setMaximumHeight(22);
        btn->setFocusPolicy(Qt::StrongFocus);
        QFont btnFont = btn->font();
        btnFont.setPointSize(8);
        btnFont.setBold(true);
        btn->setFont(btnFont);
        btn->setToolTip(QString("Click to toggle %1 flag (Bit %2)").arg(name).arg(bit));

        connect(btn, &QPushButton::clicked, this, [this, bit]() {
            handleFlagClicked(bit);
        });

        flagButtons_.emplace_back(bit, btn);
        flagsLayout->addWidget(btn);
    }
    flagsLayout->addStretch(1);
    mainLayout->addLayout(flagsLayout);

    // Tab Widget for GPR vs FP
    tabWidget_ = new QTabWidget(this);
    tabWidget_->setTabPosition(QTabWidget::North);

    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(9);

    // GPR Table
    gprTable_ = new QTableWidget(this);
    gprTable_->setObjectName("gprTable");
    gprTable_->setColumnCount(4);
    gprTable_->setHorizontalHeaderLabels({"Register", "Hex Value", "Comment / Dereference", "Decimal"});
    gprTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    gprTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    gprTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    gprTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    gprTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    gprTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    gprTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    gprTable_->verticalHeader()->setVisible(false);
    gprTable_->setShowGrid(true);
    gprTable_->setFont(monoFont);
    gprTable_->installEventFilter(this);

    tabWidget_->addTab(gprTable_, "General (GPR)");

    // FP / SSE Table
    fpTable_ = new QTableWidget(this);
    fpTable_->setObjectName("fpTable");
    fpTable_->setColumnCount(4);
    fpTable_->setHorizontalHeaderLabels({"Register", "Hex (128-bit)", "4x Float", "2x Double"});
    fpTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    fpTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    fpTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    fpTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    fpTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    fpTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    fpTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    fpTable_->verticalHeader()->setVisible(false);
    fpTable_->setShowGrid(true);
    fpTable_->setFont(monoFont);
    fpTable_->installEventFilter(this);

    tabWidget_->addTab(fpTable_, "FPU / SSE (XMM)");

    mainLayout->addWidget(tabWidget_, 1);

    gprTable_->setFont(ConfigurationManager::instance().appearance().registerFont);
    fpTable_->setFont(ConfigurationManager::instance().appearance().registerFont);

    connect(&ConfigurationManager::instance(), &ConfigurationManager::configurationChanged, this, [this]() {
        gprTable_->setFont(ConfigurationManager::instance().appearance().registerFont);
        fpTable_->setFont(ConfigurationManager::instance().appearance().registerFont);
        refresh();
    });

    connect(gprTable_, &QTableWidget::cellDoubleClicked, this, &RegisterView::handleGprDoubleClicked);
    connect(fpTable_, &QTableWidget::cellDoubleClicked, this, &RegisterView::handleFpDoubleClicked);

    gprTable_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(gprTable_, &QWidget::customContextMenuRequested, this, &RegisterView::handleGprContextMenu);
}

void RegisterView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
    refresh();
}

void RegisterView::refresh() {
    updateFlagsDisplay();
    updateGprDisplay();
    updateFpDisplay();
}

void RegisterView::updateFlagsDisplay() {
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) {
        for (auto& [bit, btn] : flagButtons_) {
            btn->setText(btn->text().left(2) + " 0");
            btn->setStyleSheet("");
        }
        return;
    }

    const auto& regs = session->registers();
    for (auto& [bit, btn] : flagButtons_) {
        bool set = (regs.raw().eflags & (1ULL << bit)) != 0;
        QString name = btn->text().left(2);
        btn->setText(name + (set ? " 1" : " 0"));
        if (set) {
            btn->setStyleSheet("QPushButton { background-color: #2e7d32; color: white; border-radius: 2px; }");
        } else {
            btn->setStyleSheet("QPushButton { color: #888888; }");
        }
    }
}

void RegisterView::handleFlagClicked(int bit) {
    auto session = session_.lock();
    if (!session || session->state() != SessionState::Paused) return;

    auto regs = session->registers();
    regs.toggleFlag(bit);
    session->setRegisters(regs);
    refresh();
}

void RegisterView::updateGprDisplay() {
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) {
        gprTable_->setRowCount(0);
        return;
    }

    const auto& cur_regs = session->registers();
    const auto& prev_regs = session->previousRegisters();
    auto reg_items = cur_regs.toList(&prev_regs);

    gprTable_->setRowCount(static_cast<int>(reg_items.size()));

    for (int r = 0; r < static_cast<int>(reg_items.size()); ++r) {
        const auto& item_data = reg_items[r];

        auto* item_name = new QTableWidgetItem(QString::fromStdString(item_data.name));
        item_name->setFont(gprTable_->font());

        std::ostringstream hex_oss;
        hex_oss << "0x" << std::hex << std::setw(16) << std::setfill('0') << item_data.value;
        auto* item_hex = new QTableWidgetItem(QString::fromStdString(hex_oss.str()));

        // Smart Dereference & Comment Analysis (x64dbg-style)
        QString comment;
        uint64_t val = item_data.value;

        // 1. Symbol matching
        if (auto symOpt = session->symbols().findNearestSymbol(Address(val))) {
            const std::string& sname = symOpt->first.displayName();
            if (symOpt->second == 0) {
                comment = QString("<%1>").arg(QString::fromStdString(sname));
            } else if (symOpt->second < 0x2000) {
                comment = QString("<%1+0x%2>").arg(QString::fromStdString(sname)).arg(symOpt->second, 0, 16);
            }
        }

        // 2. Stack pointer proximity
        uint64_t rsp_val = cur_regs.rsp().value();
        uint64_t rbp_val = cur_regs.rbp().value();
        if (val == rsp_val) {
            comment = "=> [RSP] (Stack Top)";
        } else if (val == rbp_val) {
            comment = "=> [RBP] (Base Pointer)";
        } else if (val > rsp_val && val < rsp_val + 0x4000) {
            if (comment.isEmpty()) {
                comment = QString("[RSP+0x%1]").arg(val - rsp_val, 0, 16);
            } else {
                comment = QString("[RSP+0x%1] %2").arg(val - rsp_val, 0, 16).arg(comment);
            }
        }

        // 3. Dereference pointer / string peek
        if (comment.isEmpty() && val >= 0x10000 && val < 0x0000800000000000ULL) {
            auto mem = session->readMemory(Address(val), 32);
            if (mem.size() >= 4) {
                bool is_str = true;
                int str_len = 0;
                for (size_t i = 0; i < mem.size(); ++i) {
                    if (mem[i] == 0) break;
                    if (mem[i] < 0x20 || mem[i] > 0x7E) {
                        is_str = false;
                        break;
                    }
                    str_len++;
                }
                if (is_str && str_len >= 3) {
                    QString s = QString::fromLatin1(reinterpret_cast<const char*>(mem.data()), str_len);
                    comment = QString("\"%1\"").arg(s);
                } else if (mem.size() >= 8) {
                    uint64_t deref_val = 0;
                    std::memcpy(&deref_val, mem.data(), 8);
                    if (deref_val >= 0x10000 && deref_val < 0x0000800000000000ULL) {
                        std::ostringstream ss;
                        ss << "-> 0x" << std::hex << std::setw(16) << std::setfill('0') << deref_val;
                        if (auto dSym = session->symbols().findNearestSymbol(Address(deref_val))) {
                            if (dSym->second < 0x2000) {
                                ss << " <" << dSym->first.displayName();
                                if (dSym->second > 0) ss << "+0x" << std::hex << dSym->second;
                                ss << ">";
                            }
                        }
                        comment = QString::fromStdString(ss.str());
                    }
                }
            }
        }

        auto* item_comment = new QTableWidgetItem(comment);
        item_comment->setForeground(QColor(100, 200, 160));
        item_comment->setFont(gprTable_->font());

        auto* item_dec = new QTableWidgetItem(QString::number(item_data.value));

        if (item_data.modified && ConfigurationManager::instance().appearance().highlightChangedRegisters) {
            QColor change_color(230, 80, 80);
            item_name->setForeground(change_color);
            item_hex->setForeground(change_color);
            item_dec->setForeground(change_color);

            QFont bold_font = gprTable_->font();
            bold_font.setBold(true);
            item_name->setFont(bold_font);
            item_hex->setFont(bold_font);
        }

        gprTable_->setItem(r, 0, item_name);
        gprTable_->setItem(r, 1, item_hex);
        gprTable_->setItem(r, 2, item_comment);
        gprTable_->setItem(r, 3, item_dec);
    }
}

void RegisterView::updateFpDisplay() {
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) {
        fpTable_->setRowCount(0);
        return;
    }

    const auto& fp = session->fpRegisters();

    // 16 XMM registers + MXCSR
    fpTable_->setRowCount(17);

    for (int i = 0; i < 16; ++i) {
        QString name = QString("XMM%1").arg(i);
        auto* itemName = new QTableWidgetItem(name);
        itemName->setFont(fpTable_->font());

        // 16 bytes for this XMM register
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&fp.xmm_space[i * 4]);

        // Hex string (in little endian display or reverse)
        std::ostringstream hexOss;
        for (int b = 15; b >= 0; --b) {
            hexOss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[b]);
            if (b > 0) hexOss << " ";
        }
        auto* itemHex = new QTableWidgetItem(QString::fromStdString(hexOss.str()));
        itemHex->setFont(fpTable_->font());

        // 4x float
        uint32_t u32[4];
        std::memcpy(u32, bytes, sizeof(u32));
        QString floatStr = QString("[%1, %2, %3, %4]")
            .arg(std::bit_cast<float>(u32[0]), 0, 'g', 4)
            .arg(std::bit_cast<float>(u32[1]), 0, 'g', 4)
            .arg(std::bit_cast<float>(u32[2]), 0, 'g', 4)
            .arg(std::bit_cast<float>(u32[3]), 0, 'g', 4);
        auto* itemFloat = new QTableWidgetItem(floatStr);

        // 2x double
        uint64_t u64[2];
        std::memcpy(u64, bytes, sizeof(u64));
        QString doubleStr = QString("[%1, %2]")
            .arg(std::bit_cast<double>(u64[0]), 0, 'g', 6)
            .arg(std::bit_cast<double>(u64[1]), 0, 'g', 6);
        auto* itemDouble = new QTableWidgetItem(doubleStr);

        fpTable_->setItem(i, 0, itemName);
        fpTable_->setItem(i, 1, itemHex);
        fpTable_->setItem(i, 2, itemFloat);
        fpTable_->setItem(i, 3, itemDouble);
    }

    // Row 16: MXCSR
    auto* itemMxcsrName = new QTableWidgetItem("MXCSR");
    itemMxcsrName->setFont(fpTable_->font());

    std::ostringstream mxcsrOss;
    mxcsrOss << "0x" << std::hex << std::setw(8) << std::setfill('0') << fp.mxcsr;
    auto* itemMxcsrHex = new QTableWidgetItem(QString::fromStdString(mxcsrOss.str()));
    itemMxcsrHex->setFont(fpTable_->font());

    auto* itemMxcsrEmpty1 = new QTableWidgetItem("");
    auto* itemMxcsrEmpty2 = new QTableWidgetItem("");

    fpTable_->setItem(16, 0, itemMxcsrName);
    fpTable_->setItem(16, 1, itemMxcsrHex);
    fpTable_->setItem(16, 2, itemMxcsrEmpty1);
    fpTable_->setItem(16, 3, itemMxcsrEmpty2);
}

void RegisterView::handleGprDoubleClicked(int row, int /*col*/) {
    auto session = session_.lock();
    if (!session || session->state() != SessionState::Paused) return;

    QString reg_name = gprTable_->item(row, 0)->text();
    QString cur_hex = gprTable_->item(row, 1)->text();

    bool ok = false;
    QString text = QInputDialog::getText(
        this,
        "Modify Register " + reg_name,
        "New Hex Value:",
        QLineEdit::Normal,
        cur_hex,
        &ok
    );

    if (ok && !text.isEmpty()) {
        bool conv_ok = false;
        uint64_t new_val = text.toULongLong(&conv_ok, 16);
        if (conv_ok) {
            auto regs = session->registers();
            if (reg_name == "RAX") regs.raw().rax = new_val;
            else if (reg_name == "RBX") regs.raw().rbx = new_val;
            else if (reg_name == "RCX") regs.raw().rcx = new_val;
            else if (reg_name == "RDX") regs.raw().rdx = new_val;
            else if (reg_name == "RSI") regs.raw().rsi = new_val;
            else if (reg_name == "RDI") regs.raw().rdi = new_val;
            else if (reg_name == "RBP") regs.raw().rbp = new_val;
            else if (reg_name == "RSP") regs.raw().rsp = new_val;
            else if (reg_name == "RIP") regs.raw().rip = new_val;
            else if (reg_name == "R8")  regs.raw().r8  = new_val;
            else if (reg_name == "R9")  regs.raw().r9  = new_val;
            else if (reg_name == "R10") regs.raw().r10 = new_val;
            else if (reg_name == "R11") regs.raw().r11 = new_val;
            else if (reg_name == "R12") regs.raw().r12 = new_val;
            else if (reg_name == "R13") regs.raw().r13 = new_val;
            else if (reg_name == "R14") regs.raw().r14 = new_val;
            else if (reg_name == "R15") regs.raw().r15 = new_val;
            else if (reg_name == "RFLAGS") regs.raw().eflags = new_val;

            session->setRegisters(regs);
            refresh();
        }
    }
}

void RegisterView::handleFpDoubleClicked(int row, int /*col*/) {
    auto session = session_.lock();
    if (!session || session->state() != SessionState::Paused) return;

    if (row < 0 || row >= 16) return;

    QString reg_name = fpTable_->item(row, 0)->text();
    QString cur_hex = fpTable_->item(row, 1)->text();

    bool ok = false;
    QString text = QInputDialog::getText(
        this,
        "Modify SSE " + reg_name,
        "16 Hex Bytes (e.g. 00 11 22 ...):",
        QLineEdit::Normal,
        cur_hex,
        &ok
    );

    if (ok && !text.isEmpty()) {
        QString clean = text.remove(' ');
        if (clean.length() == 32) {
            auto fp = session->fpRegisters();
            uint8_t* dst = reinterpret_cast<uint8_t*>(&fp.xmm_space[row * 4]);
            for (int i = 0; i < 16; ++i) {
                QString byteStr = clean.mid(i * 2, 2);
                bool bOk = false;
                dst[15 - i] = static_cast<uint8_t>(byteStr.toUInt(&bOk, 16));
            }
            session->setFpRegisters(fp);
            refresh();
        }
    }
}

namespace {

static void setNamedGpr(RegisterContext& regs, const QString& regName, uint64_t newVal) {
    if (regName == "RAX") regs.raw().rax = newVal;
    else if (regName == "RBX") regs.raw().rbx = newVal;
    else if (regName == "RCX") regs.raw().rcx = newVal;
    else if (regName == "RDX") regs.raw().rdx = newVal;
    else if (regName == "RSI") regs.raw().rsi = newVal;
    else if (regName == "RDI") regs.raw().rdi = newVal;
    else if (regName == "RBP") regs.raw().rbp = newVal;
    else if (regName == "RSP") regs.raw().rsp = newVal;
    else if (regName == "RIP") regs.raw().rip = newVal;
    else if (regName == "R8")  regs.raw().r8 = newVal;
    else if (regName == "R9")  regs.raw().r9 = newVal;
    else if (regName == "R10") regs.raw().r10 = newVal;
    else if (regName == "R11") regs.raw().r11 = newVal;
    else if (regName == "R12") regs.raw().r12 = newVal;
    else if (regName == "R13") regs.raw().r13 = newVal;
    else if (regName == "R14") regs.raw().r14 = newVal;
    else if (regName == "R15") regs.raw().r15 = newVal;
}

} // namespace

void RegisterView::adjustSelectedGpr(int row, int64_t delta) {
    if (row < 0 || row >= gprTable_->rowCount()) return;
    auto s = session_.lock();
    if (!s || s->state() == SessionState::Stopped) return;

    QString regName = gprTable_->item(row, 0)->text();
    QString curHex = gprTable_->item(row, 1)->text();
    bool convOk = false;
    uint64_t val = curHex.toULongLong(&convOk, 16);
    if (!convOk) return;

    auto regs = s->registers();
    setNamedGpr(regs, regName, val + delta);
    s->setRegisters(regs);
    refresh();
}

void RegisterView::setSelectedGpr(int row, uint64_t val) {
    if (row < 0 || row >= gprTable_->rowCount()) return;
    auto s = session_.lock();
    if (!s || s->state() == SessionState::Stopped) return;

    QString regName = gprTable_->item(row, 0)->text();
    auto regs = s->registers();
    setNamedGpr(regs, regName, val);
    s->setRegisters(regs);
    refresh();
}

void RegisterView::toggleSelectedGpr(int row) {
    if (row < 0 || row >= gprTable_->rowCount()) return;
    auto s = session_.lock();
    if (!s || s->state() == SessionState::Stopped) return;

    QString regName = gprTable_->item(row, 0)->text();
    QString curHex = gprTable_->item(row, 1)->text();
    bool convOk = false;
    uint64_t val = curHex.toULongLong(&convOk, 16);
    if (!convOk) return;

    auto regs = s->registers();
    setNamedGpr(regs, regName, ~val);
    s->setRegisters(regs);
    refresh();
}

bool RegisterView::eventFilter(QObject* watched, QEvent* event) {
    if (watched == gprTable_ && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        int row = gprTable_->currentRow();
        if (row >= 0 && row < gprTable_->rowCount()) {
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                handleGprDoubleClicked(row, 1);
                return true;
            } else if (keyEvent->key() == Qt::Key_Plus || keyEvent->text() == "+") {
                adjustSelectedGpr(row, 1);
                return true;
            } else if (keyEvent->key() == Qt::Key_Minus || keyEvent->text() == "-") {
                adjustSelectedGpr(row, -1);
                return true;
            } else if (keyEvent->key() == Qt::Key_0) {
                setSelectedGpr(row, 0);
                return true;
            } else if (keyEvent->key() == Qt::Key_AsciiTilde || keyEvent->text() == "~") {
                toggleSelectedGpr(row);
                return true;
            } else if (keyEvent->key() == Qt::Key_Space) {
                handleGprDoubleClicked(row, 1);
                return true;
            }
        }
    } else if (watched == fpTable_ && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        int row = fpTable_->currentRow();
        if (row >= 0 && row < fpTable_->rowCount()) {
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Space) {
                handleFpDoubleClicked(row, 1);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void RegisterView::handleGprContextMenu(const QPoint& pos) {
    int row = gprTable_->currentRow();
    if (row < 0 || row >= gprTable_->rowCount()) return;

    QString regName = gprTable_->item(row, 0)->text();
    QString curHex = gprTable_->item(row, 1)->text();
    bool convOk = false;
    uint64_t val = 0;
    if (curHex.startsWith("0x", Qt::CaseInsensitive)) {
        val = curHex.toULongLong(&convOk, 16);
    } else {
        val = curHex.toULongLong(&convOk, 16);
    }
    Address regAddr(val);

    QMenu menu(this);
    menu.addAction(QString("Follow %1 (%2) in Disassembly").arg(regName, QString::fromStdString(regAddr.toHex())), [this, regAddr]() {
        Q_EMIT jumpToDisassemblyRequested(regAddr);
    });

    auto* dumpSub = menu.addMenu(QString("Follow %1 (%2) in Dump").arg(regName, QString::fromStdString(regAddr.toHex())));
    for (int d = 0; d < 4; ++d) {
        dumpSub->addAction(QString("Dump %1").arg(d + 1), [this, regAddr, d]() {
            Q_EMIT jumpToMemoryRequested(regAddr, d);
        });
    }

    menu.addAction(QString("Follow %1 (%2) in Stack").arg(regName, QString::fromStdString(regAddr.toHex())), [this, regAddr]() {
        Q_EMIT jumpToStackRequested(regAddr);
    });

    menu.addSeparator();

    menu.addAction("Modify Value... (Enter)", [this, row]() {
        handleGprDoubleClicked(row, 1);
    });

    menu.addAction("Increment (+1 / +)", [this, row]() {
        adjustSelectedGpr(row, 1);
    });

    menu.addAction("Decrement (-1 / -)", [this, row]() {
        adjustSelectedGpr(row, -1);
    });

    menu.addAction("Zero Register (0)", [this, row]() {
        setSelectedGpr(row, 0);
    });

    menu.addAction("Toggle Value (~)", [this, row]() {
        toggleSelectedGpr(row);
    });

    menu.addSeparator();

    auto* copyMenu = menu.addMenu("Copy");
    copyMenu->addAction("Copy Hex Value", [curHex]() {
        QApplication::clipboard()->setText(curHex);
    });
    copyMenu->addAction("Copy Unsigned Decimal", [val]() {
        QApplication::clipboard()->setText(QString::number(val));
    });
    copyMenu->addAction("Copy Signed Decimal", [val]() {
        QApplication::clipboard()->setText(QString::number(static_cast<int64_t>(val)));
    });
    copyMenu->addAction("Copy Register Name", [regName]() {
        QApplication::clipboard()->setText(regName);
    });

    menu.exec(gprTable_->mapToGlobal(pos));
}

} // namespace edb_next
