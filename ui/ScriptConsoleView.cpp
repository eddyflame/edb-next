#include "ScriptConsoleView.hpp"
#include "DebugSession.hpp"
#include "core/ConfigurationManager.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QKeyEvent>
#include <QScrollBar>
#include <QFontDatabase>

namespace edb_next {

ScriptConsoleView::ScriptConsoleView(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

ScriptConsoleView::~ScriptConsoleView() = default;

void ScriptConsoleView::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // Top Control Bar
    auto* topBar = new QHBoxLayout();
    topBar->setSpacing(8);

    auto* langLabel = new QLabel("Engine:", this);
    langLabel->setStyleSheet("color: #90a4ae; font-weight: bold;");
    topBar->addWidget(langLabel);

    langCombo_ = new QComboBox(this);
    langCombo_->addItem("Python 3", "python");
    langCombo_->addItem("Lua 5.4", "lua");
    langCombo_->setStyleSheet(
        "QComboBox { background-color: #21252b; color: #61afef; border: 1px solid #3b4048; border-radius: 3px; padding: 3px 8px; font-weight: bold; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background-color: #21252b; color: #abb2bf; selection-background-color: #3e4451; }"
    );
    topBar->addWidget(langCombo_);

    runFileBtn_ = new QPushButton(QString::fromUtf8("\u25B6 Run File..."), this);
    runFileBtn_->setStyleSheet(
        "QPushButton { background-color: #2c313a; color: #98c379; border: 1px solid #3e4451; border-radius: 3px; padding: 3px 10px; font-weight: bold; }"
        "QPushButton:hover { background-color: #353b45; border-color: #98c379; }"
    );
    connect(runFileBtn_, &QPushButton::clicked, this, &ScriptConsoleView::loadAndRunFile);
    topBar->addWidget(runFileBtn_);

    clearBtn_ = new QPushButton("Clear", this);
    clearBtn_->setStyleSheet(
        "QPushButton { background-color: #2c313a; color: #e06c75; border: 1px solid #3e4451; border-radius: 3px; padding: 3px 8px; }"
        "QPushButton:hover { background-color: #353b45; border-color: #e06c75; }"
    );
    connect(clearBtn_, &QPushButton::clicked, this, &ScriptConsoleView::clearConsole);
    topBar->addWidget(clearBtn_);

    topBar->addStretch(1);

    statusLabel_ = new QLabel("Ready", this);
    statusLabel_->setStyleSheet("color: #5c6370; font-size: 9pt; font-family: monospace;");
    topBar->addWidget(statusLabel_);

    mainLayout->addLayout(topBar);

    // Console Output Area
    outputView_ = new QPlainTextEdit(this);
    outputView_->setReadOnly(true);
    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(10);
    outputView_->setFont(monoFont);
    outputView_->setStyleSheet(
        "QPlainTextEdit { background-color: #1e1e24; color: #abb2bf; border: 1px solid #282c34; border-radius: 4px; line-height: 1.3; }"
    );
    mainLayout->addWidget(outputView_, 1);

    // Input Bar
    auto* inputBar = new QHBoxLayout();
    inputBar->setSpacing(6);

    inputEdit_ = new QLineEdit(this);
    inputEdit_->setFont(monoFont);
    inputEdit_->setPlaceholderText("Enter script expression or code... (e.g. print(edb.get_regs()))");
    inputEdit_->setStyleSheet(
        "QLineEdit { background-color: #21252b; color: #00e5ff; border: 1px solid #3b4048; border-radius: 3px; padding: 4px 8px; }"
        "QLineEdit:focus { border: 1px solid #61afef; }"
    );
    inputEdit_->installEventFilter(this);
    connect(inputEdit_, &QLineEdit::returnPressed, this, &ScriptConsoleView::executeCurrentInput);
    inputBar->addWidget(inputEdit_, 1);

    executeBtn_ = new QPushButton(QString::fromUtf8("\u23CE Exec"), this);
    executeBtn_->setStyleSheet(
        "QPushButton { background-color: #3e4451; color: #61afef; border: 1px solid #4b5263; border-radius: 3px; padding: 4px 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #4b5263; color: #528bff; }"
    );
    connect(executeBtn_, &QPushButton::clicked, this, &ScriptConsoleView::executeCurrentInput);
    inputBar->addWidget(executeBtn_);

    mainLayout->addLayout(inputBar);

    connect(langCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int /*index*/) {
        updateEngineStatus();
    });

    // Welcome banner
    appendLog("=== edb-next Embedded Scripting Engine ===", "#98c379", true);
    appendLog("Supported: Python 3 (full security ecosystem) & Lua 5.4 (ultra-fast hooks)", "#5c6370");
    appendLog("API namespace: 'edb' (e.g. edb.get_regs(), edb.read_memory(addr, size), edb.step_into())", "#61afef");
    appendLog("", "#abb2bf");

