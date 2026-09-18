#include "DisassemblyView.hpp"
#include "XRefDialog.hpp"
#include "core/ConfigurationManager.hpp"
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
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <unordered_set>
#include <string_view>
#include <cctype>

namespace edb_next {

namespace {

enum class MnemonicClass {
    Call,
    Jump,
    CondJump,
    Return,
    Trap,
    Stack,
    Nop,
    Comparison,
    Normal
};

static MnemonicClass classifyMnemonic(std::string_view m) {
    char buf[16];
    size_t len = std::min(m.size(), sizeof(buf) - 1);
    for (size_t i = 0; i < len; ++i) {
        buf[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(m[i])));
    }
    buf[len] = '\0';
    std::string_view lowerM(buf, len);

    if (lowerM == "call" || lowerM == "callq") return MnemonicClass::Call;
    if (lowerM == "jmp" || lowerM == "jmpq") return MnemonicClass::Jump;
    if (lowerM == "ret" || lowerM == "retn" || lowerM == "retf" || lowerM == "retq" || lowerM == "iret" || lowerM == "iretd" || lowerM == "iretq") return MnemonicClass::Return;
    if (lowerM == "push" || lowerM == "pop" || lowerM == "pushf" || lowerM == "pushfq" || lowerM == "popf" || lowerM == "popfq" || lowerM == "enter" || lowerM == "leave") return MnemonicClass::Stack;
    if (lowerM == "syscall" || lowerM == "sysenter" || lowerM == "int" || lowerM == "int3" || lowerM == "int1" || lowerM == "into" || lowerM == "ud2" || lowerM == "hlt") return MnemonicClass::Trap;
    if (lowerM == "nop" || lowerM == "pause") return MnemonicClass::Nop;
    if (lowerM == "cmp" || lowerM == "test") return MnemonicClass::Comparison;
    if (lowerM.starts_with('j') || lowerM.starts_with("loop")) return MnemonicClass::CondJump;
    return MnemonicClass::Normal;
}

static const std::unordered_set<std::string_view> kRegisters = {
    // 64-bit GPR
    "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    // 32-bit GPR
    "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp",
    "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d",
    // 16-bit GPR
    "ax", "bx", "cx", "dx", "si", "di", "bp", "sp",
    "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w",
    // 8-bit GPR
    "al", "bl", "cl", "dl", "ah", "bh", "ch", "dh",
    "sil", "dil", "bpl", "spl",
    "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b",
    // Special / IP / Flags / Segments
    "rip", "eip", "ip", "rflags", "eflags", "flags",
    "cs", "ds", "es", "fs", "gs", "ss",
    // SIMD / Float
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
    "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15",
    "ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5", "ymm6", "ymm7",
    "ymm8", "ymm9", "ymm10", "ymm11", "ymm12", "ymm13", "ymm14", "ymm15",
    "zmm0", "zmm1", "zmm2", "zmm3", "zmm4", "zmm5", "zmm6", "zmm7",
    "zmm8", "zmm9", "zmm10", "zmm11", "zmm12", "zmm13", "zmm14", "zmm15",
    "st0", "st1", "st2", "st3", "st4", "st5", "st6", "st7",
    "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7",
    // Control / Debug
    "cr0", "cr2", "cr3", "cr4", "cr8",
    "dr0", "dr1", "dr2", "dr3", "dr6", "dr7"
};

static const std::unordered_set<std::string_view> kSizeKeywords = {
    "byte", "word", "dword", "qword", "tbyte", "xmmword", "ymmword", "zmmword", "ptr", "short", "near", "far"
};

struct OperandToken {
    QString text;
    QColor color;
};

static std::vector<OperandToken> tokenizeOperands(std::string_view ops) {
    std::vector<OperandToken> tokens;
    size_t i = 0;
    while (i < ops.size()) {
        if (std::isspace(static_cast<unsigned char>(ops[i]))) {
            size_t start = i;
            while (i < ops.size() && std::isspace(static_cast<unsigned char>(ops[i]))) {
                ++i;
            }
            tokens.push_back({QString::fromLatin1(ops.data() + start, static_cast<int>(i - start)), QColor(200, 200, 200)});
        } else if (ops[i] == '[' || ops[i] == ']') {
            tokens.push_back({QString(ops[i]), QColor(0x64, 0xb5, 0xf6)}); // #64b5f6 blue brackets
            ++i;
        } else if (ops[i] == ',' || ops[i] == ':' || ops[i] == '+' || ops[i] == '-' || ops[i] == '*') {
            tokens.push_back({QString(ops[i]), QColor(0x88, 0x88, 0x88)}); // #888888 operators
            ++i;
        } else if (ops[i] == '0' && i + 1 < ops.size() && (ops[i + 1] == 'x' || ops[i + 1] == 'X')) {
            size_t start = i;
            i += 2;
            while (i < ops.size() && std::isxdigit(static_cast<unsigned char>(ops[i]))) {
                ++i;
            }
            tokens.push_back({QString::fromLatin1(ops.data() + start, static_cast<int>(i - start)), QColor(0xff, 0xcc, 0x80)}); // #ffcc80 peach/gold
        } else if (std::isdigit(static_cast<unsigned char>(ops[i]))) {
            size_t start = i;
            while (i < ops.size() && (std::isalnum(static_cast<unsigned char>(ops[i])) || ops[i] == 'h' || ops[i] == 'H')) {
                ++i;
            }
            tokens.push_back({QString::fromLatin1(ops.data() + start, static_cast<int>(i - start)), QColor(0xff, 0xcc, 0x80)});
        } else if (std::isalpha(static_cast<unsigned char>(ops[i])) || ops[i] == '_' || ops[i] == '.') {
            size_t start = i;
            while (i < ops.size() && (std::isalnum(static_cast<unsigned char>(ops[i])) || ops[i] == '_' || ops[i] == '.')) {
                ++i;
            }
            std::string_view word = ops.substr(start, i - start);
            char lowerBuf[32];
            QColor col(0xe0, 0xe0, 0xe0);
            if (word.size() < sizeof(lowerBuf)) {
                for (size_t k = 0; k < word.size(); ++k) {
                    lowerBuf[k] = static_cast<char>(std::tolower(static_cast<unsigned char>(word[k])));
                }
                std::string_view lowerWord(lowerBuf, word.size());
                if (kRegisters.contains(lowerWord)) {
                    col = QColor(0x90, 0xca, 0xf9); // #90caf9 light sky blue
                } else if (kSizeKeywords.contains(lowerWord)) {
                    col = QColor(0xb0, 0xbe, 0xc5); // #b0bec5 muted slate
                }
            }
            tokens.push_back({QString::fromLatin1(word.data(), static_cast<int>(word.size())), col});
        } else {
            tokens.push_back({QString(ops[i]), QColor(0xe0, 0xe0, 0xe0)});
            ++i;
        }
    }
    return tokens;
}

static std::optional<Address> extractBranchTarget(
    const DisassembledInstruction& insn,
    const std::shared_ptr<DebugSession>& session)
{
    const std::string& ops = insn.operands;
    // 1. Direct branch/call without memory indirect brackets (e.g. "0x555555555297", "0x7ffff7fe51d0", "401000h")
    if (ops.find('[') == std::string::npos && ops.find(']') == std::string::npos) {
        auto pos = ops.find("0x");
        if (pos == std::string::npos) pos = ops.find("0X");
        if (pos != std::string::npos) {
            size_t end = pos + 2;
            while (end < ops.size() && std::isxdigit(static_cast<unsigned char>(ops[end]))) {
                ++end;
            }
            if (end > pos + 2) {
                try {
                    return Address(std::stoull(ops.substr(pos, end - pos), nullptr, 16));
                } catch (...) {}
            }
        }
        // Handle hex with suffix 'h' / 'H', e.g. "401000h"
        size_t hpos = ops.rfind('h');
        if (hpos == std::string::npos) hpos = ops.rfind('H');
        if (hpos != std::string::npos && hpos > 0) {
            size_t start = hpos;
            while (start > 0 && std::isxdigit(static_cast<unsigned char>(ops[start - 1]))) {
                --start;
            }
            if (start < hpos) {
                try {
                    return Address(std::stoull(ops.substr(start, hpos - start), nullptr, 16));
                } catch (...) {}
            }
        }
    }

    // 2. Indirect or computed branch: check inspector if session available
    if (session) {
        auto details = session->inspectInstruction(insn.address);
        if (details.isBranch && !details.branchTarget.isNull()) {
            return details.branchTarget;
        }
        if (details.hasMemoryOperand && !details.effectiveAddress.isNull()) {
            if (details.memoryReadSuccess && details.memoryValue != 0) {
                return Address(details.memoryValue);
            }
            return details.effectiveAddress;
        }
    }

    return std::nullopt;
}

class InstructionHighlightDelegate : public QStyledItemDelegate {
public:
    explicit InstructionHighlightDelegate(DisassemblyView* view, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), view_(view) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        const auto* insn = view_ ? view_->instructionAtRow(index.row()) : nullptr;
        if (!insn) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        // Clear text so default CE_ItemViewItem draws only backgrounds and selection overlays
        opt.text.clear();
        QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        painter->save();
        painter->setClipRect(opt.rect);

