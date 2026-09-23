#include "SessionTabWidget.hpp"
#include "IRefreshable.hpp"
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

    decompilerView_ = new DecompilerView(&navBus_, codeTabs_);

    codeTabs_->addTab(disasmView_, "Disassembly (Alt+C)");
    codeTabs_->addTab(sourceView_, "Source Code (Alt+S)");
    codeTabs_->addTab(decompilerView_, "Decompiler (F5)");

    connect(codeTabs_, &QTabWidget::currentChanged, this, [this](int index) {
        QWidget* w = codeTabs_->widget(index);
        if (auto* ref = dynamic_cast<IRefreshable*>(w)) {
            if (ref->isDirty()) {
                ref->refresh();
            }
        }
    });

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

    // Time-Travel Debugging (TTD) scrub bar
    timeTravelWidget_ = new TimeTravelWidget(top_widget);
    top_layout->addWidget(timeTravelWidget_);

    if (session_) {
        connect(timeTravelWidget_, &TimeTravelWidget::stepBackRequested, this, [this] {
            session_->stepBack();
        });
        connect(timeTravelWidget_, &TimeTravelWidget::stepForwardRequested, this, [this] {
            session_->stepForward();
        });
        connect(timeTravelWidget_, &TimeTravelWidget::reverseContinueRequested, this, [this] {
            session_->reverseContinue();
        });
        connect(timeTravelWidget_, &TimeTravelWidget::seekFrameRequested, this, [this](size_t idx) {
            session_->seekTimeTravelFrame(idx);
        });
        connect(timeTravelWidget_, &TimeTravelWidget::liveResumeRequested, this, [this] {
            if (session_->timeTravelEngine().frameCount() > 0) {
                session_->seekTimeTravelFrame(session_->timeTravelEngine().frameCount() - 1);
            }
        });
    }

    connect(disasmView_, &DisassemblyView::instructionInspected, insnStatusBar_, &QLabel::setText);

    v_splitter->addWidget(top_widget);
    v_splitter->setChildrenCollapsible(true);

    // Bottom Pane: Splitter holding Multi-tab workstation drawer (Left) + Dedicated Stack View (Right)
    bottomSplitter_ = new QSplitter(Qt::Horizontal, v_splitter);
    bottomSplitter_->setChildrenCollapsible(true);

    bottomTabs_ = new QTabWidget(bottomSplitter_);
    bottomTabs_->setUsesScrollButtons(true);
    bottomTabs_->setMinimumWidth(200);

    connect(bottomTabs_, &QTabWidget::currentChanged, this, [this](int index) {
        QWidget* w = bottomTabs_->widget(index);
        if (auto* ref = dynamic_cast<IRefreshable*>(w)) {
            if (ref->isDirty()) {
                ref->refresh();
            }
        }
    });

    // Tab 1: Multi-Tab Memory Dump (Dump 1 ~ 4)
    multiDumpWidget_ = new MultiDumpWidget(bottomTabs_);
    multiDumpWidget_->setSession(session_);
    memDumpView_ = multiDumpWidget_->activeDump();
    bottomTabs_->addTab(multiDumpWidget_, "Dump (1-4)");

    // Tab 2: Call Stack
    callStackView_ = new CallStackView(bottomTabs_);
    callStackView_->setSession(session_);
    bottomTabs_->addTab(callStackView_, "Call Stack");

    // Tab 4: Breakpoints
    bpView_ = new BreakpointManagerView(bottomTabs_);
    bpView_->setSession(session_);
    bottomTabs_->addTab(bpView_, "Breakpoints");

    // Tab 5: Memory Regions
    regionsView_ = new MemoryRegionsView(bottomTabs_);
    regionsView_->setSession(session_);
    bottomTabs_->addTab(regionsView_, "Memory Regions");

    // Tab 6: Strings
    strRefView_ = new StringReferencesView(bottomTabs_);
    strRefView_->setSession(session_);
    bottomTabs_->addTab(strRefView_, "Strings");

    // Tab 7: Symbols
    symView_ = new SymbolViewer(bottomTabs_);
    symView_->setSession(session_);
    bottomTabs_->addTab(symView_, "Symbols");

    // Tab 8: Heap Analysis
    heapView_ = new HeapView(bottomTabs_);
    heapView_->setSession(session_);
    bottomTabs_->addTab(heapView_, "Heap Analysis");

    // Tab 9: Process Properties
    procPropView_ = new ProcessPropertiesView(bottomTabs_);
    procPropView_->setSession(session_);
    bottomTabs_->addTab(procPropView_, "Process Info");

    // Tab 10: Threads
    threadsView_ = new ThreadsView(bottomTabs_);
    threadsView_->setSession(session_);
    connect(threadsView_, &ThreadsView::threadSwitched, this, [this](Tid) {
        refreshAll();
    });
    bottomTabs_->addTab(threadsView_, "Threads");

    // Tab 11: ROP Tool
    ropView_ = new ROPToolView(bottomTabs_);
    ropView_->setSession(session_);
    bottomTabs_->addTab(ropView_, "ROP Tool");

    // Tab 12: Watches
    watchView_ = new WatchView(bottomTabs_);
    watchView_->setSession(session_);
    bottomTabs_->addTab(watchView_, "Watches");

    // Tab 13: Trace & Coverage
    traceView_ = new TraceView(traceEngine_, bottomTabs_);
    traceView_->setSession(session_);
    bottomTabs_->addTab(traceView_, "Trace & Coverage");

    // Tab 14: Control Flow Graph (CFG)
    cfgView_ = new CFGGraphView(bottomTabs_);
    cfgView_->setSession(session_);
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
    bottomTabs_->addTab(binaryInfoView_, "Binary Info");

    // Tab 18: Intermodular Calls
    intermodularCallsView_ = new IntermodularCallsView(bottomTabs_);
    intermodularCallsView_->setSession(session_);
    bottomTabs_->addTab(intermodularCallsView_, "Intermodular Calls");

    // Tab 19: Opcode Search
    opcodeSearcherView_ = new OpcodeSearcherView(bottomTabs_);
    opcodeSearcherView_->setSession(session_);
    bottomTabs_->addTab(opcodeSearcherView_, "Opcode Search");

    // Tab 20: Script Console (Python 3 & Lua 5.4)
    scriptConsoleView_ = new ScriptConsoleView(bottomTabs_);
    scriptConsoleView_->setSession(session_.get());
    bottomTabs_->addTab(scriptConsoleView_, "Script Console");

    // Tab 21: Memory Scanner (CheatEngine-style Differential Scanner)
    memScannerView_ = new MemoryScannerView(bottomTabs_);
    memScannerView_->setSession(session_);
    bottomTabs_->addTab(memScannerView_, "Memory Scanner");

    // Tab 22: Type Viewer (Struct Layout & Compound Types)
    typeViewer_ = new TypeViewer(bottomTabs_);
    typeViewer_->setSession(session_);
    bottomTabs_->addTab(typeViewer_, "Type Viewer");

    // Right: Dedicated Stack View (Classic 4-Quadrant Workstation)
    stackView_ = new StackView(bottomSplitter_);
    stackView_->setMinimumWidth(150);
    stackView_->setSession(session_);

    bottomSplitter_->addWidget(bottomTabs_);
    bottomSplitter_->addWidget(stackView_);
    bottomSplitter_->setStretchFactor(0, 3);
    bottomSplitter_->setStretchFactor(1, 1);

    v_splitter->addWidget(bottomSplitter_);
    v_splitter->setStretchFactor(0, 3);
    v_splitter->setStretchFactor(1, 2);

    main_layout->addWidget(v_splitter);

    // Wire up centralized NavigationBus for all cross-view navigation requests
    setupNavigationBus();
}

