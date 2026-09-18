#pragma once

#include "DebugSession.hpp"
#include "IRefreshable.hpp"
#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <memory>

namespace edb_next {

class ThreadsView : public QWidget, public IRefreshable {
    Q_OBJECT

public:
    explicit ThreadsView(QWidget* parent = nullptr);
    ~ThreadsView() override = default;

    void setSession(std::shared_ptr<DebugSession> session);
    void refresh() override;

Q_SIGNALS:
    void jumpToAddressRequested(Address addr);
    void threadSwitched(Tid tid);

private Q_SLOTS:
    void onCellDoubleClicked(int row, int column);
    void onCustomContextMenu(const QPoint& pos);
    void onSwitchThreadClicked();
    void onFreezeThawClicked();
    void onFreezeAllClicked();
    void onThawAllClicked();

private:
    void setupUi();

    std::weak_ptr<DebugSession> session_;
    QTableWidget* table_{nullptr};
    QPushButton* btnRefresh_{nullptr};
    QPushButton* btnSwitch_{nullptr};
    QPushButton* btnFreezeThaw_{nullptr};
    QPushButton* btnFreezeAll_{nullptr};
    QPushButton* btnThawAll_{nullptr};
    QLabel* statusLabel_{nullptr};
    std::vector<ThreadInfo> currentThreads_;
};

} // namespace edb_next
