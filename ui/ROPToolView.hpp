#pragma once

#include "DebugSession.hpp"
#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <memory>

namespace edb_next {

class ROPToolView : public QWidget {
    Q_OBJECT

public:
    explicit ROPToolView(QWidget* parent = nullptr);
    ~ROPToolView() override = default;

    void setSession(std::shared_ptr<DebugSession> session);
    void scanGadgets();

Q_SIGNALS:
    void jumpToAddressRequested(Address addr);

private Q_SLOTS:
    void onScanClicked();
    void onFilterChanged();
    void onCellDoubleClicked(int row, int col);
    void onContextMenu(const QPoint& pos);

private:
    void setupUi();
    void displayGadgets(const std::vector<ROPGadget>& list);

    std::weak_ptr<DebugSession> session_;
    QLineEdit* filterEdit_{nullptr};
    QComboBox* categoryCombo_{nullptr};
    QSpinBox* spinMaxInsn_{nullptr};
    QPushButton* btnScan_{nullptr};
    QLabel* statusLabel_{nullptr};
    QTableWidget* table_{nullptr};

    std::vector<ROPGadget> allGadgets_;
    std::vector<ROPGadget> filteredGadgets_;
};

} // namespace edb_next