        QFont normalFont = opt.font;
        QFont boldFont = normalFont;
        boldFont.setBold(true);

        QFontMetrics fm(normalFont);
        QFontMetrics fmBold(boldFont);

        int x = opt.rect.left() + 6;
        int y = opt.rect.top() + (opt.rect.height() + fm.ascent() - fm.descent()) / 2;

        // 1. Draw Mnemonic
        MnemonicClass mclass = classifyMnemonic(insn->mnemonic);
        QColor mnemonicColor;
        bool boldMnemonic = false;
        switch (mclass) {
            case MnemonicClass::Call:
                mnemonicColor = QColor(0x4f, 0xc3, 0xf7); // #4fc3f7
                boldMnemonic = true;
                break;
            case MnemonicClass::Jump:
                mnemonicColor = QColor(0xff, 0xa7, 0x26); // #ffa726
                boldMnemonic = true;
                break;
            case MnemonicClass::CondJump:
                mnemonicColor = QColor(0xff, 0xb7, 0x4d); // #ffb74d
                boldMnemonic = true;
                break;
            case MnemonicClass::Return:
                mnemonicColor = QColor(0xef, 0x53, 0x50); // #ef5350
                boldMnemonic = true;
                break;
            case MnemonicClass::Trap:
                mnemonicColor = QColor(0xba, 0x68, 0xc8); // #ba68c8
                boldMnemonic = true;
                break;
            case MnemonicClass::Stack:
                mnemonicColor = QColor(0x81, 0xc7, 0x84); // #81c784
                break;
            case MnemonicClass::Nop:
                mnemonicColor = QColor(0x75, 0x75, 0x75); // #757575
                break;
            case MnemonicClass::Comparison:
                mnemonicColor = QColor(0x4d, 0xd0, 0xe1); // #4dd0e1
                break;
            case MnemonicClass::Normal:
            default:
                mnemonicColor = QColor(0xe0, 0xe0, 0xe0); // #e0e0e0
                break;
        }