void SessionTabWidget::setupNavigationBus() {
    // 1. Destination bindings: NavigationBus signals -> Target view actions
    connect(&navBus_, &NavigationBus::navigateToDisassembly, this, [this](Address addr) {
        showDisassemblyView();
        if (disasmView_) disasmView_->gotoAddress(addr);
    });
    connect(&navBus_, &NavigationBus::navigateToDump, this, [this](Address addr, int tabIndex) {
        if (multiDumpWidget_) {
            multiDumpWidget_->jumpToAddress(addr, tabIndex);
            switchToBottomTab(multiDumpWidget_);
        }
    });
    connect(&navBus_, &NavigationBus::navigateToStack, this, [this](Address addr) {
        if (stackView_) stackView_->setBaseAddress(addr);
    });
    connect(&navBus_, &NavigationBus::navigateToStruct, this, [this](Address addr) {
        if (typeViewer_) {
            switchToBottomTab(typeViewer_);
            typeViewer_->setInspectAddress(addr);
            typeViewer_->refresh();
        }
    });
    connect(&navBus_, &NavigationBus::navigateToStringReferences, this, [this]() {
        if (bottomTabs_ && strRefView_) {
            switchToBottomTab(strRefView_);
            strRefView_->onScanClicked();
        }
    });
    connect(&navBus_, &NavigationBus::navigateToIntermodularCalls, this, [this]() {
        if (bottomTabs_ && intermodularCallsView_) {
            switchToBottomTab(intermodularCallsView_);
            intermodularCallsView_->handleScanClicked();
        }
    });
    connect(&navBus_, &NavigationBus::navigateToBottomTab, this, [this](QWidget* widget) {
        switchToBottomTab(widget);
    });
    connect(&navBus_, &NavigationBus::navigateToBottomTabIndex, this, [this](int idx) {
        selectBottomTab(idx);
    });
    connect(&navBus_, &NavigationBus::breakpointChangedNotification, this, [this]() {
        if (disasmView_) disasmView_->refresh();
        if (bpView_) bpView_->refresh();
    });

    // 2. Source bindings: Route all subview navigation requests through NavigationBus
    if (disasmView_) {
        connect(disasmView_, &DisassemblyView::jumpToMemoryRequested, &navBus_, &NavigationBus::requestDump);
        connect(disasmView_, &DisassemblyView::jumpToStackRequested, &navBus_, &NavigationBus::requestStack);
        connect(disasmView_, &DisassemblyView::searchStringsRequested, &navBus_, &NavigationBus::requestStringReferences);
        connect(disasmView_, &DisassemblyView::searchIntermodularCallsRequested, &navBus_, &NavigationBus::requestIntermodularCalls);
        connect(disasmView_, &DisassemblyView::breakpointToggled, &navBus_, &NavigationBus::notifyBreakpointChanged);
    }
    if (sourceView_) {
        connect(sourceView_, &SourceView::jumpToDisassemblyRequested, &navBus_, &NavigationBus::requestDisassembly);
        connect(sourceView_, &SourceView::breakpointToggled, &navBus_, &NavigationBus::notifyBreakpointChanged);
    }
    if (regView_) {
        connect(regView_, &RegisterView::jumpToMemoryRequested, &navBus_, &NavigationBus::requestDump);
        connect(regView_, &RegisterView::jumpToDisassemblyRequested, &navBus_, &NavigationBus::requestDisassembly);
        connect(regView_, &RegisterView::jumpToStackRequested, &navBus_, &NavigationBus::requestStack);
    }
    if (multiDumpWidget_) {
        connect(multiDumpWidget_, &MultiDumpWidget::jumpToDisassemblyRequested, &navBus_, &NavigationBus::requestDisassembly);
        connect(multiDumpWidget_, &MultiDumpWidget::jumpToStackRequested, &navBus_, &NavigationBus::requestStack);
        connect(multiDumpWidget_, &MultiDumpWidget::inspectWithTypeViewerRequested, &navBus_, &NavigationBus::requestStruct);
    }
    if (stackView_) {
        connect(stackView_, &StackView::jumpToDisassemblyRequested, &navBus_, &NavigationBus::requestDisassembly);
        connect(stackView_, &StackView::jumpToMemoryRequested, &navBus_, [this](Address addr, int tab) {
            navBus_.requestDump(addr, tab);
        });
        connect(stackView_, &StackView::jumpToStackRequested, &navBus_, &NavigationBus::requestStack);
    }
    if (callStackView_) {
        connect(callStackView_, &CallStackView::jumpToAddressRequested, &navBus_, &NavigationBus::requestDisassembly);
    }
    if (bpView_) {
        connect(bpView_, &BreakpointManagerView::jumpToAddressRequested, &navBus_, &NavigationBus::requestDisassembly);
        connect(bpView_, &BreakpointManagerView::breakpointChanged, &navBus_, &NavigationBus::notifyBreakpointChanged);
    }
    if (regionsView_) {
        connect(regionsView_, &MemoryRegionsView::jumpToAddressRequested, &navBus_, [this](Address addr, bool isExec) {
            if (isExec) navBus_.requestDisassembly(addr);
            else navBus_.requestDump(addr);
        });
    }
    if (strRefView_) {
        connect(strRefView_, &StringReferencesView::jumpToDisassemblyRequested, &navBus_, &NavigationBus::requestDisassembly);
        connect(strRefView_, &StringReferencesView::jumpToMemoryRequested, &navBus_, [this](Address addr) {
            navBus_.requestDump(addr);
        });
    }
    if (symView_) {
        connect(symView_, &SymbolViewer::jumpToAddressRequested, &navBus_, &NavigationBus::requestDisassembly);
    }
    if (heapView_) {
        connect(heapView_, &HeapView::jumpToMemoryRequested, &navBus_, [this](Address addr) {
            navBus_.requestDump(addr);
        });
    }
    if (threadsView_) {
        connect(threadsView_, &ThreadsView::jumpToAddressRequested, &navBus_, &NavigationBus::requestDisassembly);
    }
    if (ropView_) {
        connect(ropView_, &ROPToolView::jumpToAddressRequested, &navBus_, &NavigationBus::requestDisassembly);
    }
    if (traceView_) {
        connect(traceView_, &TraceView::jumpToDisassemblyRequested, &navBus_, &NavigationBus::requestDisassembly);
    }
    if (cfgView_) {
        connect(cfgView_, &CFGGraphView::jumpToDisassemblyRequested, &navBus_, &NavigationBus::requestDisassembly);
    }
    if (binaryInfoView_) {
        connect(binaryInfoView_, &BinaryInfoView::jumpToAddressRequested, &navBus_, [this](Address addr, bool isExec) {
            if (isExec) navBus_.requestDisassembly(addr);
            else navBus_.requestDump(addr);
        });
    }
    if (intermodularCallsView_) {
        connect(intermodularCallsView_, &IntermodularCallsView::jumpToAddressRequested, &navBus_, [this](Address addr, bool) {
            navBus_.requestDisassembly(addr);
        });
    }
    if (opcodeSearcherView_) {
        connect(opcodeSearcherView_, &OpcodeSearcherView::jumpToDisassemblyRequested, &navBus_, &NavigationBus::requestDisassembly);
    }
    if (memScannerView_) {
        connect(memScannerView_, &MemoryScannerView::jumpToDisassemblyRequested, &navBus_, &NavigationBus::requestDisassembly);
        connect(memScannerView_, &MemoryScannerView::jumpToMemoryRequested, &navBus_, [this](Address addr) {
            navBus_.requestDump(addr);
        });
    }
    if (typeViewer_) {
        connect(typeViewer_, &TypeViewer::jumpToDisassemblyRequested, &navBus_, &NavigationBus::requestDisassembly);
        connect(typeViewer_, &TypeViewer::jumpToMemoryRequested, &navBus_, [this](Address addr) {
            navBus_.requestDump(addr);
        });
    }
}

