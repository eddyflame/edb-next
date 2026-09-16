#include "MemoryHexView.hpp"
#include <QHeaderView>
#include <QFontDatabase>
#include <QInputDialog>
#include <QMessageBox>
#include <QMenu>
#include <QShortcut>
#include <QKeySequence>
#include <QFileDialog>
#include <QClipboard>
#include <QApplication>
#include <iomanip>
#include <sstream>
#include <cctype>

namespace edb_next {

MemoryHexView::MemoryHexView(QWidget* parent) : QTableWidget(parent) {
    setupUi();
}

void MemoryHexView::setupUi() {
    // Columns: Address (0), Hex 00..0F (1..16), ASCII (17)
    setColumnCount(18);

    QStringList headers;
    headers << "Address";
    for (int i = 0; i < 16; ++i) {
        std::ostringstream oss;
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << i;
        headers << QString::fromStdString(oss.str());
    }
    headers << "ASCII";
    setHorizontalHeaderLabels(headers);

    horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    for (int i = 1; i <= 16; ++i) {
        horizontalHeader()->setSectionResizeMode(i, QHeaderView::Fixed);
        setColumnWidth(i, 32);
    }
    horizontalHeader()->setSectionResizeMode(17, QHeaderView::Stretch);

    setSelectionBehavior(QAbstractItemView::SelectItems);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    verticalHeader()->setVisible(false);
    setShowGrid(false);

    QFont mono_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono_font.setPointSize(9);
    setFont(mono_font);

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &MemoryHexView::handleCustomContextMenu);

    auto* sc_goto = new QShortcut(QKeySequence("Ctrl+G"), this);
    connect(sc_goto, &QShortcut::activated, this, &MemoryHexView::gotoAddress);

    auto* sc_find = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(sc_find, &QShortcut::activated, this, &MemoryHexView::findPatternPrompt);

    auto* sc_edit = new QShortcut(QKeySequence("Ctrl+E"), this);
    connect(sc_edit, &QShortcut::activated, this, &MemoryHexView::editBytesPrompt);
}

void MemoryHexView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
    refresh();
}

void MemoryHexView::setBaseAddress(Address addr) {
    baseAddress_ = addr;
    refresh();
}

void MemoryHexView::refresh() {
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) {
        setRowCount(0);
        return;
    }

    if (baseAddress_.isNull()) {
        baseAddress_ = session->registers().rsp();
        if (baseAddress_.isNull()) {
            baseAddress_ = session->registers().rip();
        }
    }

    size_t total_bytes = rowCount_ * 16;
    auto mem_data = session->readMemory(baseAddress_, total_bytes);

    setRowCount(static_cast<int>(rowCount_));

    for (size_t r = 0; r < rowCount_; ++r) {
        Address row_addr = baseAddress_ + (r * 16);

        // Address
        auto* item_addr = new QTableWidgetItem(QString::fromStdString(row_addr.toHex()));
        item_addr->setForeground(QColor(100, 150, 200));
        setItem(static_cast<int>(r), 0, item_addr);

        // 16 Hex Bytes & ASCII buffer
        QString ascii_str;
        for (size_t c = 0; c < 16; ++c) {
            size_t idx = r * 16 + c;
            if (idx < mem_data.size()) {
                uint8_t byte_val = mem_data[idx];

                std::ostringstream oss;
                oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte_val);
                auto* item_byte = new QTableWidgetItem(QString::fromStdString(oss.str()));
                item_byte->setTextAlignment(Qt::AlignCenter);

                Address cell_addr = row_addr + c;
                if (session->hasBreakpoint(cell_addr)) {
                    item_byte->setBackground(QColor(160, 40, 40, 160));
                    item_byte->setForeground(Qt::white);
                    item_byte->setToolTip(QString("Breakpoint active at %1").arg(QString::fromStdString(cell_addr.toHex())));
                } else if (byte_val == 0) {
                    item_byte->setForeground(QColor(100, 100, 100));
                } else if (std::isprint(byte_val)) {
                    item_byte->setForeground(QColor(220, 220, 220));
                } else {
                    item_byte->setForeground(QColor(180, 150, 90));
                }

                setItem(static_cast<int>(r), static_cast<int>(c + 1), item_byte);

                if (std::isprint(byte_val)) {
                    ascii_str.append(static_cast<char>(byte_val));
                } else {
                    ascii_str.append('.');
                }
            } else {
                auto* item_byte = new QTableWidgetItem("??");
                item_byte->setTextAlignment(Qt::AlignCenter);
                item_byte->setForeground(QColor(180, 70, 70));
                setItem(static_cast<int>(r), static_cast<int>(c + 1), item_byte);
                ascii_str.append('?');
            }
        }

        auto* item_ascii = new QTableWidgetItem(ascii_str);
        item_ascii->setForeground(QColor(140, 180, 140));
        setItem(static_cast<int>(r), 17, item_ascii);
    }
}

