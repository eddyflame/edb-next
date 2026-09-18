#pragma once

#include "DebugSession.hpp"
#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <memory>

namespace edb_next {

class StackView : public QWidget {
    Q_OBJECT

public:
    explicit StackView(QWidget* parent = nullptr);
    ~StackView() override = default;

    void setSession(std::shared_ptr<DebugSession> session);
    void refresh();
    void setBaseAddress(Address addr);
    [[nodiscard]] Address baseAddress() const noexcept { return baseAddr_; }

Q_SIGNALS:
    void jumpToDisassemblyRequested(Address addr);
    void jumpToMemoryRequested(Address addr, int tabIndex = -1);
    void jumpToStackRequested(Address addr);

private Q_SLOTS:
    void onCellDoubleClicked(int row, int column);
    void onCustomContextMenuRequested(const QPoint& pos);
    void onGotoAddressClicked();
    void onSyncToRspClicked();
    void onSyncToRbpClicked();
    void onModifyValueClicked();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupUi();
    void updateTable();

    std::shared_ptr<DebugSession> session_;
    Address baseAddr_{0};
    bool autoSyncRsp_{true};

    QLabel* headerLabel_{nullptr};
    QPushButton* btnSyncRsp_{nullptr};
    QPushButton* btnSyncRbp_{nullptr};
    QPushButton* btnGoto_{nullptr};
    QTableWidget* table_{nullptr};
};

} // namespace edb_next
