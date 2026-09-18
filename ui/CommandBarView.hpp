#pragma once

#include "Types.hpp"
#include "CommandRegistry.hpp"
#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QStringList>
#include <QCompleter>
#include <functional>
#include <memory>

namespace edb_next {

class DebugSession;

class CommandBarView : public QWidget {
    Q_OBJECT

public:
    explicit CommandBarView(QWidget* parent = nullptr);
    ~CommandBarView() override = default;

    void setSession(std::shared_ptr<DebugSession> session);

    [[nodiscard]] CommandRegistry& registry() noexcept { return registry_; }
    [[nodiscard]] const CommandRegistry& registry() const noexcept { return registry_; }

    // Register custom commands (e.g. from plugins!)
    void registerCommand(const std::string& cmd,
                         std::function<void(const std::vector<std::string>&)> handler,
                         const std::string& helpText = "");

    void executeCommand(const QString& line);

Q_SIGNALS:
    void outputLogged(const QString& msg, bool isError);
    void jumpToDisassemblyRequested(Address addr);
    void jumpToMemoryRequested(Address addr);
    void switchSessionRequested(const QString& idOrPid);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private Q_SLOTS:
    void onReturnPressed();

private:
    void setupUi();
    void updateCompleter();
    Address parseAddress(const std::string& token);
    [[nodiscard]] CommandContext createCommandContext();

    std::shared_ptr<DebugSession> session_;

    QLabel* promptLabel_{nullptr};
    QLineEdit* cmdInput_{nullptr};
    QCompleter* completer_{nullptr};

    QStringList history_;
    int historyIndex_{-1};

    CommandRegistry registry_;
};

} // namespace edb_next

