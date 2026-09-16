#pragma once

#include "Types.hpp"
#include "LinuxDebugEngine.hpp"
#include "BreakpointManager.hpp"
#include "EventLoopThread.hpp"
#include "RegisterContext.hpp"
#include "AnnotationManager.hpp"
#include <QObject>
#include <memory>
#include <string>
#include <vector>
#include <optional>

#include "ElfParser.hpp"
#include "DwarfParser.hpp"
#include "SourceFileManager.hpp"
#include "CallStackUnwinder.hpp"
#include "Assembler.hpp"
#include "ROPScanner.hpp"
#include "InstructionInspector.hpp"
#include "CodeXRefFinder.hpp"
#include "PatternSearcher.hpp"
#include "ScriptEngineManager.hpp"

namespace edb_next {

struct DisassembledInstruction {
    Address address{0};
    std::string mnemonic;
    std::string operands;
    std::vector<uint8_t> bytes;
    std::string symbol;
    bool isCurrentRip{false};
    bool hasBreakpoint{false};
    std::string sourceFile;
    std::string sourceFullPath;
    int sourceLine{0};
    std::string sourceText;
    bool isSourceLineStart{false};
};

class DebugSession : public QObject {
    Q_OBJECT

public:
    explicit DebugSession(std::string id, std::string name, QObject* parent = nullptr);
    ~DebugSession() override;

    [[nodiscard]] const std::string& id() const noexcept { return id_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] SessionState state() const noexcept { return state_; }
    [[nodiscard]] Pid pid() const noexcept { return engine_.pid(); }
    [[nodiscard]] Tid tid() const noexcept { return engine_.mainTid(); }

    // Lifecycle
    bool launch(const std::string& path, const std::vector<std::string>& args);
    bool attach(Pid pid);
    void detach();
    bool restart();
    void terminate();

    // Control
    void resume(bool passSignal = false);
    void stepInto(bool passSignal = false);
    void stepOver(bool passSignal = false);
    void stepOut();
    void runUntilReturn();
    void runTo(Address addr);
    void pause();
    [[nodiscard]] int lastSignal() const noexcept { return lastSignal_; }

    // Breakpoints
    bool toggleBreakpoint(Address addr);
    bool addBreakpoint(Address addr, const std::string& symbol = "");
    bool addHardwareBreakpoint(Address addr, HardwareBpType type = HardwareBpType::Execute, HardwareBpSize size = HardwareBpSize::Byte1, const std::string& symbol = "");
    bool removeBreakpoint(Address addr);
    bool enableBreakpoint(Address addr);
    bool disableBreakpoint(Address addr);
    [[nodiscard]] bool hasBreakpoint(Address addr) const;
    [[nodiscard]] std::vector<Breakpoint> breakpoints() const;

    // Thread control
    [[nodiscard]] std::vector<ThreadInfo> getThreads() const;
    bool switchThread(Tid tid);
    [[nodiscard]] Tid activeTid() const noexcept { return engine_.activeTid(); }

    // Advanced Breakpoints
    bool setBreakpointCondition(Address addr, const std::string& cond);
    bool setBreakpointIgnoreCount(Address addr, uint32_t count);
    bool setBreakpointLogOnly(Address addr, bool logOnly, const std::string& fmt);

    // Assembly & Analysis
    Result<std::vector<uint8_t>> assemble(const std::string& insn, Address origin = Address(0));
    std::vector<ROPGadget> scanROP(size_t maxGadgetLength = 4, size_t maxResults = 500, const std::string& filter = "");
    InstructionDetails inspectInstruction(Address addr);
    std::vector<CodeXRef> findCodeXRefs(Address targetAddr);
    std::vector<Address> searchPattern(const std::string& pattern, bool execOnly = true);

    // Registers & Memory
    [[nodiscard]] const RegisterContext& registers() const noexcept { return currentRegs_; }
    [[nodiscard]] const RegisterContext& previousRegisters() const noexcept { return previousRegs_; }
    bool setRegisters(const RegisterContext& regs);
    [[nodiscard]] const user_fpregs_struct& fpRegisters() const noexcept { return currentFpRegs_; }
    bool setFpRegisters(const user_fpregs_struct& fpregs);
    std::vector<DisassembledInstruction> disassemble(Address start_addr, size_t count = 35);
    std::vector<uint8_t> readMemory(Address addr, size_t size);
    bool writeMemory(Address addr, const void* data, size_t size);
    std::optional<Address> searchMemory(Address start, size_t max_bytes, const std::vector<uint8_t>& pattern);
    std::vector<MemoryRegion> memoryRegions() const;
    [[nodiscard]] const ElfParser& symbols() const noexcept { return symbols_; }
    [[nodiscard]] std::optional<Address> resolveSymbol(const std::string& name) const { return symbols_.findSymbolAddress(name); }
    std::vector<StackFrame> callStack();
    bool dumpMemoryToFile(Address start, size_t size, const std::string& filepath);

