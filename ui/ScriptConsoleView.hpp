#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include <memory>

class QPlainTextEdit;
class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;

namespace edb_next {

class DebugSession;

class ScriptConsoleView : public QWidget {
    Q_OBJECT

public:
    explicit ScriptConsoleView(QWidget* parent = nullptr);
    ~ScriptConsoleView() override;

    void setSession(DebugSession* session);

public Q_SLOTS:
    void executeCurrentInput();
    void executeScript(const QString& lang, const QString& code);
    void loadAndRunFile();
    void clearConsole();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupUi();
    void appendLog(const QString& text, const QString& colorHex = "#eceff1", bool isBold = false);
    void updateEngineStatus();

    DebugSession* session_{nullptr};

    QComboBox* langCombo_{nullptr};
    QPushButton* runFileBtn_{nullptr};
    QPushButton* clearBtn_{nullptr};
    QLabel* statusLabel_{nullptr};

    QPlainTextEdit* outputView_{nullptr};
    QLineEdit* inputEdit_{nullptr};
    QPushButton* executeBtn_{nullptr};

    QStringList history_;
    int historyIndex_{-1};
};

} // namespace edb_next
