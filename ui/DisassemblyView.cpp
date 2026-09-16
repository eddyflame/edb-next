#include "DisassemblyView.hpp"
#include "XRefDialog.hpp"
#include <QHeaderView>
#include <QFontDatabase>
#include <QMenu>
#include <QColor>
#include <QInputDialog>
#include <QMessageBox>
#include <QShortcut>
#include <QKeySequence>
#include <QClipboard>
#include <QApplication>
#include <iomanip>
#include <sstream>

namespace edb_next {

DisassemblyView::DisassemblyView(QWidget* parent) : QTableWidget(parent) {
    setupUi();
}

void DisassemblyView::setupUi() {
    setColumnCount(6);
    setHorizontalHeaderLabels({"Mark", "Address", "Bytes", "Instruction", "Symbol / Label", "Comment"});

    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    setColumnWidth(0, 50);

    horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
    horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);

    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setShowGrid(false);
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(22);

    QFont monoFont("Monospace", 9);
    monoFont.setStyleHint(QFont::Monospace);
    setFont(monoFont);

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QTableWidget::customContextMenuRequested, this, &DisassemblyView::handleCustomContextMenu);
    connect(this, &QTableWidget::cellDoubleClicked, this, &DisassemblyView::handleCellDoubleClicked);
    connect(this, &QTableWidget::currentCellChanged, this, &DisassemblyView::onCurrentCellChanged);

    // Shortcuts
    auto* sc_f2 = new QShortcut(QKeySequence("F2"), this);
    connect(sc_f2, &QShortcut::activated, this, [this]() {
        int row = currentRow();
        if (auto addr = addressAtRow(row)) {
            if (auto session = session_.lock()) {
                session->toggleBreakpoint(*addr);
                refresh();
                Q_EMIT breakpointToggled(*addr);
            }
        }
    });

    auto* sc_goto = new QShortcut(QKeySequence("Ctrl+G"), this);
    connect(sc_goto, &QShortcut::activated, this, &DisassemblyView::gotoAddressPrompt);

    auto* sc_runto = new QShortcut(QKeySequence("F4"), this);
    connect(sc_runto, &QShortcut::activated, this, &DisassemblyView::runToSelection);

    auto* sc_comment = new QShortcut(QKeySequence(";"), this);
    connect(sc_comment, &QShortcut::activated, this, &DisassemblyView::editCommentPrompt);

    auto* sc_bmark = new QShortcut(QKeySequence("Ctrl+B"), this);
    connect(sc_bmark, &QShortcut::activated, this, &DisassemblyView::toggleBookmark);

    auto* sc_next_bm = new QShortcut(QKeySequence("Ctrl+Alt+Down"), this);
    connect(sc_next_bm, &QShortcut::activated, this, &DisassemblyView::gotoNextBookmark);

    auto* sc_prev_bm = new QShortcut(QKeySequence("Ctrl+Alt+Up"), this);
    connect(sc_prev_bm, &QShortcut::activated, this, &DisassemblyView::gotoPrevBookmark);

    auto* sc_patch = new QShortcut(QKeySequence("Ctrl+E"), this);
    connect(sc_patch, &QShortcut::activated, this, &DisassemblyView::patchBytesPrompt);

    auto* sc_assemble = new QShortcut(QKeySequence(Qt::Key_Space), this);
    connect(sc_assemble, &QShortcut::activated, this, &DisassemblyView::assemblePrompt);

    auto* sc_xref = new QShortcut(QKeySequence("X"), this);
    connect(sc_xref, &QShortcut::activated, this, &DisassemblyView::findXRefsPrompt);

    // x64dbg-style fast branch navigation & history
    auto* sc_enter = new QShortcut(QKeySequence(Qt::Key_Return), this);
    connect(sc_enter, &QShortcut::activated, this, &DisassemblyView::followSelectedBranch);

    auto* sc_enter2 = new QShortcut(QKeySequence(Qt::Key_Enter), this);
    connect(sc_enter2, &QShortcut::activated, this, &DisassemblyView::followSelectedBranch);

    auto* sc_back = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(sc_back, &QShortcut::activated, this, &DisassemblyView::navigateHistoryBack);

    auto* sc_back2 = new QShortcut(QKeySequence(Qt::Key_Backspace), this);
    connect(sc_back2, &QShortcut::activated, this, &DisassemblyView::navigateHistoryBack);

    auto* sc_back3 = new QShortcut(QKeySequence("Alt+Left"), this);
    connect(sc_back3, &QShortcut::activated, this, &DisassemblyView::navigateHistoryBack);

    auto* sc_fwd = new QShortcut(QKeySequence("Alt+Right"), this);
    connect(sc_fwd, &QShortcut::activated, this, &DisassemblyView::navigateHistoryForward);

    auto* sc_mixed = new QShortcut(QKeySequence("Ctrl+Shift+S"), this);
    connect(sc_mixed, &QShortcut::activated, this, &DisassemblyView::toggleMixedSourceMode);
}

