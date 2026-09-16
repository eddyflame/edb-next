#pragma once

#include "Types.hpp"
#include "ElfParser.hpp"
#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <memory>
#include <vector>

namespace edb_next {

class DebugSession;

class SymbolViewer : public QWidget {
    Q_OBJECT

public:
    explicit SymbolViewer(QWidget* parent = nullptr);

    void setSession(std::shared_ptr<DebugSession> session);
    void refresh();

Q_SIGNALS:
    void jumpToAddressRequested(Address addr);

private Q_SLOTS:
    void onFilterChanged(const QString& filter);
    void onCellDoubleClicked(int row, int col);

private:
    void setupUi();
    void updateTableDisplay();

    std::weak_ptr<DebugSession> session_;
    std::vector<SymbolInfo> cachedSymbols_;

    QLineEdit* filterEdit_{nullptr};
    QLabel* countLabel_{nullptr};
    QTableWidget* table_{nullptr};
};

} // namespace edb_next