    // Remote memory management & protection
    bool changeMemoryProtection(Address addr, size_t size, int prot);
    std::optional<Address> allocateMemory(size_t size, int prot = 7);
    bool freeMemory(Address addr, size_t size);
    void setInstructionPointer(Address addr);

    // Auto-Trace execution
    struct AutoTraceResult {
        size_t stepsExecuted{0};
        bool stoppedByCondition{false};
        bool hitBreakpoint{false};
        std::string message;
    };
    AutoTraceResult autoTrace(bool stepOver, size_t maxSteps = 100, const std::string& stopCondition = "");

    [[nodiscard]] const ElfParser& elfParser() const noexcept { return symbols_; }
    [[nodiscard]] const DwarfParser& dwarfParser() const noexcept { return dwarfParser_; }
    [[nodiscard]] DwarfParser& dwarfParser() noexcept { return dwarfParser_; }
    [[nodiscard]] bool hasDebugInfo() const noexcept { return dwarfParser_.hasDebugInfo(); }
    [[nodiscard]] std::optional<SourceLocation> currentSourceLocation() const;
    [[nodiscard]] std::optional<SourceLocation> resolveSourceLocation(Address addr) const;
    [[nodiscard]] std::optional<Address> resolveSourceLine(const std::string& file, int line) const;

    // Source-level Breakpoint & Stepping
    bool toggleSourceBreakpoint(const std::string& file, int line);
    bool hasSourceBreakpoint(const std::string& file, int line) const;
    bool stepSourceOver(int maxInsnSteps = 250);
    bool stepSourceInto(int maxInsnSteps = 250);

    [[nodiscard]] const std::string& targetPath() const noexcept { return targetPath_; }
    [[nodiscard]] BreakpointManager& breakpointManager() noexcept { return bpMgr_; }
    [[nodiscard]] const BreakpointManager& breakpointManager() const noexcept { return bpMgr_; }
    [[nodiscard]] AnnotationManager& annotationManager() noexcept { return annotations_; }
    [[nodiscard]] const AnnotationManager& annotationManager() const noexcept { return annotations_; }
    [[nodiscard]] const AnnotationManager& annotations() const noexcept { return annotations_; }
    [[nodiscard]] AnnotationManager& annotations() noexcept { return annotations_; }
    [[nodiscard]] ScriptEngineManager& scriptEngines() noexcept { return scriptEngines_; }
    [[nodiscard]] const ScriptEngineManager& scriptEngines() const noexcept { return scriptEngines_; }

Q_SIGNALS:
    void stateChanged(edb_next::SessionState state);
    void eventOccurred(const edb_next::DebugEvent& event);
    void registersUpdated();
    void memoryUpdated();
    void breakpointsUpdated();
    void activeThreadChanged(Tid tid);
    void sourceLocationChanged(const edb_next::SourceLocation& loc);

private Q_SLOTS:
    void handleEvent(const edb_next::DebugEvent& event);

private:
    void setState(SessionState s);
    void refreshRegisters();

    std::string id_;
    std::string name_;
    SessionState state_{SessionState::Stopped};

    LinuxDebugEngine engine_;
    BreakpointManager bpMgr_;
    EventLoopThread eventLoop_;
    ElfParser symbols_;
    DwarfParser dwarfParser_;
    AnnotationManager annotations_;
    ScriptEngineManager scriptEngines_;

    RegisterContext currentRegs_;
    RegisterContext previousRegs_;
    user_fpregs_struct currentFpRegs_{};
    user_fpregs_struct previousFpRegs_{};
    bool isStepOverBreak_{false};

    std::string targetPath_;
    std::vector<std::string> targetArgs_;
    std::optional<Address> tempRunToBp_;
    int lastSignal_{0};
};

} // namespace edb_next

Q_DECLARE_METATYPE(edb_next::SessionState)
Q_DECLARE_METATYPE(edb_next::SourceLocation)
