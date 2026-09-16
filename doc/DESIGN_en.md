# edb-next Software Architecture & Engineering Design Document (SDD)

> **Project Name**: edb-next (Next-Generation Linux Binary Debugger & Reverse Engineering Platform)  
> **Document Version**: v1.0.0 (Release Candidate)  
> **Language & Standards**: C++20 / Qt 5.15+ / Capstone Disassembly Engine  
> **Target Architecture**: Linux x86_64  
> **Project Goal**: Standalone, industrial-grade open-source binary analysis and dynamic reverse engineering platform for Linux on GitHub.

---

## Table of Contents

1. [Project Vision & Positioning](#1-project-vision--positioning)
   - 1.1 Motivation & Industry Pain Points
   - 1.2 Competitive Analysis (edb-next vs. Original edb vs. x64dbg)
   - 1.3 Core Engineering Principles
2. [Key Characteristics & Architectural Highlights](#2-key-characteristics--architectural-highlights)
3. [Implemented Features Breakdown (10 Core Subsystems)](#3-implemented-features-breakdown-10-core-subsystems)
   - 3.1 Core Kernel & Execution Control Engine
   - 3.2 Disassembly & Control Flow Analysis
   - 3.3 CPU Registers & Machine State Management
   - 3.4 Memory Management, Multi-Dump & Disk Patching
   - 3.5 Runtime Stack & Call Frame Unwinding
   - 3.6 Heap Deconstruction & Linux OS Introspection
   - 3.7 Exploitation & Advanced Reverse Engineering Toolkit
   - 3.8 Project Database & Session Persistence
   - 3.9 Interactive CLI Console & Plugin System
   - 3.10 4-Quadrant Golden Workspace & UI/UX Design
   - 3.11 DWARF Source-Level Debugging & Line Mapping Subsystem
   - 3.12 Embedded Python 3 & Lua 5.4 Dual Scripting Automation Engine
4. [Unimplemented Features & Technical Roadmap](#4-unimplemented-features--technical-roadmap)
   - 4.1 Multi-Architecture & Cross-Debugging Support (ARM64 / x86-32)
   - 4.2 Advanced Anti-Anti-Debugging & Stealth Breakpoints
   - 4.3 C++ Symbol Demangling & Type Layout Reconstruction
   - 4.4 Multi-Process Follow-Fork & IPC Tracing
   - 4.5 Fine-Grained Hardware Watchpoint UI & Page-Guard Traps
   - 4.6 GDB Remote Serial Protocol (RSP) Client Support
   - 4.7 Automated Shared Library Loading Interception (_r_debug Rendezvous)
   - 4.8 Script-Driven Breakpoint Actions & High-Frequency Hooking
   - 4.9 Independent Thread Freeze & Thaw Execution Control
   - 4.10 Differential Memory Pattern & Value Scanner
   - 4.11 Priority & Importance Evaluation Matrix
5. [Codebase Structure & Module Architecture](#5-codebase-structure--module-architecture)
   - 5.1 Complete Source Tree & Responsibilities
   - 5.2 Layered System Topology
   - 5.3 Class Hierarchy & Interface Relations
   - 5.4 Key Operational Sequence Flows
6. [Detailed Technical Schemes & Implementations](#6-detailed-technical-schemes--implementations)
   - 6.1 Asynchronous Non-Blocking Concurrency Model
   - 6.2 Breakpoint Lifecycle & Step-Over State Machine
   - 6.3 Remote Syscall Atomic Injection (`executeRemoteSyscall`)
   - 6.4 Recursive Descent Expression Evaluator (`ExpressionEvaluator`)
   - 6.5 ELF Physical Disk Patching Algorithm (`patchFileToDisk`)
   - 6.6 Modern C++20 Decoupled Plugin Gateway
   - 6.7 Project Database Serialization & Deserialization (`.edb_db`)
7. [Build, Installation & Quality Assurance](#7-build-installation--quality-assurance)
   - 7.1 System Dependencies
   - 7.2 Compilation Commands
   - 7.3 Automated Test Suite Execution
8. [GitHub Release & Open Source Specifications](#8-github-release--open-source-specifications)

---

## 1. Project Vision & Positioning

### 1.1 Motivation & Industry Pain Points
On the Windows platform, reverse engineers benefit from **x64dbg**, a legendary, full-featured graphical debugger celebrated for its 4-quadrant golden workflow, interactive CLI console, multi-tab memory hex dumps, and persistent database.

Conversely, the Linux ecosystem has suffered from a distinct gap:
1. **GDB / GEF / pwndbg**: Immensely capable command-line utilities, but challenging when inspecting complex cross-sectional memory, navigating basic-block topology, comparing multi-window hex dumps, or conducting real-time hot-patching.
2. **Original edb-debugger**: A pioneering Qt debugger for Linux, but hampered by legacy architectural design:
   - **Over-fragmentation**: Core features were divided into 22 separate plugins communicating through a fragile global Service Locator (`edb::v1::*`);
   - **Event Loop Freezes & Deadlocks**: The GUI thread relied on a 0ms `QTimer` polling `waitpid()`, creating event starvation and UI hangs during high-frequency breakpoints or signal processing;
   - **Missing Reverse Engineering Capabilities**: Limited to a single memory dump, unable to patch read-only memory, unable to export patched ELF binaries to disk, and missing session persistence (all annotations and breakpoints were lost on exit).

### 1.2 Competitive Analysis (edb-next vs. Original edb vs. x64dbg)

| Dimension / Feature | Original edb (Linux) | x64dbg (Windows) | edb-next (Modern Linux Rewrite) | Key Advantage |
| :--- | :---: | :---: | :---: | :--- |
| **Language Standard** | C++11 with C macros | C++14/17 | **Modern C++20 Standard** | Strong typing, RAII, concepts, strict memory safety |
| **Event Concurrency** | 0ms QTimer polling (prone to deadlocks) | Complex sync events | **Decoupled EventLoopThread** | Dedicated thread `waitpid(WNOHANG)` + atomic suspension, 0% UI freeze |
| **Workspace Layout** | Single bottom drawer (excessive tab switching) | Classic 4 quadrants | **4-Quadrant Golden Layout** | Disassembly, Registers, Multi-Dump, Dedicated Stack synced in parallel |
| **Memory Dumps** | Single Dump view | Dump 1 ~ Dump 5 | **4-Way MultiDumpWidget** | Independent offsets, histories, and context menu follow |
| **Read-Only Memory Patching** | Errors / Rejected | VirtualProtect simulation | **Remote Syscall Injection** | Injects `SYS_mprotect` in-target to unlock pages; `SYS_mmap` for payload alloc |
| **Disk Binary Patching**| None (Memory-only) | Patched EXE export | **Innovative `patchFileToDisk`**| Translates `VAddr -> FileOffset`, exports standalone modified ELF binaries |
| **Session Persistence**| Lost on application exit | `.dd64` project database | **`.edb_db` JSON Database** | Auto-loads matching hash/binary path: restores comments, marks, breakpoints, notes |
| **Command-Line CLI** | None | Core bottom input | **x64dbg-Style CommandBar** | Built-in high-frequency commands, history, expression evaluation, plugin extensible |
| **Linux Heap Analysis**| Outdated plugin | N/A (Windows-only) | **Native Glibc ptmalloc Analyzer**| Traverses `malloc_chunk` lists, flags (`A\|M\|P`), double-click to follow in Dump |
| **Signals & Syscalls** | Basic pass-through | N/A | **Signal Matrix & Remote Syscalls** | Matrix filter for signals 1-64, `Shift+F7/F8/F9` pass-through execution |
| **Exploit Tooling** | Basic ROP plugin | 3rd party plugins | **Built-in ROP Engine** | Sliding-window scanner, categories, one-click Python `p64(...)` script export |

---

## 2. Key Characteristics & Architectural Highlights

1. **Non-Blocking Multithreaded Event Loop**:
   - `EventLoopThread` runs in a dedicated `QThread`, polling `waitpid(-1, &status, __WALL | WNOHANG)`.
   - Thread-safe Qt signal-slot dispatching (`Qt::QueuedConnection`) delivers events to the GUI without UI lag.
   - Foreground atomic suspension (`suspended_`) synchronizes remote syscall injections without race conditions.
2. **In-Target Remote Syscall Injection (`executeRemoteSyscall`)**:
   - Injects temporary `0x0F 0x05` (`syscall`) opcodes, sets AMD64 ABI argument registers (`RAX`, `RDI`, `RSI`, `RDX`, `R10`, `R8`, `R9`), single-steps, captures return values, and atomically restores machine state.
   - Enables `remoteMprotect` (page permission elevation to RWX), `remoteMmap` (dynamic allocation in target space), and `remoteMunmap`.
3. **Physical ELF Disk Patching Engine (`patchFileToDisk`)**:
   - Parses ELF `PT_LOAD` Program Headers and translates Virtual Memory Addresses ($VAddr$) to file offsets ($FileOffset = VAddr - Segment.p\_vaddr + Segment.p\_offset$).
   - Writes memory modifications directly back into a new ELF binary on disk with `0755` executable permissions.
4. **Persistent Project Database (`.edb_db`)**:
   - Automatically saves and restores comments, bookmarks, breakpoints (with conditions & ignore counts), watches, memory patches, and reverse engineering scratch notes in standard JSON format.
5. **Classic 4-Quadrant Golden Workspace Layout**:
   - **Top-Left**: `DisassemblyView` (syntax coloring, jump route arrows, operand previews, branch history stack).
   - **Top-Right**: `RegisterView` (16 GPRs, smart dereferencing, live EFLAGS toggle badges, SSE/AVX vectors).
   - **Bottom-Left**: `MultiDumpWidget` (Dump 1 ~ Dump 4 multi-tab container) + 18 analysis drawer tools.
   - **Bottom-Right**: `StackView` (dedicated 64-bit QWORD alignment, `=> RSP` high-contrast indicator, relative offset calculations, symbol binding).

---

## 3. Implemented Features Breakdown (10 Core Subsystems)

### 3.1 Core Kernel & Execution Control Engine
- **Process Lifecycle**: Launch via `fork()` + `PTRACE_TRACEME` with automatic `personality(ADDR_NO_RANDOMIZE)` ASLR disabling; `PTRACE_ATTACH` attaching; clean detach, termination, and instant restart (`Ctrl+F2`).
- **Execution Steering**: Continue (`F9`), Single Step Into (`F7`), Single Step Over (`F8`), Step Out (`Shift+F11`), Run to Selection (`F4`), and asynchronous target pause.
- **Breakpoints**: INT3 (`0xCC`) software breakpoints with automatic single-step-restore state machine; DR0~DR7 hardware execution and read/write watchpoints; conditional expressions (`rdi == 7`, `[rbp-8] > 0`); ignore count skipping; format-string log-only tracepoints.
- **Signal Control**: 1~64 POSIX signal intercept/pass matrix; `Shift+F7/F8/F9` signal pass-through execution.
- **Remote Syscall Execution**: Native `SYS_mprotect`, `SYS_mmap`, `SYS_munmap` injection; `Ctrl+*` Set RIP.
- **Multithreading**: Full thread enumeration via `/proc/<pid>/task/`; active thread switching (active TID) with independent register context inspection.

### 3.2 Disassembly & Control Flow Analysis
- **Capstone Disassembly**: Full x86_64 syntax highlighting, operand previews, auto-resolution of `[rip + disp]` targets with symbol binding.
- **Dynamic Branch Prediction**: Real-time evaluation of condition flags (ZF, SF, OF, CF) on the bottom bar: `[JUMP TAKEN]` vs. `[JUMP NOT TAKEN]`.
- **Keyboard Branch Navigation**: **Enter** to follow branches (`call`, `jmp`, `jcc`) with automatic history stack recording; **Esc** / **Backspace** / **Alt+Left** to return; **Alt+Right** to advance.
- **Code XREFs (`X`)**: Instant search for all incoming `CALL`, `JMP`, and `LEA [rip+disp]` references to the selected address.
- **Native GNU Inline Assembler (`Space`)**: Invokes GNU `as` + `objcopy` in-memory; automatic NOP padding for instruction alignment preservation.
- **Opcode Searcher (Tab 19)**: Fast scanning for exploitable instructions (`JMP reg`, `CALL reg`, `PUSH reg; RET`, `Syscall`) and custom regex.
- **Control Flow Graph (Tab 14)**: Basic-block separation and hierarchical directed graph rendering with colored branch routing (green: taken, red: fall-through, blue: unconditional).

### 3.3 CPU Registers & Machine State Management
- **16 General Purpose Registers (GPRs)**: 64-bit hex display, delta-highlighting in red on modifications.
- **Smart Dereference Column**: Automatic symbol offset matching (`<main+0x10>`), stack pointer indicator (`=> [RSP]`), ASCII string previews, and secondary pointer chains.
- **Live EFLAGS Toggle Badges**: Clickable badges for CF, PF, AF, ZF, SF, TF, IF, DF, OF with real-time color feedback (emerald green for set, dark gray for clear) and immediate register writeback.
- **SSE / AVX Vector Panel**: 128-bit hex, 4x Float32, and 2x Double64 interactive inspection.
- **StateDumper (`Ctrl+D`)**: Complete CPU snapshot report generation (PID, registers, flags, disassembly context, stack top QWORDs) copied to system clipboard.

### 3.4 Memory Management, Multi-Dump & Disk Patching
- **4-Way MultiDumpWidget**: Independent Dump 1 ~ Dump 4 tabs with separate base addresses, scroll offsets, and history stacks.
- **In-Place Byte Patching (`Ctrl+E`)**: Real-time hex editing, zero-fill, and NOP-fill.
- **PatchManager (`Ctrl+P`)**: Centralized patch review, side-by-side original/patched byte diffs, individual/bulk revert & reapply.
- **Physical Disk Patching (`patchFileToDisk`)**: Writes modified sections back to executable ELF files on disk with valid execution flags.
- **Memory Segment Dump**: Exports any memory region as a raw binary file (`.bin`).
- **Pattern Search**: Memory search supporting `??` wildcards (e.g. `f3 0f ?? fa`).
- **String Scanner**: Extraction of printable ASCII strings ($\ge 4$ chars) cross-referenced to code references.

### 3.5 Runtime Stack & Call Frame Unwinding
- **Dedicated StackView**: 8-byte QWORD aligned layout; dynamic relative offset to RSP/RBP; intelligent double-click routing (disassembly jump for code pointers, hex dump jump for data pointers); `[RSP]` reset button; `Shift+S` expand/collapse shortcut.
- **CallStackUnwinder (Tab 3)**: Safe RBP frame chain traversal with alignment, monotonic increment, and memory readability checks; symbol-annotated Frame #0~#N.

### 3.6 Heap Deconstruction & Linux OS Introspection
- **Glibc ptmalloc Heap Analyzer (Tab 9)**: Traverses `malloc_chunk` structures in `[heap]`, parses size and `A|M|P` flags, identifies Allocated/Free/Top chunks, double-click to follow in Dump.
- **ELF Binary Info (Tab 17)**: ELF 64 header, Program Headers, Section Headers, Dynamic tags (`DT_NEEDED` library tree).
- **Intermodular Calls Finder (Tab 18)**: Identifies PLT/GOT calls to external libraries (`libc.so.6`), supports instant filtering and double-click jump.
- **Process Properties (Tab 8)**: `/proc/<pid>/cmdline`, `environ`, `status`; file descriptor inspection (`/proc/<pid>/fd/`) with symbolic link resolution into Regular Files, Sockets, Pipes, and PTYs.

### 3.7 Exploitation & Advanced Reverse Engineering Toolkit
- **ROP Scanner (Tab 11)**: Reverse sliding-window scanner categorizing gadgets into StackPivot, Syscall, MemoryWrite, Arithmetic, and General; one-click Python `p64(...)` payload export.
- **Execution Trace (Tab 13)**: Hit Trace (coverage visualization) and Run Trace (step history delta logging with `< Step Back` / `Step Forward >` time-travel navigation); Auto Trace with conditional stop criteria.
- **Dynamic Watches (Tab 12)**: Live watches for registers (`rax`), pointer dereferences (`[rbp-8]`), and compound arithmetic expressions.
- **Analysis Notes (Tab 15)**: Integrated monospace scratchpad with one-click RIP and timestamp insertion, synchronized to project database.

### 3.8 Project Database & Session Persistence
- **DatabaseManager**: High-performance JSON project format storing all comments, bookmarks, breakpoints, watches, patches, and scratch notes; automatically matches and loads `<binary>.edb_db` on program startup.

### 3.9 Interactive CLI Console & Plugin System
- **CommandBarView**: Bottom interactive CLI supporting `bp`, `bph`, `bc`, `bd`, `be`, `r`, `d`, `u`, `step`, `stepo`, `ret`, `run`, `eval`, `mprotect`, `alloc`, `free`, `dumpstate`, `help`.
- **C++20 Plugin System**: Decoupled `IPlugin` and `IPluginContext` contract supporting dynamic `.so` loading via `QPluginLoader`, menu injection, CLI command registration, and breakpoint/debug event hooks.

### 3.10 4-Quadrant Golden Workspace & UI/UX Design
- **Modern Dark Geek Theme**: Flat tabs, 2px active blue accents, high-contrast tables, and minimal scrollbars.
- **Desktop Environment Adaptability**: Wayland window constraint resolution, sub-400px minimum width support for seamless maximizing across high-DPI and laptop screens.

### 3.11 DWARF Source-Level Debugging & Line Mapping Subsystem
- **Native libdw Extraction & Line Mapping**: Integrates `libdw` (`core/DwarfParser` and `core/SourceFileManager`) to parse ELF `.debug_info`, `.debug_line`, and `.debug_str` sections, building a fast lookup cache for bidirectional `Address <-> (File:Line:Column)` mapping. Automatically resolves absolute and relative source paths with read-only file caching.
- **Mixed-Mode Disassembly (`Ctrl+Shift+S`)**: Inline dark-green banners in `DisassemblyView` displaying matching C/C++ source statements and line numbers above assembly basic blocks.
- **Dedicated Source Browser (`SourceView`, `Alt+S`)**: Central tab providing multi-file browsing, instruction pointer indicators (`➔`), breakpoint markers (`●`), double-click source breakpoints, and source step-over (`stepSourceOver`) and step-into (`stepSourceInto`) execution.

### 3.12 Embedded Python 3 & Lua 5.4 Dual Scripting Automation Engine
- **Unified Dual-Engine Architecture**: Managed by `IScriptEngine` and `ScriptEngineManager`, providing dynamic language routing by extension (`.py` / `.lua`) or CLI command prefix (`py` / `lua`), while synchronizing lifecycle with active `DebugSession`.
- **Python 3 C-API Embedding**: Embedded CPython 3 runtime exporting native built-in module `edb` (registers, memory I/O, breakpoints, stepping, expression evaluation, process state) with complete `sys.stdout`/`sys.stderr` capture and traceback formatting redirected to the console.
- **Lua 5.4 High-Performance Embedding**: Embedded Lua 5.4 runtime providing symmetric APIs under global table `edb` and redirected `print()`, optimized for microsecond-latency condition evaluation, high-frequency hooks, and GIL-free automation.
- **Interactive Script Console (`ScriptConsoleView`, `Alt+P`)**: Bottom drawer terminal with language switcher, one-click script file execution (`▶ Run File...`), history navigation, and dark syntax color rendering.
- **CommandBar Quick Commands & CI Coverage**: Integrated single-line execution (`py <expr>` / `lua <expr>`) with 100% automated regression test suite (`tests/test_scripting.cpp`).

### 3.13 C++ Symbol Demangling System
- **Itanium ABI Demangler Integration**: Core integration of GNU `<cxxabi.h>` `abi::__cxa_demangle` in `ElfParser::demangle()`, with zero-copy fallback for non-mangled C symbols and zero external dependencies.
- **Universal Readability Injection**: Extended `SymbolInfo` to hold both `mangledName` and `demangledName` alongside a unified `displayName()` accessor. Bidirectional lookup tree (`nameToAddress_`) supports lookups by either name. Full pipeline integration across CallStack backtraces, disassembly banners, register smart dereferences, and stack memory annotations.
- **Enhanced Symbol Viewer (`SymbolViewer`, `Alt+E`)**: Presents clean demangled function signatures in table columns, tooltip preservation of raw mangled identifiers, and real-time bidirectional search filtering.

### 3.14 Fine-Grained Hardware Watchpoint UI & Breakpoint Cell Highlighting
- **Memory Hex View Context Menu**: Complete "Breakpoint" submenu on right-click in `MemoryHexView`:
  - **Set Hardware Write Watchpoint**: 1, 2, 4, or 8 bytes.
  - **Set Hardware Read/Write Watchpoint**: 1, 2, 4, or 8 bytes.
  - **Set Hardware Execute Breakpoint**: 1 byte.
### 3.15 Script-Driven Breakpoint Actions & Silent Dynamic Hooking
- **Unified Breakpoint Script Hooks**: Extended `Breakpoint` with `scriptCode` and `scriptLanguage` (`python` / `lua`). Defined `executeHook(const std::string& code)` across `IScriptEngine`, `PythonScriptEngine` (protecting user code inside `def __edb_bp_hook__(): ...`), and `LuaScriptEngine` (wrapping in local closure).
- **Microsecond Silent Hooking Bypass**: When a breakpoint is hit, the corresponding scripting engine executes the attached script. If the script explicitly returns `false` (e.g. `return False` in Python, `return false` in Lua), the engine executes an automatic single-step and resumes execution at full speed with 0 UI freeze and 0 interruptions, ideal for high-frequency unpackers, key loggers, and fuzzing payloads. If `true` or no value is returned, execution halts and updates the GUI.
- **Breakpoint Manager UI Integration (`BreakpointManagerView`)**: Added 8th table column "Script Action" displaying language and line counts with tooltips; added top bar and context menu action "Edit Script Action..." presenting an interactive modal editor with language selection and syntax help.
- **Database Persistence (`.edb_db`)**: Breakpoint script payloads and language selections are serialized and deserialized automatically by `DatabaseManager`.

---

## 4. Unimplemented Features & Technical Roadmap

1. **Multi-Architecture Support**:
   - Abstract `IRegisterContext` and engine factories to support 32-bit x86 (`compat_ptrace`) and AArch64 / ARM64 (`NT_PRSTATUS` / `PTRACE_GETREGSET`).
2. **Anti-Anti-Debugging & Stealth**:
   - Cloak `TracerPid` in `/proc/<pid>/status`, smooth `rdtsc` execution differences, and introduce page-guard memory breakpoints to bypass integrity checks.
3. **Compound Data Type Reconstruction & Struct Layout Visualization**:
   - Import C headers or user-defined struct specifications; overlay fields and alignments onto the memory hex dump.
4. **Multi-Process Follow-Fork**:
   - Intercept `PTRACE_EVENT_FORK` / `VFORK` / `CLONE` and manage hierarchical child sessions via multi-tab session views.
5. **Hardware Watchpoint DR6 Status Attribution & Page-Guard Watchpoints**:
   - Parse debug status register DR6 (`B0`~`B3`) to display precise status bar alerts ("Hardware watchpoint triggered: Address 0x... written"); gracefully fallback to page-guard exceptions when hardware debug registers are exhausted.
6. **GDB Remote Serial Protocol (RSP) Support**:
   - Introduce an `RspDebugEngine` client to connect to remote `gdbserver` or QEMU instances for embedded firmware and Android debugging.
7. **Automated Shared Library Loading Interception (`_r_debug` Rendezvous)**:
   - Hook glibc's `struct r_debug.r_brk` (`_dl_debug_state`) to capture runtime `dlopen()` and `dlclose()` events, automatically re-enumerating memory regions and reloading symbols/DWARF.
9. **Independent Thread Freeze & Thaw Execution Control**:
   - Enable thread-isolated stepping by freezing non-target threads (`SIGSTOP` / event-loop masking) to prevent state corruption in complex multithreaded race conditions.
10. **Differential Memory Pattern & Value Scanner**:
    - Implement a CheatEngine-style multi-pass differential scanner over readable/writable heap/data pages (initial search, increased, decreased, changed, unchanged value convergence).

---

### 4.11 Priority & Importance Evaluation Matrix

To guide engineering milestones effectively, each unimplemented roadmap capability is quantitatively prioritized:

| Roadmap Capability | Impact | Complexity | Priority | Recommended Target Milestone |
| :--- | :---: | :---: | :---: | :--- |
| **C++ Symbol Demangling** | ★★★★★ | Low | **Completed (v1.0)** | **Fully implemented in §3.13**. `<cxxabi.h>` demangling active across symbols, stack, registers, and disassembler. |
| **Fine-Grained Hardware Watchpoint UI** | ★★★★☆ | Low | **Completed (v1.0)** | **Fully implemented in §3.14**. 1/2/4/8-byte read/write watchpoint assignment and cell highlights in Hex Dumps. |
| **Script-Driven Breakpoint Actions** | ★★★★☆ | Medium | **Completed (v1.0)** | **Fully implemented in §3.15**. Python 3 & Lua 5.4 dynamic hooks with `return false` silent bypass. |
| **4.2 Advanced Anti-Anti-Debugging (Page-Guard)** | ★★★★★ | Medium | **P1 (Core Moat)** | **Highest Current Priority**. Closes Linux stealth gap, neutralizing CRC checks and `/proc/self/status` `TracerPid`. |
| **4.7 Automated Shared Library Rendezvous (`_r_debug`)** | ★★★★☆ | Medium | **P1 (Core Moat)** | **Highest Current Priority**. Solves runtime `dlopen()` symbol omission; aligns with GDB core debug capabilities. |
| **4.4 Multi-Process Follow-Fork** | ★★★★☆ | Medium | **P2 (Advanced)** | Essential for Linux daemons and multiprocess CTF challenges via `PTRACE_O_TRACEFORK`. |
| **4.9 Independent Thread Freeze & Thaw** | ★★★☆☆ | Medium | **P2 (Advanced)** | Eliminates race condition interference during multithreaded step-through analysis. |
| **4.10 Differential Memory Pattern Scanner** | ★★★☆☆ | High | **P2 (Advanced)** | Multi-pass memory convergence tool for key discovery, dynamic offset search, and game analysis. |
| **4.3 Type Reconstruction & Struct Layout (Type Viewer)** | ★★★☆☆ | Medium | **P2 (Advanced)** | Format memory views using custom C struct definitions. |
| **4.1 Multi-Architecture Support (ARM64 / x86-32)** | ★★★★☆ | Very High | **P3 (Long-Term)** | Broad architectural refactor across register models and ptrace adapters; tackle after x86_64 stabilizes. |
| **4.6 GDB Remote Serial Protocol (RSP) Client** | ★★★☆☆ | High | **P3 (Long-Term)** | Extends edb-next UI as a universal frontend for QEMU, Android, and embedded targets. |

---

## 5. Codebase Structure & Module Architecture

### 5.1 Complete Source Tree & Responsibilities

```text
edb-next/
├── CMakeLists.txt              # Top-level CMake configuration (C++20, Qt5, Capstone)
├── main.cpp                    # Application entry point, dark QSS injection, MainWindow boot
├── core/                       # Headless Core Logic & Debugging Subsystems
│   ├── Types.hpp               # Core domain types: Address, Pid, Tid, DebugEvent, MemoryRegion
│   ├── UniqueFd.hpp            # RAII file descriptor wrapper
│   ├── RegisterContext.hpp     # 64-bit general purpose register access & EFLAGS bitmask operators
│   ├── LinuxDebugEngine.hpp/cpp# ptrace syscall layer, DR0-7, /proc/mem I/O, remote syscall injection
│   ├── BreakpointManager.hpp/cpp# Breakpoint registry, conditions, ignore counts, step-over state machine
│   ├── EventLoopThread.hpp/cpp # Background QThread event loop (waitpid + WNOHANG + atomic suspension)
│   ├── ElfParser.hpp/cpp       # ELF64 headers, segments, sections, symbol resolution, dynamic tags
│   ├── DwarfParser.hpp/cpp     # libdw-based DWARF debug info and line mapping parser
│   ├── SourceFileManager.hpp/cpp# Physical source file reader and line caching manager
│   ├── CallStackUnwinder.hpp/cpp# RBP-based safe call stack frame unwinding
│   ├── StringScanner.hpp/cpp   # Readable memory ASCII string extraction & code cross-referencing
│   ├── AnnotationManager.hpp/cpp# User comments & bookmark management
│   ├── FunctionFinder.hpp/cpp  # Heuristic prologue/epilogue function boundary detection
│   ├── HeapAnalyzer.hpp/cpp    # Glibc ptmalloc malloc_chunk parser
│   ├── Assembler.hpp/cpp       # GNU as + objcopy inline assembler
│   ├── ExpressionEvaluator.hpp/cpp# Recursive descent expression parser with memory dereferences
│   ├── ROPScanner.hpp/cpp      # Reverse sliding-window ROP gadget scanner & payload exporter
│   ├── InstructionInspector.hpp/cpp# Effective address calculation & dynamic EFLAGS branch predictor
│   ├── CodeXRefFinder.hpp/cpp  # Code cross-reference scanner (CALL/JMP/LEA targets)
│   ├── PatternSearcher.hpp/cpp # Byte pattern searcher with '??' wildcard masks
│   ├── ConfigurationManager.hpp/cpp# Settings persistence (~/.config/edb-next/config.ini via QSettings)
│   ├── IPlugin.hpp             # Modern C++20 pure virtual plugin interface contract
│   ├── IPluginContext.hpp      # Plugin host capability gateway
│   ├── PluginManager.hpp/cpp   # Dynamic plugin scanner, loader, and lifecycle manager
│   ├── PatchManager.hpp/cpp    # Memory patch manager & patchFileToDisk physical ELF exporter
│   ├── TraceEngine.hpp/cpp     # Hit Trace coverage & Run Trace frame difference/time-travel engine
│   ├── LogManager.hpp/cpp      # High-throughput thread-safe logging and event bus
│   ├── DatabaseManager.hpp/cpp # .edb_db JSON project database persistence
│   ├── IntermodularCallsFinder.hpp/cpp# PLT/GOT external dynamic library call scanner
│   ├── OpcodeSearcher.hpp/cpp  # Capstone instruction pattern search engine
│   ├── StateDumper.hpp/cpp     # Formatted CPU machine state snapshot generator
│   ├── IScriptEngine.hpp       # Embedded scripting engine interface contract (Python/Lua)
│   ├── PythonScriptEngine.hpp/cpp# Embedded Python 3 interpreter and edb module exporter
│   ├── LuaScriptEngine.hpp/cpp # Embedded Lua 5.4 interpreter and global edb table binding
│   ├── ScriptEngineManager.hpp/cpp# Multi-engine lifecycle and language routing manager
│   ├── DebugSession.hpp/cpp    # Facade aggregating engine, breakpoints, symbols, and thread control
│   └── SessionManager.hpp/cpp  # Multi-session container and active session dispatcher
├── ui/                         # Qt5 Presentation Layer
│   ├── DisassemblyView.hpp/cpp # Core disassembly view with branch arrows and syntax highlighting
│   ├── SourceView.hpp/cpp      # Standalone source code viewer (file switcher, breakpoints, step)
│   ├── ScriptConsoleView.hpp/cpp# Interactive script terminal (Python 3/Lua 5.4 dual-mode)
│   ├── RegisterView.hpp/cpp    # Register view with smart dereferences and interactive EFLAGS badges
│   ├── MemoryHexView.hpp/cpp   # Virtualized memory hex editor with in-place patching
│   ├── MultiDumpWidget.hpp/cpp # Multi-tab memory dump container (Dump 1 ~ Dump 4)
│   ├── StackView.hpp/cpp       # Dedicated 64-bit QWORD stack view with RSP relative offsets
│   ├── MemoryRegionsView.hpp/cpp# Memory page table with mprotect/mmap/munmap integration
│   ├── BreakpointManagerView.hpp/cpp# Breakpoint management table (conditions, ignore counts, log-only)
│   ├── CallStackView.hpp/cpp   # Call stack frame backtrace view
│   ├── StringReferencesView.hpp/cpp# String extraction and cross-reference browser
│   ├── SymbolViewer.hpp/cpp    # Global symbol browser with search
│   ├── ProcessPropertiesView.hpp/cpp# Process command line, environment, and file descriptors
│   ├── HeapView.hpp/cpp        # Heap chunk table and visual status inspector
│   ├── ThreadsView.hpp/cpp     # Thread table with instant active TID switching
│   ├── ROPToolView.hpp/cpp     # ROP gadget browser and Python payload generator
│   ├── WatchView.hpp/cpp       # Dynamic expression watch window
│   ├── TraceView.hpp/cpp       # Trace configuration, execution step history, and time-travel bar
│   ├── CFGGraphView.hpp/cpp    # Interactive basic-block control flow graph view
│   ├── NotesView.hpp/cpp       # Monospace scratchpad notes editor
│   ├── LogView.hpp/cpp         # Real-time filterable event console
│   ├── BinaryInfoView.hpp/cpp  # Comprehensive ELF structural explorer
│   ├── IntermodularCallsView.hpp/cpp# External API call searcher
│   ├── OpcodeSearcherView.hpp/cpp# Instruction sequence search panel (Tab 19)
│   ├── CommandBarView.hpp/cpp  # Bottom interactive CLI console
│   ├── PreferencesDialog.hpp/cpp# Comprehensive 7-category preferences dialog
│   ├── LaunchArgumentsDialog.hpp/cpp# Target argv and cwd setup dialog
│   ├── PluginManagerDialog.hpp/cpp# Plugin manager and hot-loader dialog
│   ├── PatchManagerDialog.hpp/cpp# Centralized patch manager & disk export dialog (Ctrl+P)
│   ├── XRefDialog.hpp/cpp      # Interactive cross-reference navigation popup
│   ├── SessionTabWidget.hpp/cpp# 4-quadrant workspace container & 18-tab drawer
│   └── MainWindow.hpp/cpp      # Main application window, menus, toolbars, global hotkeys
├── plugins/
│   └── SamplePlugin/           # Reference C++20 plugin implementation (sample_plugin.so)
│       ├── SamplePlugin.hpp/cpp
│       └── CMakeLists.txt
└── tests/                      # Automated Regression & Unit Test Suites
    ├── test_target.c           # Multithreaded test binary source
    ├── test_core.cpp           # Regression test suite for core phases 1 to 5
    ├── test_dwarf.cpp          # DWARF source debugging and line mapping test suite
    ├── test_advanced.cpp       # Regression test suite for advanced phases 6 to 8
    ├── test_scripting.cpp      # Python 3 & Lua 5.4 embedded scripting test suite
    └── test_exit.cpp           # Window destruction and process teardown stress test
```

---

## 6. Detailed Technical Schemes & Implementations

### 6.1 Asynchronous Non-Blocking Concurrency Model
To eliminate UI lockups during blocking system calls:
- `EventLoopThread` runs a dedicated loop executing `waitpid(-1, &status, __WALL | WNOHANG)`:
  ```cpp
  while (running_.load()) {
      if (suspended_.load()) {
          msleep(5);
          continue;
      }
      int status = 0;
      pid_t p = waitpid(-1, &status, __WALL | WNOHANG);
      if (p > 0) {
          DebugEvent ev = processWaitStatus(status, p);
          Q_EMIT eventReceived(ev); // Dispatched safely via Qt QueuedConnection
      } else {
          msleep(5);
      }
  }
  ```
- When the foreground UI thread issues a synchronous operation (such as injecting a remote syscall), it temporarily asserts `setSuspended(true)` to avoid race conditions.

### 6.2 Breakpoint Lifecycle & Step-Over State Machine
1. **Arming**: Reads original opcode byte and replaces it with `0xCC` via `PTRACE_POKETEXT`.
2. **Hit**: Kernel stops process on `0xCC`, triggers `SIGTRAP`. IP is advanced to `rip+1`. `EventLoopThread` catches the trap, detects the breakpoint, and rewinds RIP by 1 byte.
3. **Condition & Ignore Check**: `ExpressionEvaluator` checks user-defined conditions. If false, or if the ignore counter has not elapsed, execution resumes silently.
4. **Step-Over**:
   - `prepareStepOver()` restores original instruction byte;
   - Executes `PTRACE_SINGLESTEP`;
   - Upon capturing single-step completion, `finishStepOver()` re-writes `0xCC`;
   - Resumes normal execution via `PTRACE_CONT`.

### 6.3 Remote Syscall Atomic Injection (`executeRemoteSyscall`)
Enables in-target execution of system calls:
1. Preserves target general purpose registers and original 2 bytes at RIP.
2. Writes `0x0F 0x05` (`syscall`) at RIP.
3. Loads parameters according to System V AMD64 ABI: `RAX`=syscall number, `RDI`=arg1, `RSI`=arg2, `RDX`=arg3, `R10`=arg4, `R8`=arg5, `R9`=arg6.
4. Performs `PTRACE_SINGLESTEP` and waits synchronously for completion.
5. Reads result from `RAX`.
6. Restores original 2 instruction bytes and original register context.

### 6.4 Recursive Descent Expression Evaluator (`ExpressionEvaluator`)
Implements an AST-less recursive descent parser supporting:
- Registers (`rax`, `rip`, `rdi`), immediate hex values (`0x401000`), decimal integers (`100`);
- Pointer dereferences (`[expr]` or `[rbp - 8]`);
- Arithmetic operations (`+`, `-`, `*`, `/`);
- Comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`).

### 6.5 ELF Physical Disk Patching Algorithm (`patchFileToDisk`)
1. Reads input binary's ELF64 headers and segments (`Program Headers`).
2. Iterates over active patches in `PatchManager`. For each target virtual address $VAddr$:
   - Finds matching `PT_LOAD` segment where $Segment.p\_vaddr \le VAddr < Segment.p\_vaddr + Segment.p\_memsz$.
   - Calculates disk offset: $FileOffset = VAddr - Segment.p\_vaddr + Segment.p\_offset$.
3. Seeks to $FileOffset$ in target file, writes replacement bytes.
4. Applies executable mode (`0755`) via `chmod()`.

---

## 7. Build, Installation & Quality Assurance

### 7.1 Compilation
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### 7.2 Verification Suite
```bash
./build/test_core       # Basic engine & breakpoint verification
./build/test_advanced   # Advanced reverse engineering suite
./build/test_exit       # Clean exit and process teardown test
```

---

## 8. GitHub Release & Open Source Specifications
- **Licensing**: Licensed under the GNU General Public License v3.0 (GPL-3.0) to align with the wider open-source debugging ecosystem.
- **Continuous Integration**: GitHub Actions configuration (`.github/workflows/ci.yml`) validating compilation and all automated test suites on Ubuntu 22.04 & 24.04.