Address MemoryHexView::addressAtCell(int row, int col) const {
    if (row < 0 || row >= static_cast<int>(rowCount_)) return baseAddress_;
    if (col >= 1 && col <= 16) {
        return baseAddress_ + (row * 16) + (col - 1);
    }
    return baseAddress_ + (row * 16);
}

void MemoryHexView::gotoAddress() {
    bool ok = false;
    QString text = QInputDialog::getText(
        this,
        "Goto Memory Address",
        "Address (Hex):",
        QLineEdit::Normal,
        QString::fromStdString(baseAddress_.toHex()),
        &ok
    );

    if (ok && !text.isEmpty()) {
        bool conv_ok = false;
        uint64_t val = 0;
        if (text.startsWith("0x", Qt::CaseInsensitive)) {
            val = text.toULongLong(&conv_ok, 16);
        } else {
            val = text.toULongLong(&conv_ok, 16);
        }
        if (conv_ok) {
            setBaseAddress(Address(val));
        }
    }
}

void MemoryHexView::findPatternPrompt() {
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) return;

    bool ok = false;
    QString text = QInputDialog::getText(
        this,
        "Find in Memory (Ctrl+F)",
        "Enter ASCII string or hex bytes (e.g. 48 89 e5):",
        QLineEdit::Normal,
        "",
        &ok
    );

    if (!ok || text.trimmed().isEmpty()) return;

    std::vector<uint8_t> pattern;
    QString trimmed = text.trimmed();

    QStringList parts = trimmed.split(' ', Qt::SkipEmptyParts);
    bool all_hex = true;
    for (const auto& part : parts) {
        if (part.length() > 2) { all_hex = false; break; }
        bool part_ok = false;
        part.toUInt(&part_ok, 16);
        if (!part_ok) { all_hex = false; break; }
    }

    if (all_hex && parts.size() > 1) {
        for (const auto& part : parts) {
            pattern.push_back(static_cast<uint8_t>(part.toUInt(nullptr, 16)));
        }
    } else {
        QByteArray utf8 = trimmed.toUtf8();
        pattern.assign(utf8.begin(), utf8.end());
    }

    auto found = session->searchMemory(baseAddress_, 1024 * 1024, pattern);
    if (found) {
        setBaseAddress(*found);
    } else {
        QMessageBox::information(this, "Memory Search", "Pattern not found within 1MB from current address.");
    }
}

