#pragma once

#include "Types.hpp"
#include "DisassemblyTypes.hpp"
#include <string>
#include <vector>
#include <optional>
#include <span>

namespace edb_next {

struct CFGInstruction {
    Address address{0};
    std::string mnemonic;
    std::string operands;
};

enum class CFGEdgeType {
    TrueBranch,   // Conditional branch taken (Green)
    FalseBranch,  // Conditional branch not taken / fallthrough (Red)
    DirectJump,   // Unconditional jump (Blue)
    Fallthrough   // Normal sequential flow (Cyan/Blue)
};

struct CFGEdge {
    int fromBlockId{0};
    int toBlockId{0};
    CFGEdgeType type{CFGEdgeType::Fallthrough};
    bool isBackEdge{false}; // Loop back-edge identified via DFS
};

struct CFGBlock {
    int id{0};
    Address startAddr{0};
    Address endAddr{0};
    std::vector<CFGInstruction> instructions;
    std::optional<Address> trueTarget;
    std::optional<Address> falseTarget;
    std::optional<Address> directTarget;
};

struct CFGGraph {
    std::vector<CFGBlock> blocks;
    std::vector<CFGEdge> edges;
    int entryBlockId{0};

    [[nodiscard]] const CFGBlock* findBlock(int id) const noexcept;
    [[nodiscard]] const CFGBlock* findBlockByAddress(Address addr) const noexcept;
    [[nodiscard]] std::vector<int> getSuccessors(int blockId) const;
    [[nodiscard]] std::vector<int> getPredecessors(int blockId) const;
};

class CFGBuilder {
public:
    // Build a complete CFG with Leader partitioning and cycle detection
    static CFGGraph build(std::span<const DisassembledInstruction> instructions);
    static CFGGraph build(const std::vector<DisassembledInstruction>& instructions) {
        return build(std::span<const DisassembledInstruction>(instructions.data(), instructions.size()));
    }

    static Address parseBranchTarget(const std::string& mnemonic, const std::string& operands);
    static bool isConditionalBranch(const std::string& mnemonic) noexcept;
    static bool isUnconditionalBranch(const std::string& mnemonic) noexcept;
    static bool isReturn(const std::string& mnemonic) noexcept;

private:
    static void detectBackEdges(CFGGraph& graph);
};

} // namespace edb_next
