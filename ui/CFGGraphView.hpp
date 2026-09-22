#pragma once

#include "core/Types.hpp"
#include "core/CFGBuilder.hpp"
#include "SugiyamaLayout.hpp"
#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QPushButton>
#include <memory>
#include <vector>
#include <string>
#include <optional>

namespace edb_next {

class DebugSession;

// Backward-compatibility alias
using CFGBasicBlock = CFGBlock;

class CFGGraphView : public QWidget {
    Q_OBJECT

public:
    explicit CFGGraphView(QWidget* parent = nullptr);
    ~CFGGraphView() override = default;

    void setSession(std::shared_ptr<DebugSession> session);
    void buildGraphForFunction(Address funcAddr);

Q_SIGNALS:
    void jumpToDisassemblyRequested(Address addr);

protected:
    void wheelEvent(QWheelEvent* event) override;

public Q_SLOTS:
    void onRefreshCurrent();

private Q_SLOTS:
    void onZoomIn();
    void onZoomOut();
    void onResetZoom();

private:
    void setupUi();
    void layoutAndDrawBlocks(const CFGGraph& graph);

    std::shared_ptr<DebugSession> session_;
    Address currentFuncAddr_{0};

    QGraphicsView* graphicsView_{nullptr};
    QGraphicsScene* scene_{nullptr};
};

} // namespace edb_next
