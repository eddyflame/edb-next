#pragma once

#include "core/Types.hpp"
#include "core/MemoryScanner.hpp"
#include "IRefreshable.hpp"
#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <memory>

namespace edb_next {

class DebugSession;

class MemoryScannerView : public QWidget, public IRefreshable {
    Q_OBJECT

public:
    explicit MemoryScannerView(QWidget* parent = nullptr);
    ~MemoryScannerView() override = default;

    void setSession(std::shared_ptr<DebugSession> session);
    void refreshResults();
    void refresh() override;

Q_SIGNALS:
    void jumpToMemoryRequested(edb_next::Address addr);
    void jumpToDisassemblyRequested(edb_next::Address addr);

private Q_SLOTS:
    void onFirstScanClicked();
    void onNextScanClicked();
    void onRefreshClicked();
    void onResetClicked();
    void onDataTypeChanged(int index);
    void onCompareTypeChanged(int index);
    void onTableDoubleClicked(int row, int col);
    void onTableContextMenu(const QPoint& pos);
    void editSelectedValue();

private:
    void setupUi();
    void updateButtons();
    ScanOptions collectOptions() const;

    std::shared_ptr<DebugSession> session_;

    QLineEdit* valueEdit_{nullptr};
    QLineEdit* deltaEdit_{nullptr};
    QComboBox* dataTypeCombo_{nullptr};
    QComboBox* compareTypeCombo_{nullptr};
    QComboBox* alignCombo_{nullptr};
    QCheckBox* writableOnlyCheck_{nullptr};

    QPushButton* firstScanBtn_{nullptr};
    QPushButton* nextScanBtn_{nullptr};
    QPushButton* refreshBtn_{nullptr};
    QPushButton* resetBtn_{nullptr};
    QLabel* statusLabel_{nullptr};

    QTableWidget* resultsTable_{nullptr};
};

} // namespace edb_next
