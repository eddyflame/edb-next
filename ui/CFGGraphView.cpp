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
#include <map>
#include <set>
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
    layout->setContentsMargins(4, 4, 4, 4);

    auto* toolbar = new QHBoxLayout();
    auto* lbl_title = new QLabel("Interactive Control Flow Graph (CFG)", this);
    lbl_title->setStyleSheet("font-weight: bold; color: #64b5f6;");

    auto* btn_refresh = new QPushButton("Refresh Current Function", this);
    connect(btn_refresh, &QPushButton::clicked, this, &CFGGraphView::onRefreshCurrent);
    auto* btn_zin = new QPushButton("Zoom In (+)", this);
    connect(btn_zin, &QPushButton::clicked, this, &CFGGraphView::onZoomIn);
    auto* btn_zout = new QPushButton("Zoom Out (-)", this);
    connect(btn_zout, &QPushButton::clicked, this, &CFGGraphView::onZoomOut);
    auto* btn_zreset = new QPushButton("Reset Zoom", this);
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
    size_t insn_count = func ? std::max<size_t>(10, func->size / 3) : 50;

    auto raw_insns = session_->disassemble(start_addr, std::min<size_t>(insn_count, 120));
    if (raw_insns.empty()) return;

    // 2. Partition into Basic Blocks
    std::vector<CFGBasicBlock> blocks;
    CFGBasicBlock curBlock;
    curBlock.id = 0;
    curBlock.startAddr = raw_insns[0].address;

    auto is_jcc = [](const std::string& m) {
        return (m.rfind("j", 0) == 0 && m != "jmp");
    };

    for (size_t i = 0; i < raw_insns.size(); ++i) {
        const auto& insn = raw_insns[i];
        curBlock.instructions.push_back(CFGInstruction{
            .address = insn.address,
            .mnemonic = insn.mnemonic,
            .operands = insn.operands
        });
        curBlock.endAddr = insn.address;

        bool is_ret = (insn.mnemonic == "ret");
        bool is_jmp = (insn.mnemonic == "jmp");
        bool is_cond = is_jcc(insn.mnemonic);

        if (is_cond || is_jmp || is_ret || i == raw_insns.size() - 1) {
            // Parse branch target address
            Address target(0);
            if (!insn.operands.empty()) {
                uint64_t val = std::strtoull(insn.operands.c_str(), nullptr, 0);
                target = Address(val);
            }

            if (is_cond) {
                curBlock.trueTarget = target;
                if (i + 1 < raw_insns.size()) {
                    curBlock.falseTarget = raw_insns[i + 1].address;
                }
            } else if (is_jmp) {
                curBlock.directTarget = target;
            }

            blocks.push_back(curBlock);

            if (i + 1 < raw_insns.size()) {
                curBlock = CFGBasicBlock();
                curBlock.id = static_cast<int>(blocks.size());
                curBlock.startAddr = raw_insns[i + 1].address;
            }
        }
    }

    // 3. Layout and render blocks
    layoutAndDrawBlocks(blocks);
}

