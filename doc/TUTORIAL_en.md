# edb-next User Manual & C++20 Plugin Development Guide

> **Project**: edb-next (Next-Generation Linux Binary Debugger & Reverse Engineering Platform)  
> **Target Version**: v1.0.0+  
> **Platform**: Linux x86_64  
> **Language**: C++20 / Qt 5.15+ / Capstone Engine

---

## Table of Contents

1. [Environment Setup & Building](#1-environment-setup--building)
   - 1.1 Prerequisites & Dependencies
   - 1.2 Build Commands
   - 1.3 Running the Automated Regression Test Suites
2. [Quick Start & Session Management](#2-quick-start--session-management)
   - 2.1 Starting the Debugger
   - 2.2 Opening a Target (Open Target & ASLR Disabling)
   - 2.3 Launch Arguments & Working Directory
   - 2.4 Attaching to a Running Process
   - 2.5 Restarting, Detaching, and Terminating
3. [4-Quadrant Core Workspace Hands-on](#3-4-quadrant-core-workspace-hands-on)
   - 3.1 Quadrant 1: Disassembly View (DisassemblyView)
   - 3.2 Quadrant 2: Register View (RegisterView)
   - 3.3 Quadrant 3: Multi-Tab Memory Dump (MultiDumpWidget)
   - 3.4 Quadrant 4: Dedicated 64-Bit Stack View (StackView)
4. [Advanced Execution Control & Debugging](#4-advanced-execution-control--debugging)
   - 4.1 Stepping (Step Into, Step Over, Step Out, Run to Selection)
   - 4.2 Software, Hardware, and Conditional Breakpoints
   - 4.3 Ignore Counts and Log-Only Tracepoints
   - 4.4 POSIX Signal Interception and Pass-Through Execution
   - 4.5 Dynamic Branch Prediction
   - 4.6 Hit Trace (Code Coverage) and Run Trace (Time-Travel Navigation)
   - 4.7 CPU Machine State Snapshot (StateDumper)
5. [Advanced Reverse Engineering Toolset](#5-advanced-reverse-engineering-toolset)
   - 5.1 Glibc ptmalloc Heap Inspection (HeapView)
   - 5.2 ROP Gadget Scanner & Python Payload Export (ROPToolView)
   - 5.3 Interactive Basic-Block Control Flow Graph (CFGGraphView)
   - 5.4 Intermodular Dynamic Library API Calls (IntermodularCallsView)
   - 5.5 Opcode & Gadget Sequence Search (OpcodeSearcherView)
   - 5.6 Comprehensive ELF Header & Section Inspector (BinaryInfoView)
   - 5.7 Linux `/proc/<pid>/` Environment & Handles (ProcessPropertiesView)
   - 5.8 Reverse Engineering Monospace Scratchpad (NotesView)
6. [Unlocking Read-Only Pages, Hot-Patching & Physical Disk Export](#6-unlocking-read-only-pages-hot-patching--physical-disk-export)
   - 6.1 Elevating Permissions via Remote Syscall (`remoteMprotect`)
   - 6.2 Allocating Executable Target Memory (`remoteMmap`)
   - 6.3 In-Place Byte Editing and NOP Padding
   - 6.4 Centralized Patch Manager (`Ctrl+P`)
   - 6.5 Exporting Standalone Patched ELF Binaries to Disk (`patchFileToDisk`)
7. [Project Database & Automatic Session Persistence (.edb_db)](#7-project-database--automatic-session-persistence-edb_db)
8. [x64dbg-Style Interactive CommandBar CLI](#8-x64dbg-style-interactive-commandbar-cli)
9. [Modern C++20 Plugin Development Guide](#9-modern-c20-plugin-development-guide)
   - 9.1 Architecture & Gateway Pattern
   - 9.2 Plugin Directory & CMake Structure
   - 9.3 Writing the Plugin Header (`.hpp`)
   - 9.4 Implementing Plugin Business Logic (`.cpp`)
   - 9.5 Building the Shared Library (`.so`)
   - 9.6 Installation, Loading, and Verification
10. [Keyboard Shortcut Cheat Sheet](#10-keyboard-shortcut-cheat-sheet)

---

## 1. Environment Setup & Building

### 1.1 Prerequisites & Dependencies

`edb-next` is built with modern C++20 and Qt 5.15+.

#### Ubuntu / Debian:
```bash
sudo apt update
sudo apt install -y build-essential cmake git pkg-config qtbase5-dev libqt5widgets5 libcapstone-dev
```

#### Fedora / RHEL:
```bash
sudo dnf install -y gcc-c++ cmake git pkgconf-pkg-config qt5-qtbase-devel capstone-devel
```

#### Arch Linux:
```bash
sudo pacman -S --needed base-devel cmake git pkgconf qt5-base capstone
```

---

### 1.2 Build Commands

```bash
cd edb-next
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Build outputs in `build/`:
- `edb_next`: Graphical user interface executable
- `libedb_core.a`: Core debugging static library
- `plugins/sample_plugin.so`: Reference plugin
- `test_core`: Core regression suite
- `test_advanced`: Advanced features regression suite
- `test_exit`: Teardown and teardown stress suite

---

### 1.3 Running the Automated Regression Test Suites

```bash
./build/test_core
./build/test_advanced
./build/test_exit
```

---

## 2. Quick Start & Session Management

### 2.1 Starting the Debugger
```bash
./build/edb_next
```

### 2.2 Opening a Target (Open Target & ASLR Disabling)
- Press **Ctrl+O** (or `File -> Open Target...`) and select any 64-bit ELF binary.
- `edb-next` starts the process, invokes `personality(ADDR_NO_RANDOMIZE)` to disable ASLR, parses the symbol table, and positions the cursor at the entry point (`main` or `_start`).

### 2.3 Launch Arguments & Working Directory
- Navigate to `Options -> Launch Arguments...`.
- Set command-line arguments (e.g. `-v --port 8080`) and working directory (`cwd`). Settings persist across restarts.

### 2.4 Attaching to a Running Process
- Navigate to `File -> Attach to Process...` and enter the target PID.
- `edb-next` invokes `PTRACE_ATTACH`, halts all target threads, and synchronizes registers.

### 2.5 Restarting, Detaching, and Terminating
- **Restart (`Ctrl+F2`)**: Teardowns current target and relaunches with identical arguments.
- **Detach**: `Debug -> Detach` removes all breakpoints and releases target execution.
- **Terminate**: `Debug -> Terminate` delivers `SIGKILL` to clean up target resources.

---

## 3. 4-Quadrant Core Workspace Hands-on

```text
┌───────────────────────────────────────┬───────────────────────────────────────┐
│     Quadrant 1: Disassembly View      │       Quadrant 2: Register View       │
│  - Syntax highlighting, RIP indicator │  - 16 GPRs with diff highlight in red │
│  - F2 Breakpoint / Enter Branch Follow│  - Smart dereferencing & symbol lookup│
│  - Esc History Return / Space Assemble│  - Clickable EFLAGS badges / SSE tabs │
├───────────────────────────────────────┼───────────────────────────────────────┤
│     Quadrant 3: Multi-Tab Dump        │       Quadrant 4: Dedicated Stack     │
│  - Dump 1 ~ Dump 4 independent tabs   │  - 8-byte QWORD aligned rows          │
│  - Ctrl+E in-place hex patching       │  - => RSP indicator & relative offsets│
│  - 18 analysis drawer tools           │  - Double-click smart router          │
└───────────────────────────────────────┴───────────────────────────────────────┘
```

### 3.1 Quadrant 1: Disassembly View (DisassemblyView)
- **Visual Status**:
  - Current RIP: Cyan background with bold green arrow **`➔`**.
  - Breakpoints: Dark red row highlight.
- **Keyboard Branch Navigation**:
  - Press **Enter** on any `CALL`, `JMP`, or `Jcc` instruction to follow the branch target and record position in the navigation history stack.
  - Press **Esc**, **Backspace**, or **Alt+Left** to return; press **Alt+Right** to advance.
- **Code XREFs (`X`)**:
  - Press **X** on any instruction to view all incoming calls and jumps. Double-click any item to jump directly.
- **Inline Assembler (`Space`)**:
  - Press **Space** on an instruction. Type Intel assembly (e.g. `xor eax, eax`).
  - Enable **Auto Fill with NOPs** to maintain subsequent instruction alignment.
- **Set RIP (`Ctrl+*`)**:
  - Right-click any instruction and choose **Set New Origin Here (Set RIP)**.

### 3.2 Quadrant 2: Register View (RegisterView)
- **GPR Table**:
  - Displays all 16 GPRs. Changed values are highlighted in red.
  - **Smart Dereferencing**: Resolves nearest function symbols (`<main+0x10>`), stack pointer positions (`=> [RSP]`), ASCII strings (`"Hello world"`), and pointer chains (`-> 0x5555...`).
  - **In-Place Modification**: Double-click any value to edit in hex.
- **Interactive EFLAGS Badges**:
  - Badges for CF, PF, AF, ZF, SF, TF, IF, DF, OF.
  - Emerald green indicates set (`1`), dark gray indicates cleared (`0`). Click any badge to physically toggle the flag in CPU state.
- **FPU / SSE Panel**:
  - Displays 128-bit hex, 4x Float32, and 2x Double64 for all 16 XMM registers.

### 3.3 Quadrant 3: Multi-Tab Memory Dump (MultiDumpWidget)
- **Dump 1 ~ Dump 4 Multi-Tabs**:
  - 4 parallel, independent hex dump windows with separate addresses and scroll positions.
  - Context menu "Follow in Dump" automatically routes data to the active tab.
- **In-Place Hex Editing (`Ctrl+E`)**:
  - Modify bytes directly; right-click to fill with zeros or NOPs.
- **Raw Binary Export**:
  - In Tab 5 (Memory Regions), right-click any page to export as `.bin`.

### 3.4 Quadrant 4: Dedicated 64-Bit Stack View (StackView)
- **QWORD Aligned Layout**:
  - Each row presents an 8-byte QWORD.
  - Highlights current stack top as **`=> RSP`** with relative offsets (`+0x08`, `+0x10`, `[RBP]`).
  - **Smart Double-Click**: Code pointers jump to Disassembly; data pointers jump to Hex Dump.
  - Quick action: Click `[RSP]` to return to stack top; press **Shift+S** to expand/collapse.

---

## 4. Advanced Execution Control & Debugging

| Action | Hotkey | Description |
| :--- | :---: | :--- |
| **Continue** | **F9** | Resumes execution until next breakpoint or signal |
| **Step Into** | **F7** | Executes a single instruction, following `call` instructions |
| **Step Over** | **F8** | Executes a single instruction, stepping over `call` instructions |
| **Step Out** | **Shift+F11** | Sets temporary breakpoint on caller frame and runs until return |
| **Run to Selection** | **F4** | Sets a one-time temporary breakpoint at cursor row |
| **Pause** | Toolbar | Interrupts and halts all threads |

### 4.2 Breakpoints & Conditions
- **Software Breakpoints (`F2`)**: INT3 (`0xCC`) injection with automatic single-step-restore state machine.
- **Hardware Breakpoints (DR0~DR3)**: Right-click -> Hardware Breakpoint -> Execute, Write (1/2/4/8B), Read/Write (1/2/4/8B).
- **Conditional Breakpoints**: Right-click breakpoint in Tab 4 -> Set Condition (e.g. `rdi == 7`, `[rbp-8] > 0`). Evaluated via recursive descent parser.
- **Ignore Count & Log-Only**: Ignore the first $N$ hits, or format strings to output data directly to system log without halting execution.

### 4.4 POSIX Signal Management
- Configure signals 1-64 under `Options -> Preferences -> Signals`.
- Use **Shift+F7/F8/F9** to pass signals directly to the target's signal handlers.

### 4.5 Dynamic Branch Prediction
- Bottom bar indicates whether the current branch will be followed:
  - **`[JUMP TAKEN]`** (Green)
  - **`[JUMP NOT TAKEN]`** (Gray)

### 4.6 Hit Trace & Run Trace
- **Hit Trace**: Real-time code coverage tracking in disassembly.
- **Run Trace**: History recorder enabling step back (**`< Step Back`**) and step forward (**`Step Forward >`**) time-travel inspection.

---

## 5. Advanced Reverse Engineering Toolset

- **Heap Analyzer (Tab 9)**: Traverses glibc `malloc_chunk` structures, parsing chunk size, flags (`A|M|P`), and allocated/free state.
- **ROP Scanner (Tab 11)**: Detects gadgets ending in `ret`, `syscall`, `int 0x80`; exports Python `p64(...)` exploits.
- **Control Flow Graph (Tab 14)**: Hierarchical basic-block directed graph with color-coded branch edges.
- **Intermodular Calls (Tab 18)**: Identifies external library API calls (PLT/GOT) with fuzzy search.
- **Opcode Searcher (Tab 19)**: Scans memory for preset sequences (`JMP reg`, `Syscall`) or custom regex.
- **Binary Info (Tab 17)**: ELF headers, segments, sections, and `DT_NEEDED` dependencies.
- **Process Properties (Tab 8)**: `/proc/<pid>/fd/` classification into Sockets, Pipes, and PTYs.
- **Notes (Tab 15)**: Integrated scratchpad supporting quick RIP and timestamp injection.

---

## 6. Unlocking Read-Only Pages, Hot-Patching & Physical Disk Export

```mermaid
flowchart LR
    A["Read-Only Page"] --> B["remoteMprotect\n(Elevation to RWX)"]
    B --> C["Ctrl+E / Space\n(Memory Hot-Patch)"]
    C --> D["Ctrl+P Patch Manager\n(Review Modifications)"]
    D --> E["Patch File to Disk\n(VAddr -> FileOffset)"]
    E --> F["Standalone Patched ELF\n(0755 Executable)"]
```

1. **Unlocking Permissions**: In Tab 5 (Memory Regions), right-click and select **Change Page Permissions (mprotect)...** to elevate to **RWX** (or via CLI: `mprotect 0x555555555000 4096 7`).
2. **Allocating Target Memory**: Right-click in Memory Regions -> **Allocate Target Memory (mmap)...** to allocate an independent executable page (or via CLI: `alloc 4096 7`).
3. **Hot-Patching**: Press **Ctrl+E** or **Space** to patch code.
4. **Patch Review (`Ctrl+P`)**: Review original vs. patched bytes; revert or reapply selectively.
5. **Physical Disk Export**: Click **Patch File to Disk** in Patch Manager. `edb-next` computes ELF Program Header offsets ($FileOffset = VAddr - Segment.p\_vaddr + Segment.p\_offset$) and writes a standalone patched executable to disk.

---

## 7. Project Database & Automatic Session Persistence (.edb_db)

- **Persistence Scope**: Instruction comments (`;`), bookmarks (`Ctrl+B`), breakpoints (with conditions), watch expressions, patches, and scratch notes.
- **Save**: Press **Ctrl+S** to generate `<binary>.edb_db`.
- **Automatic Restore**: Reopening the binary detects and restores the `.edb_db` project database seamlessly.

---

## 8. x64dbg-Style Interactive CommandBar CLI

| Command | Description & Example |
| :--- | :--- |
| `bp <addr \| sym>` | Set software breakpoint. Ex: `bp main`, `bp 0x401000` |
| `bph <addr>` | Set hardware execution breakpoint. Ex: `bph 0x401000` |
| `bc <addr>` / `bd <addr>` / `be <addr>` | Clear / Disable / Enable breakpoint |
| `r <reg> <val>` | Set register value. Ex: `r rax 0x1337` |
| `d <addr>` | Follow address in active hex dump. Ex: `d 0x7fffffffd7d0` |
| `u <addr \| sym>` | Follow address in disassembly. Ex: `u calculate_fib` |
| `step` / `stepo` / `ret` / `run` | Step into (`s`) / Step over (`so`) / Step out (`rto`) / Run (`g`) |
| `eval <expr>` | Evaluate expression. Ex: `eval rax + 0x20`, `eval [rbp-8]` |
| `mprotect <addr> <size> <prot>` | Change target page protections (7=RWX). Ex: `mprotect 0x555555555000 4096 7` |
| `alloc <size> [prot]` | Allocate target memory page. Ex: `alloc 4096 7` |
| `dumpstate` | Format and copy full CPU state snapshot to clipboard |

---

## 9. Modern C++20 Plugin Development Guide

Plugins are compiled as standard Linux shared objects (`.so`) using the `IPlugin` and `IPluginContext` contracts.

### 9.2 Writing a Plugin Header (`MyPlugin.hpp`)
```cpp
#pragma once
#include "core/IPlugin.hpp"
#include "core/IPluginContext.hpp"
#include <QObject>
#include <QMenu>

namespace my_plugin {

class MyPlugin : public QObject, public edb_next::IPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EDB_NEXT_PLUGIN_IID)
    Q_INTERFACES(edb_next::IPlugin)

public:
    MyPlugin() = default;
    ~MyPlugin() override = default;

    [[nodiscard]] edb_next::PluginMetadata metadata() const override {
        return edb_next::PluginMetadata{
            .id = "my_tool",
            .name = "Custom Security Tool",
            .version = "1.0.0",
            .author = "Analyst",
            .description = "Custom CLI command & Breakpoint Hook demo"
        };
    }

    bool initialize(edb_next::IPluginContext* context) override;
    void shutdown() override;
    QMenu* createMenu(QWidget* parent = nullptr) override;

private:
    edb_next::IPluginContext* ctx_{nullptr};
};

} // namespace my_plugin
```

### 9.3 Implementing Plugin Logic (`MyPlugin.cpp`)
```cpp
#include "MyPlugin.hpp"
#include <QMessageBox>

namespace my_plugin {

bool MyPlugin::initialize(edb_next::IPluginContext* context) {
    ctx_ = context;
    if (!ctx_) return false;

    // Register a CLI command: my_ping <args...>
    ctx_->registerCommand("my_ping", [this](const std::vector<std::string>& args) {
        ctx_->logMessage(QString("[MyPlugin] Ping received with %1 argument(s).").arg(args.size()));
        auto session = ctx_->activeSession();
        if (session && session->state() == edb_next::SessionState::Paused) {
            ctx_->logMessage(QString("[MyPlugin] Current RIP: 0x%1").arg(session->registers().rip(), 0, 16));
        }
    }, "my_ping [args...] - Test command from MyPlugin");

    // Hook breakpoint hit events
    ctx_->registerDebugEventListener([this](const edb_next::DebugEvent& ev) {
        if (ev.reason == edb_next::StopReason::Breakpoint) {
            ctx_->logMessage(QString("[MyPlugin Hook] Target halted at BP: 0x%1")
                             .arg(QString::fromStdString(ev.address.toHex())));
        }
    });

    return true;
}

void MyPlugin::shutdown() {
    if (ctx_) {
        ctx_->logMessage("[MyPlugin] Unloaded cleanly.");
        ctx_ = nullptr;
    }
}

QMenu* MyPlugin::createMenu(QWidget* parent) {
    auto* menu = new QMenu("Custom Tool", parent);
    auto* act = menu->addAction("Scan Target...");
    connect(act, &QAction::triggered, [parent]{
        QMessageBox::information(parent, "MyPlugin", "Executing custom target memory scan!");
    });
    return menu;
}

} // namespace my_plugin
```

### 9.4 CMake Build Definition (`CMakeLists.txt`)
```cmake
cmake_minimum_required(VERSION 3.16)
project(MyPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui)

set(EDB_NEXT_ROOT "/path/to/edb-next")
include_directories(${EDB_NEXT_ROOT} ${EDB_NEXT_ROOT}/core ${EDB_NEXT_ROOT}/ui)

add_library(my_plugin SHARED MyPlugin.cpp)
set_target_properties(my_plugin PROPERTIES PREFIX "")
target_link_libraries(my_plugin PRIVATE Qt5::Widgets Qt5::Core)
```

### 9.5 Loading and Using the Plugin
- **Auto-Loading**: Copy `my_plugin.so` to `~/.config/edb-next/plugins/`.
- **Manual Loading**: In the GUI, select `Plugins -> Manage Plugins...` and click **Load Plugin...**.
- **Execution**: The action appears under the **Plugins** menu, and `my_ping` is available in the bottom CommandBar.

---

## 10. Keyboard Shortcut Cheat Sheet

| Hotkey | Description | Hotkey | Description |
| :--- | :--- | :--- | :--- |
| **F9** | Continue | **Enter** | Follow Branch |
| **F7** | Step Into | **Esc / Backspace** | Go Back in History |
| **F8** | Step Over | **Space** | Assemble In-Place |
| **Shift+F11** | Step Out | **; (Semicolon)** | Add / Edit Comment |
| **F4** | Run to Selection | **Ctrl+B** | Toggle Bookmark |
| **Ctrl+F2** | Restart Session | **X** | Show Cross References |
| **Ctrl+\*** | Set RIP (Origin) | **Ctrl+E** | Modify Hex Bytes |
| **F2** | Toggle Breakpoint | **Ctrl+P** | Patch Manager & Disk Export |
| **Ctrl+S** | Save Project Database | **Ctrl+D** | Dump CPU State Snapshot |
| **Shift+S** | Toggle Stack View | **Shift+F7/F8/F9** | Pass Signal Step / Run |
| **Alt+C** | Focus CPU View | **Alt+D** | Switch to Hex Dump |
| **Alt+K** | Switch to Call Stack | **Alt+B** | Switch to Breakpoints |
| **Alt+M** | Switch to Memory Regions | **Alt+E** | Switch to Symbol Viewer |
| **Alt+L** | Switch to System Log | | |
