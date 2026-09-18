#include "CommandBarView.hpp"
#include "DebugSession.hpp"
#include "ExpressionEvaluator.hpp"
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QStringListModel>
#include <cctype>

namespace edb_next {

CommandBarView::CommandBarView(QWidget* parent) : QWidget(parent) {
    setupUi();
    updateCompleter();
}

void CommandBarView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
}

void CommandBarView::setupUi() {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(6);

    promptLabel_ = new QLabel("edb-next >", this);
    promptLabel_->setStyleSheet("font-weight: bold; color: #42a5f5; font-family: Monospace; font-size: 10pt;");
    layout->addWidget(promptLabel_);

    cmdInput_ = new QLineEdit(this);
    cmdInput_->setPlaceholderText("Enter command (e.g. 'bp main', 'r rax = 0x42', 'd rsp', 'step', 'help')...");
    cmdInput_->setFont(QFont("Monospace", 9));
    cmdInput_->installEventFilter(this);
    connect(cmdInput_, &QLineEdit::returnPressed, this, &CommandBarView::onReturnPressed);
    layout->addWidget(cmdInput_, 1);
}

void CommandBarView::updateCompleter() {
    auto matches = registry_.complete("");
    QStringList wordList;
    wordList.reserve(static_cast<qsizetype>(matches.size()));
    for (const auto& m : matches) {
        wordList.append(QString::fromStdString(m));
    }

    if (!completer_) {
        completer_ = new QCompleter(wordList, this);
        completer_->setCaseSensitivity(Qt::CaseInsensitive);
        completer_->setFilterMode(Qt::MatchStartsWith);
        cmdInput_->setCompleter(completer_);
    } else {
        completer_->setModel(new QStringListModel(wordList, completer_));
    }
}

void CommandBarView::registerCommand(const std::string& cmd,
                                     std::function<void(const std::vector<std::string>&)> handler,
                                     const std::string& helpText)
{
    registry_.registerCommand(cmd, std::move(handler), helpText, CommandCategory::Plugin);
    updateCompleter();
}

bool CommandBarView::eventFilter(QObject* obj, QEvent* event) {
    if (obj == cmdInput_ && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Up) {
            if (!history_.isEmpty() && historyIndex_ > 0) {
                historyIndex_--;
                cmdInput_->setText(history_[historyIndex_]);
            } else if (!history_.isEmpty() && historyIndex_ == -1) {
                historyIndex_ = history_.size() - 1;
                cmdInput_->setText(history_[historyIndex_]);
            }
            return true;
        } else if (keyEvent->key() == Qt::Key_Down) {
            if (!history_.isEmpty() && historyIndex_ < history_.size() - 1 && historyIndex_ >= 0) {
                historyIndex_++;
                cmdInput_->setText(history_[historyIndex_]);
            } else {
                historyIndex_ = -1;
                cmdInput_->clear();
            }
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void CommandBarView::onReturnPressed() {
    QString raw = cmdInput_->text().trimmed();
    if (raw.isEmpty()) return;

    cmdInput_->clear();
    history_.append(raw);
    historyIndex_ = -1;

    executeCommand(raw);
}

void CommandBarView::executeCommand(const QString& line) {
    if (line.trimmed().isEmpty()) return;
    registry_.execute(line.toStdString(), createCommandContext());
}

CommandContext CommandBarView::createCommandContext() {
    CommandContext ctx;
    ctx.session = session_;
    ctx.addressParser = [this](const std::string& token) {
        return parseAddress(token);
    };
    ctx.jumpToDisassembly = [this](Address addr) {
        Q_EMIT jumpToDisassemblyRequested(addr);
    };
    ctx.jumpToMemory = [this](Address addr) {
        Q_EMIT jumpToMemoryRequested(addr);
    };
    ctx.switchSession = [this](const QString& idOrPid) {
        Q_EMIT switchSessionRequested(idOrPid);
    };
    ctx.outputLogger = [this](const QString& msg, bool isError) {
        Q_EMIT outputLogged(msg, isError);
    };
    ctx.commandExecutor = [this](const QString& cmdLine) {
        executeCommand(cmdLine);
    };
    return ctx;
}

Address CommandBarView::parseAddress(const std::string& token) {
    if (!session_) return Address(0);

    // Try resolving as symbol
    auto sym = session_->resolveSymbol(token);
    if (sym.has_value()) {
        return *sym;
    }

    // Try evaluating as expression (supports registers and arithmetic)
    auto res = ExpressionEvaluator::evaluate(token, session_->registers(), nullptr);
    if (res.has_value()) {
        return Address(*res);
    }

    // Try parsing hex or decimal
    uint64_t val = 0;
    if (token.rfind("0x", 0) == 0 || token.rfind("0X", 0) == 0) {
        val = std::strtoull(token.c_str(), nullptr, 16);
    } else {
        val = std::strtoull(token.c_str(), nullptr, 0);
    }
    return Address(val);
}

} // namespace edb_next
