#pragma once

#include "Types.hpp"
#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QStringList>
#include <functional>
#include <map>
#include <memory>

namespace edb_next {

class DebugSession;

class CommandBarView : public QWidget {
    Q_OBJECT

public:
    explicit CommandBarView(QWidget* parent = nullptr);
    ~CommandBarView() override = default;

    void setSession(std::shared_ptr<DebugSession> session);

    // Register custom commands (e.g. from plugins!)
    void registerCommand(const std::string& cmd,
                         std::function<void(const std::vector<std::string>&)> handler,
                         const std::string& helpText = "");

    void executeCommand(const QString& line);

Q_SIGNALS:
    void outputLogged(const QString& msg, bool isError);
    void jumpToDisassemblyRequested(Address addr);
    void jumpToMemoryRequested(Address addr);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private Q_SLOTS:
    void onReturnPressed();

private:
    void setupUi();
    void setupDefaultCommands();
    Address parseAddress(const std::string& token);

    std::shared_ptr<DebugSession> session_;

    QLabel* promptLabel_{nullptr};
    QLineEdit* cmdInput_{nullptr};

    QStringList history_;
    int historyIndex_{-1};

    struct CommandEntry {
        std::function<void(const std::vector<std::string>&)> handler;
        std::string help;
    };
    std::map<std::string, CommandEntry> commands_;
};

} // namespace edb_next