void SessionTabWidget::toggleStackView() {
    if (stackView_) {
        stackView_->setVisible(!stackView_->isVisible());
    }
}

void SessionTabWidget::showSourceView() {
    if (codeTabs_ && sourceView_) {
        bool changed = (codeTabs_->currentWidget() != sourceView_);
        codeTabs_->setCurrentWidget(sourceView_);
        if (!changed && sourceView_->isDirty()) {
            sourceView_->refresh();
        }
    }
}

void SessionTabWidget::showDisassemblyView() {
    if (codeTabs_ && disasmView_) {
        codeTabs_->setCurrentWidget(disasmView_);
    }
}

void SessionTabWidget::showDecompilerView() {
    if (codeTabs_ && decompilerView_) {
        codeTabs_->setCurrentWidget(decompilerView_);
        if (session_) {
            Address rip = session_->registers().rip();
            if (!rip.isNull()) {
                decompilerView_->decompileAt(session_, rip);
            }
        }
    }
}

void SessionTabWidget::refreshAll() {
    // 1. Core quadrant views (always visible or main execution views)
    disasmView_->refresh();
    regView_->refresh();
    stackView_->refresh();

    if (sourceView_) {
        if (codeTabs_ && codeTabs_->currentWidget() == sourceView_) {
            sourceView_->refresh();
        } else {
            sourceView_->markDirty();
        }
    }

    if (session_ && session_->state() != SessionState::Stopped) {
        // Record execution trace
        Address cur_rip = session_->registers().rip();
        traceEngine_.recordHit(cur_rip);
        auto insns = session_->disassemble(cur_rip, 1);
        if (!insns.empty()) {
            traceEngine_.recordFrame(cur_rip, insns[0].mnemonic, insns[0].operands, session_->registers());
        }
    }

    // 2. Active bottom tab: refresh immediately
    QWidget* activeTab = bottomTabs_ ? bottomTabs_->currentWidget() : nullptr;
    if (auto* ref = dynamic_cast<IRefreshable*>(activeTab)) {
        ref->refresh();
    }

    // 3. Inactive bottom tabs: mark dirty (lazy refresh on tab activation)
    if (bottomTabs_) {
        for (int i = 0; i < bottomTabs_->count(); ++i) {
            QWidget* w = bottomTabs_->widget(i);
            if (w != activeTab) {
                if (auto* ref = dynamic_cast<IRefreshable*>(w)) {
                    ref->markDirty();
                }
            }
        }
    }

    memDumpView_ = multiDumpWidget_->activeDump();
    if (scriptConsoleView_) scriptConsoleView_->setSession(session_.get());
}

