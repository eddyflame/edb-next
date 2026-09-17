#include "SessionTabWidget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFontDatabase>

namespace edb_next {

SessionTabWidget::SessionTabWidget(std::shared_ptr<DebugSession> session, QWidget* parent)
    : QWidget(parent), session_(std::move(session)) {
    setupUi();

    if (session_) {
        connect(session_.get(), &DebugSession::stateChanged, this, &SessionTabWidget::onSessionStateChanged);
        connect(session_.get(), &DebugSession::registersUpdated, this, &SessionTabWidget::onRegistersUpdated);
        connect(session_.get(), &DebugSession::memoryUpdated, this, &SessionTabWidget::onMemoryUpdated);
        connect(session_.get(), &DebugSession::breakpointsUpdated, this, &SessionTabWidget::onBreakpointsUpdated);
    }
}

SessionTabWidget::~SessionTabWidget() = default;

void SessionTabWidget::setupUi() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(2);

    auto* v_splitter = new QSplitter(Qt::Vertical, this);

    // Top Pane: Disassembly (Left) + Registers (Right)
    auto* top_widget = new QWidget(this);
    auto* top_layout = new QVBoxLayout(top_widget);
    top_layout->setContentsMargins(0, 0, 0, 0);
    top_layout->setSpacing(0);

    auto* h_splitter = new QSplitter(Qt::Horizontal, top_widget);
    h_splitter->setChildrenCollapsible(true);

    codeTabs_ = new QTabWidget(h_splitter);
    codeTabs_->setMinimumWidth(200);

    disasmView_ = new DisassemblyView(codeTabs_);
    disasmView_->setSession(session_);

    sourceView_ = new SourceView(codeTabs_);
    sourceView_->setSession(session_);

    codeTabs_->addTab(disasmView_, "Disassembly (Alt+C)");
    codeTabs_->addTab(sourceView_, "Source Code (Alt+S)");

    regView_ = new RegisterView(h_splitter);
    regView_->setMinimumWidth(150);
    regView_->setSession(session_);

    h_splitter->addWidget(codeTabs_);
    h_splitter->addWidget(regView_);
    h_splitter->setStretchFactor(0, 3);
    h_splitter->setStretchFactor(1, 1);

    top_layout->addWidget(h_splitter, 1);

    // Dynamic branch prediction bar
    insnStatusBar_ = new QLabel("Ready.", top_widget);
    insnStatusBar_->setTextFormat(Qt::RichText);
    insnStatusBar_->setStyleSheet("background-color: #1a1a1a; color: #80c0ff; padding: 2px 6px; font-family: monospace; border-top: 1px solid #333;");
    top_layout->addWidget(insnStatusBar_);

    connect(disasmView_, &DisassemblyView::instructionInspected, insnStatusBar_, &QLabel::setText);

    connect(disasmView_, &DisassemblyView::jumpToMemoryRequested, this, [this](Address addr) {
        multiDumpWidget_->jumpToAddress(addr);
        bottomTabs_->setCurrentWidget(multiDumpWidget_);
    });
    connect(regView_, &RegisterView::jumpToMemoryRequested, this, [this](Address addr) {
        multiDumpWidget_->jumpToAddress(addr);
        bottomTabs_->setCurrentWidget(multiDumpWidget_);
    });
    connect(regView_, &RegisterView::jumpToDisassemblyRequested, this, [this](Address addr) {
        showDisassemblyView();
        disasmView_->gotoAddress(addr);
    });
    connect(sourceView_, &SourceView::jumpToDisassemblyRequested, this, [this](Address addr) {
        showDisassemblyView();
        disasmView_->gotoAddress(addr);
    });
    connect(sourceView_, &SourceView::breakpointToggled, this, [this](Address) {
        disasmView_->refresh();
        bpView_->refresh();
    });

    v_splitter->addWidget(top_widget);
    v_splitter->setChildrenCollapsible(true);

    // Bottom Pane: Splitter holding Multi-tab workstation drawer (Left) + Dedicated Stack View (Right)
    bottomSplitter_ = new QSplitter(Qt::Horizontal, v_splitter);
    bottomSplitter_->setChildrenCollapsible(true);

    bottomTabs_ = new QTabWidget(bottomSplitter_);
    bottomTabs_->setUsesScrollButtons(true);
    bottomTabs_->setMinimumWidth(200);

