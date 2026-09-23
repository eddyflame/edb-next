# edb-next

<div align="center">

<h3>Next-Generation Linux Binary Debugger & Reverse Engineering Platform</h3>

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg?style=flat-square&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/23)
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

Built from scratch using **C++23**, **Qt 6.4+**, and **Capstone / Zydis dual disassembly engines**, `edb-next` deeply adopts the tactile four-quadrant reverse engineering workflow popularized by Windows' **x64dbg**, while innovating natively on Linux through a reactive kernel event loop, native C pseudo-code decompilation, time-travel debugging, Z3 symbolic execution, headless DAP serving, and physical ELF disk patching.

```text
┌───────────────────────────────────────┬───────────────────────────────────────┐
│     Quadrant 1: Disassembly View      │       Quadrant 2: Register View       │
│  - One Dark syntax highlighting & RIP │  - 16 GPRs with diff highlight in red │
│  - Dynamic branch & deref preview bar │  - Quick +1/-1 & Follow in Stack      │
│  - F2 Breakpoint / Enter Follow / Esc │  - Clickable EFLAGS badges / SSE tabs │
│  - F5 Native C Pseudo-Code Decompiler │  - TimeTravelWidget Scrub Bar (Ctrl+F7)│
├───────────────────────────────────────┼───────────────────────────────────────┤
│     Quadrant 3: Multi-Tab Dump        │       Quadrant 4: Dedicated Stack     │
│  - Dump 1 ~ Dump 4 independent tabs   │  - 8-byte QWORD aligned rows          │
│  - Alt+Left/Right history navigation  │  - [Return Address] amber detection   │
│  - Ctrl+E in-place patch / TypeViewer │  - => RSP indicator & smart router    │
└───────────────────────────────────────┴───────────────────────────────────────┘
```

---

## Key Highlights

- 🚀 **4-Quadrant Golden Workflow & Modern Ergonomics**: Synchronous four-way workspace linking Disassembly, Registers, Multi-Dump (Dump 1~4), and 64-bit Stack. Features x64dbg-style syntax highlighting, dynamic branch prediction, memory operand dereferencing previews, 5-track call/jump control flow lines, and continuous in-place assembly (`Space`).
- ⚡ **Microsecond-Level Reactive Kernel Event Loop (`pidfd` + `epoll`)**: Native Linux 5.3+ `pidfd` and `epoll` architecture with `eventfd` self-pipe wakeups, achieving 0% idle CPU utilization and microsecond response times; non-intrusive target file descriptor and socket introspection via `pidfd_getfd`.
- 🧩 **Native C Pseudo-Code Decompiler & Symbolic Analysis**: Modern C++23 native decompiler (`DecompilerEngine`) lifting assembly into AST control flow with bidirectional `F5` source mapping; integrated SSA Micro-IR and Z3 SMT solver for automated branch reachability solving and dynamic taint tracking.
- ⏳ **Time-Travel Debugging (TTD) & Stealth Monitoring**: Deterministic execution frame rewind and delta diff tracking (`Ctrl+F7` Step Back, `Ctrl+Shift+F9` Reverse Continue, timeline scrubber); native Linux `userfaultfd` stealth watchpoints (zero `0xCC` injection) and dirty page tracking.
- 💾 **Remote Syscall Injection & Physical ELF Disk Patching**: Bypasses read-only memory by injecting `SYS_mprotect` (RWX elevation) and `SYS_mmap` directly inside the target; computes virtual-to-physical ELF offsets and exports standalone patched executables to disk.
- 🔌 **Dual Scripting & Headless DAP Ecosystem**: Embedded Python 3 and Lua 5.4 scripting engines (`Alt+P`) with silent breakpoint hooking; built-in Microsoft DAP (Debug Adapter Protocol) JSON-RPC server (`--dap`) for native VS Code and Neovim integration.
- 🗄️ **ACID Transactional Project Database**: Embedded SQLite3 + Zstandard (`libzstd`) transactional database (`.edb_db`), persisting comments, labels, bookmarks, breakpoints, and patches with 100% transparent migration from legacy JSON projects.

> **For architectural design and implementation details**, please refer to the Software Design Document: [doc/DESIGN_en.md](doc/DESIGN_en.md).

---

## Documentation

Project documentation is maintained under the `doc/` directory:

| Document | English | 简体中文 | Scope & Description |
| :--- | :--- | :--- | :--- |
| **Software Design Document (SDD)** | [doc/DESIGN_en.md](doc/DESIGN_en.md) | [doc/DESIGN_zh.md](doc/DESIGN_zh.md) | In-depth architecture, internal mechanisms, state machines, data structures, and roadmap |
| **Tutorial & Plugin Development Guide** | [doc/TUTORIAL_en.md](doc/TUTORIAL_en.md) | [doc/TUTORIAL_zh.md](doc/TUTORIAL_zh.md) | User manuals, operation guides, shortcut cheat sheets, and C++20 plugin walkthroughs |
| **Contributing Guide** | [doc/CONTRIBUTING_en.md](doc/CONTRIBUTING_en.md) | [doc/CONTRIBUTING_zh.md](doc/CONTRIBUTING_zh.md) | Coding style, commit guidelines, and Pull Request workflows |

---

## Quick Start

### 1. Install Dependencies (Ubuntu / Debian)
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
    liblua5.4-dev \
    libzstd-dev \
    libsqlite3-dev \
    libclang-dev \
    libz3-dev