        if (boldMnemonic) {
            painter->setFont(boldFont);
        } else {
            painter->setFont(normalFont);
        }
        painter->setPen(mnemonicColor);

        QString mnemonicStr = QString::fromStdString(insn->mnemonic);
        painter->drawText(x, y, mnemonicStr);

        int mWidth = boldMnemonic ? fmBold.horizontalAdvance(mnemonicStr) : fm.horizontalAdvance(mnemonicStr);
        // Align operands: allocate at least 8 characters width for mnemonic (or mWidth + 1 space if longer)
        int tabStop = fm.horizontalAdvance("        ");
        int spacing = (mWidth < tabStop) ? (tabStop - mWidth) : fm.horizontalAdvance(" ");
        x += mWidth + spacing;

        // 2. Draw Operands
        painter->setFont(normalFont);
        if (!insn->operands.empty()) {
            auto tokens = tokenizeOperands(insn->operands);
            for (const auto& token : tokens) {
                painter->setPen(token.color);
                painter->drawText(x, y, token.text);
                x += fm.horizontalAdvance(token.text);
            }
        }

        painter->restore();
    }

private:
    DisassemblyView* view_{nullptr};
};

} // namespace

DisassemblyView::DisassemblyView(QWidget* parent) : QTableWidget(parent) {
    setupUi();
}

