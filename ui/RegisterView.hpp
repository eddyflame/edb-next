#pragma once

#include "DebugSession.hpp"
#include <QWidget>
#include <QTableWidget>
#include <QTabWidget>
#include <QPushButton>
#include <memory>
#include <vector>

namespace edb_next {

class RegisterView : public QWidget {
    Q_OBJECT

public:
    explicit RegisterView(QWidget* parent = nullptr);

    void setSession(std::shared_ptr<DebugSession> session);
    void refresh();

Q_SIGNALS:
    void jumpToDisassemblyRequested(Address addr);
    void jumpToMemoryRequested(Address addr, int tabIndex = -1);
    void jumpToStackRequested(Address addr);

private Q_SLOTS:
    void handleGprDoubleClicked(int row, int col);
    void handleFpDoubleClicked(int row, int col);
    void handleFlagClicked(int bit);
    void handleGprContextMenu(const QPoint& pos);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupUi();
    void updateFlagsDisplay();
    void updateGprDisplay();
    void updateFpDisplay();

    void adjustSelectedGpr(int row, int64_t delta);
    void setSelectedGpr(int row, uint64_t val);
    void toggleSelectedGpr(int row);

    std::weak_ptr<DebugSession> session_;

    // UI Widgets
    QTabWidget* tabWidget_{nullptr};
    QTableWidget* gprTable_{nullptr};
    QTableWidget* fpTable_{nullptr};
    std::vector<std::pair<int, QPushButton*>> flagButtons_; // (bitIndex, button)
};

} // namespace edb_next
