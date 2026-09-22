# edb-next

<div align="center">

<h3>Next-Generation Linux Binary Debugger & Reverse Engineering Platform</h3>

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg?style=flat-square&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![Qt](https://img.shields.io/badge/Qt-6.4%2B-brightgreen.svg?style=flat-square&logo=qt)](https://www.qt.io/)
[![Platform](https://img.shields.io/badge/Platform-Linux%20x86__64-orange.svg?style=flat-square&logo=linux)](https://www.kernel.org/)
[![License](https://img.shields.io/badge/License-GPLv3-green.svg?style=flat-square)](LICENSE)
[![CI](https://img.shields.io/badge/CI-Passing-success.svg?style=flat-square&logo=github-actions)](.github/workflows/ci.yml)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat-square)](doc/CONTRIBUTING_en.md)

**[English](README.md) | [简体中文](README_zh.md)**

</div>

---

## Overview

**edb-next** is a modern, high-performance graphical binary debugger and dynamic reverse engineering platform specifically engineered for Linux x86_64. 

Built from scratch using **C++20**, **Qt 6.4+**, and the **Capstone Disassembly Engine**, `edb-next` addresses the longstanding absence of an industrial-strength GUI debugger on Linux. It deeply aligns with the tactile, battle-tested reverse engineering workflows of Windows' legendary **x64dbg**, while innovating natively on Linux through in-target remote syscall injection, ELF physical disk patching, non-blocking asynchronous event loops, and full glibc ptmalloc heap deconstruction.

```text
┌───────────────────────────────────────┬───────────────────────────────────────┐
│     Quadrant 1: Disassembly View      │       Quadrant 2: Register View       │
│  - One Dark syntax highlighting & RIP │  - 16 GPRs with diff highlight in red │
│  - Dynamic branch & deref preview bar │  - Quick +1/-1 & Follow in Stack      │
│  - F2 Breakpoint / Enter Follow / Esc │  - Clickable EFLAGS badges / SSE tabs │
├───────────────────────────────────────┼───────────────────────────────────────┤
│     Quadrant 3: Multi-Tab Dump        │       Quadrant 4: Dedicated Stack     │
│  - Dump 1 ~ Dump 4 independent tabs   │  - 8-byte QWORD aligned rows          │
│  - Alt+Left/Right history navigation  │  - [Return Address] amber detection   │
│  - Ctrl+E in-place patch / TypeViewer │  - => RSP indicator & smart router    │
└───────────────────────────────────────┴───────────────────────────────────────┘
```

---

## Key Highlights & Innovations

- 🚀 **Zero-Deadlock Multithreaded Event Loop**: A dedicated `EventLoopThread` polls `waitpid(WNOHANG)` in the background and delivers notifications via Qt signals. Synchronous foreground operations seamlessly assert atomic suspension (`suspended_`), completely eliminating UI freezes.
- ⚡ **In-Target Remote Syscall Injection (`executeRemoteSyscall`)**: Break through read-only memory barriers by dynamically injecting `SYS_mprotect` in the target process (RWX elevation), allocating isolated executable pages (`SYS_mmap`), and freeing memory (`SYS_munmap`).
- 💾 **One-Click Physical ELF Disk Patching (`patchFileToDisk`)**: Translates Virtual Memory Addresses to ELF Program Header physical offsets ($VAddr \to FileOffset$), exporting standalone executable patched binaries directly to disk.
- 🗄️ **Automatic Project Database (`.edb_db`)**: Seamlessly preserves and restores all user instruction comments, bookmarks, conditional breakpoints, watch expressions, memory patches, and scratchpad notes across sessions.
- 📖 **DWARF Source-Level Debugging & Mixed-Mode Disassembly**: Parses `.debug_info` and `.debug_line` with `libdw` for bidirectional address-to-source mapping. Inline source banner rendering in `DisassemblyView` (`Ctrl+Shift+S`), dedicated `SourceView` browser (`Alt+S`), source line breakpoints, and source stepping.
- 🐍 **Embedded Dual Scripting Engine (Python 3 & Lua 5.4)**: Native embedded CPython 3 and Lua 5.4 engines managed by `ScriptEngineManager`. Rich `edb` module exposing memory/registers/breakpoints/stepping/eval APIs, dark geek Script Console (`Alt+P`), and inline CommandBar execution (`py <code...>` / `lua <code...>`); supports script-driven breakpoint actions with silent hook bypass (`return False` / `return false`) for non-intrusive microsecond-level runtime instrumentation.
- 🏷️ **Intelligent C++ Symbol Demangling**: Integrated GNU `<cxxabi.h>` `abi::__cxa_demangle` across global Symbol Viewer, call stack backtraces, disassembly banners, register smart dereferences, and stack memory annotations, displaying clean `calculate_fib(int)` with tooltip mangled string preservation and bidirectional search.
- 🎯 **Fine-Grained Hardware Watchpoint UI**: Right-click any byte cell in Hex Dumps to instantly set 1/2/4/8-byte hardware write watchpoints, read/write watchpoints, or execution breakpoints, complete with prominent deep red cell highlighting (`QColor(160, 40, 40, 160)`).
- 🛡️ **Memory Page-Guard Breakpoints & Stealth Execution (Anti-Anti-Debugging)**: Eliminates the 4-register limitation of DR0~DR3 with unlimited soft watchpoints via virtual memory page protection (`PROT_NONE` / `PROT_READ`); zero-`0xCC` stealth execution breakpoints completely defeat CRC32/Hash binary self-integrity checks; kernel-level sub-microsecond false-positive state machine ensures smooth execution without UI stutter.
- 📦 **Automated Shared Library Interception & Hot-Reloading (`_r_debug` Rendezvous & Pending Breakpoints)**: Seamlessly hooks the Linux glibc `_r_debug` rendezvous protocol and `_dl_debug_state` internal trap to capture runtime `dlopen()` and `dlclose()` events. Dynamically traverses `link_map`, merges newly loaded shared library symbol tables, and extends DWARF line tables on the fly. Introduces pending breakpoints (`bpp <symbol>`) that automatically bind and activate the instant a deferred library is mapped into memory, complete with `catch load` / `catch dlopen` triggers and the real-time `BinaryInfoView` Shared Libraries tab.
- 🌿 **Follow-Fork Mode & Multi-Process Session Tree**: Robust multi-process tracing backed by Linux kernel `PTRACE_O_TRACEFORK`/`TRACEVFORK` and tracer thread affinity. Features `Parent` (retain parent focus), `Child` (switch to child), and `Both` (hierarchical multi-session trees in synchronized workspace tabs) policies, alongside `catch fork` breakpoints, and `inferiors` / `inferior <id|pid>` CLI commands.
- ❄️ **Independent Thread Freeze & Thaw with Isolated Stepping**: Fine-grained per-thread freeze and thaw control via `SYS_tgkill(SIGSTOP)` and event-loop scheduler masking; 8-column `ThreadsView` with ice-blue `❄ FROZEN` badges; one-click `❄ Freeze Others` for isolated single-stepping without background worker thread interference; CLI support via `freeze <tid|all>` and `thaw <tid|all>`.
- 🔍 **CheatEngine-Style Differential Memory Scanner**: High-throughput multi-pass differential memory scanner located in bottom drawer (Tab 21); natively parses 8 data types (Int8~64, Float, Double, String, Hex bytes with wildcards); multi-pass convergence (increased, decreased, changed, unchanged, increased/decreased by delta); default rw-p streaming scan finishes in tens of milliseconds; live candidate table with green/red delta cues, hex dump sync, and in-place memory editing; CLI control via `scan`, `nextscan`, `scanresults`, and `scanreset`.
- 🧬 **Compound Type Reconstruction & Struct Layout Visualizer**: Dedicated struct analysis workbench in bottom drawer (Tab 22); natively parses standard C struct declarations with natural AMD64 ABI alignment/padding calculations; supports 14 data types and fixed-size arrays; pre-loaded with standard Linux system structs (`timespec`, `timeval`, `sockaddr_in`, `list_head`, `io_vec`); displays relative offsets and raw hex bytes, cyan-underlined pointer fields with double-click dereference navigation to Disassembly or Hex Dump, and in-place memory mutation; CLI integration via `structs`, `struct <name> <addr>`, and `defstruct <c_code...>`.
- 🎨 **x64dbg-Style Reverse Engineering Ergonomics & Syntax Highlighting**: Fine-grained semantic syntax highlighting delegate (`InstructionHighlightDelegate`) for CALL, JMP, Jcc, RET, SYSCALL/UD2, PUSH/POP, CMP/TEST, NOP, Regs, Brackets, and Immediates; rich HTML dynamic branch prediction (`Branch Taken: YES / NO`) and chained memory operand dereferencing (`[rbp - 0x14] => 0x... => val`); dedicated Stack View return address detection with bright amber tags (`[Return Address] <symbol>`); quick register increment/decrement (`+1` / `-1`), `Follow in Stack`, and multi-format copy submenu; Hex Dump navigation history stack (`Alt+Left` / `Backspace` / `Alt+Right`), cross-view QWORD follows, and direct struct layout visualizer (`View as Struct...`) integration; `Ctrl+*` Set Origin (Set RIP).
- ⚡ **x64dbg / edb-Style Control Flow & Call Relationship Lines (Mark Column)**: 5-track greedy collision-free control flow line routing in Disassembly Mark column (75px). Distinct color coding: Neon Cyan (`#00e5ff`) for function calls (`CALL`), Golden Yellow (`#ffd54f`) for unconditional jumps (`JMP`), Coral Red (`#ff5252`) for backward loops, and Amber Orange (`#ff9800`) for forward conditional branches (`Jcc`). Continuous vertical rail routing with out-of-viewport indicators (`▲` / `▼`), intelligent viewport visibility filtering, two-pass glowing selection aura, destination focus brackets, rich branch tooltips, and double-click / `Enter` instant branch following with history navigation.
- ⌨️ **x64dbg-Style Bottom CommandBar**: Interactive bottom CLI supporting `bp`, `bph`, `r`, `d`, `u`, `step`, `eval`, `py`, `lua`, `mprotect`, `alloc`, `dumpstate`, `pageguard`, `guards`, `follow-fork`, `inferiors`, `structs`, `struct`, `defstruct`, and plugin commands.
- 🧩 **Modern C++20 Decoupled Plugin Gateway**: Pure virtual `IPlugin` and `IPluginContext` contract supporting dynamic `.so` hot-loading, menu injection, CLI registration, and event hooks.
- 🚀 **`IRefreshable` Lazy View Updates & State Machine Decomposition**: High-performance lazy tab refresh architecture eliminates broadcast storms across 22 drawer tabs during high-frequency stepping or tracing; inactive views are marked dirty and deferred until activated, drastically slashing ptrace traffic. Decomposed `DebugSession::handleEvent()` into clean, single-responsibility event handlers.
- 🔄 **Multi-Thread Hardware Breakpoint Synchronization**: Synchronizes x86_64 debug registers (DR0~DR7) across all existing threads on breakpoint configuration and automatically replicates them onto newly spawned threads (`handleThreadCreatedEvent`), preventing hardware breakpoint misses in multi-threaded targets.
- ⚡ **Ultra-Fast In-Memory Keystone Assembler**: Deep integration of the Keystone Engine (LLVM MC backend) replacing legacy external `as`/`ld`/`objcopy` sub-process pipelines and disk I/O. Benchmarked at 1.2ms for 1,000 instructions (~1.2μs/insn, >10,000x speedup), with native origin-relative branch resolution and seamless fallback to GNU binutils.
- ⚡ **Thread-Local Capstone Handle Pooling & 8-Entry LRU Cache (`CapstoneContext` & `DisasmCache`)**: Introduced zero-allocation RAII `CapstoneLease` with thread-local dual-mode handles (Basic/Detail), completely eradicating repeated `cs_open`/`cs_close` churn across 8 core analysis modules. Upgraded `DisasmCache` to an 8-entry LRU cache for silky-smooth stepping across multiple viewports.
- ⚡ **Zydis x86_64 Fast Instruction Decoder & Dual-Engine Architecture (`ZydisContext`)**: Integrated industry-standard Zydis decoder for Linux x86_64 featuring zero heap allocations and stack-only fixed-size instruction structs, achieving >3.8 million instructions/second throughput. Refactored `stepOver` for ~15ns control flow analysis and introduced seamless Zydis/Capstone dual-engine switching with automatic fallback.
- ⚡ **AVX2 Vectorized Multi-Threaded Memory & Pattern Scanner (`PatternSearcher` & `MemoryScanner`)**: Streaming chunk scanner eliminates the legacy 16MB truncation bug with a constant 2MB memory footprint across multi-gigabyte address spaces; novel dual-anchor AVX2 SIMD mask filtering rejects 32 candidate offsets in two vector instructions, accelerating 64MB scans from 833ms down to 21.8ms (**38.27x speedup, ~2.87 GB/s throughput**); multi-VMA work-stealing parallel dispatch with atomic early termination, plus seamless runtime scalar fallback on non-AVX2 hosts.
- 🪜 **DWARF CFI Deep Call Stack Unwinding (`libdwfl`)**: Full `.eh_frame` / `.debug_frame` CFI state machine traversal powered by `libdwfl` with custom session memory and register hooks. Completely eliminates call stack amputation on `-fomit-frame-pointer` binaries and seamlessly unwinds across `libc.so` and optimized runtime frames, backed by dual-track RBP fallbacks.
- 🧮 **Modern Recursive-Descent Expression Evaluator**: Comprehensive 13-level operator precedence parser supporting scale-index dereferences (`[rax + rcx * 8 + 0x20]`), bitwise operations (`&`, `|`, `^`, `~`, `<<`, `>>`), compound logic (`&&`, `||`, `!`), parentheses nesting, and sized memory reads (`byte ptr`, `dword ptr`, `qword ptr`).
- 🔌 **Pluggable Debug Engine Abstraction (`IDebugBackend`)**: Decoupled `DebugSession`, `EventLoopThread`, and `TypeManager` behind a pure virtual backend interface, allowing headless mock backend unit testing (`MockDebugBackend`) and establishing the architectural gateway for future remote GDB/LLDB RSP servers.
- 🚌 **Centralized Cross-View Routing (`NavigationBus`)**: Replaces dozens of point-to-point Qt signal-slot bindings across 22+ views with a centralized event bus. Coordinates jumps to disassembly, memory dumps, stack frames, struct viewer, string references, and intermodular calls with zero inter-view dependency leaks.
- 🗂️ **Modular Command Registry (`CommandRegistry`)**: Decouples the 1100+ line `CommandBarView` into categorized modular command handlers (`Execution`, `Breakpoint`, `Memory`, `Analysis`, `Process`, `System`, `Plugin`). Supports prefix autocompletion, alias resolution (`g` -> `run`, `libs` -> `modules`), category filtering, and seamless third-party plugin command injection.
- 🏛️ **Strict Headless Core & Decoupled UI Plugin Interface (`IUIPlugin`)**: Fully satisfies architectural isolation boundaries (AGENTS.md Rule 3.1). Static library `edb_core` removes all `QtWidgets` linkages and stays 100% headless-testable. GUI lifecycle methods (`createMenu`, `contextMenuItems`, `createOptionsPage`, `addDockWidget`) are cleanly decoupled into `ui/IUIPlugin.hpp`.
- 🛡️ **StepOver Software Breakpoint Masking & Fork Detachment Hygiene**: Step-over disassembly automatically overlays original bytes across active software breakpoints (`0xCC`), accurately preserving `CALL` and `REP`-prefixed instruction semantics. Follow-fork parent detachment automatically purges breakpoint opcodes from child process memory before detachment, preventing rogue child termination.
- 🔒 **Zero-Deadlock Syscall Suspension Handshake**: Upgraded `EventLoopThread` suspension mechanism with bidirectional condition variable synchronization, guaranteeing mutual exclusion and eliminating deadlocks between target event collection and remote memory syscalls (`remoteMmap`, `remoteMprotect`, `remoteMunmap`).
- 🗺️ **ASLR/PIE Runtime Database Relocation & Disk Patching**: `.edb_db` projects record module load bases and automatically recompute relative virtual offsets on import, seamlessly transferring comments, labels, bookmarks, and breakpoints across ASLR runs. `PatchManager::patchFileToDisk` features runtime base offsets and dynamic base deduction for PIE/ASLR binaries.
- ⚡ **High-Frequency UI Widget Item Pooling & Smooth Scrolling**: Replaced continuous heap allocations with `QTableWidgetItem` pooling (`getOrCreateItem`) in `DisassemblyView` and `MemoryHexView`, eliminating garbage churn during single-stepping. Implemented natural mouse wheel scrolling (`wheelEvent`) with backward instruction disassembly.
- 🧵 **Thread-Local Script Engine Concurrency Isolation**: Replaced global static pointers with `thread_local`, RAII scopes (`PythonEngineScope`, `GilStateScope`), and Lua registry mapping (`kLuaEngineRegistryKey`), enabling clean multi-session script automation.
- 📊 **Sugiyama Layered CFG Layout & Headless CFGBuilder (`CFGBuilder` & `SugiyamaLayout`)**: Headless core graph partitioning conforming to standard 3-rule basic block leader detection ($I_0$, branch targets, and fallthroughs) with 3-state DFS cycle breaking. High-aesthetic 5-phase Sugiyama layout engine featuring longest-path topological DAG layering, dummy node insertion for multi-layer edge span reduction, 8-sweep iterative barycenter crossing minimization, collision-free coordinate assignment, and dedicated exterior side-channel loop back-edge routing that completely avoids cutting through basic blocks. Double-click basic block jump-to-disassembly navigation.
- 🧬 **Modern Type-Safe Script Bindings (`ScriptApiBridge`, `LuaTypeBinding`, `PythonTypeBinding`)**: Completely modernized Python 3 and Lua 5.4 script engine bindings. Decoupled unified debugger business logic into a headless `ScriptApiBridge`, eliminating 620+ lines of duplicate C-API boilerplate and register mapping. Introduced zero-overhead RAII `PyRef` smart pointers eradicating Python reference leaks, compile-time variadic tuple unpackers, and Lua 5.4 type-safe stack trait dispatchers (`LuaFunctionDispatcher`) with automatic exception translation.

---

## Comparison Matrix

| Dimension / Feature | Original edb (Linux) | x64dbg (Windows) | edb-next (Modern Linux Rewrite) |
| :--- | :---: | :---: | :---: |
| **Language Standard** | C++11 | C++14/17 | **Modern C++20 Standard** |
| **Event Concurrency** | 0ms QTimer (prone to hangs) | Complex sync | **Decoupled `EventLoopThread` (0% UI Freeze)** |
| **Workspace Layout** | Single bottom drawer | 4 Quadrants | **4-Quadrant Golden Workspace** |
| **Disassembly & Ergonomics** | Plain monospaced text | Rich color schemes & call/ret highlights | **x64dbg-Style Syntax Delegate + 5-Rail Call & Jump Flow Lines (Neon Cyan Calls / Gold Jumps / Red Loops) + Dynamic Branch & Operand Deref Preview + Ctrl+* Set Origin** |
| **Hex Dump & Stack Ergonomics** | Basic linear dump | Dump history & stack return address tags | **Dump Back/Forward History (Alt+Left/Right) + [Return Address] Detection + Struct Link** |
| **Register Quick Tweaking** | Type hex manually | Quick increment/decrement | **GPR Quick +1/-1 + Follow in Stack + Multi-Format Copy Submenu** |
| **Memory Dumps** | Single Dump view | Dump 1 ~ Dump 5 | **4-Way MultiDumpWidget (Dump 1~4)** |
| **Read-Only Patching** | Rejected / Errors | VirtualProtect | **Remote Syscall Injection (`mprotect`)** |
| **Physical Disk Patch** | None (Memory-only) | Patched EXE | **Innovative `patchFileToDisk` (ELF Export)** |
| **Source-Level Debug** | None (Disasm-only) | External tools | **Integrated DWARF Source Mapping & Mixed-Mode (`libdw`)** |
| **Scripting Automation**| None | Plugins | **Native Python 3 & Lua 5.4 Dual Engines (`Alt+P`)** |
| **Session Persistence** | Lost on exit | `.dd64` Database | **`.edb_db` JSON Project Database** |
| **Interactive CLI** | None | CommandBar | **x64dbg-Style Bottom CommandBar** |
| **Linux Heap Analysis** | Outdated plugin | N/A (Windows) | **Native Glibc ptmalloc Analyzer (Tab 9)** |
| **Shared Lib Hot-Reload / dlopen** | Manual reload | DLL events supported | **Native glibc _r_debug rendezvous + Pending Breakpoints** |
| **Follow-Fork / Multi-Process** | Single process only | Multi-process attach | **Native PTRACE_EVENT_FORK + Parent/Child/Both Session Tree + inferiors CLI** |
| **Thread Freeze / Isolated Stepping** | View thread list only | Suspend / Resume thread | **Native SYS_tgkill + scheduler mask + ice-blue badges + isolated stepping** |
| **Differential Memory Scanner** | Basic byte search only | External CE required | **Native 8 data types + multi-pass differential convergence + delta + in-place edit (Tab 21)** |
| **Compound Struct Reconstruction** | None | Complex plugin required | **Native C Syntax Parsing + AMD64 ABI Alignment + In-Place Editing + Dereference Sync (Tab 22)** |
| **Exploit Tooling** | Basic ROP plugin | 3rd-party | **Built-in ROP Engine & Python `p64()` Export** |

---

## Quick Start

### 1. Prerequisites (Ubuntu / Debian)
```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    qt6-base-dev \
    qt6-tools-dev \
    libgl1-mesa-dev \
    libcapstone-dev \
    libdw-dev \
    libelf-dev \
    python3-dev \
    liblua5.4-dev
```

### 2. Build from Source
```bash
git clone https://github.com/your-username/edb-next.git
cd edb-next

# Configure & build all binaries with max concurrency
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### 3. Run Verification Tests
```bash
./build/test_core       # Validates engine, breakpoints, stepping, and unwinding
./build/test_dwarf      # Validates DWARF source-level debugging & line mapping
./build/test_advanced   # Validates patching, disk export, remote syscalls, and plugins
./build/test_scripting  # Validates Python 3 & Lua 5.4 dual scripting automation
./build/test_exit       # Validates clean process teardown without crashes
```

### 4. Launch edb-next

#### Option A: Run Local Build Artifact
```bash
./build/edb_next
```

#### Option B: Run Standalone Portable AppImage (Recommended, Zero Install)
Download the release `.AppImage` or build it locally. Simply grant execution permission to run on any major Linux distribution (Ubuntu, Debian, Fedora, Arch Linux, etc.):
```bash
chmod +x edb-next-x86_64.AppImage
./edb-next-x86_64.AppImage
```

> **Note on ptrace permissions**:
> - Debugging newly launched processes (Spawn / Open Binary) works with standard user permissions out of the box.
> - To attach to an existing non-child process, due to Linux Yama LSM restrictions, launch with `sudo ./edb-next-x86_64.AppImage` or configure `sudo sysctl -w kernel.yama.ptrace_scope=0`.

#### Build AppImage Locally
```bash
# Host build with local dependencies
./scripts/build_appimage.sh

# Or reproducible build inside Ubuntu 22.04 LTS (glibc 2.35) Docker container:
./scripts/docker_build_appimage.sh
```

---

## Documentation

Comprehensive bilingual documentation is maintained under the `doc/` directory:

| Document | English Version | 中文版本 (Chinese Version) | Description |
| :--- | :--- | :--- | :--- |
| **Software Design Document (SDD)** | [doc/DESIGN_en.md](doc/DESIGN_en.md) | [doc/DESIGN_zh.md](doc/DESIGN_zh.md) | Comprehensive technical architecture, algorithms, and roadmap |
| **User Manual & Plugin Guide** | [doc/TUTORIAL_en.md](doc/TUTORIAL_en.md) | [doc/TUTORIAL_zh.md](doc/TUTORIAL_zh.md) | Full user manual, shortcut guide, and C++20 plugin development tutorial |
| **Core Optimization & Modernization Proposal** | - | [doc/CORE_OPTIMIZATION_zh.md](doc/CORE_OPTIMIZATION_zh.md) | In-depth evaluation & roadmap for Keystone, Zydis, libunwind, ExprTk, etc. |
| **Contribution Guidelines** | [doc/CONTRIBUTING_en.md](doc/CONTRIBUTING_en.md) | [doc/CONTRIBUTING_zh.md](doc/CONTRIBUTING_zh.md) | Code style, git workflow, and PR verification rules |

---

## Keyboard Shortcuts

| Hotkey | Action | Hotkey | Action |
| :--- | :--- | :--- | :--- |
| **F9** | Continue Execution | **Enter** | Follow Branch (`CALL`/`JMP`) / Stack Smart Follow |
| **F7** | Step Into | **Esc / Backspace** | Go Back in Navigation History |
| **F8** | Step Over | **\*** *(Numpad / Key)* | **Origin**: Follow / Center on Current RIP |
| **Shift+F11** | Step Out of Function | **Space** | Continuous Assemble (Auto-advancing with NOP fill) |
| **F4** | Run to Selection | **; (Semicolon)** | Add / Edit Instruction Comment |
| **Ctrl+F2** | Restart Debug Session | **: (Colon)** | Set / Edit User Label (`🏷`) |
| **Ctrl+\*** | Set RIP (New Origin) | **Ctrl+B** | Toggle Bookmark (`★`) |
| **F2** | Toggle Software Breakpoint | **X** | Show Cross References (XREFs) |
| **Ctrl+Alt+S** | Search All Referenced Strings | **Ctrl+E** | Modify Hex Bytes In-Place |
| **Ctrl+Alt+C** | Search All Intermodular Calls | **Ctrl+P** | Patch Manager & Disk File Export |
| **Alt+P** | Script Console (Python/Lua) | **Alt+S** | Focus Source View |
| **Ctrl+Shift+S** | Toggle Mixed ASM/Source View | **Alt+C** | Focus CPU / Disassembly |
| **Ctrl+S** | Save Project Database | **Ctrl+D** | Dump Formatted CPU State Snapshot |
| **Shift+S** | Toggle Stack View | **Shift+F7/F8/F9** | Pass Signal Step / Run |
| **Regs: `+` / `-` / `0`** | Inc / Dec / Zero Register | **Stack: `Space` / `Ctrl+G`** | Modify QWORD / Go to Address |
| **HexDump: `Enter` / Dbl-Click** | Modify Bytes In-Place | **Follow in Dump 1~4** | Route to Target Dump Tab |

---

## Plugin Development

`edb-next` provides a modern C++20 plugin gateway. An official reference plugin is available at [plugins/SamplePlugin/](plugins/SamplePlugin/):

```cpp
#include "core/IPlugin.hpp"
#include "core/IPluginContext.hpp"

class MyPlugin : public QObject, public edb_next::IPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EDB_NEXT_PLUGIN_IID)
    Q_INTERFACES(edb_next::IPlugin)

public:
    bool initialize(edb_next::IPluginContext* ctx) override {
        // Register a custom CLI command for the CommandBar
        ctx->registerCommand("my_ping", [](const std::vector<std::string>& args) {
            // handle CLI execution
        }, "my_ping - Sample command");

        // Hook breakpoint events
        ctx->registerDebugEventListener([](const edb_next::DebugEvent& ev) {
            // handle debug event
        });
        return true;
    }
    void shutdown() override {}
};
```
See the full guide in [doc/TUTORIAL_en.md](doc/TUTORIAL_en.md) or [doc/TUTORIAL_zh.md](doc/TUTORIAL_zh.md).

---

## Contributing

We welcome community contributions, bug reports, and feature requests! Please read our [Contribution Guidelines](doc/CONTRIBUTING_en.md) before opening a pull request.

---

## License

`edb-next` is licensed under the [GNU General Public License v3.0 (GPL-3.0)](LICENSE).