    // Tab 1: Multi-Tab Memory Dump (Dump 1 ~ 4)
    multiDumpWidget_ = new MultiDumpWidget(bottomTabs_);
    multiDumpWidget_->setSession(session_);
    memDumpView_ = multiDumpWidget_->activeDump();
    connect(multiDumpWidget_, &MultiDumpWidget::jumpToDisassemblyRequested, this, [this](Address addr) {
        disasmView_->gotoAddress(addr);
    });
    bottomTabs_->addTab(multiDumpWidget_, "Dump (1-4)");

    // Tab 2: Call Stack
    callStackView_ = new CallStackView(bottomTabs_);
    callStackView_->setSession(session_);
    connect(callStackView_, &CallStackView::jumpToAddressRequested, this, [this](Address ip) {
        disasmView_->gotoAddress(ip);
    });
    bottomTabs_->addTab(callStackView_, "Call Stack");

    // Tab 4: Breakpoints
    bpView_ = new BreakpointManagerView(bottomTabs_);
    bpView_->setSession(session_);
    connect(bpView_, &BreakpointManagerView::jumpToAddressRequested, this, [this](Address addr) {
        disasmView_->gotoAddress(addr);
    });
    bottomTabs_->addTab(bpView_, "Breakpoints");

    // Tab 5: Memory Regions
    regionsView_ = new MemoryRegionsView(bottomTabs_);
    regionsView_->setSession(session_);
    connect(regionsView_, &MemoryRegionsView::jumpToAddressRequested, this, [this](Address addr, bool isExecutable) {
        if (isExecutable) {
            disasmView_->gotoAddress(addr);
        } else {
            multiDumpWidget_->jumpToAddress(addr);
            bottomTabs_->setCurrentWidget(multiDumpWidget_);
        }
    });
    bottomTabs_->addTab(regionsView_, "Memory Regions");

    // Tab 6: Strings
    strRefView_ = new StringReferencesView(bottomTabs_);
    strRefView_->setSession(session_);
    connect(strRefView_, &StringReferencesView::jumpToDisassemblyRequested, this, [this](Address addr) {
        disasmView_->gotoAddress(addr);
    });
    connect(strRefView_, &StringReferencesView::jumpToMemoryRequested, this, [this](Address addr) {
        multiDumpWidget_->jumpToAddress(addr);
        bottomTabs_->setCurrentWidget(multiDumpWidget_);
    });
    bottomTabs_->addTab(strRefView_, "Strings");

    // Tab 7: Symbols
    symView_ = new SymbolViewer(bottomTabs_);
    symView_->setSession(session_);
    connect(symView_, &SymbolViewer::jumpToAddressRequested, this, [this](Address addr) {
        disasmView_->gotoAddress(addr);
    });
    bottomTabs_->addTab(symView_, "Symbols");

    // Tab 8: Heap Analysis
    heapView_ = new HeapView(bottomTabs_);
    heapView_->setSession(session_);
    connect(heapView_, &HeapView::jumpToMemoryRequested, this, [this](Address addr) {
        multiDumpWidget_->jumpToAddress(addr);
        bottomTabs_->setCurrentWidget(multiDumpWidget_);
    });
    bottomTabs_->addTab(heapView_, "Heap Analysis");

    // Tab 9: Process Properties
    procPropView_ = new ProcessPropertiesView(bottomTabs_);
    procPropView_->setSession(session_);
    bottomTabs_->addTab(procPropView_, "Process Info");

    // Tab 10: Threads
    threadsView_ = new ThreadsView(bottomTabs_);
    threadsView_->setSession(session_);
    connect(threadsView_, &ThreadsView::jumpToAddressRequested, this, [this](Address addr) {
        disasmView_->gotoAddress(addr);
    });
    connect(threadsView_, &ThreadsView::threadSwitched, this, [this](Tid) {
        refreshAll();
    });
    bottomTabs_->addTab(threadsView_, "Threads");

    // Tab 11: ROP Tool
    ropView_ = new ROPToolView(bottomTabs_);
    ropView_->setSession(session_);
    connect(ropView_, &ROPToolView::jumpToAddressRequested, this, [this](Address addr) {
        disasmView_->gotoAddress(addr);
    });
    bottomTabs_->addTab(ropView_, "ROP Tool");