```

### 2. Build from Source
```bash
git clone https://github.com/your-username/edb-next.git
cd edb-next

# Configure and compile with all available cores
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### 3. Run Verification Test Suites
```bash
./build/test_core           # Core tests (breakpoints, stepping, threads, ELF parsing)
./build/test_dwarf          # DWARF source debugging and line mapping tests
./build/test_advanced       # Advanced tests (disk patching, tracing, CFG, remote syscalls)
./build/test_scripting      # Embedded Python 3 & Lua 5.4 scripting tests
./build/test_exit           # Teardown safety and resource cleanup stress tests
./build/test_nextgen        # pidfd loop, target FD introspection, libclang AST, SQLite3+zstd, C++23
./build/test_p4_advanced_re # SSA Micro-IR, Z3 symbolic solver, decompiler, TTD replay, DAP, eBPF
./build/test_p5_ultimate    # userfaultfd, BTF compact types, DR6 attribution & PageGuard fallback, anti-anti-debug, NEON
```

### 4. Launch the Debugger

#### Method A: Direct Execution
```bash
./build/edb_next
```

#### Method B: Standalone Portable AppImage (Recommended)
Download or build the standalone AppImage, make it executable, and run on any major Linux distribution (Ubuntu, Debian, Fedora, Arch Linux, etc.):
```bash
chmod +x edb-next-x86_64.AppImage
./edb-next-x86_64.AppImage
```

> **Note (ptrace privileges)**:
> - Spawning new targets (Open Binary) works out of the box without root privileges.
> - When attaching to existing non-child processes restricted by Linux Yama LSM, run with `sudo ./edb-next-x86_64.AppImage` or set: `sudo sysctl -w kernel.yama.ptrace_scope=0`.

#### Build AppImage Locally
```bash
# Build and package on host
./scripts/build_appimage.sh

# Or package in an Ubuntu 22.04 LTS Docker container for maximum distribution compatibility
./scripts/docker_build_appimage.sh
```

---

## Global Shortcut Cheat Sheet

| Shortcut | Action | Shortcut | Action |
| :--- | :--- | :--- | :--- |
| **F9** | Continue (Run) | **Enter** | Follow Branch / Smart Stack Follow |
| **F7** | Step Into | **Esc / Backspace** | Go Back (History Stack) |
| **F8** | Step Over | **\*** *(Numpad / Key)* | **Origin**: Center current RIP |
| **Shift+F11** | Step Out | **Space** | Continuous In-Place Assembly (+ NOP fill) |
| **F4** | Run to Selection | **; (Semicolon)** | Add / Edit Instruction Comment |
| **F5** | **Native C Decompiler** | **: (Colon)** | Set / Edit User Label (`🏷`) |
| **Ctrl+F7** | **Step Back** (Time-Travel) | **Ctrl+B** | Toggle Bookmark |
| **Ctrl+Shift+F9** | **Reverse Continue** | **X** | Cross References (XREFs) |
| **Ctrl+F2** | Restart Session | **Ctrl+E** | Modify Hex Bytes |
| **Ctrl+\*** | Set RIP to Selection | **Ctrl+P** | Patch Manager & Disk Export |
| **F2** | Toggle Breakpoint | **Alt+S** | Focus Source View |
| **Ctrl+Alt+S** | Search All Strings | **Alt+C** | Focus CPU / Disassembly |
| **Ctrl+Alt+C** | Search Intermodular Calls | **Ctrl+D** | Dump Machine State Snapshot |
| **Alt+P** | Script Console (Python/Lua) | **Shift+F7/F8/F9** | Pass Signal Step / Run |
| **Ctrl+Shift+S** | Toggle Mixed ASM / Source | **Stack: `Space` / `Ctrl+G`** | Edit QWORD / Go to Address |
| **Ctrl+S** | Save Project (.edb_db) | **Follow in Dump 1~4** | Route to Dedicated Dump Tab |
| **Shift+S** | Toggle Stack View | **Regs: `+` / `-` / `0`** | GPR Quick Increment / Zero |
| **Dump: `Enter` / DblClick** | In-Place Edit Hex Bytes | | |

---

## Plugin Development Example

`edb-next` provides a lightweight, decoupled C++20 plugin interface. An official sample plugin is provided at [plugins/SamplePlugin/](plugins/SamplePlugin/):

```cpp
#include "core/IPlugin.hpp"
#include "core/IPluginContext.hpp"

class MyPlugin : public QObject, public edb_next::IPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EDB_NEXT_PLUGIN_IID)
    Q_INTERFACES(edb_next::IPlugin)

public:
    bool initialize(edb_next::IPluginContext* ctx) override {
        // Register custom CLI commands to the CommandBar
        ctx->registerCommand("my_ping", [](const std::vector<std::string>& args) {
            // Command execution logic
        }, "my_ping - Sample extended command");

        // Hook debug events
        ctx->registerDebugEventListener([](const edb_next::DebugEvent& ev) {
            // Handle debug event
        });
        return true;
    }
    void shutdown() override {}
};
```
For detailed plugin development instructions, see [doc/TUTORIAL_en.md](doc/TUTORIAL_en.md).

---

## Contributing

We welcome contributions from the open-source community! Before opening a PR, please read our [Contributing Guide](doc/CONTRIBUTING_en.md).

---

## License

This project is licensed under the [GNU General Public License v3.0 (GPL-3.0)](LICENSE).