void CFGGraphView::layoutAndDrawBlocks(const std::vector<CFGBasicBlock>& blocks) {
    if (blocks.empty()) return;

    std::map<Address, QPointF> blockPositions;
    std::map<Address, QSizeF> blockSizes;

    qreal curY = 40.0;
    const qreal blockWidth = 340.0;
    const qreal startX = 60.0;

    // Draw blocks sequentially vertically, branching horizontally if branching
    for (size_t i = 0; i < blocks.size(); ++i) {
        const auto& b = blocks[i];
        qreal blockHeight = 40.0 + b.instructions.size() * 18.0;

        qreal x = startX;
        if (b.trueTarget.has_value() && (i % 2 == 1)) {
            x += (blockWidth + 50.0);
        }

        QRectF rect(x, curY, blockWidth, blockHeight);
        blockPositions[b.startAddr] = QPointF(x, curY);
        blockSizes[b.startAddr] = QSizeF(blockWidth, blockHeight);

        // Container Box
        auto* box = new ClickableBlockItem(b.startAddr, rect, [this](Address addr) {
            Q_EMIT jumpToDisassemblyRequested(addr);
        });
        box->setBrush(QColor(32, 36, 42));
        box->setPen(QPen(QColor(70, 78, 90), 1.5));
        scene_->addItem(box);

        // Title text item
        auto* title = new QGraphicsTextItem(box);
        title->setHtml(QString("<b style='color: #64b5f6;'>loc_%1:</b>")
            .arg(QString::fromStdString(b.startAddr.toHex())));
        title->setPos(x + 8, curY + 4);
        title->setFont(QFont("Monospace", 9));

        // Instructions text item
        QString insnHtml = "<div style='color: #dcdcdc; font-family: Monospace; font-size: 8.5pt;'>";
        for (const auto& insn : b.instructions) {
            QString mColor = "#81c784";
            if (insn.mnemonic == "call") mColor = "#ffb74d";
            else if (insn.mnemonic.rfind("j", 0) == 0) mColor = "#4dd0e1";
            else if (insn.mnemonic == "ret") mColor = "#e57373";

            insnHtml += QString("<span style='color: #888;'>%1</span>  <span style='color: %2; font-weight: bold;'>%3</span> %4<br>")
                .arg(QString::fromStdString(insn.address.toHex()))
                .arg(mColor)
                .arg(QString::fromStdString(insn.mnemonic))
                .arg(QString::fromStdString(insn.operands).toHtmlEscaped());
        }
        insnHtml += "</div>";

        auto* body = new QGraphicsTextItem(box);
        body->setHtml(insnHtml);
        body->setPos(x + 8, curY + 24);

        curY += (blockHeight + 50.0);
    }

    // Draw routing arrows between blocks
    for (const auto& b : blocks) {
        auto fromPosIt = blockPositions.find(b.startAddr);
        auto fromSizeIt = blockSizes.find(b.startAddr);
        if (fromPosIt == blockPositions.end() || fromSizeIt == blockSizes.end()) continue;

        QPointF fromBottomCenter(fromPosIt->second.x() + fromSizeIt->second.width() / 2,
                                 fromPosIt->second.y() + fromSizeIt->second.height());

        // Helper to draw curved bezier edge
        auto draw_edge = [this](QPointF p1, QPointF p2, const QColor& color, const QString& label) {
            QPainterPath path;
            path.moveTo(p1);
            qreal midY = (p1.y() + p2.y()) / 2.0;
            path.cubicTo(QPointF(p1.x(), midY), QPointF(p2.x(), midY), p2);

            auto* edgeItem = new QGraphicsPathItem(path);
            edgeItem->setPen(QPen(color, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            scene_->addItem(edgeItem);

            // Arrow head
            qreal arrowSize = 7.0;
            QPolygonF arrowHead;
            arrowHead << p2 << QPointF(p2.x() - arrowSize, p2.y() - arrowSize) << QPointF(p2.x() + arrowSize, p2.y() - arrowSize);
            auto* headItem = scene_->addPolygon(arrowHead, QPen(color), QBrush(color));
            (void)headItem;

            if (!label.isEmpty()) {
                auto* lbl = scene_->addText(label, QFont("Monospace", 8));
                lbl->setDefaultTextColor(color);
                lbl->setPos((p1.x() + p2.x()) / 2 + 5, midY - 10);
            }
        };

        // True branch (Taken) -> Green
        if (b.trueTarget.has_value()) {
            auto toIt = blockPositions.find(*b.trueTarget);
            if (toIt != blockPositions.end()) {
                QPointF toTop(toIt->second.x() + blockWidth / 2, toIt->second.y());
                draw_edge(QPointF(fromBottomCenter.x() - 40, fromBottomCenter.y()), toTop, QColor(76, 175, 80), "True");
            }
        }

        // False branch (Not Taken) -> Red
        if (b.falseTarget.has_value()) {
            auto toIt = blockPositions.find(*b.falseTarget);
            if (toIt != blockPositions.end()) {
                QPointF toTop(toIt->second.x() + blockWidth / 2, toIt->second.y());
                draw_edge(QPointF(fromBottomCenter.x() + 40, fromBottomCenter.y()), toTop, QColor(244, 67, 54), "False");
            }
        }

        // Direct branch / Fallthrough -> Blue/Cyan
        if (b.directTarget.has_value()) {
            auto toIt = blockPositions.find(*b.directTarget);
            if (toIt != blockPositions.end()) {
                QPointF toTop(toIt->second.x() + blockWidth / 2, toIt->second.y());
                draw_edge(fromBottomCenter, toTop, QColor(33, 150, 243), "Jump");
            }
        }
    }
}

} // namespace edb_next
