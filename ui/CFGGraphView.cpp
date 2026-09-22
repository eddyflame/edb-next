#include "CFGGraphView.hpp"
#include "DebugSession.hpp"
#include "FunctionFinder.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QGraphicsPathItem>
#include <QWheelEvent>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <map>
#include <cmath>

namespace edb_next {

class ClickableBlockItem : public QGraphicsRectItem {
public:
    ClickableBlockItem(Address addr, const QRectF& rect, std::function<void(Address)> onDblClick)
        : QGraphicsRectItem(rect), addr_(addr), callback_(onDblClick)
    {
        setFlag(QGraphicsItem::ItemIsSelectable);
    }

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*) override {
        if (callback_) callback_(addr_);
    }

private:
    Address addr_;
    std::function<void(Address)> callback_;
};

CFGGraphView::CFGGraphView(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void CFGGraphView::setSession(std::shared_ptr<DebugSession> session) {
    session_ = session;
}

void CFGGraphView::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(8, 4, 8, 4);

    auto* lbl_title = new QLabel("Interactive Control Flow Graph (CFG)", this);
    lbl_title->setStyleSheet("font-weight: bold; color: #80cbc4;");

    auto* btn_refresh = new QPushButton("Refresh", this);
    connect(btn_refresh, &QPushButton::clicked, this, &CFGGraphView::onRefreshCurrent);

    auto* btn_zin = new QPushButton("+", this);
    connect(btn_zin, &QPushButton::clicked, this, &CFGGraphView::onZoomIn);

    auto* btn_zout = new QPushButton("-", this);
    connect(btn_zout, &QPushButton::clicked, this, &CFGGraphView::onZoomOut);

    auto* btn_zreset = new QPushButton("100%", this);
    connect(btn_zreset, &QPushButton::clicked, this, &CFGGraphView::onResetZoom);

    toolbar->addWidget(lbl_title);
    toolbar->addStretch();
    toolbar->addWidget(btn_refresh);
    toolbar->addWidget(btn_zin);
    toolbar->addWidget(btn_zout);
    toolbar->addWidget(btn_zreset);
    layout->addLayout(toolbar);

    scene_ = new QGraphicsScene(this);
    scene_->setBackgroundBrush(QColor(24, 26, 30));

    graphicsView_ = new QGraphicsView(scene_, this);
    graphicsView_->setRenderHint(QPainter::Antialiasing);
    graphicsView_->setDragMode(QGraphicsView::ScrollHandDrag);
    graphicsView_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    layout->addWidget(graphicsView_);
}

void CFGGraphView::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0) {
            graphicsView_->scale(1.15, 1.15);
        } else {
            graphicsView_->scale(0.85, 0.85);
        }
        event->accept();
    } else {
        QWidget::wheelEvent(event);
    }
}

void CFGGraphView::onZoomIn() {
    graphicsView_->scale(1.2, 1.2);
}

void CFGGraphView::onZoomOut() {
    graphicsView_->scale(0.8, 0.8);
}

void CFGGraphView::onResetZoom() {
    graphicsView_->resetTransform();
}

void CFGGraphView::onRefreshCurrent() {
    if (session_) {
        Address rip = session_->registers().rip();
        buildGraphForFunction(rip);
    }
}

void CFGGraphView::buildGraphForFunction(Address targetAddr) {
    if (!session_ || targetAddr.isNull()) return;

    currentFuncAddr_ = targetAddr;
    scene_->clear();

    // 1. Find function boundary
    auto func = FunctionFinder::findEnclosingFunction(*session_, targetAddr);
    Address start_addr = func ? func->startAddress : targetAddr;
    size_t insn_count = func ? std::max<size_t>(10, func->size / 3) : 60;

    auto raw_insns = session_->disassemble(start_addr, std::min<size_t>(insn_count, 150));
    if (raw_insns.empty()) return;

    // 2. Build precise CFG with Leader detection & cycle breaking
    CFGGraph graph = CFGBuilder::build(raw_insns);

    // 3. Layout and render blocks via Sugiyama layered algorithm
    layoutAndDrawBlocks(graph);
}

