#pragma once

#include "core/Types.hpp"
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

struct CFGInstruction {
    Address address{0};
    std::string mnemonic;
    std::string operands;
};

struct CFGBasicBlock {
    int id{0};
    Address startAddr{0};
    Address endAddr{0};
    std::vector<CFGInstruction> instructions;
    std::optional<Address> trueTarget;   // Branch taken (green)
    std::optional<Address> falseTarget;  // Branch not taken (red)
    std::optional<Address> directTarget; // Unconditional branch / call (blue)
};

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
    void layoutAndDrawBlocks(const std::vector<CFGBasicBlock>& blocks);

    std::shared_ptr<DebugSession> session_;
    Address currentFuncAddr_{0};

    QGraphicsView* graphicsView_{nullptr};
    QGraphicsScene* scene_{nullptr};
};

} // namespace edb_next
