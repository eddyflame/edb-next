#pragma once

#include "DebugSession.hpp"
#include "DisassemblyView.hpp"
#include "RegisterView.hpp"
#include "MemoryHexView.hpp"
#include "CallStackView.hpp"
#include "BreakpointManagerView.hpp"
#include "MemoryRegionsView.hpp"
#include "StringReferencesView.hpp"
#include "SymbolViewer.hpp"
#include "ProcessPropertiesView.hpp"
#include "HeapView.hpp"
#include "ThreadsView.hpp"
#include "ROPToolView.hpp"
#include "WatchView.hpp"
#include "TraceView.hpp"
#include "CFGGraphView.hpp"
#include "NotesView.hpp"
#include "LogView.hpp"
#include "BinaryInfoView.hpp"
#include "IntermodularCallsView.hpp"
#include "MultiDumpWidget.hpp"
#include "OpcodeSearcherView.hpp"
#include "StackView.hpp"
#include "SourceView.hpp"
#include "ScriptConsoleView.hpp"
#include "MemoryScannerView.hpp"
#include "core/TraceEngine.hpp"
#include <QWidget>
#include <QSplitter>
#include <QTabWidget>
#include <QLabel>
#include <memory>

namespace edb_next {

class SessionTabWidget : public QWidget {
    Q_OBJECT

public:
    explicit SessionTabWidget(std::shared_ptr<DebugSession> session, QWidget* parent = nullptr);
    ~SessionTabWidget() override;

    [[nodiscard]] std::shared_ptr<DebugSession> session() const noexcept { return session_; }
    void refreshAll();

    DisassemblyView* disasmView() const noexcept { return disasmView_; }
    SourceView* sourceView() const noexcept { return sourceView_; }
    QTabWidget* codeTabWidget() const noexcept { return codeTabs_; }
    void showSourceView();
    void showDisassemblyView();

    RegisterView* registerView() const noexcept { return regView_; }
    MemoryHexView* memoryDumpView() const noexcept { return memDumpView_; }
    StackView* stackView() const noexcept { return stackView_; }
    CallStackView* callStackView() const noexcept { return callStackView_; }
    BreakpointManagerView* breakpointView() const noexcept { return bpView_; }
    MemoryRegionsView* memoryRegionsView() const noexcept { return regionsView_; }
    StringReferencesView* stringReferencesView() const noexcept { return strRefView_; }
    SymbolViewer* symbolViewer() const noexcept { return symView_; }
    ProcessPropertiesView* processPropertiesView() const noexcept { return procPropView_; }
    HeapView* heapView() const noexcept { return heapView_; }
    ThreadsView* threadsView() const noexcept { return threadsView_; }
    ROPToolView* ropView() const noexcept { return ropView_; }
    WatchView* watchView() const noexcept { return watchView_; }
    TraceView* traceView() const noexcept { return traceView_; }
    CFGGraphView* cfgGraphView() const noexcept { return cfgView_; }
    NotesView* notesView() const noexcept { return notesView_; }
    LogView* logView() const noexcept { return logView_; }
    BinaryInfoView* binaryInfoView() const noexcept { return binaryInfoView_; }
    IntermodularCallsView* intermodularCallsView() const noexcept { return intermodularCallsView_; }
    MultiDumpWidget* multiDumpWidget() const noexcept { return multiDumpWidget_; }
    OpcodeSearcherView* opcodeSearcherView() const noexcept { return opcodeSearcherView_; }
    ScriptConsoleView* scriptConsoleView() const noexcept { return scriptConsoleView_; }
    MemoryScannerView* memoryScannerView() const noexcept { return memScannerView_; }
    TraceEngine& traceEngine() noexcept { return traceEngine_; }

    QTabWidget* bottomTabs() const noexcept { return bottomTabs_; }
    QSplitter* bottomSplitter() const noexcept { return bottomSplitter_; }
    void selectBottomTab(int index);
    void toggleStackView();

Q_SIGNALS:
    void requestSaveDatabase();

private Q_SLOTS:
    void onSessionStateChanged(SessionState state);
    void onRegistersUpdated();
    void onMemoryUpdated();
    void onBreakpointsUpdated();

private:
    void setupUi();

    std::shared_ptr<DebugSession> session_;
    TraceEngine traceEngine_;

    DisassemblyView* disasmView_{nullptr};
    SourceView* sourceView_{nullptr};
    QTabWidget* codeTabs_{nullptr};
    RegisterView* regView_{nullptr};
    QLabel* insnStatusBar_{nullptr};

    QSplitter* bottomSplitter_{nullptr};
    QTabWidget* bottomTabs_{nullptr};
    MemoryHexView* memDumpView_{nullptr};
    StackView* stackView_{nullptr};
    CallStackView* callStackView_{nullptr};
    BreakpointManagerView* bpView_{nullptr};
    MemoryRegionsView* regionsView_{nullptr};
    StringReferencesView* strRefView_{nullptr};
    SymbolViewer* symView_{nullptr};
    ProcessPropertiesView* procPropView_{nullptr};
    HeapView* heapView_{nullptr};
    ThreadsView* threadsView_{nullptr};
    ROPToolView* ropView_{nullptr};
    WatchView* watchView_{nullptr};
    TraceView* traceView_{nullptr};
    CFGGraphView* cfgView_{nullptr};
    NotesView* notesView_{nullptr};
    LogView* logView_{nullptr};
    BinaryInfoView* binaryInfoView_{nullptr};
    IntermodularCallsView* intermodularCallsView_{nullptr};
    MultiDumpWidget* multiDumpWidget_{nullptr};
    OpcodeSearcherView* opcodeSearcherView_{nullptr};
    ScriptConsoleView* scriptConsoleView_{nullptr};
    MemoryScannerView* memScannerView_{nullptr};
};

} // namespace edb_next