void DisassemblyView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
    followRip_ = true;
    viewAddress_ = Address(0);
    navHistory_.clear();
    navForward_.clear();
    refresh();
}

void DisassemblyView::toggleMixedSourceMode() {
    setMixedSourceMode(!showMixedSource_);
}

void DisassemblyView::setMixedSourceMode(bool enabled) {
    if (showMixedSource_ != enabled) {
        showMixedSource_ = enabled;
        refresh();
    }
}

void DisassemblyView::refresh() {
    auto session = session_.lock();
    if (!session || session->state() == SessionState::Stopped) {
        setRowCount(0);
        displayRows_.clear();
        currentInstructions_.clear();
        return;
    }

    Address start_addr = (followRip_ || viewAddress_.isNull()) ? session->registers().rip() : viewAddress_;
    if (start_addr.isNull()) {
        setRowCount(0);
        displayRows_.clear();
        currentInstructions_.clear();
        return;
    }

    currentInstructions_ = session->disassemble(start_addr, 40);

    displayRows_.clear();
    for (size_t i = 0; i < currentInstructions_.size(); ++i) {
        const auto& insn = currentInstructions_[i];
        if (showMixedSource_ && insn.isSourceLineStart && insn.sourceLine > 0 && !insn.sourceText.empty()) {
            displayRows_.push_back(DisplayRow{RowType::SourceBanner, i});
        }
        displayRows_.push_back(DisplayRow{RowType::Instruction, i});
    }

    setRowCount(static_cast<int>(displayRows_.size()));

    int target_scroll_row = -1;

    for (int r = 0; r < static_cast<int>(displayRows_.size()); ++r) {
        const auto& drow = displayRows_[r];
        const auto& insn = currentInstructions_[drow.insnIndex];

        if (drow.type == RowType::SourceBanner) {
            auto* item_mark = new QTableWidgetItem("[SRC]");
            item_mark->setTextAlignment(Qt::AlignCenter);
            item_mark->setForeground(QColor(128, 203, 196));

            auto* item_addr = new QTableWidgetItem(QString("Line %1").arg(insn.sourceLine));
            item_addr->setTextAlignment(Qt::AlignCenter);
            item_addr->setForeground(QColor(130, 210, 245));

            auto* item_bytes = new QTableWidgetItem("");

            QString src_str = QString("/* %1 */").arg(QString::fromStdString(insn.sourceText).trimmed());
            auto* item_asm = new QTableWidgetItem(src_str);
            item_asm->setForeground(QColor(130, 210, 245));
            QFont srcFont = font();
            srcFont.setItalic(true);
            srcFont.setBold(true);
            item_asm->setFont(srcFont);

            auto* item_sym = new QTableWidgetItem(QString::fromStdString(insn.sourceFile));
            item_sym->setForeground(QColor(100, 160, 210));

            auto* item_comment = new QTableWidgetItem("");

            QColor banner_bg(22, 38, 54, 230);
            item_mark->setBackground(banner_bg);
            item_addr->setBackground(banner_bg);
            item_bytes->setBackground(banner_bg);
            item_asm->setBackground(banner_bg);
            item_sym->setBackground(banner_bg);
            item_comment->setBackground(banner_bg);

            setItem(r, 0, item_mark);
            setItem(r, 1, item_addr);
            setItem(r, 2, item_bytes);
            setItem(r, 3, item_asm);
            setItem(r, 4, item_sym);
            setItem(r, 5, item_comment);
            continue;
        }

        bool isBookmarked = session->annotations().isBookmarked(insn.address);

        // Column 0: Mark (Breakpoint / Current RIP / Bookmark)
        QString mark;
        if (insn.hasBreakpoint) mark += "● ";
        if (insn.isCurrentRip) mark += "➔ ";
        if (isBookmarked) mark += "★";
        mark = mark.trimmed();

        auto* item_mark = new QTableWidgetItem(mark);
        item_mark->setTextAlignment(Qt::AlignCenter);
        if (insn.hasBreakpoint) {
            item_mark->setForeground(QColor(255, 80, 80));
        } else if (insn.isCurrentRip) {
            item_mark->setForeground(QColor(80, 220, 140));
        } else if (isBookmarked) {
            item_mark->setForeground(QColor(255, 215, 0));
        }

        // Column 1: Address
        auto* item_addr = new QTableWidgetItem(QString::fromStdString(insn.address.toHex()));

        // Column 2: Bytes
        std::ostringstream byte_oss;
        for (uint8_t b : insn.bytes) {
            byte_oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b) << " ";
        }
        auto* item_bytes = new QTableWidgetItem(QString::fromStdString(byte_oss.str()));
        item_bytes->setForeground(QColor(130, 130, 130));

        // Column 3: Instruction Mnemonic & Operands
        QString asm_text = QString::fromStdString(insn.mnemonic + " " + insn.operands);
        auto* item_asm = new QTableWidgetItem(asm_text);

        // Column 4: Symbol / Label
        auto* item_sym = new QTableWidgetItem(QString::fromStdString(insn.symbol));
        item_sym->setForeground(QColor(70, 190, 220));

        // Column 5: User Comment
        std::string commentStr = session->annotations().getComment(insn.address);
        auto* item_comment = new QTableWidgetItem(QString::fromStdString(commentStr));
        item_comment->setForeground(QColor(152, 195, 121));
        QFont commentFont = font();
        commentFont.setItalic(true);
        item_comment->setFont(commentFont);

        // High-contrast x64dbg-style line highlight
        if (insn.hasBreakpoint && insn.isCurrentRip) {
            target_scroll_row = r;
            QColor bp_rip_bg(140, 40, 70, 110);
            item_mark->setBackground(bp_rip_bg);
            item_addr->setBackground(bp_rip_bg);
            item_bytes->setBackground(bp_rip_bg);
            item_asm->setBackground(bp_rip_bg);
            item_sym->setBackground(bp_rip_bg);
            item_comment->setBackground(bp_rip_bg);

            QFont bold_font = font();
            bold_font.setBold(true);
            item_asm->setFont(bold_font);
        } else if (insn.hasBreakpoint) {
            QColor bp_bg(130, 30, 30, 80);
            item_mark->setBackground(bp_bg);
            item_addr->setBackground(bp_bg);
            item_bytes->setBackground(bp_bg);
            item_asm->setBackground(bp_bg);
            item_sym->setBackground(bp_bg);
            item_comment->setBackground(bp_bg);
        } else if (insn.isCurrentRip) {
            target_scroll_row = r;
            QColor rip_bg(30, 80, 140, 100);
            item_mark->setBackground(rip_bg);
            item_addr->setBackground(rip_bg);
            item_bytes->setBackground(rip_bg);
            item_asm->setBackground(rip_bg);
            item_sym->setBackground(rip_bg);
            item_comment->setBackground(rip_bg);

            QFont bold_font = font();
            bold_font.setBold(true);
            item_asm->setFont(bold_font);
        }

        setItem(r, 0, item_mark);
        setItem(r, 1, item_addr);
        setItem(r, 2, item_bytes);
        setItem(r, 3, item_asm);
        setItem(r, 4, item_sym);
        setItem(r, 5, item_comment);
    }

    if (target_scroll_row >= 0) {
        scrollToItem(item(target_scroll_row, 0), QAbstractItemView::PositionAtCenter);
    }
}

