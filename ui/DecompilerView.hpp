#pragma once

#include "Types.hpp"
#include "DecompilerEngine.hpp"
#include "NavigationBus.hpp"
#include <QWidget>
#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QLabel>
#include <QPushButton>
#include <memory>

namespace edb_next {

class DebugSession;

class CDecompilerHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit CDecompilerHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> rules_;
    QTextCharFormat keywordFormat_;
    QTextCharFormat typeFormat_;
    QTextCharFormat labelFormat_;
    QTextCharFormat numberFormat_;
    QTextCharFormat commentFormat_;
    QTextCharFormat functionFormat_;
};

class DecompilerView : public QWidget {
    Q_OBJECT

public:
    explicit DecompilerView(NavigationBus* navBus = nullptr, QWidget* parent = nullptr);
    ~DecompilerView() override;

    void setDecompiledFunction(const DecompiledFunction& fn);
    void decompileAt(std::shared_ptr<DebugSession> session, Address addr);
    void highlightAddress(Address addr);

    [[nodiscard]] const DecompiledFunction& decompiledFunction() const noexcept { return currentFn_; }

Q_SIGNALS:
    void addressSelected(Address addr);

private Q_SLOTS:
    void onCursorPositionChanged();
    void onRefreshClicked();

private:
    void setupUi();

    NavigationBus* navBus_{nullptr};
    std::shared_ptr<DebugSession> currentSession_;
    Address currentTargetAddr_{0};
    DecompiledFunction currentFn_;

    QPlainTextEdit* editor_{nullptr};
    CDecompilerHighlighter* highlighter_{nullptr};
    QLabel* infoLabel_{nullptr};
    QPushButton* btnRefresh_{nullptr};
    bool isSyncing_{false};
};

} // namespace edb_next