void MemoryHexView::editBytesPrompt() {
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) {
        QMessageBox::warning(this, "Edit Memory", "Target must be paused or running to edit memory.");
        return;
    }

    int r = currentRow();
    int c = currentColumn();
    Address target_addr = addressAtCell(r, c);

    bool ok = false;
    QString text = QInputDialog::getText(
        this,
        "Edit Memory Bytes (Ctrl+E)",
        QString("Enter hex bytes to write at %1 (e.g. 90 90 90 or 48 31 c0):")
            .arg(QString::fromStdString(target_addr.toHex())),
        QLineEdit::Normal,
        "",
        &ok
    );

    if (!ok || text.trimmed().isEmpty()) return;

    QStringList tokens = text.trimmed().split(' ', Qt::SkipEmptyParts);
    std::vector<uint8_t> new_bytes;
    for (const auto& tok : tokens) {
        bool b_ok = false;
        uint b_val = tok.toUInt(&b_ok, 16);
        if (b_ok && b_val <= 0xFF) {
            new_bytes.push_back(static_cast<uint8_t>(b_val));
        } else {
            QMessageBox::warning(this, "Hex Parse Error", "Invalid hex byte: " + tok);
            return;
        }
    }

    if (new_bytes.empty()) return;

    auto old_bytes = session->readMemory(target_addr, new_bytes.size());
    bool wrote = session->writeMemory(target_addr, new_bytes.data(), new_bytes.size());
    if (wrote) {
        Q_EMIT patchCreated(target_addr, old_bytes, new_bytes, "Hex View Edit");
        refresh();
    } else {
        QMessageBox::critical(this, "Write Memory Error", "Failed to write memory at address: " + QString::fromStdString(target_addr.toHex()));
    }
}

void MemoryHexView::fillNops() {
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) return;

    int r = currentRow();
    int c = currentColumn();
    Address target_addr = addressAtCell(r, c);

    bool ok = false;
    int count = QInputDialog::getInt(this, "Fill with NOPs", "Number of NOP bytes (0x90) to fill:", 4, 1, 4096, 1, &ok);
    if (!ok || count <= 0) return;

    std::vector<uint8_t> nops(count, 0x90);
    auto old_bytes = session->readMemory(target_addr, count);
    if (session->writeMemory(target_addr, nops.data(), count)) {
        Q_EMIT patchCreated(target_addr, old_bytes, nops, "NOP Fill");
        refresh();
    }
}

void MemoryHexView::fillZeros() {
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) return;

    int r = currentRow();
    int c = currentColumn();
    Address target_addr = addressAtCell(r, c);

    bool ok = false;
    int count = QInputDialog::getInt(this, "Fill with Zeros", "Number of zero bytes (0x00) to fill:", 4, 1, 4096, 1, &ok);
    if (!ok || count <= 0) return;

    std::vector<uint8_t> zeros(count, 0x00);
    auto old_bytes = session->readMemory(target_addr, count);
    if (session->writeMemory(target_addr, zeros.data(), count)) {
        Q_EMIT patchCreated(target_addr, old_bytes, zeros, "Zero Fill");
        refresh();
    }
}

void MemoryHexView::dumpMemoryRangePrompt() {
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) {
        QMessageBox::warning(this, "Dump Error", "Target must be paused or running to dump memory.");
        return;
    }

    int r = currentRow();
    int c = currentColumn();
    Address start_addr = addressAtCell(r, c);

    bool ok = false;
    int size = QInputDialog::getInt(this, "Dump Memory Range", "Number of bytes to dump to file:", 512, 1, 1024 * 1024 * 64, 1, &ok);
    if (!ok || size <= 0) return;

    QString default_name = QString("dump_%1_%2b.bin").arg(QString::fromStdString(start_addr.toHex())).arg(size);
    QString path = QFileDialog::getSaveFileName(this, "Save Memory Dump", default_name, "Binary Files (*.bin);;All Files (*)");
    if (path.isEmpty()) return;

    if (session->dumpMemoryToFile(start_addr, size, path.toStdString())) {
        QMessageBox::information(this, "Dump Successful", QString("Successfully dumped %1 bytes to:\n%2").arg(size).arg(path));
    } else {
        QMessageBox::critical(this, "Dump Error", "Failed to dump memory range. Check address validity and disk permissions.");
    }
}