void SessionTabWidget::onSessionStateChanged(SessionState state) {
    if (state == SessionState::Paused) {
        if (disasmView_) {
            disasmView_->setFollowRip(true);
        }
        refreshAll();
    } else if (state == SessionState::Terminated || state == SessionState::Stopped) {
        refreshAll();
    }
}

void SessionTabWidget::onRegistersUpdated() {
    regView_->refresh();
    disasmView_->refresh();
    if (sourceView_) {
        if (codeTabs_ && codeTabs_->currentWidget() == sourceView_) {
            sourceView_->refresh();
        } else {
            sourceView_->markDirty();
        }
    }
    if (decompilerView_ && session_) {
        Address rip = session_->registers().rip();
        if (!rip.isNull()) {
            decompilerView_->highlightAddress(rip);
        }
    }
    if (timeTravelWidget_ && session_) {
        timeTravelWidget_->updateTimeline(session_->timeTravelEngine());
    }
    stackView_->refresh();

    QWidget* activeTab = bottomTabs_ ? bottomTabs_->currentWidget() : nullptr;
    if (callStackView_) {
        if (activeTab == callStackView_) {
            callStackView_->refresh();
        } else {
            callStackView_->markDirty();
        }
    }
    if (typeViewer_) {
        if (activeTab == typeViewer_) {
            typeViewer_->refresh();
        } else {
            typeViewer_->markDirty();
        }
    }
}

