#pragma once

#include "Types.hpp"
#include "RegisterContext.hpp"
#include <deque>
#include <set>
#include <string>
#include <memory>
#include <optional>
#include <QObject>

namespace edb_next {

class DebugSession;

struct TraceFrame {
    uint64_t stepIndex{0};
    Address address{0};
    std::string mnemonic;
    std::string operands;
    RegisterContext registers;
    std::vector<std::string> changedRegs;
};

class TraceEngine : public QObject {
    Q_OBJECT

public:
    explicit TraceEngine(QObject* parent = nullptr);
    ~TraceEngine() override = default;

    // Hit Trace (Code Coverage)
    void recordHit(Address addr);
    [[nodiscard]] bool isHit(Address addr) const;
    void clearHitTrace();
    [[nodiscard]] size_t hitCount() const noexcept { return hitAddresses_.size(); }
    [[nodiscard]] const std::set<Address>& hitAddresses() const noexcept { return hitAddresses_; }

    // Run Trace (Execution History Record)
    void recordFrame(Address addr, const std::string& mnemonic, const std::string& operands, const RegisterContext& regs);
    void clearRunTrace();
    [[nodiscard]] const std::deque<TraceFrame>& traceFrames() const noexcept { return traceHistory_; }
    [[nodiscard]] size_t frameCount() const noexcept { return traceHistory_.size(); }
    [[nodiscard]] size_t currentFrameIndex() const noexcept { return currentFrameIndex_; }
    void setCurrentFrameIndex(size_t idx);
    [[nodiscard]] std::optional<TraceFrame> currentFrame() const;
    bool stepBack();
    bool stepForward();

    // Trace Mode
    void setHitTraceEnabled(bool enabled) noexcept { hitTraceEnabled_ = enabled; }
    [[nodiscard]] bool isHitTraceEnabled() const noexcept { return hitTraceEnabled_; }

    void setRunTraceEnabled(bool enabled) noexcept { runTraceEnabled_ = enabled; }
    [[nodiscard]] bool isRunTraceEnabled() const noexcept { return runTraceEnabled_; }

Q_SIGNALS:
    void traceUpdated();

private:
    bool hitTraceEnabled_{true};
    bool runTraceEnabled_{true};

    std::set<Address> hitAddresses_;
    std::deque<TraceFrame> traceHistory_;
    size_t currentFrameIndex_{0};
    std::optional<RegisterContext> lastRegs_;
    uint64_t stepCounter_{0};
    static constexpr size_t kMaxTraceFrames = 10000;
};

} // namespace edb_next
