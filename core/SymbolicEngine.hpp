#pragma once

#include "Types.hpp"
#include "MicroIR.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>
#include <memory>

// Forward declaration of Z3 internal types if needed, or include <z3++.h>
namespace z3 {
    class context;
    class expr;
    class solver;
}

namespace edb_next {

struct SymbolicModelResult {
    bool satisfiable{false};
    std::unordered_map<std::string, uint64_t> registerValues;
    std::unordered_map<uint64_t, uint8_t> memoryBytes;
    std::string summary;
};

class SymbolicEngine {
public:
    SymbolicEngine();
    ~SymbolicEngine();

    // Disable copy, allow move
    SymbolicEngine(const SymbolicEngine&) = delete;
    SymbolicEngine& operator=(const SymbolicEngine&) = delete;
    SymbolicEngine(SymbolicEngine&&) noexcept;
    SymbolicEngine& operator=(SymbolicEngine&&) noexcept;

    [[nodiscard]] static bool isZ3Available() noexcept;

    void reset();

    // Symbolic register configuration
    void makeRegisterSymbolic(const std::string& regName, uint8_t size = 8);
    void setRegisterConcrete(const std::string& regName, uint64_t val, uint8_t size = 8);
    [[nodiscard]] std::optional<uint64_t> evaluateRegister(const std::string& regName) const;

    // Symbolic memory configuration
    void setMemoryConcrete(uint64_t addr, uint8_t byteVal);
    void setMemorySymbolic(uint64_t addr, const std::string& symName);

    // Taint analysis
    void markTainted(const std::string& target);
    void clearTaint(const std::string& target);
    [[nodiscard]] bool isTainted(const std::string& target) const noexcept;
    [[nodiscard]] const std::unordered_set<std::string>& taintedSet() const noexcept { return tainted_; }

    // Execution
    void stepIR(const IRInstruction& insn);
    void executeBlock(const IRBlock& block);

    // Path reachability solving
    [[nodiscard]] SymbolicModelResult solveReachability(const IRFunction& fn, Address targetAddr);
    [[nodiscard]] SymbolicModelResult checkSatisfiability();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
    std::unordered_set<std::string> tainted_;
};

} // namespace edb_next