    // Tab 12: Watches
    watchView_ = new WatchView(bottomTabs_);
    watchView_->setSession(session_);
    bottomTabs_->addTab(watchView_, "Watches");

    // Tab 13: Trace & Coverage
    traceView_ = new TraceView(traceEngine_, bottomTabs_);
    traceView_->setSession(session_);
    connect(traceView_, &TraceView::jumpToDisassemblyRequested, this, [this](Address addr) {
        disasmView_->gotoAddress(addr);
    });
    bottomTabs_->addTab(traceView_, "Trace & Coverage");

    // Tab 14: Control Flow Graph (CFG)
    cfgView_ = new CFGGraphView(bottomTabs_);
    cfgView_->setSession(session_);
    connect(cfgView_, &CFGGraphView::jumpToDisassemblyRequested, this, [this](Address addr) {
        disasmView_->gotoAddress(addr);
    });
    bottomTabs_->addTab(cfgView_, "Control Flow Graph");

    // Tab 15: Notes (Scratchpad)
    notesView_ = new NotesView(bottomTabs_);
    notesView_->setSession(session_);
    connect(notesView_, &NotesView::requestSaveDatabase, this, [this]() {
        Q_EMIT requestSaveDatabase();
    });
    bottomTabs_->addTab(notesView_, "Notes");

    // Tab 16: Debug Log
    logView_ = new LogView(bottomTabs_);
    bottomTabs_->addTab(logView_, "Debug Log");

    // Tab 17: Binary Info
    binaryInfoView_ = new BinaryInfoView(bottomTabs_);
    binaryInfoView_->setSession(session_);
    connect(binaryInfoView_, &BinaryInfoView::jumpToAddressRequested, this, [this](Address addr, bool isExec) {
        if (isExec) {
            disasmView_->gotoAddress(addr);
        } else {
            multiDumpWidget_->jumpToAddress(addr);
            bottomTabs_->setCurrentWidget(multiDumpWidget_);
        }
    });
    bottomTabs_->addTab(binaryInfoView_, "Binary Info");

    // Tab 18: Intermodular Calls
    intermodularCallsView_ = new IntermodularCallsView(bottomTabs_);
    intermodularCallsView_->setSession(session_);
    connect(intermodularCallsView_, &IntermodularCallsView::jumpToAddressRequested, this, [this](Address addr, bool) {
        disasmView_->gotoAddress(addr);
    });
    bottomTabs_->addTab(intermodularCallsView_, "Intermodular Calls");

    // Tab 19: Opcode Search
    opcodeSearcherView_ = new OpcodeSearcherView(bottomTabs_);
    opcodeSearcherView_->setSession(session_);
    connect(opcodeSearcherView_, &OpcodeSearcherView::jumpToDisassemblyRequested, this, [this](Address addr) {
        disasmView_->gotoAddress(addr);
    });
    bottomTabs_->addTab(opcodeSearcherView_, "Opcode Search");

    // Tab 20: Script Console (Python 3 & Lua 5.4)
    scriptConsoleView_ = new ScriptConsoleView(bottomTabs_);
    scriptConsoleView_->setSession(session_.get());
    bottomTabs_->addTab(scriptConsoleView_, "Script Console");

    // Tab 21: Memory Scanner (CheatEngine-style Differential Scanner)
    memScannerView_ = new MemoryScannerView(bottomTabs_);
    memScannerView_->setSession(session_);
    connect(memScannerView_, &MemoryScannerView::jumpToDisassemblyRequested, this, [this](Address addr) {
        disasmView_->gotoAddress(addr);
    });
    connect(memScannerView_, &MemoryScannerView::jumpToMemoryRequested, this, [this](Address addr) {
        multiDumpWidget_->jumpToAddress(addr);
        bottomTabs_->setCurrentWidget(multiDumpWidget_);
    });
    bottomTabs_->addTab(memScannerView_, "Memory Scanner");