void DisassemblyView::gotoAddressPrompt() {
    auto session = session_.lock();
    if (!session) return;

    bool ok = false;
    QString text = QInputDialog::getText(
        this,
        "Goto Address or Symbol (Ctrl+G)",
        "Enter address (hex) or symbol name (e.g. main):",
        QLineEdit::Normal,
        "",
        &ok
    );

    if (!ok || text.trimmed().isEmpty()) return;
    std::string query = text.trimmed().toStdString();

    // 1. Try symbol resolution
    if (auto sym_addr = session->resolveSymbol(query)) {
        gotoAddress(*sym_addr);
        return;
    }

    // 2. Try hex address parsing
    bool conv_ok = false;
    uint64_t val = 0;
    if (text.startsWith("0x", Qt::CaseInsensitive)) {
        val = text.toULongLong(&conv_ok, 16);
    } else {
        val = text.toULongLong(&conv_ok, 16);
        if (!conv_ok) {
            val = text.toULongLong(&conv_ok, 10);
        }
    }

    if (conv_ok) {
        gotoAddress(Address(val));
    }
}

void DisassemblyView::gotoAddress(Address addr) {
    if (!viewAddress_.isNull() && viewAddress_ != addr) {
        if (navHistory_.empty() || navHistory_.back() != viewAddress_) {
            navHistory_.push_back(viewAddress_);
            if (navHistory_.size() > 64) {
                navHistory_.erase(navHistory_.begin());
            }
        }
        navForward_.clear();
    }
    viewAddress_ = addr;
    followRip_ = false;
    refresh();
}

