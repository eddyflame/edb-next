#pragma once

#include "core/DebugSession.hpp"
#include "core/IntermodularCallsFinder.hpp"
#include "IRefreshable.hpp"
#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <memory>
#include <vector>

namespace edb_next {

class IntermodularCallsView : public QWidget, public IRefreshable {
    Q_OBJECT

public:
    explicit IntermodularCallsView(QWidget* parent = nullptr);

    void setSession(std::shared_ptr<DebugSession> session);
    void refresh() override;

Q_SIGNALS:
    void jumpToAddressRequested(Address addr, bool isExec);

public Q_SLOTS:
    void handleScanClicked();

private Q_SLOTS:
    void handleFilterChanged(const QString& text);
    void handleCellDoubleClicked(int row, int col);

private:
    void setupUi();
    void renderTable();

    std::weak_ptr<DebugSession> session_;
    std::vector<IntermodularCall> allCalls_;
    std::vector<IntermodularCall> displayedCalls_;

    QLineEdit* filterEdit_{nullptr};
    QPushButton* scanBtn_{nullptr};
    QLabel* statusLabel_{nullptr};
    QTableWidget* table_{nullptr};
};

} // namespace edb_next
