#pragma once

#include "Types.hpp"
#include "Breakpoint.hpp"
#include "DebugSession.hpp"
#include <QWidget>
#include <QTableWidget>
#include <memory>
#include <vector>

namespace edb_next {

class BreakpointManagerView : public QWidget {
    Q_OBJECT

public:
    explicit BreakpointManagerView(QWidget* parent = nullptr);

    void setSession(std::shared_ptr<DebugSession> session);
    void refresh();

Q_SIGNALS:
    void jumpToAddressRequested(Address addr);
    void breakpointChanged();

private Q_SLOTS:
    void handleCellDoubleClicked(int row, int col);
    void handleItemChanged(QTableWidgetItem* item);
    void handleContextMenu(const QPoint& pos);
    void onAddBreakpointClicked();
    void onDeleteBreakpointClicked();
    void onToggleBreakpointClicked();
    void onEditConditionClicked();
    void onEditScriptActionClicked();

private:
    void setupUi();

    std::weak_ptr<DebugSession> session_;
    QTableWidget* table_{nullptr};
    std::vector<Breakpoint> currentBreakpoints_;
    bool isUpdatingTable_{false};
};

} // namespace edb_next