void DisassemblyView::followRip() {
    followRip_ = true;
    viewAddress_ = Address(0);
    refresh();
}

void DisassemblyView::followSelectedBranch() {
    int row = currentRow();
    if (auto* insn = instructionAtRow(row)) {
        if (auto session = session_.lock()) {
            auto details = session->inspectInstruction(insn->address);
            if (details.isBranch && !details.branchTarget.isNull()) {
                gotoAddress(details.branchTarget);
            }
        }
    }
}

void DisassemblyView::navigateHistoryBack() {
    if (navHistory_.empty()) return;
    Address prev = navHistory_.back();
    navHistory_.pop_back();
    if (!viewAddress_.isNull()) {
        navForward_.push_back(viewAddress_);
    }
    viewAddress_ = prev;
    followRip_ = false;
    refresh();
}

void DisassemblyView::navigateHistoryForward() {
    if (navForward_.empty()) return;
    Address nxt = navForward_.back();
    navForward_.pop_back();
    if (!viewAddress_.isNull()) {
        navHistory_.push_back(viewAddress_);
    }
    viewAddress_ = nxt;
    followRip_ = false;
    refresh();
}

std::optional<Address> DisassemblyView::addressAtRow(int row) const {
    if (row >= 0 && row < static_cast<int>(displayRows_.size())) {
        size_t idx = displayRows_[row].insnIndex;
        if (idx < currentInstructions_.size()) {
            return currentInstructions_[idx].address;
        }
    }
    return std::nullopt;
}

const DisassembledInstruction* DisassemblyView::instructionAtRow(int row) const {
    if (row >= 0 && row < static_cast<int>(displayRows_.size())) {
        size_t idx = displayRows_[row].insnIndex;
        if (idx < currentInstructions_.size()) {
            return &currentInstructions_[idx];
        }
    }
    return nullptr;
}

