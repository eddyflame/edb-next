#pragma once

#include "core/Types.hpp"
#include "core/OpcodeSearcher.hpp"
#include "IRefreshable.hpp"
#include <QWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <memory>
#include <vector>

namespace edb_next {

class DebugSession;

class OpcodeSearcherView : public QWidget, public IRefreshable {
    Q_OBJECT

public:
    explicit OpcodeSearcherView(QWidget* parent = nullptr);
    ~OpcodeSearcherView() override = default;

    void setSession(std::shared_ptr<DebugSession> session);
    void refresh() override;

Q_SIGNALS:
    void jumpToDisassemblyRequested(Address addr);

private Q_SLOTS:
    void onSearchClicked();
    void onPatternTypeChanged(int index);
    void onFilterChanged(const QString& text);
    void onCellDoubleClicked(int row, int col);
    void handleCustomContextMenu(const QPoint& pos);

private:
    void setupUi();
    void renderTable();

    std::weak_ptr<DebugSession> session_;
    QComboBox* typeCombo_{nullptr};
    QLineEdit* queryEdit_{nullptr};
    QPushButton* searchBtn_{nullptr};
    QLineEdit* filterEdit_{nullptr};
    QTableWidget* table_{nullptr};

    std::vector<OpcodeSearchResult> allResults_;
    std::vector<OpcodeSearchResult> displayedResults_;
};

} // namespace edb_next