void DisassemblyView::setupUi() {
    setColumnCount(6);
    setHorizontalHeaderLabels({"Mark", "Address", "Bytes", "Instruction", "Symbol / Label", "Comment"});
    setItemDelegateForColumn(3, new InstructionHighlightDelegate(this, this));

    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    setColumnWidth(0, 75);

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

    setFont(ConfigurationManager::instance().appearance().disasmFont);
    setColumnHidden(2, !ConfigurationManager::instance().disasm().showBytesInHex);

    connect(&ConfigurationManager::instance(), &ConfigurationManager::configurationChanged, this, [this]() {
        setFont(ConfigurationManager::instance().appearance().disasmFont);
        setColumnHidden(2, !ConfigurationManager::instance().disasm().showBytesInHex);
        refresh();
    });

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QTableWidget::customContextMenuRequested, this, &DisassemblyView::handleCustomContextMenu);
    connect(this, &QTableWidget::cellDoubleClicked, this, &DisassemblyView::handleCellDoubleClicked);
    connect(this, &QTableWidget::currentCellChanged, this, &DisassemblyView::onCurrentCellChanged);
    connect(this, &QTableWidget::itemSelectionChanged, viewport(), qOverload<>(&QWidget::update));

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

    auto* sc_origin = new QShortcut(QKeySequence("Ctrl+*"), this);
    connect(sc_origin, &QShortcut::activated, this, &DisassemblyView::setOriginToSelection);

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
            auto* item_mark = new QTableWidgetItem("SRC");
            item_mark->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
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
        item_mark->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        if (insn.hasBreakpoint) {
            item_mark->setForeground(QColor(255, 80, 80));
        } else if (insn.isCurrentRip) {
            item_mark->setForeground(QColor(80, 220, 140));
        } else if (isBookmarked) {
            item_mark->setForeground(QColor(255, 215, 0));
        }

        // Add rich tooltip on mark item for call and jump instructions
        if (auto target = extractBranchTarget(insn, session)) {
            QString targetStr = target->toQString(true, ConfigurationManager::instance().appearance().showAddressColon);
            if (auto sym = session->symbols().findNearestSymbol(*target)) {
                if (sym->second == 0) {
                    targetStr += QString(" <%1>").arg(QString::fromStdString(sym->first.displayName()));
                } else {
                    targetStr += QString(" <%1+0x%2>").arg(QString::fromStdString(sym->first.displayName())).arg(sym->second, 0, 16);
                }
            }
            if (classifyMnemonic(insn.mnemonic) == MnemonicClass::Call) {
                item_mark->setToolTip(QString("CALL ➔ %1 (Double-click or Enter to follow)").arg(targetStr));
            } else {
                item_mark->setToolTip(QString("JUMP ➔ %1 (Double-click or Enter to follow)").arg(targetStr));
            }
        }

        // Column 1: Address
        auto* item_addr = new QTableWidgetItem(insn.address.toQString(true, ConfigurationManager::instance().appearance().showAddressColon));

        // Column 2: Bytes
        QString bytes_str;
        bytes_str.reserve(static_cast<int>(insn.bytes.size()) * 3);
        for (uint8_t b : insn.bytes) {
            bytes_str += QString::asprintf("%02x ", b);
        }
        auto* item_bytes = new QTableWidgetItem(bytes_str);
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
        auto session = session_.lock();
        if (auto target = extractBranchTarget(*insn, session)) {
            gotoAddress(*target);
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
        if (displayRows_[row].type != RowType::Instruction) {
            return std::nullopt;
        }
        size_t idx = displayRows_[row].insnIndex;
        if (idx < currentInstructions_.size()) {
            return currentInstructions_[idx].address;
        }
    }
    return std::nullopt;
}