void DisassemblyView::handleCellDoubleClicked(int row, int col) {
    if (col == 5) {
        // Double clicking comment column edits the comment!
        editCommentPrompt();
        return;
    }

    if (col == 3) {
        // Double clicking instruction column follows branch if it is a branch/call!
        if (auto* insn = instructionAtRow(row)) {
            if (auto session = session_.lock()) {
                auto details = session->inspectInstruction(insn->address);
                if (details.isBranch && !details.branchTarget.isNull()) {
                    gotoAddress(details.branchTarget);
                    return;
                }
            }
        }
    }

    if (auto addr = addressAtRow(row)) {
        if (auto session = session_.lock()) {
            session->toggleBreakpoint(*addr);
            refresh();
            Q_EMIT breakpointToggled(*addr);
        }
    }
}

void DisassemblyView::runToSelection() {
    int row = currentRow();
    if (auto addr = addressAtRow(row)) {
        if (auto session = session_.lock()) {
            session->runTo(*addr);
        }
    }
}

void DisassemblyView::patchBytesPrompt() {
    int row = currentRow();
    auto* insn = instructionAtRow(row);
    auto session = session_.lock();
    if (!insn || !session || session->state() == SessionState::Stopped) return;

    std::ostringstream cur_bytes_oss;
    for (uint8_t b : insn->bytes) {
        cur_bytes_oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b) << " ";
    }

    bool ok = false;
    QString text = QInputDialog::getText(
        this,
        "Patch Bytes (Ctrl+E)",
        QString("Enter hex bytes to write at %1:").arg(QString::fromStdString(insn->address.toHex())),
        QLineEdit::Normal,
        QString::fromStdString(cur_bytes_oss.str()).trimmed(),
        &ok
    );

    if (!ok || text.trimmed().isEmpty()) return;

    QString clean = text.trimmed().remove(' ');
    if (clean.length() % 2 != 0) {
        QMessageBox::warning(this, "Patch Error", "Hex string must have an even number of hex digits.");
        return;
    }

    std::vector<uint8_t> patchBytes;
    patchBytes.reserve(clean.length() / 2);
    for (int i = 0; i < clean.length(); i += 2) {
        bool bOk = false;
        uint8_t byteVal = static_cast<uint8_t>(clean.mid(i, 2).toUInt(&bOk, 16));
        if (!bOk) {
            QMessageBox::warning(this, "Patch Error", "Invalid hex character entered.");
            return;
        }
        patchBytes.push_back(byteVal);
    }

    if (!patchBytes.empty()) {
        session->writeMemory(insn->address, patchBytes.data(), patchBytes.size());
        refresh();
    }
}

void DisassemblyView::fillWithNops() {
    int row = currentRow();
    auto* insn = instructionAtRow(row);
    auto session = session_.lock();
    if (!insn || !session || session->state() == SessionState::Stopped) return;

    size_t len = insn->bytes.size();
    if (len == 0) return;

    std::vector<uint8_t> nops(len, 0x90);
    session->writeMemory(insn->address, nops.data(), nops.size());
    refresh();
}

void DisassemblyView::editCommentPrompt() {
    int row = currentRow();
    auto addr = addressAtRow(row);
    auto session = session_.lock();
    if (!addr || !session) return;

    std::string existing = session->annotations().getComment(*addr);

    bool ok = false;
    QString text = QInputDialog::getText(
        this,
        "Edit Comment (;)",
        QString("Comment for %1:").arg(QString::fromStdString(addr->toHex())),
        QLineEdit::Normal,
        QString::fromStdString(existing),
        &ok
    );

    if (ok) {
        session->annotations().setComment(*addr, text.trimmed().toStdString());
        refresh();
    }
}

