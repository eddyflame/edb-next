#include "StackView.hpp"
#include "core/ConfigurationManager.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QKeyEvent>
#include <QShortcut>
#include <cctype>

namespace edb_next {

StackView::StackView(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void StackView::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // Top Header Bar
    auto* top_bar = new QWidget(this);
    auto* top_layout = new QHBoxLayout(top_bar);
    top_layout->setContentsMargins(4, 2, 4, 2);
    top_layout->setSpacing(4);

    headerLabel_ = new QLabel("STACK [RSP: -]", top_bar);
    headerLabel_->setStyleSheet("font-weight: bold; color: #4dc2ff; font-family: monospace; font-size: 11px;");

    btnSyncRsp_ = new QPushButton("RSP", top_bar);
    btnSyncRsp_->setToolTip("Sync stack view to current RSP");
    btnSyncRsp_->setFixedWidth(42);
    btnSyncRsp_->setStyleSheet("padding: 2px 4px; font-size: 10px; font-family: monospace; font-weight: bold;");

    btnSyncRbp_ = new QPushButton("RBP", top_bar);
    btnSyncRbp_->setToolTip("Sync stack view to current RBP");
    btnSyncRbp_->setFixedWidth(42);
    btnSyncRbp_->setStyleSheet("padding: 2px 4px; font-size: 10px; font-family: monospace; font-weight: bold;");

    btnGoto_ = new QPushButton("Go to...", top_bar);
    btnGoto_->setToolTip("Jump stack to custom virtual address");
    btnGoto_->setFixedWidth(54);
    btnGoto_->setStyleSheet("padding: 2px 4px; font-size: 10px; font-family: monospace;");

    connect(btnSyncRsp_, &QPushButton::clicked, this, &StackView::onSyncToRspClicked);
    connect(btnSyncRbp_, &QPushButton::clicked, this, &StackView::onSyncToRbpClicked);
    connect(btnGoto_, &QPushButton::clicked, this, &StackView::onGotoAddressClicked);

    top_layout->addWidget(headerLabel_, 1);
    top_layout->addWidget(btnSyncRsp_);
    top_layout->addWidget(btnSyncRbp_);
    top_layout->addWidget(btnGoto_);

    layout->addWidget(top_bar);

    // Main Stack Table
    table_ = new QTableWidget(this);
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({"Address", "Value (QWORD)", "Offset", "Symbol / Comment"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(20);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);

    table_->setFont(ConfigurationManager::instance().appearance().stackFont);

    connect(&ConfigurationManager::instance(), &ConfigurationManager::configurationChanged, this, [this]() {
        table_->setFont(ConfigurationManager::instance().appearance().stackFont);
        refresh();
    });
    table_->horizontalHeader()->setFont(QFont("sans-serif", 8, QFont::Bold));

    table_->setColumnWidth(0, 145);
    table_->setColumnWidth(1, 145);
    table_->setColumnWidth(2, 90);

    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QTableWidget::cellDoubleClicked, this, &StackView::onCellDoubleClicked);
    connect(table_, &QTableWidget::customContextMenuRequested, this, &StackView::onCustomContextMenuRequested);

    table_->installEventFilter(this);
    auto* scGoto = new QShortcut(QKeySequence("Ctrl+G"), this, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
    connect(scGoto, &QShortcut::activated, this, &StackView::onGotoAddressClicked);

    layout->addWidget(table_, 1);
}

void StackView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
    refresh();
}

void StackView::setBaseAddress(Address addr) {
    baseAddr_ = addr;
    autoSyncRsp_ = false;
    refresh();
}

void StackView::onSyncToRspClicked() {
    autoSyncRsp_ = true;
    if (session_ && session_->state() != SessionState::Stopped) {
        baseAddr_ = session_->registers().rsp();
    }
    refresh();
}

void StackView::onSyncToRbpClicked() {
    autoSyncRsp_ = false;
    if (session_ && session_->state() != SessionState::Stopped) {
        baseAddr_ = session_->registers().rbp();
    }
    refresh();
}

void StackView::onGotoAddressClicked() {
    bool ok = false;
    QString current_hex = QString("0x%1").arg(static_cast<qulonglong>(baseAddr_.value()), 0, 16);
    QString text = QInputDialog::getText(this, "Go to Stack Address",
                                         "Virtual Address (Hex):", QLineEdit::Normal,
                                         current_hex, &ok);
    if (ok && !text.isEmpty()) {
        uint64_t addr = text.toULongLong(&ok, 16);
        if (ok) {
            setBaseAddress(Address(addr));
        }
    }
}

void StackView::refresh() {
    if (!session_ || session_->state() == SessionState::Stopped) {
        headerLabel_->setText("STACK [RSP: -]");
        table_->setRowCount(0);
        return;
    }

    Address cur_rsp = session_->registers().rsp();
    Address cur_rbp = session_->registers().rbp();

    if (autoSyncRsp_ || baseAddr_.isNull()) {
        baseAddr_ = cur_rsp;
    }

    headerLabel_->setText(QString("STACK [RSP: 0x%1 | RBP: 0x%2]")
                          .arg(static_cast<qulonglong>(cur_rsp.value()), 16, 16, QChar('0'))
                          .arg(static_cast<qulonglong>(cur_rbp.value()), 16, 16, QChar('0')));

    updateTable();
}

void StackView::updateTable() {
    if (!session_) return;

    Address cur_rsp = session_->registers().rsp();
    Address cur_rbp = session_->registers().rbp();
    auto regions = session_->memoryRegions();

    constexpr int kRowCount = 64; // Display 64 QWORDs (512 bytes of stack)
    table_->setRowCount(kRowCount);

    Address start_addr = baseAddr_;

    for (int i = 0; i < kRowCount; ++i) {
        Address saddr = start_addr + (i * 8);
        auto optVal = session_->read<uint64_t>(saddr);
        bool read_ok = optVal.has_value();
        uint64_t val = optVal.value_or(0);
        Address val_addr(val);

        // Check if value points to executable module memory following a call instruction (Return Address)
        bool is_return_addr = false;
        if (read_ok && val != 0) {
            for (const auto& r : regions) {
                if (r.contains(val_addr) && r.isExecutable()) {
                    if (val_addr.value() >= 7) {
                        auto preBytes = session_->readMemory(val_addr - 7, 7);
                        if (preBytes.size() == 7) {
                            // Direct call: E8 xx xx xx xx -> byte at offset 2 (val_addr - 5) is 0xE8
                            if (preBytes[2] == 0xe8) {
                                is_return_addr = true;
                            }
                            // Indirect call reg/mem: FF /2 -> byte at offset 5 is 0xFF and reg field is 2
                            else if (preBytes[5] == 0xff && ((preBytes[6] >> 3) & 7) == 2) {
                                is_return_addr = true;
                            }
                            // Indirect call with disp8: FF /2
                            else if (preBytes[4] == 0xff && ((preBytes[5] >> 3) & 7) == 2) {
                                is_return_addr = true;
                            }
                            // Indirect call rip-relative disp32: FF 15 xx xx xx xx -> byte at offset 1 is 0xFF
                            else if (preBytes[1] == 0xff && ((preBytes[2] >> 3) & 7) == 2) {
                                is_return_addr = true;
                            }
                        }
                    }
                    break;
                }
            }
        }

        // 1. Address Item
        auto* item_addr = new QTableWidgetItem(saddr.toQString(true, ConfigurationManager::instance().appearance().showAddressColon));
        item_addr->setForeground(QBrush(QColor("#70d0ff")));
        item_addr->setData(Qt::UserRole, QVariant::fromValue(static_cast<qulonglong>(saddr.value())));

        // 2. Value Item
        QString val_str = read_ok ? QString("0x%1").arg(static_cast<qulonglong>(val), 16, 16, QChar('0')) : "????????????????";
        auto* item_val = new QTableWidgetItem(val_str);
        if (read_ok) {
            if (is_return_addr) {
                item_val->setForeground(QBrush(QColor("#ffb74d"))); // Amber for return address
                QFont bfont = item_val->font();
                bfont.setBold(true);
                item_val->setFont(bfont);
            } else if (val == 0) {
                item_val->setForeground(QBrush(QColor("#777777")));
            } else {
                item_val->setForeground(QBrush(QColor("#50fa7b"))); // Emerald green
            }
            item_val->setData(Qt::UserRole, QVariant::fromValue(static_cast<qulonglong>(val)));
        } else {
            item_val->setForeground(QBrush(QColor("#ff5555")));
        }

        // 3. Offset Item
        QString offset_str;
        if (saddr == cur_rsp) {
            offset_str = "=> RSP";
        } else if (saddr > cur_rsp) {
            offset_str = QString("+0x%1").arg(static_cast<qulonglong>(saddr.value() - cur_rsp.value()), 0, 16).toUpper();
        } else {
            offset_str = QString("-0x%1").arg(static_cast<qulonglong>(cur_rsp.value() - saddr.value()), 0, 16).toUpper();
        }

        if (saddr == cur_rbp) {
            offset_str += " [RBP]";
        }

        auto* item_offset = new QTableWidgetItem(offset_str);
        if (saddr == cur_rsp) {
            item_offset->setForeground(QBrush(QColor("#ffb86c"))); // Amber
            item_offset->setFont(QFont("Monospace", 9, QFont::Bold));
        } else if (saddr == cur_rbp) {
            item_offset->setForeground(QBrush(QColor("#bd93f9"))); // Purple
            item_offset->setFont(QFont("Monospace", 9, QFont::Bold));
        } else {
            item_offset->setForeground(QBrush(QColor("#a0a0a0")));
        }

        // 4. Symbol / Comment Item
        QString comment_str;
        if (read_ok && val != 0) {
            auto sym_opt = session_->symbols().findNearestSymbol(Address(val));
            if (sym_opt && sym_opt->second < 0x10000) {
                const std::string& sname = sym_opt->first.displayName();
                if (sym_opt->second == 0) {
                    comment_str = QString::fromStdString(sname);
                } else {
                    comment_str = QString("%1+0x%2").arg(QString::fromStdString(sname))
                                                    .arg(sym_opt->second, 0, 16);
                }
            } else {
                // Try reading ASCII string preview
                auto str_bytes = session_->readMemory(Address(val), 24);
                if (!str_bytes.empty()) {
                    bool is_printable = true;
                    int plen = 0;
                    for (size_t c = 0; c < str_bytes.size() && str_bytes[c] != '\0'; ++c) {
                        if (!std::isprint(str_bytes[c])) {
                            is_printable = false;
                            break;
                        }
                        plen++;
                    }
                    if (is_printable && plen >= 3) {
                        std::string s(reinterpret_cast<const char*>(str_bytes.data()), plen);
                        comment_str = QString("\"%1\"").arg(QString::fromStdString(s));
                    }
                }
            }
        }

        if (is_return_addr) {
            if (comment_str.isEmpty()) {
                comment_str = "[Return Address]";
            } else {
                comment_str = QString("[Return Address] %1").arg(comment_str);
            }
        }

        auto* item_comment = new QTableWidgetItem(comment_str);
        if (is_return_addr) {
            item_comment->setForeground(QBrush(QColor("#ffb74d"))); // Amber
            QFont bfont = item_comment->font();
            bfont.setBold(true);
            item_comment->setFont(bfont);
        } else {
            item_comment->setForeground(QBrush(QColor("#8be9fd")));
        }

        // Highlight RSP row
        if (saddr == cur_rsp) {
            QColor rsp_bg(40, 60, 45);
            item_addr->setBackground(rsp_bg);
            item_val->setBackground(rsp_bg);
            item_offset->setBackground(rsp_bg);
            item_comment->setBackground(rsp_bg);
        }

        table_->setItem(i, 0, item_addr);
        table_->setItem(i, 1, item_val);
        table_->setItem(i, 2, item_offset);
        table_->setItem(i, 3, item_comment);
    }
}

void StackView::onCellDoubleClicked(int row, int column) {
    if (!session_) return;

    if (column == 0) {
        // Double clicked address -> Follow Address in Dump
        auto* item = table_->item(row, 0);
        if (item) {
            uint64_t raw_addr = item->data(Qt::UserRole).toULongLong();
            Q_EMIT jumpToMemoryRequested(Address(raw_addr));
        }
    } else if (column == 1 || column == 3) {
        // Double clicked value / comment
        auto* item = table_->item(row, 1);
        if (item) {
            uint64_t val = item->data(Qt::UserRole).toULongLong();
            if (val != 0) {
                Address target(val);
                auto insns = session_->disassemble(target, 1);
                if (!insns.empty() && insns[0].address == target) {
                    Q_EMIT jumpToDisassemblyRequested(target);
                } else {
                    Q_EMIT jumpToMemoryRequested(target);
                }
            }
        }
    }
}

void StackView::onCustomContextMenuRequested(const QPoint& pos) {
    int row = table_->rowAt(pos.y());
    if (row < 0) return;

    auto* item_addr = table_->item(row, 0);
    auto* item_val = table_->item(row, 1);
    if (!item_addr || !item_val) return;

    uint64_t saddr_val = item_addr->data(Qt::UserRole).toULongLong();
    uint64_t val = item_val->data(Qt::UserRole).toULongLong();

    Address saddr(saddr_val);
    Address val_addr(val);

    QMenu menu(this);

    auto* act_follow_disasm = menu.addAction("Follow Value in Disassembly");
    auto* act_follow_dump = menu.addAction("Follow Value in Dump");
    auto* act_follow_stack = menu.addAction("Follow Value in Stack");
    menu.addSeparator();
    auto* act_follow_saddr_dump = menu.addAction("Follow Stack Address in Dump");
    auto* act_follow_saddr_disasm = menu.addAction("Follow Stack Address in Disassembly");
    auto* act_modify_val = menu.addAction("Modify Stack Value (Space)...");
    menu.addSeparator();

    auto* copyMenu = menu.addMenu("Copy");
    copyMenu->addAction("Copy Address", [item_addr] {
        QApplication::clipboard()->setText(item_addr->text());
    });
    copyMenu->addAction("Copy Value (Hex)", [item_val] {
        QApplication::clipboard()->setText(item_val->text());
    });
    copyMenu->addAction("Copy Value (Unsigned Dec)", [val] {
        QApplication::clipboard()->setText(QString::number(val));
    });
    copyMenu->addAction("Copy Value (Signed Dec)", [val] {
        QApplication::clipboard()->setText(QString::number(static_cast<int64_t>(val)));
    });
    copyMenu->addAction("Copy Row", [this, row] {
        QString row_text = QString("%1\t%2\t%3\t%4")
                           .arg(table_->item(row, 0)->text())
                           .arg(table_->item(row, 1)->text())
                           .arg(table_->item(row, 2)->text())
                           .arg(table_->item(row, 3)->text());
        QApplication::clipboard()->setText(row_text);
    });
    menu.addSeparator();
    auto* act_sync_rsp = menu.addAction("Sync to RSP");
    auto* act_goto = menu.addAction("Go to Address... (Ctrl+G)");

    connect(act_follow_disasm, &QAction::triggered, this, [this, val_addr] {
        if (!val_addr.isNull()) Q_EMIT jumpToDisassemblyRequested(val_addr);
    });
    connect(act_follow_dump, &QAction::triggered, this, [this, val_addr] {
        if (!val_addr.isNull()) Q_EMIT jumpToMemoryRequested(val_addr);
    });
    connect(act_follow_stack, &QAction::triggered, this, [this, val_addr] {
        if (!val_addr.isNull()) {
            setBaseAddress(val_addr);
            Q_EMIT jumpToStackRequested(val_addr);
        }
    });
    connect(act_follow_saddr_dump, &QAction::triggered, this, [this, saddr] {
        Q_EMIT jumpToMemoryRequested(saddr);
    });
    connect(act_follow_saddr_disasm, &QAction::triggered, this, [this, saddr] {
        Q_EMIT jumpToDisassemblyRequested(saddr);
    });
    connect(act_modify_val, &QAction::triggered, this, &StackView::onModifyValueClicked);
    connect(act_sync_rsp, &QAction::triggered, this, &StackView::onSyncToRspClicked);
    connect(act_goto, &QAction::triggered, this, &StackView::onGotoAddressClicked);

    menu.exec(table_->viewport()->mapToGlobal(pos));
}

void StackView::onModifyValueClicked() {
    int row = table_->currentRow();
    if (row < 0 || !session_) return;

    auto* item_addr = table_->item(row, 0);
    auto* item_val = table_->item(row, 1);
    if (!item_addr || !item_val) return;

    uint64_t saddr_val = item_addr->data(Qt::UserRole).toULongLong();
    uint64_t current_val = item_val->data(Qt::UserRole).toULongLong();
    Address saddr(saddr_val);

    bool ok = false;
    QString current_hex = QString("0x%1").arg(static_cast<qulonglong>(current_val), 0, 16);
    QString text = QInputDialog::getText(this, "Modify Stack Value",
                                         QString("Enter new 64-bit Hex value for address 0x%1:")
                                         .arg(static_cast<qulonglong>(saddr.value()), 16, 16, QChar('0')),
                                         QLineEdit::Normal, current_hex, &ok);
    if (ok && !text.isEmpty()) {
        uint64_t new_val = text.toULongLong(&ok, 16);
        if (ok) {
            if (session_->writeMemory(saddr, &new_val, sizeof(new_val))) {
                refresh();
            } else {
                QMessageBox::warning(this, "Modify Stack Value", "Failed to write memory at target address.");
            }
        }
    }
}

bool StackView::eventFilter(QObject* watched, QEvent* event) {
    if (watched == table_ && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            int row = table_->currentRow();
            if (row >= 0) {
                onCellDoubleClicked(row, 1);
                return true;
            }
        } else if (keyEvent->key() == Qt::Key_Space) {
            onModifyValueClicked();
            return true;
        } else if ((keyEvent->modifiers() & Qt::ControlModifier) && keyEvent->key() == Qt::Key_G) {
            onGotoAddressClicked();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace edb_next