void MemoryHexView::handleCustomContextMenu(const QPoint& pos) {
    int r = currentRow();
    int c = currentColumn();
    Address sel_addr = addressAtCell(r, c);

    auto session = session_.lock();

    QMenu menu(this);
    menu.addAction("Follow in Disassembler", [this, sel_addr]() {
        Q_EMIT jumpToDisassemblyRequested(sel_addr);
    });

    if (session && session->state() != SessionState::Stopped) {
        auto* bpMenu = menu.addMenu("Breakpoint");

        bool hasBp = session->hasBreakpoint(sel_addr);
        if (hasBp) {
            bpMenu->addAction("Remove Breakpoint", [session, sel_addr, this]() {
                session->removeBreakpoint(sel_addr);
                refresh();
            });
        } else {
            bpMenu->addAction("Toggle Software Breakpoint (0xCC)", [session, sel_addr, this]() {
                session->toggleBreakpoint(sel_addr);
                refresh();
            });
        }
        bpMenu->addSeparator();

        auto* hwWriteMenu = bpMenu->addMenu("Set Hardware Write Watchpoint");
        hwWriteMenu->addAction("1 Byte", [session, sel_addr, this]() {
            session->addHardwareBreakpoint(sel_addr, HardwareBpType::Write, HardwareBpSize::Byte1);
            refresh();
        });
        hwWriteMenu->addAction("2 Bytes", [session, sel_addr, this]() {
            session->addHardwareBreakpoint(sel_addr, HardwareBpType::Write, HardwareBpSize::Byte2);
            refresh();
        });
        hwWriteMenu->addAction("4 Bytes", [session, sel_addr, this]() {
            session->addHardwareBreakpoint(sel_addr, HardwareBpType::Write, HardwareBpSize::Byte4);
            refresh();
        });
        hwWriteMenu->addAction("8 Bytes", [session, sel_addr, this]() {
            session->addHardwareBreakpoint(sel_addr, HardwareBpType::Write, HardwareBpSize::Byte8);
            refresh();
        });

        auto* hwAccessMenu = bpMenu->addMenu("Set Hardware Read/Write Watchpoint");
        hwAccessMenu->addAction("1 Byte", [session, sel_addr, this]() {
            session->addHardwareBreakpoint(sel_addr, HardwareBpType::ReadWrite, HardwareBpSize::Byte1);
            refresh();
        });
        hwAccessMenu->addAction("2 Bytes", [session, sel_addr, this]() {
            session->addHardwareBreakpoint(sel_addr, HardwareBpType::ReadWrite, HardwareBpSize::Byte2);
            refresh();
        });
        hwAccessMenu->addAction("4 Bytes", [session, sel_addr, this]() {
            session->addHardwareBreakpoint(sel_addr, HardwareBpType::ReadWrite, HardwareBpSize::Byte4);
            refresh();
        });
        hwAccessMenu->addAction("8 Bytes", [session, sel_addr, this]() {
            session->addHardwareBreakpoint(sel_addr, HardwareBpType::ReadWrite, HardwareBpSize::Byte8);
            refresh();
        });

        bpMenu->addAction("Set Hardware Execute Breakpoint", [session, sel_addr, this]() {
            session->addHardwareBreakpoint(sel_addr, HardwareBpType::Execute, HardwareBpSize::Byte1);
            refresh();
        });
    }

    menu.addSeparator();
    menu.addAction("Edit Bytes... (Ctrl+E)", this, &MemoryHexView::editBytesPrompt);
    menu.addAction("Fill with NOPs (0x90)...", this, &MemoryHexView::fillNops);
    menu.addAction("Fill with Zeros (0x00)...", this, &MemoryHexView::fillZeros);

    menu.addSeparator();
    menu.addAction("Dump Memory Range to File (*.bin)...", this, &MemoryHexView::dumpMemoryRangePrompt);

    menu.addSeparator();
    menu.addAction("Copy Selected Address", [sel_addr]() {
        QApplication::clipboard()->setText(QString::fromStdString(sel_addr.toHex()));
    });

    auto* cur_item = currentItem();
    if (cur_item) {
        menu.addAction("Copy Value", [cur_item]() {
            QApplication::clipboard()->setText(cur_item->text());
        });
    }

    menu.addSeparator();
    menu.addAction("Goto Address... (Ctrl+G)", this, &MemoryHexView::gotoAddress);
    menu.addAction("Find in Memory... (Ctrl+F)", this, &MemoryHexView::findPatternPrompt);
    menu.addAction("Refresh", this, &MemoryHexView::refresh);

    menu.exec(mapToGlobal(pos));
}

} // namespace edb_next