void DisassemblyView::toggleBookmark() {
    int row = currentRow();
    auto addr = addressAtRow(row);
    auto session = session_.lock();
    if (!addr || !session) return;

    session->annotations().toggleBookmark(*addr);
    refresh();
}

void DisassemblyView::gotoNextBookmark() {
    auto session = session_.lock();
    if (!session) return;

    Address current = (viewAddress_.isNull()) ? session->registers().rip() : viewAddress_;
    if (auto next = session->annotations().nextBookmark(current)) {
        gotoAddress(*next);
    }
}

void DisassemblyView::gotoPrevBookmark() {
    auto session = session_.lock();
    if (!session) return;

    Address current = (viewAddress_.isNull()) ? session->registers().rip() : viewAddress_;
    if (auto prev = session->annotations().prevBookmark(current)) {
        gotoAddress(*prev);
    }
}

void DisassemblyView::handleCustomContextMenu(const QPoint& pos) {
    int row = rowAt(pos.y());
    auto addr = addressAtRow(row);

    QMenu menu(this);
    auto session = session_.lock();

    if (addr && session) {
        bool has_bp = session->hasBreakpoint(*addr);
        QAction* act_toggle = menu.addAction(has_bp ? "Remove Breakpoint (F2)" : "Set Breakpoint (F2)");
        connect(act_toggle, &QAction::triggered, this, [this, addr]() {
            if (auto session = session_.lock()) {
                session->toggleBreakpoint(*addr);
                refresh();
                Q_EMIT breakpointToggled(*addr);
            }
        });

        QAction* act_runto = menu.addAction("Run to Cursor (F4)");
        connect(act_runto, &QAction::triggered, this, [this, addr]() {
            if (auto session = session_.lock()) {
                session->runTo(*addr);
            }
        });

        menu.addSeparator();

        // Comments & Bookmarks
        QAction* act_comment = menu.addAction("Set / Edit Comment... (;)");
        connect(act_comment, &QAction::triggered, this, &DisassemblyView::editCommentPrompt);

        bool isBmk = session->annotations().isBookmarked(*addr);
        QAction* act_bmk = menu.addAction(isBmk ? "Remove Bookmark (Ctrl+B)" : "Add Bookmark (Ctrl+B)");
        connect(act_bmk, &QAction::triggered, this, &DisassemblyView::toggleBookmark);

        menu.addSeparator();

        // Origin & Navigation
        QAction* act_origin = menu.addAction("Set New Origin Here (Set RIP) (Ctrl+*)");
        connect(act_origin, &QAction::triggered, this, &DisassemblyView::setOriginToSelection);

        auto* dumpSub = menu.addMenu("Follow in Dump");
        dumpSub->addAction(QString("Follow Selection (%1) in Dump").arg(QString::fromStdString(addr->toHex())), [this, addr]() {
            Q_EMIT jumpToMemoryRequested(*addr);
        });

        auto details = session->inspectInstruction(*addr);
        if (details.hasMemoryOperand && !details.effectiveAddress.isNull()) {
            Address memAddr = details.effectiveAddress;
            dumpSub->addAction(QString("Follow Memory Address (%1) in Dump").arg(QString::fromStdString(memAddr.toHex())), [this, memAddr]() {
                Q_EMIT jumpToMemoryRequested(memAddr);
            });
        }

        if (details.isBranch && !details.branchTarget.isNull()) {
            Address brTarget = details.branchTarget;
            menu.addAction(QString("Follow Jump / Call Target (%1)").arg(QString::fromStdString(brTarget.toHex())), [this, brTarget]() {
                gotoAddress(brTarget);
            });
        }

        menu.addSeparator();

        // Hardware Breakpoints Submenu
        auto* hwSub = menu.addMenu("Hardware Breakpoint");
        hwSub->addAction("Hardware Execute (DR0~DR3)", [this, addr]() {
            if (auto s = session_.lock()) {
                s->addHardwareBreakpoint(*addr, HardwareBpType::Execute, HardwareBpSize::Byte1);
                refresh();
            }
        });
        hwSub->addAction("Hardware Write (1 Byte)", [this, addr]() {
            if (auto s = session_.lock()) {
                s->addHardwareBreakpoint(*addr, HardwareBpType::Write, HardwareBpSize::Byte1);
                refresh();
            }
        });
        hwSub->addAction("Hardware Write (4 Bytes)", [this, addr]() {
            if (auto s = session_.lock()) {
                s->addHardwareBreakpoint(*addr, HardwareBpType::Write, HardwareBpSize::Byte4);
                refresh();
            }
        });
        hwSub->addAction("Hardware Write (8 Bytes)", [this, addr]() {
            if (auto s = session_.lock()) {
                s->addHardwareBreakpoint(*addr, HardwareBpType::Write, HardwareBpSize::Byte8);
                refresh();
            }
        });
        hwSub->addAction("Hardware Read/Write (1 Byte)", [this, addr]() {
            if (auto s = session_.lock()) {
                s->addHardwareBreakpoint(*addr, HardwareBpType::ReadWrite, HardwareBpSize::Byte1);
                refresh();
            }
        });
        hwSub->addAction("Hardware Read/Write (4 Bytes)", [this, addr]() {
            if (auto s = session_.lock()) {
                s->addHardwareBreakpoint(*addr, HardwareBpType::ReadWrite, HardwareBpSize::Byte4);
                refresh();
            }
        });
        hwSub->addAction("Hardware Read/Write (8 Bytes)", [this, addr]() {
            if (auto s = session_.lock()) {
                s->addHardwareBreakpoint(*addr, HardwareBpType::ReadWrite, HardwareBpSize::Byte8);
                refresh();
            }
        });

        menu.addSeparator();

        // Patching & Assemble
        QAction* act_assemble = menu.addAction("Assemble Instruction... (Space)");
        connect(act_assemble, &QAction::triggered, this, &DisassemblyView::assemblePrompt);

        QAction* act_patch = menu.addAction("Patch Instruction / Bytes... (Ctrl+E)");
        connect(act_patch, &QAction::triggered, this, &DisassemblyView::patchBytesPrompt);

        QAction* act_nop = menu.addAction("Fill Instruction with NOPs (0x90)");
        connect(act_nop, &QAction::triggered, this, &DisassemblyView::fillWithNops);

        menu.addSeparator();

        // XREFS
        QAction* act_xref = menu.addAction("Find References to Address... (X)");
        connect(act_xref, &QAction::triggered, this, &DisassemblyView::findXRefsPrompt);

        menu.addSeparator();

        // Copy Submenu
        auto* copySub = menu.addMenu("Copy");
        copySub->addAction("Copy Address", [addr]() {
            QApplication::clipboard()->setText(QString::fromStdString(addr->toHex()));
        });
        if (auto* insn = instructionAtRow(row)) {
            copySub->addAction("Copy Disassembly", [insn]() {
                QApplication::clipboard()->setText(QString::fromStdString(insn->mnemonic + " " + insn->operands));
            });
            copySub->addAction("Copy Bytes (Hex)", [insn]() {
                std::ostringstream ss;
                for (uint8_t b : insn->bytes) ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b) << " ";
                QApplication::clipboard()->setText(QString::fromStdString(ss.str()).trimmed());
            });
        }

        menu.addSeparator();
    }

    QAction* act_goto = menu.addAction("Goto Address / Symbol... (Ctrl+G)");
    connect(act_goto, &QAction::triggered, this, &DisassemblyView::gotoAddressPrompt);

    QAction* act_follow = menu.addAction("Follow Current RIP");
    connect(act_follow, &QAction::triggered, this, &DisassemblyView::followRip);

    menu.addSeparator();
    QAction* act_mixed = menu.addAction("Show C/C++ Source Lines (Mixed Mode, Ctrl+Shift+S)");
    act_mixed->setCheckable(true);
    act_mixed->setChecked(showMixedSource_);
    connect(act_mixed, &QAction::toggled, this, &DisassemblyView::setMixedSourceMode);

    menu.exec(mapToGlobal(pos));
}

