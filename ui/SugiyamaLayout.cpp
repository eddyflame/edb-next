#include "SugiyamaLayout.hpp"
#include <algorithm>
#include <cmath>
#include <queue>
#include <set>

namespace edb_next {

CFGLayoutResult SugiyamaLayout::layout(
    const CFGGraph& graph,
    const std::map<int, QSizeF>& blockSizes,
    qreal layerGap,
    qreal nodeGap) {

    CFGLayoutResult result;
    if (graph.blocks.empty()) return result;

    const int numBlocks = static_cast<int>(graph.blocks.size());

    // Single block edge case
    if (numBlocks == 1) {
        QSizeF sz = blockSizes.count(0) ? blockSizes.at(0) : QSizeF(340, 100);
        QRectF r(60.0, 40.0, sz.width(), sz.height());
        result.blockRects[0] = r;
        result.totalBounds = r.adjusted(-40, -40, 40, 40);
        return result;
    }

    // Phase 1: Cycle Breaking (already identified by CFGBuilder::detectBackEdges)
    // Separate forward edges and back edges
    std::vector<CFGEdge> forwardEdges;
    std::vector<CFGEdge> backEdges;
    for (const auto& e : graph.edges) {
        if (e.isBackEdge) {
            backEdges.push_back(e);
        } else {
            forwardEdges.push_back(e);
        }
    }

    // Phase 2: Layering (Longest-Path Ranking on DAG)
    std::vector<int> layerOf(numBlocks, 0);
    layerOf[graph.entryBlockId] = 0;

    // Relax forward edges iteratively
    for (int iter = 0; iter < numBlocks; ++iter) {
        bool changed = false;
        for (const auto& e : forwardEdges) {
            int u = e.fromBlockId;
            int v = e.toBlockId;
            if (u >= 0 && u < numBlocks && v >= 0 && v < numBlocks) {
                if (layerOf[v] < layerOf[u] + 1) {
                    layerOf[v] = layerOf[u] + 1;
                    changed = true;
                }
            }
        }
        if (!changed) break;
    }

    int maxLayer = 0;
    for (int l : layerOf) {
        maxLayer = std::max(maxLayer, l);
    }
    const int numLayers = maxLayer + 1;

    // Track dummy nodes for long forward edges
    struct DummyNode {
        int id;
        int layer;
        int originalEdgeIdx;
    };
    std::vector<DummyNode> dummyNodes;
    int nextDummyId = -1;

    // Map: original edge index -> list of dummy IDs
    std::map<int, std::vector<int>> edgeDummies;

    for (size_t eIdx = 0; eIdx < forwardEdges.size(); ++eIdx) {
        const auto& e = forwardEdges[eIdx];
        int u = e.fromBlockId;
        int v = e.toBlockId;
        int span = layerOf[v] - layerOf[u];
        if (span > 1) {
            for (int l = layerOf[u] + 1; l < layerOf[v]; ++l) {
                int dId = nextDummyId--;
                dummyNodes.push_back(DummyNode{dId, l, static_cast<int>(eIdx)});
                edgeDummies[static_cast<int>(eIdx)].push_back(dId);
            }
        }
    }

    // Phase 3: Crossing Reduction via Barycenter sweeps
    std::vector<std::vector<int>> layers(numLayers);
    for (int i = 0; i < numBlocks; ++i) {
        layers[layerOf[i]].push_back(i);
    }
    for (const auto& d : dummyNodes) {
        layers[d.layer].push_back(d.id);
    }

    // Helper to get connected predecessors in layer k-1
    auto getPredecessorsInPrevLayer = [&](int nodeId, int k) -> std::vector<int> {
        std::vector<int> preds;
        if (nodeId >= 0) {
            // Real block
            for (size_t eIdx = 0; eIdx < forwardEdges.size(); ++eIdx) {
                const auto& e = forwardEdges[eIdx];
                if (e.toBlockId == nodeId) {
                    if (edgeDummies.count(static_cast<int>(eIdx)) && !edgeDummies[static_cast<int>(eIdx)].empty()) {
                        preds.push_back(edgeDummies[static_cast<int>(eIdx)].back());
                    } else if (layerOf[e.fromBlockId] == k - 1) {
                        preds.push_back(e.fromBlockId);
                    }
                }
            }
        } else {
            // Dummy node
            for (const auto& [eIdx, dList] : edgeDummies) {
                for (size_t i = 0; i < dList.size(); ++i) {
                    if (dList[i] == nodeId) {
                        if (i == 0) {
                            preds.push_back(forwardEdges[eIdx].fromBlockId);
                        } else {
                            preds.push_back(dList[i - 1]);
                        }
                    }
                }
            }
        }
        return preds;
    };

    // Helper to get connected successors in layer k+1
    auto getSuccessorsInNextLayer = [&](int nodeId, int k) -> std::vector<int> {
        std::vector<int> succs;
        if (nodeId >= 0) {
            // Real block
            for (size_t eIdx = 0; eIdx < forwardEdges.size(); ++eIdx) {
                const auto& e = forwardEdges[eIdx];
                if (e.fromBlockId == nodeId) {
                    if (edgeDummies.count(static_cast<int>(eIdx)) && !edgeDummies[static_cast<int>(eIdx)].empty()) {
                        succs.push_back(edgeDummies[static_cast<int>(eIdx)].front());
                    } else if (layerOf[e.toBlockId] == k + 1) {
                        succs.push_back(e.toBlockId);
                    }
                }
            }
        } else {
            // Dummy node
            for (const auto& [eIdx, dList] : edgeDummies) {
                for (size_t i = 0; i < dList.size(); ++i) {
                    if (dList[i] == nodeId) {
                        if (i + 1 < dList.size()) {
                            succs.push_back(dList[i + 1]);
                        } else {
                            succs.push_back(forwardEdges[eIdx].toBlockId);
                        }
                    }
                }
            }
        }
        return succs;
    };

    // 8 Iterations of Barycenter crossing reduction
    for (int pass = 0; pass < 8; ++pass) {
        // Downward sweep
        for (int k = 1; k < numLayers; ++k) {
            std::map<int, qreal> nodePosInPrev;
            for (size_t idx = 0; idx < layers[k - 1].size(); ++idx) {
                nodePosInPrev[layers[k - 1][idx]] = static_cast<qreal>(idx);
            }

            std::vector<std::pair<int, qreal>> sortedNodes;
            for (size_t idx = 0; idx < layers[k].size(); ++idx) {
                int nid = layers[k][idx];
                auto preds = getPredecessorsInPrevLayer(nid, k);
                if (preds.empty()) {
                    sortedNodes.push_back({nid, static_cast<qreal>(idx)});
                } else {
                    qreal sum = 0.0;
                    for (int p : preds) sum += nodePosInPrev[p];
                    sortedNodes.push_back({nid, sum / preds.size()});
                }
            }

            std::stable_sort(sortedNodes.begin(), sortedNodes.end(), [](const auto& a, const auto& b) {
                return a.second < b.second;
            });
            for (size_t idx = 0; idx < sortedNodes.size(); ++idx) {
                layers[k][idx] = sortedNodes[idx].first;
            }
        }

        // Upward sweep
        for (int k = numLayers - 2; k >= 0; --k) {
            std::map<int, qreal> nodePosInNext;
            for (size_t idx = 0; idx < layers[k + 1].size(); ++idx) {
                nodePosInNext[layers[k + 1][idx]] = static_cast<qreal>(idx);
            }

            std::vector<std::pair<int, qreal>> sortedNodes;
            for (size_t idx = 0; idx < layers[k].size(); ++idx) {
                int nid = layers[k][idx];
                auto succs = getSuccessorsInNextLayer(nid, k);
                if (succs.empty()) {
                    sortedNodes.push_back({nid, static_cast<qreal>(idx)});
                } else {
                    qreal sum = 0.0;
                    for (int s : succs) sum += nodePosInNext[s];
                    sortedNodes.push_back({nid, sum / succs.size()});
                }
            }

            std::stable_sort(sortedNodes.begin(), sortedNodes.end(), [](const auto& a, const auto& b) {
                return a.second < b.second;
            });
            for (size_t idx = 0; idx < sortedNodes.size(); ++idx) {
                layers[k][idx] = sortedNodes[idx].first;
            }
        }
    }

    // Phase 4: Coordinate Assignment (Y and X)
    std::vector<qreal> layerY(numLayers, 0.0);
    std::vector<qreal> layerHeight(numLayers, 0.0);
    const qreal defaultWidth = 340.0;

    auto getNodeSize = [&](int nid) -> QSizeF {
        if (nid >= 0) {
            return blockSizes.count(nid) ? blockSizes.at(nid) : QSizeF(defaultWidth, 100.0);
        }
        return QSizeF(20.0, 10.0); // Minimal dummy size
    };

    qreal currentY = 50.0;
    for (int k = 0; k < numLayers; ++k) {
        qreal maxH = 40.0;
        for (int nid : layers[k]) {
            maxH = std::max(maxH, getNodeSize(nid).height());
        }
        layerHeight[k] = maxH;
        layerY[k] = currentY;
        currentY += (maxH + layerGap);
    }

    // X Coordinates: Resolve widths and overlap
    std::map<int, qreal> nodeX;
    qreal maxLayerWidth = 0.0;

    for (int k = 0; k < numLayers; ++k) {
        qreal currentX = 0.0;
        for (size_t idx = 0; idx < layers[k].size(); ++idx) {
            int nid = layers[k][idx];
            QSizeF sz = getNodeSize(nid);
            nodeX[nid] = currentX;
            currentX += (sz.width() + nodeGap);
        }
        if (!layers[k].empty()) {
            qreal layerWidth = currentX - nodeGap;
            maxLayerWidth = std::max(maxLayerWidth, layerWidth);
        }
    }

    // Center each layer relative to maxLayerWidth
    const qreal baseMarginX = 80.0;
    for (int k = 0; k < numLayers; ++k) {
        if (layers[k].empty()) continue;
        int lastNid = layers[k].back();
        qreal layerW = (nodeX[lastNid] + getNodeSize(lastNid).width()) - nodeX[layers[k].front()];
        qreal shift = (maxLayerWidth - layerW) / 2.0;
        for (int nid : layers[k]) {
            nodeX[nid] += (shift + baseMarginX);
        }
    }

    // Assign final QRectF to real blocks and record dummy positions
    std::map<int, QPointF> dummyPos;
    for (int k = 0; k < numLayers; ++k) {
        for (int nid : layers[k]) {
            QSizeF sz = getNodeSize(nid);
            qreal x = nodeX[nid];
            qreal y = layerY[k];
            if (nid >= 0) {
                result.blockRects[nid] = QRectF(x, y, sz.width(), sz.height());
            } else {
                dummyPos[nid] = QPointF(x + sz.width() / 2.0, y + sz.height() / 2.0);
            }
        }
    }

    // Calculate bounding box of all blocks
    qreal minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
    for (const auto& [id, r] : result.blockRects) {
        minX = std::min(minX, r.left());
        maxX = std::max(maxX, r.right());
        minY = std::min(minY, r.top());
        maxY = std::max(maxY, r.bottom());
    }

    // Phase 5: Edge Routing
    int loopLeftCount = 0;
    int loopRightCount = 0;

    for (size_t eIdx = 0; eIdx < graph.edges.size(); ++eIdx) {
        const auto& edge = graph.edges[eIdx];
        if (!result.blockRects.count(edge.fromBlockId) || !result.blockRects.count(edge.toBlockId)) {
            continue;
        }

        const QRectF& fromRect = result.blockRects[edge.fromBlockId];
        const QRectF& toRect = result.blockRects[edge.toBlockId];

        CFGRoutedEdge routed;
        routed.fromBlockId = edge.fromBlockId;
        routed.toBlockId = edge.toBlockId;
        routed.type = edge.type;
        routed.isBackEdge = edge.isBackEdge;

        // Styling
        switch (edge.type) {
            case CFGEdgeType::TrueBranch:
                routed.color = QColor(76, 175, 80); // Green
                routed.label = edge.isBackEdge ? "Loop (True)" : "True";
                break;
            case CFGEdgeType::FalseBranch:
                routed.color = QColor(244, 67, 54); // Red
                routed.label = edge.isBackEdge ? "Loop (False)" : "False";
                break;
            case CFGEdgeType::DirectJump:
                routed.color = QColor(33, 150, 243); // Blue
                routed.label = edge.isBackEdge ? "Loop" : "Jump";
                break;
            case CFGEdgeType::Fallthrough:
                routed.color = QColor(41, 182, 246); // Cyan
                routed.label = edge.isBackEdge ? "Loop" : "";
                break;
        }

        if (edge.isBackEdge) {
            // Loop back-edge: route through outer exterior lane around blocks
            bool routeLeft = (fromRect.center().x() <= (minX + maxX) / 2.0);
            qreal laneX;
            QPointF startPt, endPt;

            if (routeLeft) {
                qreal laneOffset = 45.0 + (loopLeftCount++ * 22.0);
                laneX = minX - laneOffset;
                startPt = QPointF(fromRect.left(), fromRect.top() + fromRect.height() * 0.6);
                endPt = QPointF(toRect.left(), toRect.top() + 25.0);

                routed.path.moveTo(startPt);
                routed.path.lineTo(laneX, startPt.y());
                routed.path.lineTo(laneX, endPt.y());
                routed.path.lineTo(endPt);

                routed.arrowHeadPos = endPt;
                routed.arrowAngle = 0.0; // pointing right into block
            } else {
                qreal laneOffset = 45.0 + (loopRightCount++ * 22.0);
                laneX = maxX + laneOffset;
                startPt = QPointF(fromRect.right(), fromRect.top() + fromRect.height() * 0.6);
                endPt = QPointF(toRect.right(), toRect.top() + 25.0);

                routed.path.moveTo(startPt);
                routed.path.lineTo(laneX, startPt.y());
                routed.path.lineTo(laneX, endPt.y());
                routed.path.lineTo(endPt);

                routed.arrowHeadPos = endPt;
                routed.arrowAngle = 180.0; // pointing left into block
            }

            minX = std::min(minX, laneX - 30.0);
            maxX = std::max(maxX, laneX + 30.0);
        } else {
            // Forward edge
            QPointF startPt;
            if (edge.type == CFGEdgeType::TrueBranch) {
                startPt = QPointF(fromRect.center().x() - 35.0, fromRect.bottom());
            } else if (edge.type == CFGEdgeType::FalseBranch) {
                startPt = QPointF(fromRect.center().x() + 35.0, fromRect.bottom());
            } else {
                startPt = QPointF(fromRect.center().x(), fromRect.bottom());
            }

            QPointF endPt(toRect.center().x(), toRect.top());

            // Check if there are dummy nodes along this edge
            auto itD = edgeDummies.find(static_cast<int>(eIdx));
            if (itD != edgeDummies.end() && !itD->second.empty()) {
                // Multi-segment curve through dummy points
                routed.path.moveTo(startPt);
                QPointF prevPt = startPt;
                for (int dId : itD->second) {
                    QPointF curD = dummyPos[dId];
                    qreal midY = (prevPt.y() + curD.y()) / 2.0;
                    routed.path.cubicTo(QPointF(prevPt.x(), midY), QPointF(curD.x(), midY), curD);
                    prevPt = curD;
                }
                qreal midY = (prevPt.y() + endPt.y()) / 2.0;
                routed.path.cubicTo(QPointF(prevPt.x(), midY), QPointF(endPt.x(), midY), endPt);
            } else {
                // Direct cubic Bezier between adjacent layers
                qreal midY = (startPt.y() + endPt.y()) / 2.0;
                routed.path.moveTo(startPt);
                routed.path.cubicTo(QPointF(startPt.x(), midY), QPointF(endPt.x(), midY), endPt);
            }

            routed.arrowHeadPos = endPt;
            routed.arrowAngle = 90.0; // pointing straight down
        }

        result.routedEdges.push_back(std::move(routed));
    }

    result.totalBounds = QRectF(minX - 60.0, minY - 60.0, (maxX - minX) + 120.0, (maxY - minY) + 120.0);
    return result;
}

} // namespace edb_next