void SessionTabWidget::onMemoryUpdated() {
    QWidget* activeTab = bottomTabs_ ? bottomTabs_->currentWidget() : nullptr;
    if (multiDumpWidget_) {
        if (activeTab == multiDumpWidget_) {
            multiDumpWidget_->refresh();
        } else {
            multiDumpWidget_->markDirty();
        }
    }
    memDumpView_ = multiDumpWidget_->activeDump();
    stackView_->refresh();
    disasmView_->refresh();

    if (sourceView_) {
        if (codeTabs_ && codeTabs_->currentWidget() == sourceView_) {
            sourceView_->refresh();
        } else {
            sourceView_->markDirty();
        }
    }
    if (typeViewer_) {
        if (activeTab == typeViewer_) {
            typeViewer_->refresh();
        } else {
            typeViewer_->markDirty();
        }
    }
}

void SessionTabWidget::onBreakpointsUpdated() {
    disasmView_->refresh();
    if (sourceView_) {
        if (codeTabs_ && codeTabs_->currentWidget() == sourceView_) {
            sourceView_->refresh();
        } else {
            sourceView_->markDirty();
        }
    }
    if (bpView_) {
        QWidget* activeTab = bottomTabs_ ? bottomTabs_->currentWidget() : nullptr;
        if (activeTab == bpView_) {
            bpView_->refresh();
        } else {
            bpView_->markDirty();
        }
    }
}

void SessionTabWidget::selectBottomTab(int index) {
    if (bottomTabs_ && index >= 0 && index < bottomTabs_->count()) {
        bool changed = (bottomTabs_->currentIndex() != index);
        bottomTabs_->setCurrentIndex(index);
        if (!changed) {
            QWidget* w = bottomTabs_->widget(index);
            if (auto* ref = dynamic_cast<IRefreshable*>(w)) {
                if (ref->isDirty()) {
                    ref->refresh();
                }
            }
        }
    }
}

void SessionTabWidget::switchToBottomTab(QWidget* widget) {
    if (!bottomTabs_ || !widget) return;
    bool changed = (bottomTabs_->currentWidget() != widget);
    bottomTabs_->setCurrentWidget(widget);
    if (!changed) {
        if (auto* ref = dynamic_cast<IRefreshable*>(widget)) {
            if (ref->isDirty()) {
                ref->refresh();
            }
        }
    }
}

} // namespace edb_next
