#pragma once

#include "core/TraceEngine.hpp"
#include "IRefreshable.hpp"
#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <memory>

namespace edb_next {

class DebugSession;

class TraceView : public QWidget, public IRefreshable {
    Q_OBJECT

public:
    explicit TraceView(TraceEngine& traceEngine, QWidget* parent = nullptr);
    ~TraceView() override = default;

    void setSession(std::shared_ptr<DebugSession> session);
    void refresh() override;

Q_SIGNALS:
    void jumpToDisassemblyRequested(Address addr);

private Q_SLOTS:
    void onRefreshTable();
    void onClearRunTrace();
    void onClearHitTrace();
    void onRowDoubleClicked(int row, int column);
    void onAutoTracePrompt(bool stepOver);
    void onStepBack();
    void onStepForward();

private:
    void setupUi();

    TraceEngine& traceEngine_;
    std::shared_ptr<DebugSession> session_;

    QTableWidget* tableTrace_{nullptr};
    QLabel* lblCoverageStats_{nullptr};
    QCheckBox* chkEnableHit_{nullptr};
    QCheckBox* chkEnableRun_{nullptr};
    QPushButton* btnClearRun_{nullptr};
    QPushButton* btnClearHit_{nullptr};
    QPushButton* btnTraceInto_{nullptr};
    QPushButton* btnTraceOver_{nullptr};
    QPushButton* btnStepBack_{nullptr};
    QPushButton* btnStepForward_{nullptr};
};

} // namespace edb_next
