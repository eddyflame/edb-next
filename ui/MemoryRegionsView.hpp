#pragma once

#include "Types.hpp"
#include "DebugSession.hpp"
#include "IRefreshable.hpp"
#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <memory>
#include <vector>

namespace edb_next {

class MemoryRegionsView : public QWidget, public IRefreshable {
    Q_OBJECT

public:
    explicit MemoryRegionsView(QWidget* parent = nullptr);

    void setSession(std::shared_ptr<DebugSession> session);
    void refresh() override;

Q_SIGNALS:
    void jumpToAddressRequested(Address addr, bool isExecutable);

private Q_SLOTS:
    void handleCellDoubleClicked(int row, int col);
    void handleFilterChanged(const QString& text);
    void handleCustomContextMenu(const QPoint& pos);
    void dumpSelectedRegion();
    void changePermissionsPrompt();
    void allocateMemoryPrompt();
    void freeMemoryPrompt();

private:
    void setupUi();
    void renderTable();

    std::weak_ptr<DebugSession> session_;
    QLineEdit* filterEdit_{nullptr};
    QTableWidget* table_{nullptr};
    std::vector<MemoryRegion> allRegions_;
    std::vector<MemoryRegion> displayedRegions_;
};

} // namespace edb_next