const DisassembledInstruction* DisassemblyView::instructionAtRow(int row) const {
    if (row >= 0 && row < static_cast<int>(displayRows_.size())) {
        if (displayRows_[row].type != RowType::Instruction) {
            return nullptr;
        }
        size_t idx = displayRows_[row].insnIndex;
        if (idx < currentInstructions_.size()) {
            return &currentInstructions_[idx];
        }
    }
    return nullptr;
}

void DisassemblyView::handleCellDoubleClicked(int row, int col) {
    if (col == 0 || col == 3) {
        // Double clicking mark column or instruction column: follow branch/call if branch/call instruction!
        if (auto* insn = instructionAtRow(row)) {
            auto session = session_.lock();
            if (auto target = extractBranchTarget(*insn, session)) {
                gotoAddress(*target);
                return;
            }
        }
        if (col == 0) {
            if (auto addr = addressAtRow(row)) {
                if (auto session = session_.lock()) {
                    session->toggleBreakpoint(*addr);
                    refresh();
                    Q_EMIT breakpointToggled(*addr);
                }
            }
        }
        return;
    }

    if (col == 5) {
        // Double clicking comment column edits the comment!
        editCommentPrompt();
        return;
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

        auto* stackSub = menu.addMenu("Follow in Stack");
        stackSub->addAction(QString("Follow Selection (%1) in Stack").arg(QString::fromStdString(addr->toHex())), [this, addr]() {
            Q_EMIT jumpToStackRequested(*addr);
        });
        if (details.hasMemoryOperand && !details.effectiveAddress.isNull()) {
            Address memAddr = details.effectiveAddress;
            stackSub->addAction(QString("Follow Memory Address (%1) in Stack").arg(QString::fromStdString(memAddr.toHex())), [this, memAddr]() {
                Q_EMIT jumpToStackRequested(memAddr);
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
    Q_EMIT instructionInspected(QString::fromStdString(details.richSummary.empty() ? details.summary : details.richSummary));

    viewport()->update();
}

void DisassemblyView::paintEvent(QPaintEvent* event) {
    QTableWidget::paintEvent(event);
    QPainter painter(viewport());
    drawFlowLines(painter);
}

void DisassemblyView::scrollContentsBy(int dx, int dy) {
    QTableWidget::scrollContentsBy(dx, dy);
    viewport()->update();
}

struct FlowArrow {
    int fromRow{-1};
    int toRow{-1};
    Address fromAddr{0};
    Address toAddr{0};
    bool isCall{false};
    bool isJump{false};
    bool isConditional{false};
    bool isLoop{false};
    bool isSelected{false};
    bool isRip{false};
    bool branchTaken{false};
    int railIndex{0};
};

void DisassemblyView::drawFlowLines(QPainter& painter) {
    if (rowCount() == 0) return;

    int colX = columnViewportPosition(0);
    int colW = columnWidth(0);
    if (colW <= 20) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    auto isRowVisible = [this](int r) -> bool {
        if (r < 0 || r >= rowCount()) return false;
        int y = rowViewportPosition(r);
        int h = rowHeight(r);
        return (y + h > 0 && y < viewport()->height());
    };

    // Build address-to-row lookup for visible table rows
    std::unordered_map<uint64_t, int> addrToRow;
    Address minVisibleAddr(UINT64_MAX);
    Address maxVisibleAddr(0);

    for (int r = 0; r < rowCount(); ++r) {
        if (auto addr = addressAtRow(r)) {
            addrToRow[addr->value()] = r;
            if (isRowVisible(r)) {
                if (*addr < minVisibleAddr) minVisibleAddr = *addr;
                if (*addr > maxVisibleAddr) maxVisibleAddr = *addr;
            }
        }
    }

    auto session = session_.lock();
    int selRow = currentRow();

    std::vector<FlowArrow> arrows;

    for (int r = 0; r < rowCount(); ++r) {
        const auto* insn = instructionAtRow(r);
        if (!insn) continue;

        MnemonicClass mclass = classifyMnemonic(insn->mnemonic);
        bool isCall = (mclass == MnemonicClass::Call);
        bool isJump = (mclass == MnemonicClass::Jump || mclass == MnemonicClass::CondJump);
        if (!isCall && !isJump) continue;

        auto targetAddr = extractBranchTarget(*insn, session);
        if (!targetAddr || targetAddr->isNull()) continue;

        int toRow = -1;
        bool isLoop = false;
        auto it = addrToRow.find(targetAddr->value());
        if (it != addrToRow.end()) {
            toRow = it->second;
            isLoop = (toRow <= r);
        } else {
            if (*targetAddr < minVisibleAddr) {
                toRow = -1; // Above visible range
                isLoop = true;
            } else {
                toRow = INT_MAX; // Below visible range
                isLoop = false;
            }
        }

        bool fromVis = isRowVisible(r);
        bool toVis = (toRow >= 0 && toRow < rowCount() && isRowVisible(toRow));

        // Only draw lines if at least source or destination is currently visible in the viewport
        if (!fromVis && !toVis) {
            continue;
        }

        FlowArrow fa;
        fa.fromRow = r;
        fa.fromAddr = insn->address;
        fa.toAddr = *targetAddr;
        fa.toRow = toRow;
        fa.isCall = isCall;
        fa.isJump = isJump;
        fa.isConditional = (mclass == MnemonicClass::CondJump);
        fa.isLoop = isLoop;
        fa.isSelected = (selRow >= 0 && (r == selRow || toRow == selRow));
        fa.isRip = insn->isCurrentRip;

        if (session && (fa.isRip || fa.isSelected)) {
            auto details = session->inspectInstruction(insn->address);
            if (details.isBranch && details.isConditional) {
                fa.branchTaken = details.branchTaken;
            }
        }

        arrows.push_back(fa);
    }

    if (arrows.empty()) {
        painter.restore();
        return;
    }

    // Sort arrows by span length so shorter jumps get inner rails
    std::sort(arrows.begin(), arrows.end(), [](const FlowArrow& a, const FlowArrow& b) {
        int spanA = std::abs(a.toRow - a.fromRow);
        int spanB = std::abs(b.toRow - b.fromRow);
        return spanA < spanB;
    });

    // Greedy rail assignment (intervals) for up to 5 concurrent tracks
    struct RailInterval {
        int startRow;
        int endRow;
    };
    std::vector<std::vector<RailInterval>> rails;

    for (auto& fa : arrows) {
        int rStart = fa.fromRow;
        int rEnd = fa.toRow;
        if (rEnd == -1) rEnd = 0;
        else if (rEnd == INT_MAX) rEnd = rowCount() - 1;
        if (rStart > rEnd) std::swap(rStart, rEnd);
        rStart = std::clamp(rStart, 0, rowCount() - 1);
        rEnd = std::clamp(rEnd, 0, rowCount() - 1);

        int assignedRail = -1;
        for (size_t k = 0; k < rails.size(); ++k) {
            bool conflict = false;
            for (const auto& iv : rails[k]) {
                if (!(rEnd < iv.startRow || rStart > iv.endRow)) {
                    conflict = true;
                    break;
                }
            }
            if (!conflict) {
                assignedRail = static_cast<int>(k);
                rails[k].push_back({rStart, rEnd});
                break;
            }
        }
        if (assignedRail == -1) {
            assignedRail = static_cast<int>(rails.size());
            rails.push_back({{rStart, rEnd}});
        }
        fa.railIndex = std::min(assignedRail, 4); // Max 5 rails (0, 1, 2, 3, 4)
    }

    // Two-pass rendering: unselected first, selected lines on top
    for (int pass = 0; pass < 2; ++pass) {
        for (const auto& fa : arrows) {
            if ((pass == 0 && fa.isSelected) || (pass == 1 && !fa.isSelected)) {
                continue;
            }

            // Color scheme (x64dbg & edb aesthetic)
            QColor color;
            if (fa.isCall) {
                // Call relationship line: Neon Cyan
                color = fa.isSelected ? QColor(0, 255, 255) : QColor(0, 215, 245, 210);
            } else if (fa.isConditional) {
                if (fa.isRip && session) {
                    if (fa.branchTaken) {
                        color = fa.isSelected ? QColor(0, 255, 128) : QColor(0, 230, 118, 220); // Emerald Green
                    } else {
                        color = fa.isSelected ? QColor(170, 185, 195) : QColor(140, 155, 165, 180); // Slate Gray
                    }
                } else if (fa.isLoop) {
                    // Backward loop conditional jump: Coral Red
                    color = fa.isSelected ? QColor(255, 60, 60) : QColor(255, 82, 82, 210);
                } else {
                    // Forward conditional jump: Amber Orange
                    color = fa.isSelected ? QColor(255, 175, 0) : QColor(255, 152, 0, 210);
                }
            } else {
                // Unconditional jump: Golden Yellow
                color = fa.isSelected ? QColor(255, 235, 59) : QColor(255, 215, 0, 210);
            }

            int arrowRightX = colX + colW - 3;
            int railX = arrowRightX - 8 - fa.railIndex * 7;
            if (railX < colX + 28) railX = colX + 28;

            bool fromVis = isRowVisible(fa.fromRow);
            bool toVis = (fa.toRow >= 0 && fa.toRow < rowCount() && isRowVisible(fa.toRow));

            int fromY = fromVis ? (rowViewportPosition(fa.fromRow) + rowHeight(fa.fromRow) / 2) : 0;
            int toY = toVis ? (rowViewportPosition(fa.toRow) + rowHeight(fa.toRow) / 2) : 0;

            qreal penWidth = fa.isSelected ? 2.4 : 1.4;

            // Subtle glow under selected lines
            if (fa.isSelected) {
                QPen glowPen(QColor(color.red(), color.green(), color.blue(), 65));
                glowPen.setWidthF(penWidth + 3.8);
                glowPen.setCapStyle(Qt::RoundCap);
                glowPen.setJoinStyle(Qt::RoundJoin);
                painter.setPen(glowPen);

                QPainterPath glowPath;
                if (fromVis) {
                    glowPath.moveTo(arrowRightX - 3, fromY);
                    glowPath.lineTo(railX, fromY);
                    if (toVis) {
                        glowPath.lineTo(railX, toY);
                        glowPath.lineTo(arrowRightX, toY);
                    } else if (fa.toRow == -1 || fa.toRow < fa.fromRow) {
                        glowPath.lineTo(railX, 4);
                    } else {
                        glowPath.lineTo(railX, viewport()->height() - 4);
                    }
                } else if (toVis) {
                    int startY = (fa.fromRow < fa.toRow) ? 4 : (viewport()->height() - 4);
                    glowPath.moveTo(railX, startY);
                    glowPath.lineTo(railX, toY);
                    glowPath.lineTo(arrowRightX, toY);
                }
                painter.drawPath(glowPath);
            }

            QPen linePen(color, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            if (fa.isConditional && !fa.isLoop && !fa.isSelected) {
                linePen.setStyle(Qt::DashLine);
            }
            painter.setPen(linePen);
            painter.setBrush(Qt::NoBrush);

            QPainterPath path;
            if (fromVis) {
                // Source row is visible: draw origin dot and connector
                path.moveTo(arrowRightX - 3, fromY);
                path.lineTo(railX, fromY);

                painter.setBrush(color);
                painter.drawEllipse(QPointF(arrowRightX - 3, fromY), 2.2, 2.2);
                painter.setBrush(Qt::NoBrush);

                if (toVis) {
                    // Both visible: connect down/up to target
                    path.lineTo(railX, toY);
                    path.lineTo(arrowRightX - 5, toY);
                    painter.drawPath(path);

                    painter.setBrush(color);
                    QPolygonF arrowHead;
                    arrowHead << QPointF(arrowRightX, toY)
                              << QPointF(arrowRightX - 5.5, toY - 3.8)
                              << QPointF(arrowRightX - 5.5, toY + 3.8);
                    painter.drawPolygon(arrowHead);
                    painter.setBrush(Qt::NoBrush);

                    if (fa.isSelected) {
                        int rH = rowHeight(fa.toRow);
                        QRect targetRect(colX + 2, rowViewportPosition(fa.toRow) + 2, colW - 4, rH - 4);
                        painter.setPen(QPen(QColor(color.red(), color.green(), color.blue(), 140), 1.2, Qt::DashLine));
                        painter.drawRoundedRect(targetRect, 3, 3);
                    }
                } else if (fa.toRow == -1 || fa.toRow < fa.fromRow) {
                    // Target above visible range -> connect all the way to top border (y = 5)
                    int topTargetY = 5;
                    path.lineTo(railX, topTargetY);
                    painter.drawPath(path);

                    painter.setBrush(color);
                    QPolygonF upHead;
                    upHead << QPointF(railX, 2)
                           << QPointF(railX - 3.8, 7)
                           << QPointF(railX + 3.8, 7);
                    painter.drawPolygon(upHead);
                    painter.setBrush(Qt::NoBrush);
                } else {
                    // Target below visible range -> connect all the way to bottom border
                    int bottomTargetY = viewport()->height() - 5;
                    path.lineTo(railX, bottomTargetY);
                    painter.drawPath(path);

                    painter.setBrush(color);
                    QPolygonF downHead;
                    downHead << QPointF(railX, viewport()->height() - 2)
                             << QPointF(railX - 3.8, viewport()->height() - 7)
                             << QPointF(railX + 3.8, viewport()->height() - 7);
                    painter.drawPolygon(downHead);
                    painter.setBrush(Qt::NoBrush);
                }
            } else if (toVis) {
                // Incoming branch from offscreen into a visible instruction
                int enterY = (fa.fromRow < fa.toRow) ? 5 : (viewport()->height() - 5);
                path.moveTo(railX, enterY);
                path.lineTo(railX, toY);
                path.lineTo(arrowRightX - 5, toY);
                painter.drawPath(path);

                painter.setBrush(color);
                QPolygonF arrowHead;
                arrowHead << QPointF(arrowRightX, toY)
                          << QPointF(arrowRightX - 5.5, toY - 3.8)
                          << QPointF(arrowRightX - 5.5, toY + 3.8);
                painter.drawPolygon(arrowHead);
                painter.setBrush(Qt::NoBrush);

                if (fa.isSelected) {
                    int rH = rowHeight(fa.toRow);
                    QRect targetRect(colX + 2, rowViewportPosition(fa.toRow) + 2, colW - 4, rH - 4);
                    painter.setPen(QPen(QColor(color.red(), color.green(), color.blue(), 140), 1.2, Qt::DashLine));
                    painter.drawRoundedRect(targetRect, 3, 3);
                }
            }
        }
    }

    painter.restore();
}

} // namespace edb_next

