#pragma once

#include "Types.hpp"
#include "CallStackUnwinder.hpp"
#include "DebugSession.hpp"
#include <QTableWidget>
#include <memory>
#include <vector>

namespace edb_next {

class CallStackView : public QTableWidget {
    Q_OBJECT

public:
    explicit CallStackView(QWidget* parent = nullptr);

    void setSession(std::shared_ptr<DebugSession> session);
    void refresh();

Q_SIGNALS:
    void jumpToAddressRequested(Address addr);

private Q_SLOTS:
    void handleCellDoubleClicked(int row, int col);
    void handleContextMenu(const QPoint& pos);

private:
    void setupUi();

    std::weak_ptr<DebugSession> session_;
    std::vector<StackFrame> currentFrames_;
};

} // namespace edb_next
