#include "DecompilerView.hpp"
#include "DebugSession.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QTextBlock>
#include <QTextCursor>

namespace edb_next {

CDecompilerHighlighter::CDecompilerHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent) {
    // 1. Keywords
    keywordFormat_.setForeground(QColor("#c586c0")); // Purple
    keywordFormat_.setFontWeight(QFont::Bold);
    const QString keywordPatterns[] = {
        QStringLiteral("\\bif\\b"), QStringLiteral("\\belse\\b"), QStringLiteral("\\bwhile\\b"),
        QStringLiteral("\\bfor\\b"), QStringLiteral("\\bdo\\b"), QStringLiteral("\\bswitch\\b"),
        QStringLiteral("\\bcase\\b"), QStringLiteral("\\bbreak\\b"), QStringLiteral("\\bcontinue\\b"),
        QStringLiteral("\\breturn\\b"), QStringLiteral("\\bgoto\\b"), QStringLiteral("\\bsyscall\\b")
    };
    for (const auto& pattern : keywordPatterns) {
        rules_.append({QRegularExpression(pattern), keywordFormat_});
    }

    // 2. Types
    typeFormat_.setForeground(QColor("#569cd6")); // Blue/Cyan
    typeFormat_.setFontWeight(QFont::Bold);
    const QString typePatterns[] = {
        QStringLiteral("\\bint64_t\\b"), QStringLiteral("\\buint64_t\\b"), QStringLiteral("\\bint32_t\\b"),
        QStringLiteral("\\buint32_t\\b"), QStringLiteral("\\bint16_t\\b"), QStringLiteral("\\buint16_t\\b"),
        QStringLiteral("\\bint8_t\\b"), QStringLiteral("\\buint8_t\\b"), QStringLiteral("\\bvoid\\b"),
        QStringLiteral("\\bsize_t\\b")
    };
    for (const auto& pattern : typePatterns) {
        rules_.append({QRegularExpression(pattern), typeFormat_});
    }

    // 3. Labels
    labelFormat_.setForeground(QColor("#4ec9b0")); // Teal
    labelFormat_.setFontWeight(QFont::Bold);
    rules_.append({QRegularExpression(QStringLiteral("^\\s*loc_[0-9a-fA-F]+:")), labelFormat_});

    // 4. Function calls
    functionFormat_.setForeground(QColor("#dcdcaa")); // Yellow
    rules_.append({QRegularExpression(QStringLiteral("\\b[a-zA-Z_][a-zA-Z0-9_]*(?=\\()")), functionFormat_});

    // 5. Numbers
    numberFormat_.setForeground(QColor("#b5cea8")); // Light Green
    rules_.append({QRegularExpression(QStringLiteral("\\b0x[0-9a-fA-F]+\\b")), numberFormat_});
    rules_.append({QRegularExpression(QStringLiteral("\\b[0-9]+\\b")), numberFormat_});

    // 6. Comments
    commentFormat_.setForeground(QColor("#6a9955")); // Muted Green
    commentFormat_.setFontItalic(true);
    rules_.append({QRegularExpression(QStringLiteral("//[^\n]*")), commentFormat_});
}

void CDecompilerHighlighter::highlightBlock(const QString& text) {
    for (const auto& rule : rules_) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}

DecompilerView::DecompilerView(NavigationBus* navBus, QWidget* parent)
    : QWidget(parent), navBus_(navBus) {
    setupUi();
}

DecompilerView::~DecompilerView() = default;

void DecompilerView::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Top control bar
    auto* topBar = new QHBoxLayout();
    topBar->setContentsMargins(0, 0, 0, 0);

    infoLabel_ = new QLabel(QStringLiteral("Decompiler (F5): No function decompiled"), this);
    infoLabel_->setStyleSheet(QStringLiteral("font-weight: bold; color: #9cdcfe; font-size: 11px;"));
    topBar->addWidget(infoLabel_);

    topBar->addStretch();

    btnRefresh_ = new QPushButton(QStringLiteral("Refresh Decompilation (F5)"), this);
    btnRefresh_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #2d2d30; color: #cccccc; border: 1px solid #3e3e42; padding: 3px 8px; border-radius: 2px; font-size: 11px; }"
        "QPushButton:hover { background: #3e3e42; color: #ffffff; }"
    ));
    connect(btnRefresh_, &QPushButton::clicked, this, &DecompilerView::onRefreshClicked);
    topBar->addWidget(btnRefresh_);

    mainLayout->addLayout(topBar);

    // Code Editor
    editor_ = new QPlainTextEdit(this);
    editor_->setReadOnly(true);
    editor_->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background-color: #1e1e1e; color: #d4d4d4; font-family: 'Fira Code', 'Cascadia Code', 'Consolas', monospace; font-size: 12px; border: 1px solid #2d2d30; }"
    ));

    QFont font(QStringLiteral("monospace"));
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(10);
    editor_->setFont(font);

    highlighter_ = new CDecompilerHighlighter(editor_->document());

    connect(editor_, &QPlainTextEdit::cursorPositionChanged, this, &DecompilerView::onCursorPositionChanged);

    mainLayout->addWidget(editor_);
}

void DecompilerView::setDecompiledFunction(const DecompiledFunction& fn) {
    currentFn_ = fn;
    isSyncing_ = true;
    editor_->setPlainText(QString::fromStdString(fn.pseudoCode));
    isSyncing_ = false;

    infoLabel_->setText(QString::asprintf("Decompiled Function: %s @ 0x%016llx (%zu lines)",
                                         fn.name.c_str(),
                                         static_cast<unsigned long long>(fn.entryAddress.value()),
                                         fn.sourceMap.size()));
}

void DecompilerView::decompileAt(std::shared_ptr<DebugSession> session, Address addr) {
    if (!session || addr.isNull()) return;

    currentSession_ = session;
    currentTargetAddr_ = addr;

    // Disassemble block/stream around addr
    auto insns = session->disassemble(addr, 40);
    if (insns.empty()) return;

    std::string funcName = "sub_" + addr.toHex(false);
    DecompiledFunction fn = DecompilerEngine::decompile(insns, funcName);
    setDecompiledFunction(fn);
}

void DecompilerView::highlightAddress(Address addr) {
    auto lineOpt = currentFn_.lineForAddress(addr);
    if (!lineOpt.has_value()) return;

    int targetLine = *lineOpt;
    isSyncing_ = true;

    QTextBlock block = editor_->document()->findBlockByLineNumber(targetLine - 1);
    if (block.isValid()) {
        QTextCursor cursor(block);
        editor_->setTextCursor(cursor);
        editor_->centerCursor();
    }

    isSyncing_ = false;
}

void DecompilerView::onCursorPositionChanged() {
    if (isSyncing_) return;

    QTextCursor cursor = editor_->textCursor();
    int line = cursor.blockNumber() + 1; // 1-based line number

    auto addrOpt = currentFn_.addressForLine(line);
    if (addrOpt.has_value() && !addrOpt->isNull()) {
        Q_EMIT addressSelected(*addrOpt);
        if (navBus_) {
            navBus_->requestDisassembly(*addrOpt);
        }
    }
}

void DecompilerView::onRefreshClicked() {
    if (currentSession_ && !currentTargetAddr_.isNull()) {
        decompileAt(currentSession_, currentTargetAddr_);
    }
}

} // namespace edb_next