void CFGGraphView::layoutAndDrawBlocks(const CFGGraph& graph) {
    if (graph.blocks.empty()) return;

    // 1. Calculate bounding boxes for all basic blocks
    std::map<int, QSizeF> blockSizes;
    const qreal blockWidth = 340.0;

    for (const auto& b : graph.blocks) {
        qreal blockHeight = 44.0 + b.instructions.size() * 18.0;
        blockSizes[b.id] = QSizeF(blockWidth, blockHeight);
    }

    // 2. Calculate Sugiyama layout
    CFGLayoutResult layoutResult = SugiyamaLayout::layout(graph, blockSizes, 70.0, 50.0);

    // 3. Render Basic Blocks
    for (const auto& b : graph.blocks) {
        if (!layoutResult.blockRects.count(b.id)) continue;
        const QRectF& rect = layoutResult.blockRects.at(b.id);

        auto* box = new ClickableBlockItem(b.startAddr, rect, [this](Address addr) {
            Q_EMIT jumpToDisassemblyRequested(addr);
        });
        box->setBrush(QColor(30, 34, 40));
        box->setPen(QPen(QColor(65, 75, 90), 1.5));
        scene_->addItem(box);

        // Title text item
        auto* title = new QGraphicsTextItem(box);
        title->setHtml(QString("<b style='color: #64b5f6;'>loc_%1:</b>")
            .arg(b.startAddr.toQString()));
        title->setPos(rect.x() + 8, rect.y() + 4);
        title->setFont(QFont("Monospace", 9));

        // Instructions text item
        QString insnHtml = "<div style='color: #dcdcdc; font-family: Monospace; font-size: 8.5pt;'>";
        for (const auto& insn : b.instructions) {
            QString mColor = "#81c784";
            if (insn.mnemonic == "call") mColor = "#ffb74d";
            else if (insn.mnemonic.rfind("j", 0) == 0) mColor = "#4dd0e1";
            else if (insn.mnemonic.rfind("ret", 0) == 0) mColor = "#e57373";

            insnHtml += QString("<span style='color: #888;'>%1</span>  <span style='color: %2; font-weight: bold;'>%3</span> %4<br>")
                .arg(insn.address.toQString())
                .arg(mColor)
                .arg(QString::fromStdString(insn.mnemonic))
                .arg(QString::fromStdString(insn.operands).toHtmlEscaped());
        }
        insnHtml += "</div>";

        auto* body = new QGraphicsTextItem(box);
        body->setHtml(insnHtml);
        body->setPos(rect.x() + 8, rect.y() + 24);
    }

    // 4. Render Routed Edges (Forward curves & Outer loop channels)
    for (const auto& edge : layoutResult.routedEdges) {
        auto* pathItem = new QGraphicsPathItem(edge.path);
        QPen pen(edge.color, 2, edge.isBackEdge ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        pathItem->setPen(pen);
        scene_->addItem(pathItem);

        // Arrow head
        QPolygonF arrowHead;
        const qreal sz = 7.0;
        if (std::abs(edge.arrowAngle - 90.0) < 5.0) {
            arrowHead << edge.arrowHeadPos
                      << QPointF(edge.arrowHeadPos.x() - sz, edge.arrowHeadPos.y() - sz)
                      << QPointF(edge.arrowHeadPos.x() + sz, edge.arrowHeadPos.y() - sz);
        } else if (std::abs(edge.arrowAngle) < 5.0) {
            arrowHead << edge.arrowHeadPos
                      << QPointF(edge.arrowHeadPos.x() - sz, edge.arrowHeadPos.y() - sz)
                      << QPointF(edge.arrowHeadPos.x() - sz, edge.arrowHeadPos.y() + sz);
        } else {
            arrowHead << edge.arrowHeadPos
                      << QPointF(edge.arrowHeadPos.x() + sz, edge.arrowHeadPos.y() - sz)
                      << QPointF(edge.arrowHeadPos.x() + sz, edge.arrowHeadPos.y() + sz);
        }
        scene_->addPolygon(arrowHead, QPen(edge.color), QBrush(edge.color));

        // Edge label
        if (!edge.label.isEmpty()) {
            auto* lbl = scene_->addText(edge.label, QFont("Monospace", 8));
            lbl->setDefaultTextColor(edge.color);
            QPointF pMid = edge.path.pointAtPercent(0.5);
            lbl->setPos(pMid.x() + 6, pMid.y() - 10);
        }
    }

    scene_->setSceneRect(layoutResult.totalBounds);
}

} // namespace edb_next