void DisassemblyView::setOriginToSelection() {
    int row = currentRow();
    if (auto addr = addressAtRow(row)) {
        if (auto session = session_.lock()) {
            session->setInstructionPointer(*addr);
            refresh();
        }
    }
}

void DisassemblyView::assemblePrompt() {
    int row = currentRow();
    auto* insn = instructionAtRow(row);
    auto session = session_.lock();
    if (!insn || !session || session->state() == SessionState::Stopped) return;

    QString defaultAsm = QString::fromStdString(insn->mnemonic + " " + insn->operands).trimmed();
    bool ok = false;
    QString text = QInputDialog::getText(
        this,
        "Assemble Instruction (Space)",
        QString("Assemble at %1 (original length: %2 bytes):")
            .arg(QString::fromStdString(insn->address.toHex()))
            .arg(insn->bytes.size()),
        QLineEdit::Normal,
        defaultAsm,
        &ok
    );

    if (!ok || text.trimmed().isEmpty()) return;

    auto res = session->assemble(text.trimmed().toStdString(), insn->address);
    if (!res) {
        QMessageBox::critical(this, "Assemble Failed", QString::fromStdString(res.error));
        return;
    }

    std::vector<uint8_t> newBytes = std::move(res.value);
    size_t origLen = insn->bytes.size();

    // If new instruction is shorter than original, pad with NOPs
    if (newBytes.size() < origLen) {
        size_t padCount = origLen - newBytes.size();
        auto reply = QMessageBox::question(
            this,
            "Pad with NOPs?",
            QString("New instruction is %1 bytes (original was %2 bytes).\nPad remaining %3 bytes with NOP (0x90)?")
                .arg(newBytes.size()).arg(origLen).arg(padCount),
            QMessageBox::Yes | QMessageBox::No
        );
        if (reply == QMessageBox::Yes) {
            newBytes.insert(newBytes.end(), padCount, 0x90);
        }
    } else if (newBytes.size() > origLen) {
        auto reply = QMessageBox::warning(
            this,
            "Instruction Overwrite Warning",
            QString("New instruction is %1 bytes, which is %2 bytes longer than original (%3 bytes).\nThis will overwrite the following instruction bytes! Continue?")
                .arg(newBytes.size()).arg(newBytes.size() - origLen).arg(origLen),
            QMessageBox::Yes | QMessageBox::Cancel
        );
        if (reply != QMessageBox::Yes) return;
    }

    session->writeMemory(insn->address, newBytes.data(), newBytes.size());
    refresh();
}

void DisassemblyView::findXRefsPrompt() {
    int row = currentRow();
    auto addr = addressAtRow(row);
    auto session = session_.lock();
    if (!addr || !session || session->state() == SessionState::Stopped) return;

    auto xrefs = session->findCodeXRefs(*addr);
    if (xrefs.empty()) {
        QMessageBox::information(this, "XREFS", QString("No cross-references found to %1").arg(QString::fromStdString(addr->toHex())));
        return;
    }

    XRefDialog dlg(*addr, xrefs, this);
    connect(&dlg, &XRefDialog::jumpRequested, this, &DisassemblyView::gotoAddress);
    dlg.exec();
}

void DisassemblyView::onCurrentCellChanged(int currentRow, int currentColumn, int previousRow, int previousColumn) {
    Q_UNUSED(currentColumn);
    Q_UNUSED(previousRow);
    Q_UNUSED(previousColumn);

    auto addr = addressAtRow(currentRow);
    auto session = session_.lock();
    if (!addr || !session || session->state() == SessionState::Stopped) {
        Q_EMIT instructionInspected("");
        return;
    }

    auto details = session->inspectInstruction(*addr);
    Q_EMIT instructionInspected(QString::fromStdString(details.summary));
}

} // namespace edb_next
