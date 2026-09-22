#pragma once

#include "core/CFGBuilder.hpp"
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QPainterPath>
#include <QColor>
#include <QString>
#include <map>
#include <vector>

namespace edb_next {

struct CFGRoutedEdge {
    int fromBlockId{0};
    int toBlockId{0};
    CFGEdgeType type{CFGEdgeType::Fallthrough};
    bool isBackEdge{false};
    QPainterPath path;
    QPointF arrowHeadPos;
    qreal arrowAngle{0.0}; // in degrees
    QString label;
    QColor color;
};

struct CFGLayoutResult {
    std::map<int, QRectF> blockRects;
    std::vector<CFGRoutedEdge> routedEdges;
    QRectF totalBounds;
};

class SugiyamaLayout {
public:
    // Computes layered graph coordinates for nodes and collision-free routed edges
    static CFGLayoutResult layout(
        const CFGGraph& graph,
        const std::map<int, QSizeF>& blockSizes,
        qreal layerGap = 65.0,
        qreal nodeGap = 50.0);
};

} // namespace edb_next
