# edb-next

<div align="center">

<h3>Next-Generation Linux Binary Debugger & Reverse Engineering Platform</h3>

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg?style=flat-square&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![Qt](https://img.shields.io/badge/Qt-5.15%2B-brightgreen.svg?style=flat-square&logo=qt)](https://www.qt.io/)
[![Platform](https://img.shields.io/badge/Platform-Linux%20x86__64-orange.svg?style=flat-square&logo=linux)](https://www.kernel.org/)
[![License](https://img.shields.io/badge/License-GPLv3-green.svg?style=flat-square)](LICENSE)
[![CI](https://img.shields.io/badge/CI-Passing-success.svg?style=flat-square&logo=github-actions)](.github/workflows/ci.yml)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat-square)](doc/CONTRIBUTING_en.md)

**[English](README.md) | [简体中文](README_zh.md)**

</div>

---

## Overview

**edb-next** is a modern, high-performance graphical binary debugger and dynamic reverse engineering platform specifically engineered for Linux x86_64. 

Built from scratch using **C++20**, **Qt 5.15+**, and the **Capstone Disassembly Engine**, `edb-next` addresses the longstanding absence of an industrial-strength GUI debugger on Linux. It deeply aligns with the tactile, battle-tested reverse engineering workflows of Windows' legendary **x64dbg**, while innovating natively on Linux through in-target remote syscall injection, ELF physical disk patching, non-blocking asynchronous event loops, and full glibc ptmalloc heap deconstruction.

```text
┌───────────────────────────────────────┬───────────────────────────────────────┐
│     Quadrant 1: Disassembly View      │       Quadrant 2: Register View       │
│  - Capstone syntax highlighting & RIP │  - 16 GPRs with diff highlight in red │
│  - F2 Breakpoint / Enter Branch Follow│  - Smart dereferencing & symbol lookup│
│  - Esc History Return / Space Assemble│  - Clickable EFLAGS badges / SSE tabs │
├───────────────────────────────────────┼───────────────────────────────────────┤
│     Quadrant 3: Multi-Tab Dump        │       Quadrant 4: Dedicated Stack     │
│  - Dump 1 ~ Dump 4 independent tabs   │  - 8-byte QWORD aligned rows          │
│  - Ctrl+E in-place hex patching       │  - => RSP indicator & relative offsets│
│  - 18 analysis drawer tools           │  - Double-click smart router          │
└───────────────────────────────────────┴───────────────────────────────────────┘
```

---

## Key Highlights & Innovations

- 🚀 **Zero-Deadlock Multithreaded Event Loop**: A dedicated `EventLoopThread` polls `waitpid(WNOHANG)` in the background and delivers notifications via Qt signals. Synchronous foreground operations seamlessly assert atomic suspension (`suspended_`), completely eliminating UI freezes.
- ⚡ **In-Target Remote Syscall Injection (`executeRemoteSyscall`)**: Break through read-only memory barriers by dynamically injecting `SYS_mprotect` in the target process (RWX elevation), allocating isolated executable pages (`SYS_mmap`), and freeing memory (`SYS_munmap`).
- 💾 **One-Click Physical ELF Disk Patching (`patchFileToDisk`)**: Translates Virtual Memory Addresses to ELF Program Header physical offsets ($VAddr \to FileOffset$), exporting standalone executable patched binaries directly to disk.
- 🗄️ **Automatic Project Database (`.edb_db`)**: Seamlessly preserves and restores all user instruction comments, bookmarks, conditional breakpoints, watch expressions, memory patches, and scratchpad notes across sessions.
- 🎯 **Classic 4-Quadrant Golden Workspace**: Disassembly, Registers, Multi-Tab Dump (Dump 1~4), and a dedicated 64-bit QWORD Stack view displayed simultaneously in real time.
- 🔍 **Native Linux Introspection**: Built-in glibc ptmalloc heap analyzer (`malloc_chunk` layout & `A|M|P` flags), ROP gadget scanner with Python `p64(...)` export, basic-block interactive CFG graph, intermodular PLT/GOT API call finder, and `/proc/<pid>/fd/` handle classification (Sockets, Pipes, PTYs).
- ⌨️ **Interactive CommandBar CLI**: Bottom x64dbg-style CLI console supporting `bp`, `bph`, `r`, `d`, `u`, `step`, `eval`, `mprotect`, `alloc`, and plugin command extensions.
- 🧩 **Decoupled Modern C++20 Plugin Architecture**: Pure virtual `IPlugin` and `IPluginContext` contract supporting dynamic `.so` hot-loading, menu injection, CLI command registration, and breakpoint hooks.

---

## Feature Comparison Matrix

| Feature | Original edb (Linux) | x64dbg (Windows) | edb-next (Modern Linux) |
| :--- | :---: | :---: | :---: |
| **Language Standard** | C++11 | C++14/17 | **Modern C++20** |
| **Event Concurrency** | 0ms QTimer (prone to hangs) | Complex sync | **Decoupled `EventLoopThread` (0% UI Freeze)** |
| **Workspace Layout** | Single bottom drawer | 4 Quadrants | **4-Quadrant Golden Workspace** |
| **Memory Dumps** | Single Dump view | Dump 1 ~ Dump 5 | **4-Way MultiDumpWidget (Dump 1~4)** |
| **Read-Only Patching** | Rejected / Errors | VirtualProtect | **Remote Syscall Injection (`mprotect`)** |
| **Physical Disk Patch** | None (Memory-only) | Patched EXE | **Innovative `patchFileToDisk` (ELF Export)** |
| **Session Persistence** | Lost on exit | `.dd64` Database | **`.edb_db` JSON Project Database** |
| **Interactive CLI** | None | CommandBar | **x64dbg-Style Bottom CommandBar** |
| **Linux Heap Analysis** | Outdated plugin | N/A (Windows) | **Native Glibc ptmalloc Analyzer (Tab 9)** |
| **Exploit Tooling** | Basic ROP plugin | 3rd-party | **Built-in ROP Engine & Python `p64()` Export** |

---

## Quick Start

### 1. Prerequisites (Ubuntu / Debian)
```bash
sudo apt update
sudo apt install -y build-essential cmake git pkg-config qtbase5-dev libqt5widgets5 libcapstone-dev
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
./build/test_advanced   # Validates patching, disk export, remote syscalls, and plugins
./build/test_exit       # Validates clean process teardown without crashes
```

### 4. Launch edb-next
```bash
./build/edb_next
```

---

## Documentation

Comprehensive bilingual documentation is maintained under the `doc/` directory:

| Document | English Version | 中文版本 (Chinese Version) | Description |
| :--- | :--- | :--- | :--- |
| **Software Design Document (SDD)** | [doc/DESIGN_en.md](doc/DESIGN_en.md) | [doc/DESIGN_zh.md](doc/DESIGN_zh.md) | Comprehensive technical architecture, algorithms, and roadmap |
| **User Manual & Plugin Guide** | [doc/TUTORIAL_en.md](doc/TUTORIAL_en.md) | [doc/TUTORIAL_zh.md](doc/TUTORIAL_zh.md) | Full user manual, shortcut guide, and C++20 plugin development tutorial |
| **Contribution Guidelines** | [doc/CONTRIBUTING_en.md](doc/CONTRIBUTING_en.md) | [doc/CONTRIBUTING_zh.md](doc/CONTRIBUTING_zh.md) | Code style, git workflow, and PR verification rules |

---

## Keyboard Shortcuts

| Hotkey | Action | Hotkey | Action |
| :--- | :--- | :--- | :--- |
| **F9** | Continue Execution | **Enter** | Follow Branch (`CALL`/`JMP`) |
| **F7** | Step Into | **Esc / Backspace** | Go Back in Navigation History |
| **F8** | Step Over | **Space** | Assemble In-Place (with NOP fill) |
| **Shift+F11** | Step Out of Function | **; (Semicolon)** | Add / Edit Instruction Comment |
| **F4** | Run to Selection | **Ctrl+B** | Toggle Bookmark (`★`) |
| **Ctrl+F2** | Restart Debug Session | **X** | Show Cross References (XREFs) |
| **Ctrl+\*** | Set RIP (New Origin) | **Ctrl+E** | Modify Hex Bytes In-Place |
| **F2** | Toggle Software Breakpoint | **Ctrl+P** | Patch Manager & Disk File Export |
| **Ctrl+S** | Save Project Database | **Ctrl+D** | Dump Formatted CPU State Snapshot |
| **Shift+S** | Toggle Stack View | **Shift+F7/F8/F9** | Pass Signal Step / Run |

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