    // Tab 22: Type Viewer (Struct Layout & Compound Types)
    typeViewer_ = new TypeViewer(bottomTabs_);
    typeViewer_->setSession(session_);
    connect(typeViewer_, &TypeViewer::jumpToDisassemblyRequested, this, [this](Address addr) {
        disasmView_->gotoAddress(addr);
    });
    connect(typeViewer_, &TypeViewer::jumpToMemoryRequested, this, [this](Address addr) {
        multiDumpWidget_->jumpToAddress(addr);
        bottomTabs_->setCurrentWidget(multiDumpWidget_);
    });
    bottomTabs_->addTab(typeViewer_, "Type Viewer");

    // Right: Dedicated Stack View (Classic 4-Quadrant Workstation)
    stackView_ = new StackView(bottomSplitter_);
    stackView_->setMinimumWidth(150);
    stackView_->setSession(session_);
    connect(stackView_, &StackView::jumpToDisassemblyRequested, this, [this](Address addr) {
        disasmView_->gotoAddress(addr);
    });
    connect(stackView_, &StackView::jumpToMemoryRequested, this, [this](Address addr) {
        multiDumpWidget_->jumpToAddress(addr);
        bottomTabs_->setCurrentWidget(multiDumpWidget_);
    });

    bottomSplitter_->addWidget(bottomTabs_);
    bottomSplitter_->addWidget(stackView_);
    bottomSplitter_->setStretchFactor(0, 3);
    bottomSplitter_->setStretchFactor(1, 1);

    v_splitter->addWidget(bottomSplitter_);
    v_splitter->setStretchFactor(0, 3);
    v_splitter->setStretchFactor(1, 2);

    main_layout->addWidget(v_splitter);
}

void SessionTabWidget::toggleStackView() {
    if (stackView_) {
        stackView_->setVisible(!stackView_->isVisible());
    }
}

void SessionTabWidget::showSourceView() {
    if (codeTabs_ && sourceView_) {
        codeTabs_->setCurrentWidget(sourceView_);
    }
}

void SessionTabWidget::showDisassemblyView() {
    if (codeTabs_ && disasmView_) {
        codeTabs_->setCurrentWidget(disasmView_);
    }
}

void SessionTabWidget::refreshAll() {
    disasmView_->refresh();
    if (sourceView_) sourceView_->refresh();
    regView_->refresh();

    if (session_ && session_->state() != SessionState::Stopped) {
        // Record execution trace
        Address cur_rip = session_->registers().rip();
        traceEngine_.recordHit(cur_rip);
        auto insns = session_->disassemble(cur_rip, 1);
        if (!insns.empty()) {
            traceEngine_.recordFrame(cur_rip, insns[0].mnemonic, insns[0].operands, session_->registers());
        }
    }
    multiDumpWidget_->refresh();
    memDumpView_ = multiDumpWidget_->activeDump();
    stackView_->refresh();
    callStackView_->refresh();
    bpView_->refresh();
    regionsView_->refresh();
    symView_->refresh();
    procPropView_->refresh();
    threadsView_->refresh();
    watchView_->refresh();
    binaryInfoView_->refresh();
    intermodularCallsView_->refresh();
    if (scriptConsoleView_) scriptConsoleView_->setSession(session_.get());
    if (memScannerView_) memScannerView_->refreshResults();
    if (typeViewer_) typeViewer_->refresh();
}

void SessionTabWidget::onSessionStateChanged(SessionState state) {
    if (state == SessionState::Paused) {
        refreshAll();
    } else if (state == SessionState::Terminated || state == SessionState::Stopped) {
        refreshAll();
    }
}

void SessionTabWidget::onRegistersUpdated() {
    regView_->refresh();
    disasmView_->refresh();
    if (sourceView_) sourceView_->refresh();
    stackView_->refresh();
    callStackView_->refresh();
    if (typeViewer_) typeViewer_->refresh();
}

void SessionTabWidget::onMemoryUpdated() {
    multiDumpWidget_->refresh();
    memDumpView_ = multiDumpWidget_->activeDump();
    stackView_->refresh();
    disasmView_->refresh();
    if (typeViewer_) typeViewer_->refresh();
}

void SessionTabWidget::onBreakpointsUpdated() {
    disasmView_->refresh();
    if (sourceView_) sourceView_->refresh();
    bpView_->refresh();
}

void SessionTabWidget::selectBottomTab(int index) {
    if (bottomTabs_ && index >= 0 && index < bottomTabs_->count()) {
        bottomTabs_->setCurrentIndex(index);
    }
}

} // namespace edb_next
