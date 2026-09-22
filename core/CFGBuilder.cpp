#include "CFGBuilder.hpp"
#include <algorithm>
#include <unordered_map>
#include <set>
#include <cctype>
#include <cstdlib>

namespace edb_next {

const CFGBlock* CFGGraph::findBlock(int id) const noexcept {
    if (id >= 0 && id < static_cast<int>(blocks.size())) {
        return &blocks[id];
    }
    return nullptr;
}

const CFGBlock* CFGGraph::findBlockByAddress(Address addr) const noexcept {
    for (const auto& b : blocks) {
        if (b.startAddr == addr || (addr >= b.startAddr && addr <= b.endAddr)) {
            return &b;
        }
    }
    return nullptr;
}

std::vector<int> CFGGraph::getSuccessors(int blockId) const {
    std::vector<int> succs;
    for (const auto& edge : edges) {
        if (edge.fromBlockId == blockId) {
            succs.push_back(edge.toBlockId);
        }
    }
    return succs;
}

std::vector<int> CFGGraph::getPredecessors(int blockId) const {
    std::vector<int> preds;
    for (const auto& edge : edges) {
        if (edge.toBlockId == blockId) {
            preds.push_back(edge.fromBlockId);
        }
    }
    return preds;
}

bool CFGBuilder::isConditionalBranch(const std::string& m) noexcept {
    return (m.rfind("j", 0) == 0 && m != "jmp" && m != "ljmp");
}

bool CFGBuilder::isUnconditionalBranch(const std::string& m) noexcept {
    return (m == "jmp" || m == "ljmp");
}

bool CFGBuilder::isReturn(const std::string& m) noexcept {
    return (m == "ret" || m == "retq" || m == "retf" || m == "retn" || m == "iret" || m == "iretd" || m == "iretq");
}

Address CFGBuilder::parseBranchTarget(const std::string& mnemonic, const std::string& operands) {
    (void)mnemonic;
    if (operands.empty()) return Address(0);

    // Look for 0x prefix or digits
    size_t hexPos = operands.find("0x");
    if (hexPos == std::string::npos) {
        hexPos = operands.find("0X");
    }

    const char* strToParse = operands.c_str();
    if (hexPos != std::string::npos) {
        strToParse += hexPos;
    } else {
        // Find first digit
        size_t digitPos = 0;
        while (digitPos < operands.size() && !std::isxdigit(static_cast<unsigned char>(operands[digitPos]))) {
            ++digitPos;
        }
        if (digitPos < operands.size()) {
            strToParse += digitPos;
        }
    }

    char* endPtr = nullptr;
    uint64_t val = std::strtoull(strToParse, &endPtr, 0);
    if (endPtr != strToParse && val != 0) {
        return Address(val);
    }
    return Address(0);
}

CFGGraph CFGBuilder::build(std::span<const DisassembledInstruction> instructions) {
    CFGGraph graph;
    if (instructions.empty()) return graph;

    // Map instruction address to its index
    std::unordered_map<uint64_t, size_t> addrToIndex;
    for (size_t i = 0; i < instructions.size(); ++i) {
        addrToIndex[instructions[i].address.value()] = i;
    }

    // Step 1: Detect Leaders (Entry points of basic blocks)
    std::set<uint64_t> leaders;
    leaders.insert(instructions[0].address.value());

    for (size_t i = 0; i < instructions.size(); ++i) {
        const auto& insn = instructions[i];
        bool isCond = isConditionalBranch(insn.mnemonic);
        bool isUncond = isUnconditionalBranch(insn.mnemonic);
        bool isRet = isReturn(insn.mnemonic);

        if (isCond || isUncond) {
            Address target = parseBranchTarget(insn.mnemonic, insn.operands);
            if (!target.isNull() && addrToIndex.find(target.value()) != addrToIndex.end()) {
                leaders.insert(target.value());
            }
            if (i + 1 < instructions.size()) {
                leaders.insert(instructions[i + 1].address.value());
            }
        } else if (isRet) {
            if (i + 1 < instructions.size()) {
                leaders.insert(instructions[i + 1].address.value());
            }
        }
    }

    // Step 2: Partition instructions into basic blocks based on leaders
    std::unordered_map<uint64_t, int> startAddrToBlockId;
    CFGBlock currentBlock;
    bool hasCurrent = false;

    for (size_t i = 0; i < instructions.size(); ++i) {
        const auto& insn = instructions[i];
        if (leaders.find(insn.address.value()) != leaders.end()) {
            if (hasCurrent && !currentBlock.instructions.empty()) {
                currentBlock.endAddr = currentBlock.instructions.back().address;
                startAddrToBlockId[currentBlock.startAddr.value()] = currentBlock.id;
                graph.blocks.push_back(currentBlock);
            }
            currentBlock = CFGBlock();
            currentBlock.id = static_cast<int>(graph.blocks.size());
            currentBlock.startAddr = insn.address;
            hasCurrent = true;
        }

        currentBlock.instructions.push_back(CFGInstruction{
            .address = insn.address,
            .mnemonic = insn.mnemonic,
            .operands = insn.operands
        });
    }

    if (hasCurrent && !currentBlock.instructions.empty()) {
        currentBlock.endAddr = currentBlock.instructions.back().address;
        startAddrToBlockId[currentBlock.startAddr.value()] = currentBlock.id;
        graph.blocks.push_back(currentBlock);
    }

    // Step 3: Connect edges between basic blocks
    for (size_t b = 0; b < graph.blocks.size(); ++b) {
        auto& block = graph.blocks[b];
        if (block.instructions.empty()) continue;

        const auto& lastInsn = block.instructions.back();
        bool isCond = isConditionalBranch(lastInsn.mnemonic);
        bool isUncond = isUnconditionalBranch(lastInsn.mnemonic);
        bool isRet = isReturn(lastInsn.mnemonic);

        if (isCond) {
            Address target = parseBranchTarget(lastInsn.mnemonic, lastInsn.operands);
            auto itTrue = startAddrToBlockId.find(target.value());
            if (itTrue != startAddrToBlockId.end()) {
                block.trueTarget = target;
                graph.edges.push_back(CFGEdge{
                    .fromBlockId = block.id,
                    .toBlockId = itTrue->second,
                    .type = CFGEdgeType::TrueBranch,
                    .isBackEdge = false
                });
            }

            // Fallthrough edge
            if (b + 1 < graph.blocks.size()) {
                const auto& nextBlock = graph.blocks[b + 1];
                block.falseTarget = nextBlock.startAddr;
                graph.edges.push_back(CFGEdge{
                    .fromBlockId = block.id,
                    .toBlockId = nextBlock.id,
                    .type = CFGEdgeType::FalseBranch,
                    .isBackEdge = false
                });
            }
        } else if (isUncond) {
            Address target = parseBranchTarget(lastInsn.mnemonic, lastInsn.operands);
            auto itTarget = startAddrToBlockId.find(target.value());
            if (itTarget != startAddrToBlockId.end()) {
                block.directTarget = target;
                graph.edges.push_back(CFGEdge{
                    .fromBlockId = block.id,
                    .toBlockId = itTarget->second,
                    .type = CFGEdgeType::DirectJump,
                    .isBackEdge = false
                });
            }
        } else if (isRet) {
            // Function return: no outgoing edges
        } else {
            // Normal sequential fallthrough to next block
            if (b + 1 < graph.blocks.size()) {
                const auto& nextBlock = graph.blocks[b + 1];
                block.directTarget = nextBlock.startAddr;
                graph.edges.push_back(CFGEdge{
                    .fromBlockId = block.id,
                    .toBlockId = nextBlock.id,
                    .type = CFGEdgeType::Fallthrough,
                    .isBackEdge = false
                });
            }
        }
    }

    // Step 4: Detect Back-Edges (Loops) via DFS
    detectBackEdges(graph);

    return graph;
}

void CFGBuilder::detectBackEdges(CFGGraph& graph) {
    if (graph.blocks.empty()) return;

    std::vector<int> state(graph.blocks.size(), 0); // 0: unvisited, 1: on-stack, 2: finished
    auto dfs = [&](auto& self, int u) -> void {
        state[u] = 1;
        for (auto& edge : graph.edges) {
            if (edge.fromBlockId == u) {
                int v = edge.toBlockId;
                if (v >= 0 && v < static_cast<int>(graph.blocks.size())) {
                    if (state[v] == 1) {
                        edge.isBackEdge = true;
                    } else if (state[v] == 0) {
                        self(self, v);
                    }
                }
            }
        }
        state[u] = 2;
    };

    dfs(dfs, graph.entryBlockId);
}

} // namespace edb_next
