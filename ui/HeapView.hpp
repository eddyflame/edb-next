#pragma once

#include "Types.hpp"
#include "HeapAnalyzer.hpp"
#include "IRefreshable.hpp"
#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <memory>
#include <vector>

namespace edb_next {

class DebugSession;

class HeapView : public QWidget, public IRefreshable {
    Q_OBJECT

public:
    explicit HeapView(QWidget* parent = nullptr);

    void setSession(std::shared_ptr<DebugSession> session);
    void refresh() override;

Q_SIGNALS:
    void jumpToMemoryRequested(Address addr);

private Q_SLOTS:
    void onAnalyzeClicked();
    void onFilterChanged(const QString& filter);
    void onCellDoubleClicked(int row, int col);

private:
    void setupUi();
    void updateTableDisplay();

    std::weak_ptr<DebugSession> session_;
    std::vector<HeapChunkInfo> cachedChunks_;

    QPushButton* analyzeBtn_{nullptr};
    QLineEdit* filterEdit_{nullptr};
    QLabel* statusLabel_{nullptr};
    QTableWidget* table_{nullptr};
};

} // namespace edb_next