    updateEngineStatus();
}

void ScriptConsoleView::setSession(DebugSession* session) {
    session_ = session;
    updateEngineStatus();
}

void ScriptConsoleView::updateEngineStatus() {
    QString lang = langCombo_->currentData().toString();
    if (lang == "lua") {
        inputEdit_->setPlaceholderText("Enter Lua 5.4 code... (e.g. print('RIP: ', string.format('0x%x', edb.get_reg('rip'))))");
        statusLabel_->setText(session_ ? "Lua 5.4 (Active Session Attached)" : "Lua 5.4 (No Session)");
    } else {
        inputEdit_->setPlaceholderText("Enter Python 3 code... (e.g. print({k: hex(v) for k, v in edb.get_regs().items()}))");
        statusLabel_->setText(session_ ? "Python 3.12 (Active Session Attached)" : "Python 3.12 (No Session)");
    }
}

void ScriptConsoleView::appendLog(const QString& text, const QString& colorHex, bool isBold) {
    QString escaped = text.toHtmlEscaped();
    escaped.replace("\n", "<br>");
    escaped.replace(" ", "&nbsp;");
    QString style = QString("color: %1;%2").arg(colorHex, isBold ? " font-weight: bold;" : "");
    outputView_->appendHtml(QString("<span style=\"%1\">%2</span>").arg(style, escaped));
    outputView_->verticalScrollBar()->setValue(outputView_->verticalScrollBar()->maximum());
}

void ScriptConsoleView::clearConsole() {
    outputView_->clear();
}

void ScriptConsoleView::executeCurrentInput() {
    QString code = inputEdit_->text().trimmed();
    if (code.isEmpty()) {
        return;
    }

    history_.append(code);
    historyIndex_ = history_.size();
    inputEdit_->clear();

    QString lang = langCombo_->currentData().toString();
    executeScript(lang, code);
}

void ScriptConsoleView::executeScript(const QString& lang, const QString& code) {
    QString prompt = (lang == "lua") ? "lua> " : ">>> ";
    appendLog(prompt + code, "#00e5ff", true);

    ScriptResult res;
    if (session_) {
        res = session_->scriptEngines().execute(lang.toStdString(), code.toStdString());
    } else {
        // Fallback standalone execution if no active session
        ScriptEngineManager mgr;
        res = mgr.execute(lang.toStdString(), code.toStdString());
    }

    if (!res.output.empty()) {
        appendLog(QString::fromStdString(res.output), "#eceff1");
    }

    if (!res.error.empty()) {
        appendLog(QString::fromStdString(res.error), "#ff5252");
    }

    if (!res.success && res.error.empty()) {
        appendLog("[Error] Script execution failed", "#ff5252");
    }
}

void ScriptConsoleView::loadAndRunFile() {
    QString filter = "Script Files (*.py *.lua);;Python Scripts (*.py);;Lua Scripts (*.lua);;All Files (*)";
    QString initialDir = ConfigurationManager::instance().directories().scriptDir;
    QString path = QFileDialog::getOpenFileName(this, "Select Script to Run", initialDir, filter);
    if (path.isEmpty()) return;

    appendLog(QString("[Loading Script File: %1]").arg(path), "#ffd740", true);

    ScriptResult res;
    if (session_) {
        res = session_->scriptEngines().executeFile(path.toStdString());
    } else {
        ScriptEngineManager mgr;
        res = mgr.executeFile(path.toStdString());
    }

    if (!res.output.empty()) {
        appendLog(QString::fromStdString(res.output), "#eceff1");
    }

    if (!res.error.empty()) {
        appendLog(QString::fromStdString(res.error), "#ff5252");
    }

    if (res.success) {
        appendLog(QString("[Script %1 executed successfully]").arg(path), "#98c379");
    } else {
        appendLog(QString("[Script %1 failed]").arg(path), "#ff5252");
    }
}

bool ScriptConsoleView::eventFilter(QObject* watched, QEvent* event) {
    if (watched == inputEdit_ && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Up) {
            if (!history_.isEmpty()) {
                if (historyIndex_ > 0) {
                    --historyIndex_;
                    inputEdit_->setText(history_[historyIndex_]);
                }
                return true;
            }
        } else if (keyEvent->key() == Qt::Key_Down) {
            if (!history_.isEmpty()) {
                if (historyIndex_ + 1 < history_.size()) {
                    ++historyIndex_;
                    inputEdit_->setText(history_[historyIndex_]);
                } else {
                    historyIndex_ = history_.size();
                    inputEdit_->clear();
                }
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace edb_next
