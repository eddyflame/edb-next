#pragma once

#include "Types.hpp"
#include "StringScanner.hpp"
#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <memory>
#include <vector>

namespace edb_next {

class DebugSession;

class StringReferencesView : public QWidget {
    Q_OBJECT

public:
    explicit StringReferencesView(QWidget* parent = nullptr);

    void setSession(std::shared_ptr<DebugSession> session);
    void refresh();

Q_SIGNALS:
    void jumpToDisassemblyRequested(Address addr);
    void jumpToMemoryRequested(Address addr);

private Q_SLOTS:
    void onScanClicked();
    void onFilterChanged(const QString& filter);
    void onCellDoubleClicked(int row, int col);

private:
    void setupUi();
    void updateTableDisplay();

    std::weak_ptr<DebugSession> session_;
    std::vector<StringItem> cachedStrings_;

    QLineEdit* filterEdit_{nullptr};
    QPushButton* scanBtn_{nullptr};
    QLabel* statusLabel_{nullptr};
    QTableWidget* table_{nullptr};
};

} // namespace edb_next
