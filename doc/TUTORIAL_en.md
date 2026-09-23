# edb-next User Manual & C++20 Plugin Development Guide

> **Project**: edb-next (Next-Generation Linux Binary Debugger & Reverse Engineering Platform)  
> **Target Version**: v1.0.0+  
> **Platform**: Linux x86_64  
> **Language**: C++20 / Pure Qt 6.4+ (GCC 13+) / Capstone Engine

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
   - 4.4 Script-Driven Breakpoint Actions & Silent Hooking
   - 4.5 Memory Page-Guard Breakpoints & Stealth Execution
   - 4.6 POSIX Signal Interception and Pass-Through Execution
   - 4.7 Dynamic Branch Prediction
   - 4.8 Hit Trace (Code Coverage) and Run Trace (Time-Travel Navigation)
   - 4.9 CPU Machine State Snapshot (StateDumper)
   - 4.10 Automated Shared Library Interception & Pending Breakpoints
   - 4.11 Follow-Fork Mode & Multi-Process Inferiors
   - 4.12 Independent Thread Freeze & Thaw (Freeze / Thaw & Isolated Stepping)
   - 4.13 Differential Memory Scanner (CheatEngine-Style Convergence)
   - 4.14 Compound Type Reconstruction & Struct Layout (Type Viewer)
   - 4.15 Native C Pseudo-Code Decompiler (`F5` & Bidirectional Mapping)
   - 4.16 Time-Travel Debugging (TTD) & Step Back Replay (`Ctrl+F7` Step Back)
   - 4.17 Headless DAP (Debug Adapter Protocol) Server & VS Code / Neovim Integration
   - 4.18 BTF (BPF Type Format) Kernel & ELF Type Import (Type Viewer)
   - 4.19 Advanced Anti-Anti-Debugging Configuration
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
9. [DWARF Source-Level Debugging Guide](#9-dwarf-source-level-debugging-guide)
   - 9.1 Compiler Options & DWARF Extraction
   - 9.2 Mixed ASM/Source View (`Ctrl+Shift+S`)
   - 9.3 Standalone Source Browser (`Alt+S`)
   - 9.4 Source-Level Stepping
10. [Embedded Scripting Automation Guide (Python 3 & Lua 5.4)](#10-embedded-scripting-automation-guide-python-3--lua-54)
   - 10.1 Dual-Engine Architecture Rationale
   - 10.2 Interactive Script Console (`Alt+P`)
   - 10.3 Python 3 Automation & `edb` API Reference
   - 10.4 Lua 5.4 Fast Condition Hooks & API Reference
   - 10.5 Running External Script Files & Automated Unpacking
   - 10.6 Inline CommandBar Script Evaluation (`py ...` / `lua ...`)
   - 10.7 Script-Driven Breakpoint Actions & Silent Hooking Hands-on
11. [Modern C++20 Plugin Development Guide](#11-modern-c20-plugin-development-guide)
   - 11.1 Architecture & Gateway Pattern
   - 11.2 Plugin Directory & CMake Structure
   - 11.3 Writing the Plugin Header (`.hpp`)
   - 11.4 Implementing Plugin Business Logic (`.cpp`)
   - 11.5 Building the Shared Library (`.so`)
   - 11.6 Installation, Loading, and Verification
12. [Keyboard Shortcut Cheat Sheet](#12-keyboard-shortcut-cheat-sheet)
13. [Conclusion](#13-conclusion)

---

## 1. Environment Setup & Building

### 1.1 Prerequisites & Dependencies

`edb-next` is built with modern C++20 and Qt 5.15+.

#### Ubuntu / Debian:
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

#### Fedora / RHEL:
```bash
sudo dnf install -y gcc-c++ cmake git pkgconf-pkg-config qt6-qtbase-devel capstone-devel elfutils-devel python3-devel lua-devel
```

#### Arch Linux:
```bash
sudo pacman -S --needed base-devel cmake git pkgconf qt6-base capstone elfutils python lua
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
- `test_dwarf`: DWARF source debugging regression suite
- `test_advanced`: Advanced features regression suite
- `test_scripting`: Python 3 & Lua 5.4 scripting regression suite
- `test_exit`: Teardown and teardown stress suite

---

### 1.3 Running the Automated Regression Test Suites

```bash
./build/test_core
./build/test_dwarf
./build/test_advanced
./build/test_scripting
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
  - Breakpoints: Dark red row highlight; composite highlight if RIP lands on a breakpoint.
- **x64dbg-Style Disassembly Syntax Highlighting**:
  The instruction column is handled by `InstructionHighlightDelegate`, implementing a high-density One Dark / x64dbg color topology:
  - **`CALL`**: Bold warm gold (`#E5C07B`), prominently emphasizing sub-routine entry points.
  - **`JMP`**: Warm orange (`#D19A66`), indicating unconditional control transfers.
  - **`Jcc`** (`JE`, `JNE`, `JZ`, `JNZ`, `JG`, `JL`, `JA`, `JB`, `JAE`, etc.): Coral salmon (`#E06C75`), highlighting decision branches.
  - **`RET` / `RETN`**: Bold violet magenta (`#C678DD`), marking function returns.
  - **`SYSCALL` / `SYSENTER` / `INT` / `UD2` / `HLT`**: Bold deep crimson (`#E06C75`), flagging kernel traps and faults.
  - **`PUSH` / `POP`**: Cyan teal (`#56B6C2`), tracking stack balancing.
  - **`CMP` / `TEST`**: Muted gold (`#E5C07B`), highlighting flag comparison sites.
  - **`NOP`**: Italic dim gray (`#5C6370`).
  - **Registers** (`RAX`..`R15`, `EAX`..`R15D`, `RSP`, `RBP`, `RIP`, etc.): Bright sky blue (`#61AFEF`).
  - **Memory Brackets** (`[...]`): Soft grass green (`#98C379`).
  - **Immediates & Constants** (hex `0x...` and numbers): Coral orange (`#D19A66`).
- **Rich HTML Dynamic Branch Prediction & Memory Operand Preview**:
  A dedicated live status bar beneath the disassembly listing evaluates runtime CPU context:
  - **Dynamic Branch Prediction**: Evaluates `EFLAGS` (`ZF`, `SF`, etc.) in real time:
    - Branch taken: Emerald green `Branch Taken: YES (ZF=1)`.
    - Branch not taken: Rose red `Branch Taken: NO (ZF=0)`.
  - **Chained Memory Operand Dereferencing**: For complex effective addresses like `[rbp - 0x14]` or `[rax + rcx*4 + 0x20]`, resolves the virtual address and fetches the 8-byte value:
    `[rbp - 0x14] => 0x7fffffffe00c => 0x00000001`.
  - **Target Symbol Resolution**: Resolves call and jump targets into human-readable symbols (e.g. `call <calculate_fib>`).
- **Keyboard Branch Navigation**:
  - Press **Enter** on any `CALL`, `JMP`, or `Jcc` instruction to follow the branch target and record position in the navigation history stack.
  - Press **Esc**, **Backspace**, or **Alt+Left** to return; press **Alt+Right** to advance.
- **Code XREFs (`X`)**:
  - Press **X** on any instruction to view all incoming calls and jumps. Double-click any item to jump directly.
- **Inline Assembler (`Space`)**:
  - Press **Space** on an instruction. Type Intel assembly (e.g. `xor eax, eax`).
  - Enable **Auto Fill with NOPs** to maintain subsequent instruction alignment.
- **Set RIP / Set Origin (`Ctrl+*`)**:
  - Press **Ctrl+\*** or right-click any instruction and select **Set New Origin Here (Set RIP)** to force CPU instruction pointer RIP to the selected instruction.

---

### 3.2 Quadrant 2: Register View (RegisterView)
- **GPR Table**:
  - Displays all 16 GPRs. Changed values are highlighted in red.
  - **Smart Dereferencing**: Resolves nearest function symbols (`<main+0x10>`), stack pointer positions (`=> [RSP]`), ASCII strings (`"Hello world"`), and pointer chains (`-> 0x5555...`).
  - **In-Place Modification**: Double-click any value to edit in hex.
- **Quick GPR Increment / Decrement (`+1` / `-1`)**:
  - Right-click any GPR (RAX~R15) and select **`+1 (Increment)`** or **`-1 (Decrement)`** to immediately adjust the register value via `ptrace(PTRACE_SETREGS)` without modal dialogs.
- **Follow in Stack**:
  - Right-click any register holding a stack address and select **`Follow in Stack`** to smoothly focus that offset in the dedicated 64-bit Stack View.
- **Multi-Format Copy Submenu (`Copy As...`)**:
  - Right-click any register and choose **`Copy As...`**:
    - `Hex (0x...)`: Standard 64-bit hex string.
    - `Decimal`: Signed and unsigned decimal integer.
    - `Dereferenced String/Bytes`: Dereferences the register as a memory pointer and copies ASCII string or hex bytes to clipboard.
- **Interactive EFLAGS Badges**:
  - Badges for CF, PF, AF, ZF, SF, TF, IF, DF, OF.
  - Emerald green indicates set (`1`), dark gray indicates cleared (`0`). Click any badge to physically toggle the flag in CPU state.
- **FPU / SSE Panel**:
  - Displays 128-bit hex, 4x Float32, and 2x Double64 for all 16 XMM registers.

---

### 3.3 Quadrant 3: Multi-Tab Memory Dump (MultiDumpWidget)
- **Dump 1 ~ Dump 4 Multi-Tabs**:
  - 4 parallel, independent hex dump windows with separate addresses and scroll positions.
  - Context menu "Follow in Dump" automatically routes data to the active tab.
- **Navigation History Stack**:
  - Tracks navigation history across `Goto Address`, `Follow in Dump`, and pointer jumps.
  - Press **Alt+Left** or **Backspace** to go back; press **Alt+Right** to go forward.
- **Cross-View QWORD Follow**:
  - Right-click any byte cell in Hex Dump:
    - **`Follow QWORD in Dump`**: Reads 8-byte QWORD and follows address in Hex Dump.
    - **`Follow QWORD in Disassembly`**: Follows 8-byte QWORD in Disassembly View.
    - **`Follow QWORD in Stack`**: Follows 8-byte QWORD in Quadrant 4 Stack View.
- **Direct Struct Layout Linking (`View as Struct...`)**:
  - Right-click any byte cell and select **`View as Struct (Type Viewer)...`** to instantly open bottom **Tab 22: Type Viewer** with the address prefilled for structured C-style decoding.
- **In-Place Hex Editing (`Ctrl+E`)**:
  - Modify bytes directly; right-click to fill with zeros or NOPs.
- **Direct Hardware Watchpoints & Cell Highlights**:
  - Right-click any byte cell in the Hex Dump and open the **"Breakpoint"** submenu;
  - Instantly deploy **Set Hardware Write Watchpoint** (1, 2, 4, or 8 bytes) or **Set Hardware Read/Write Watchpoint** (1, 2, 4, or 8 bytes), as well as Hardware Execute breakpoints or software `0xCC` breakpoints;
  - Active breakpoint cells are highlighted with deep red backgrounds (`QColor(160, 40, 40, 160)`) and bright white text, with hovering tooltips displaying `Breakpoint active at 0x...`.
- **Page-Guard Memory Protection Breakpoints**:
  - Right-click any byte cell or selection -> `Breakpoint -> Page-Guard Breakpoint`;
  - Deploy **No Access (`PROT_NONE`)**, **Write Only (`PROT_READ`)**, **Execute Only (`PROT_EXEC`)**, or custom byte spans;
  - Cells under Page-Guard surveillance are highlighted with warm amber gold backgrounds (`QColor(180, 110, 20, 160)`), while the surrounding 4KB page displays a gentle gold tint;
  - Completely eliminates the 4-register limit of hardware debug registers.
- **Raw Binary Export**:
  - In Tab 5 (Memory Regions), right-click any page to export as `.bin`.

---

### 3.4 Quadrant 4: Dedicated 64-Bit Stack View (StackView)
- **QWORD Aligned Layout**:
  - Each row presents an 8-byte QWORD.
  - Highlights current stack top as **`=> RSP`** with relative offsets (`+0x08`, `+0x10`, `[RBP]`).
  - **Smart Double-Click**: Code pointers jump to Disassembly; data pointers jump to Hex Dump.
- **Return Address Detection & Amber Highlighting**:
  - Evaluates stack QWORDs against executable module segments and preceding `CALL` instructions.
  - Automatically identifies return addresses, rendering a bright amber label in the comment column: **`[Return Address] <symbol+offset>`** (e.g. `[Return Address] __libc_start_main+0x80`).
  - Context menu options: `Follow in Disassembly`, `Follow in Dump`, `Copy Address`, and `Copy QWORD Value`.
- **Quick Action Bar**:
  - Click `[RSP]` to return to stack top; click `[RBP]` to jump to base pointer; press **Shift+S** to expand/collapse.

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

### 4.3 Ignore Counts and Log-Only Tracepoints
- **Ignore Count**: Right-click breakpoint -> Set Ignore Count. The debugger ignores the first $N$ hits before breaking.
- **Log-Only Tracepoints**: Check "Log Only" and provide a format string (e.g. `Hit func! RAX={rax}`). Prints to system log without halting execution.

### 4.4 Script-Driven Breakpoint Actions & Silent Hooking
- **Action Configuration**:
  - In Tab 4 **Breakpoint Manager**, select any breakpoint and click **"Edit Script..."** on the toolbar, or right-click -> **"Edit Script Action..."**.
  - Choose between **Python 3** or **Lua 5.4** and enter automation logic.
- **Silent Hooking Protocol**:
  - **Silent Bypass**: If the script finishes and explicitly returns false (`return False` in Python, `return false` in Lua), `edb-next` **does NOT halt the UI or target threads**. Instead, the debugging engine executes a single-step over the breakpoint instruction and resumes full-speed execution automatically! This provides zero-overhead, microsecond-level runtime instrumentation and non-intrusive telemetry without modifying binary source code or recompiling.
  - **Normal Pause**: If the script returns `True` / `true` or does not return a boolean false, the debugger pauses normally and shifts focus to the disassembly view.
  - **Crash-Resistant Guard**: Uncaught script exceptions produce clean tracebacks in the console and log, safely pausing the target without crashing the debugger host.
  - **Project Persistence**: Script actions and language preferences are serialized into the `.edb_db` reverse engineering database.

### 4.5 Memory Page-Guard Breakpoints & Stealth Execution (Anti-Anti-Debugging)
- **Zero-0xCC Stealth Breakpoints**: Defeats binary integrity self-checksumming routines (CRC32/Hash) by guarding code pages with `PROT_READ` rather than writing `0xCC` opcodes. Code integrity scans read authentic instructions without alarm, while execution attempts trigger kernel page-faults caught seamlessly by the debugger.
- **Unlimited Soft Watchpoints**: Overcomes the physical 4-register limitation of CPU DR0~DR3 registers by leveraging virtual memory page protections (`PROT_READ` for write watchpoints, `PROT_NONE` for full access traps).
- **Deployment**:
  - **Hex Dump Context Menu**: Right-click byte cell -> `Breakpoint -> Page-Guard Breakpoint` -> `No Access`, `Write Only`, `Execute Only`, or custom byte range.
  - **CommandBar CLI**: `pageguard <addr> [size] [none|ro|xo]` (alias `guard`), `unpageguard <addr>` (`unguard`), `pageguards` (`guards`).
- **Visual Highlighting**: Guarded addresses are highlighted in warm amber gold (`QColor(180, 110, 20, 160)`), with subtle golden backgrounds for the rest of the 4KB page.
- **Sub-Microsecond False-Positive Bypass**: If the target accesses an unrelated variable on the same 4KB page, `edb-next`'s kernel-level state machine temporarily lifts protection, single-steps 1 instruction (`PTRACE_SINGLESTEP`), re-applies protection, and resumes target execution automatically in microseconds with 0 UI stutter.

### 4.6 POSIX Signal Management
- Configure signals 1-64 under `Options -> Preferences -> Signals`.
- Use **Shift+F7/F8/F9** to pass signals directly to the target's signal handlers.

### 4.7 Dynamic Branch Prediction
- Bottom bar indicates whether the current branch will be followed:
  - **`[JUMP TAKEN]`** (Green)
  - **`[JUMP NOT TAKEN]`** (Gray)

### 4.8 Hit Trace & Run Trace
- **Hit Trace**: Real-time code coverage tracking in disassembly.
- **Run Trace**: History recorder enabling step back (**`< Step Back`**) and step forward (**`Step Forward >`**) time-travel inspection.

### 4.9 CPU Machine State Snapshot (StateDumper)
- Press **Ctrl+D** or menu `Debug -> Dump CPU State` to export registers, stack memory, and disassembly context to log and clipboard.

### 4.10 Automated Shared Library Interception & Pending Breakpoints

When debugging modern software with modular plugin architectures, dynamically unpacked payloads, or delayed `dlopen()` invocations, traditional debuggers cannot resolve symbols beforehand and often require manual re-enumeration after modules load. `edb-next` addresses this through the Linux glibc `_r_debug` Rendezvous protocol:

#### 1. Pending Breakpoints (`bpp <symbol>`)
When a plugin or shared library has not yet been loaded (e.g. `plugin_calc.so`), its exported functions (`plugin_calc_magic`) do not yet have a valid memory address:
- In the bottom CommandBar, execute:
  ```text
  bpp plugin_calc_magic
  ```
  (or confirm conversion when `bp <symbol>` reports an unresolved symbol);
- The **Breakpoints** drawer (Tab 5) highlights the entry with a distinctive cyan `[Pending]` tag and amber status;
- Once the target invokes `dlopen()`, `edb-next` traps the rendezvous event, resolves the relocations immediately, and seamlessly upgrades the pending breakpoint into an active physical breakpoint;
- Execution halts precisely on the entry of the newly loaded library function!

#### 2. Module Load Catching (`catch load` / `catch dlopen`)
- By default, the engine steps over the internal rendezvous trap silently in microseconds without UI pauses;
- If you need to inspect constructor initialization (`.init` / `.init_array`):
  ```text
  catch load       # Toggle pause on shared library mapping
  catch dlopen     # Alias for catch load
  ```

#### 3. Querying Loaded Modules (`modules` / `libs` / `solist`)
- Run `modules`, `libs`, or `solist` in CommandBar to print a formatted table of all active shared libraries, base addresses, paths, and dynamic headers.

---

### 4.11 Follow-Fork Mode & Multi-Process Inferiors

When analyzing multi-process architectures (such as Nginx master/worker patterns, distributed servers, or CTF Pwn sandbox escape challenges), targets frequently spawn child processes via `fork()` or `vfork()`. `edb-next` provides complete follow-fork control and multi-inferior workspace management:

#### 1. Configuring Follow-Fork Mode (`follow-fork [parent|child|both]`)
Inspect and toggle follow-fork behavior at runtime in the bottom CommandBar:
```text
show follow-fork-mode          # Query active follow-fork mode (Parent / Child / Both)
follow-fork parent             # Default mode: continue tracking parent, detach child freely
follow-fork child              # Switch mode: detach parent, refocus session on new child
follow-fork both               # Dual mode: keep parent session and spawn dedicated child session
set follow-fork-mode <mode>    # GDB-style alias
```

#### 2. Catching Fork Events (`catch fork` / `catch vfork`)
To pause execution precisely when `fork()` returns in the parent:
```text
catch fork         # Toggle catch on fork events
catch vfork        # Toggle catch on vfork events
```
When `fork()` is called, the parent halts immediately at the syscall return boundary, reporting:
```text
[Fork Event] Process 12345 forked child 12347 (mode: Parent)
```
Inspect registers, stack arguments, or set breakpoints in the newly created child before pressing **F9** (`run`) to resume.

#### 3. Listing & Switching Multi-Process Inferiors (`inferiors` / `inferior <id|pid>`)
In `follow-fork both` mode, `SessionManager` automatically spawns a dedicated child workspace tab (e.g. `Child [PID: 12347]`) with its own registers, disassembly, hex dumps, and call stack.
- **List all managed inferiors**:
  ```text
  inferiors          # Print session ID, PID, state, and target path
  processes          # Alias for inferiors
  ```
  Sample output:
  ```text
  === Active Debug Sessions (Inferiors) ===
    ID: 1 | PID: 12345 | Name: nginx_master | State: Paused | Path: /usr/sbin/nginx
  * ID: 2 | PID: 12347 | Name: Child [PID: 12347] | State: Running | Path: /usr/sbin/nginx
  ```
- **Switch active workspace focus**:
  ```text
  inferior 1         # Switch to parent session by ID
  inferior 12347     # Switch to child session directly by PID
  process 12347      # Alias for inferior
  ```
  The workspace tabs, registers, and bottom CommandBar immediately align to the chosen process!

---

### 4.12 Independent Thread Freeze & Thaw (Freeze / Thaw & Isolated Stepping)

When debugging multithreaded Linux applications (such as high-throughput networking daemons or multithreaded obfuscated packers), stepping through code in one thread often causes other background worker threads to continue executing concurrently, triggering unrelated breakpoints or modifying shared state.

`edb-next` introduces thread freeze/thaw control and isolated single-stepping:

#### 1. ThreadsView Indicators & Controls (Tab 10)
Switch to the **Threads** tab in the bottom drawer (Tab 10):
- **8-Column Detail View**: Displays TID, Thread Name, State, **Frozen status**, Current RIP, Function Symbol, RSP, and Active focus marker.
- **Ice-Blue Badge Highlighting**: Frozen threads are highlighted with a prominent ice-blue `❄ FROZEN` badge and row highlight.
- **Toolbar Quick Actions**:
  - **`❄ Freeze / Thaw`**: Toggles frozen state of the currently selected thread.
  - **`❄ Freeze Others`**: Freezes all threads except the currently active debug focus thread.
  - **`🔥 Thaw All`**: Resumes all threads to normal concurrent execution.
- **Context Menu**: Right-click any thread entry to freeze or thaw individual threads or all background threads.

#### 2. Isolated Single-Stepping
- **Workflow**:
  1. Click **`❄ Freeze Others`** in the Threads view (or run `freeze all` in the CommandBar).
  2. All background threads are locked via operating system signal masking and engine event filtering.
  3. Press **F7** (Step Into) or **F8** (Step Over). The engine drives only the focused thread forward while all other threads remain frozen.
  4. Once analysis is complete, click **`🔥 Thaw All`** (or run `thaw all`).

#### 3. CommandBar CLI Commands
```text
threads            # List all lightweight threads with RIP, symbol, and [FROZEN] badge
thread <tid>       # Switch active thread focus directly by TID
freeze <tid>       # Freeze specific thread
freeze all         # Freeze all threads except active focus thread
thaw <tid>         # Thaw specific thread
thaw all           # Thaw all threads
```

---

### 4.13 Differential Memory Scanner (CheatEngine-Style Convergence)

When conducting vulnerability research, game reverse engineering, or analyzing malware packers, critical variables (such as dynamic session tokens, encryption keys, player health/currency, or unpacked payload buffers) reside dynamically on heap allocations or uninitialized data sections. Static byte pattern searches are insufficient when hunting for live changing data.

`edb-next` incorporates a native, CheatEngine-grade multi-pass differential memory scanner (Tab 21: "Memory Scanner"):

#### 1. Scanner Panel Overview (Tab 21)
Open the **Memory Scanner** tab in the bottom drawer (Tab 21):
- **Scan Controls**:
  - **Value**: Enter integer (`100`, `0x1337`), float (`3.1415`), double, string, or hex bytes (`48 89 ?? 55`).
  - **Delta (+/-)**: Input value difference for `Increased By...` or `Decreased By...`.
  - **Data Type**: `Int32 (4 Bytes)`, `Int64 (8 Bytes)`, `Int16 (2 Bytes)`, `Int8 (1 Byte)`, `Float (Single)`, `Double`, `String (Text)`, `Hex Bytes (ByteArray)`.
  - **Scan Type**:
    - `Exact Value`: Match input value.
    - `Increased Value` (`>`): Value grew compared to previous pass.
    - `Decreased Value` (`<`): Value shrank compared to previous pass.
    - `Changed Value` (`!=`): Any change.
    - `Unchanged Value` (`==`): Exactly identical.
    - `Increased By...`: Value increased by specified Delta.
    - `Decreased By...`: Value decreased by specified Delta.
    - `Unknown Initial Value`: Capture baseline snapshot of all memory.
  - **Writable Memory Only**: Checked by default; scans `rw-p` sections (heap, stack, data), completing in tens of milliseconds.
  - **Alignment**: 4 bytes (default), 1 byte, 2 bytes, or 8 bytes.

#### 2. Multi-Pass Differential Convergence Workflow
- **Pass 1 (First Scan)**:
  1. Input current observed value (e.g. `100`);
  2. Click **`🔍 First Scan`** (or CommandBar: `scan 100 int32`);
  3. Establishes thousands of baseline candidates.
- **Pass 2 (Next Scan)**:
  1. Resume target program (F9) to let the value change (e.g. taken damage down to `85`, or simply "decreased");
  2. Pause execution; select `Decreased Value` (or input `85` directly);
  3. Click **`⚡ Next Scan`** (or CommandBar: `nextscan <` / `nextscan 85`);
  4. Candidate count drops dramatically to dozens.
- **Pass 3 (Pinpoint Target)**:
  1. Repeat another modification and click `⚡ Next Scan`;
  2. Candidate list converges cleanly to the exact target physical address!

#### 3. Candidate Interactions & In-Place Memory Editing
- **Delta Color Coding**: Increased values highlighted in light green (`+15`), decreased values in light red (`-15`).
- **Hex Dump Sync**: Double-click any row to jump directly to that address in the Hex Dump.
- **Context Menu**:
  - `Follow in Hex Dump` / `Follow in Disassembly`
  - `Copy Address`
  - **`Edit / Write Value...`**: Pop up an input dialog to write new values directly to the target process memory (e.g. freeze or lock value), refreshing live candidates immediately.

#### 4. CommandBar CLI Commands
```text
scan <value|unknown> [type]   # Initiate first pass (e.g. scan 100 int32, scan 0x1337 int64, scan "admin" str)
nextscan <compare> [val]      # Next differential pass (e.g. nextscan >, nextscan <, nextscan ==, nextscan 105)
scanresults [limit]           # Print top candidate addresses from current scan
scanreset                     # Reset scanner and clear candidate list
```

---

### 4.14 Compound Type Reconstruction & Struct Layout (Type Viewer)

In reverse engineering, protocol parsing, and firmware analysis, data in memory is structured as complex C composite types (`struct`) rather than flat byte sequences. Raw hexadecimal dumps make tracking member boundaries and padding alignment tedious.

`edb-next` introduces a CheatEngine-style struct layout inspector and compound type reconstruction workbench (**Tab 22: "Type Viewer"**):

#### 1. Type Viewer Interface (Tab 22)
Switch to the bottom drawer **Type Viewer** (Tab 22):
- **Struct Combo**: Select from pre-loaded system definitions (`timespec`, `timeval`, `sockaddr_in`, `list_head`, `io_vec`) or custom structs.
- **`➕ Define Struct...` Button**: Opens a dialog to paste standard C struct definitions and parse them immediately.
- **Address Edit**: Target base address expression supporting registers and math (e.g. `rsp`, `rbp - 0x40`, `0x7fffffffd7d0`).
- **`🔬 Inspect` Button**: Evaluates live target memory using the selected struct definition and populates all fields.
- **`🔄 Refresh` Button**: Re-reads and updates fields as the target steps or executes.
- **Size / Alignment Label**: Displays total byte size and maximum member alignment (e.g. `Size: 56 bytes (align 8)`).

#### 2. Field Table & Pointer Dereference Navigation
- **`Offset`**: Hexadecimal offset relative to struct base (`+0x000`, `+0x008`, `+0x010`), clearly displaying System V AMD64 ABI padding.
- **`Field Name` / `Type` / `Size`**: Variable identifier, data type (including arrays like `char[16]`), and physical size.
- **`Raw Hex`**: Hexadecimal memory bytes in little-endian format.
- **`Value / Dereference`**:
  - Integers formatted in dual decimal and hexadecimal.
  - Characters displayed with ASCII glyphs (`42 ('*')`).
  - Character arrays formatted as quoted strings (`"PlayerOne"`).
  - **Pointer Dereferencing**: Pointer fields are highlighted in cyan with an underline (e.g. `0x00007fffffffe100`). **Double-clicking** a pointer cell instantly navigates to that address:
    - Code segment targets jump in the Disassembly view.
    - Data/stack/heap targets jump in the MultiDump Hex Dump.
- **Right-Click Context Menu**:
  - **`Edit Field Value...`**: Pop up an editor to modify the field's memory value directly in the live process.
  - `Follow in Hex Dump` / `Follow in Disassembly`.
  - `Copy Field Value`: Copies formatted string to clipboard.

#### 3. Defining Custom C Structs
Click `➕ Define Struct...` or run `defstruct` in the CommandBar:
```c
struct PlayerState {
    char id;
    short level;
    int health;
    long score;
    void* pTarget;
    float moveSpeed;
    double mana;
    char heroName[16];
};
```
The internal parser automatically calculates natural alignment padding (e.g. `short` at offset 2, `int` at offset 4, `long`/pointer at offset 8), ensuring exact correspondence with GCC/Clang compiled binaries.

#### 4. CommandBar CLI Commands
```text
structs                       # List all registered struct types, sizes, and field counts
struct <name> <addr_or_expr>  # Parse and print struct fields at given memory address
defstruct <c_code...>         # Dynamically define and register a new C struct
```

---

### 4.15 Native C Pseudo-Code Decompiler (`F5` & Bidirectional Mapping)

Disassembly views are precise but can be overwhelming for large nested control flow graphs. `edb-next` includes an embedded C++23 AST decompiler:

#### 1. Invoking & Refreshing the Decompiler
- While navigating any function in Disassembly, press **`F5`** (or select **Analysis -> Decompile Function (F5)**);
- The **DecompilerView** opens instantly, restructuring discrete assembly instructions into legible C pseudo-code (`while`, `for`, `if-else`, and expression trees).

#### 2. Bidirectional Mapping & Focus Locking
- **Pseudo-Code to Assembly**: Click any pseudo-code line to highlight and focus the corresponding assembly instruction range in Quadrant 1;
- **Assembly to Pseudo-Code**: Single-stepping (`F7` / `F8`) in Disassembly automatically locks the active high-level statement row with an emerald green focus border.

---

### 4.16 Time-Travel Debugging (TTD) & Step Back Replay (`Ctrl+F7` Step Back)

Conventional debugging is strictly unidirectional. If you step past a critical conditional branch or variable write, you must restart from scratch. `edb-next` provides deterministic time-travel debugging:

#### 1. Rewind and Reverse Execution
- **Step Back**: Press **`Ctrl+F7`** to rewind execution by one instruction cycle, instantly restoring the previous CPU register state and memory diffs (`MemoryDeltaDiff`);
- **Reverse Continue**: Press **`Ctrl+Shift+F9`** to execute backwards at full speed until hitting a preceding breakpoint or reaching the session start.

#### 2. Time-Travel Scrubber Widget (`TimeTravelWidget`)
- Located at the bottom of Quadrant 2 (Registers);
- Drag the slider to scrub through recorded execution frames;
- Integrates with Linux `perf_event_open` hardware branch tracing for deterministic replay fidelity.

---

### 4.17 Headless DAP (Debug Adapter Protocol) Server & VS Code / Neovim Integration

In addition to its standalone GUI, `edb-next` can run as a headless DAP server to power modern developer environments:

#### 1. Launching the Headless DAP Daemon
Launch edb-next with the `--dap` flag:
```bash
# Start DAP server on default port 4711
./build/edb_next --dap

# Or specify a custom port
./build/edb_next --dap --dap-port 5555
```

#### 2. VS Code Configuration (`launch.json`)
Configure your `.vscode/launch.json` client:
```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "edb-next DAP Attach",
            "type": "edb-next",
            "request": "attach",
            "port": 4711,
            "program": "${workspaceFolder}/target_binary"
        }
    ]
}
```
VS Code can now set breakpoints, step through code, and inspect variables using the edb-next reactive Linux kernel engine.

---

### 4.18 BTF (BPF Type Format) Kernel & ELF Type Import (Type Viewer)

When analyzing stripped binaries or programs interacting with the Linux kernel without DWARF debug symbols, BTF provides compact, high-density type definitions:

#### 1. Importing Linux Kernel vmlinux BTF
1. Open the bottom drawer **Type Viewer** (Tab 22);
2. Click **"📦 Import BTF..."** in the toolbar;
3. Select **"Load Kernel BTF (/sys/kernel/btf/vmlinux)"**;
4. The parser decodes over 170,000 kernel types (such as `task_struct`, `sk_buff`, `files_struct`) in microseconds;
5. Select any type from the Struct combo to inspect target memory with accurate field layouts.

#### 2. Importing ELF `.BTF` Sections
If the target binary contains an embedded `.BTF` section, select **"Load ELF .BTF Section"** to automatically extract its custom structures and bitfield definitions.

---

### 4.19 Advanced Anti-Anti-Debugging Configuration

Malware and protected binaries often inspect `/proc/[pid]/status` for `TracerPid` or measure single-step execution delays using `RDTSC`:

#### 1. Configuring Anti-Anti-Debugging
- Select **Debug -> 🛡️ Anti-Anti-Debugging...** from the main menu;
- **TracerPid Spoofing**: Check "Spoof TracerPid as 0 in /proc/status" to sanitize tracer detection;
- **RDTSC Smoothing**: Check "Smooth RDTSC Cycle Differences" to mask single-step delays;
- **Self-Termination Interception**: Check "Intercept PTRACE_TRACEME & PR_SET_DUMPABLE" to neutralize self-integrity traps.

---

## 5. Advanced Reverse Engineering Toolset

- **Heap Analyzer (Tab 9)**: Traverses glibc `malloc_chunk` structures, parsing chunk size, flags (`A|M|P`), and allocated/free state.
- **ROP Scanner (Tab 11)**: Detects gadgets ending in `ret`, `syscall`, `int 0x80`; exports Python `p64(...)` exploits.
- **Control Flow Graph (Tab 14)**: Hierarchical basic-block directed graph with color-coded branch edges.
- **Intermodular Calls (Tab 18)**: Identifies external library API calls (PLT/GOT) with fuzzy search.
- **Opcode Searcher (Tab 19)**: Scans memory for preset sequences (`JMP reg`, `Syscall`) or custom regex.
- **Binary Info (Tab 17)**: ELF headers, segments, sections, `DT_NEEDED` dependencies, and a dedicated **Loaded Shared Libraries (`_r_debug`)** 5th tab tracking the live dynamic linker `link_map` with double-click jumps to Disassembly and Hex Dump.
- **Process Properties (Tab 8)**: `/proc/<pid>/fd/` classification into Sockets, Pipes, and PTYs.
- **Symbol Viewer with C++ Demangling (Tab 6 / Alt+E)**: Full global symbol browser automatically resolving GCC/Clang mangled identifiers into clean C++ signatures via `<cxxabi.h>`, with raw symbol tooltips, bidirectional name filtering, and double-click disassembly jump.
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

- **Persistence Scope**: Instruction comments (`;`), custom user labels (`:` with `🏷` badge, globally resolved across the session), bookmarks (`Ctrl+B`), breakpoints (with conditions), watch expressions, patches, and scratch notes.
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
| `step` / `stepo` / `ret` / `run` | Step into (`s`, `sti`) / Step over (`so`, `sto`) / Step out (`rtr`, `rto`) / Run (`g`) |
| `origin [addr]` / `rip` | Center disassembly on current RIP (`origin` / `rip`), or set RIP if address given |
| `setrip <addr>` | Explicitly set target instruction pointer (`setrip 0x401000`) |
| `lbl [addr] [name]` / `label` | List all user labels, inspect, set, or remove (`-`/`del`) custom labels |
| `eval <expr>` | Evaluate expression. Ex: `eval rax + 0x20`, `eval [rbp-8]` |
| `mprotect <addr> <size> <prot>` | Change target page protections (7=RWX). Ex: `mprotect 0x555555555000 4096 7` |
| `alloc <size> [prot]` | Allocate target memory page. Ex: `alloc 4096 7` |
| `free <addr> <size>` | Free dynamically allocated target page. Ex: `free 0x7ffff7fbc000 4096` |
| `pageguard <addr> [sz] [type]` | Set Page-Guard memory protection breakpoint (`guard`). Ex: `guard 0x401000 8 ro` |
| `unpageguard <addr>` | Remove Page-Guard breakpoint (`unguard`). Ex: `unguard 0x401000` |
| `pageguards` | List all active Page-Guard breakpoints (`guards`) |
| `modules` / `libs` / `solist` | Print all currently loaded shared libraries, base addresses, and paths (`libs`) |
| `catch load` / `catch dlopen` | Toggle execution halt upon shared library load/unload (`catch load`) |
| `bpp <symbol>` | Set a deferred Pending Breakpoint that binds automatically upon module load |
| `follow-fork [mode]` | Query or set follow-fork mode (`parent` / `child` / `both`). Ex: `follow-fork both` |
| `set follow-fork-mode <mode>` | GDB-style alias to set follow-fork mode |
| `show follow-fork-mode` | Print currently active follow-fork mode |
| `catch fork` / `catch vfork` | Toggle breakpoint halt upon target process `fork()` |
| `threads` | List all threads with RIP, symbol, and freeze status |
| `thread <tid>` | Switch active thread focus to specified TID |
| `freeze <tid|all>` | Freeze specific thread or all non-focus threads |
| `thaw <tid|all>` | Thaw specific thread or all threads |
| `scan <val> [type]` | Initiate first memory scan pass (CheatEngine style). Ex: `scan 100 int32`, `scan unknown` |
| `nextscan <cmp> [val]` | Execute next differential scan pass. Ex: `nextscan >`, `nextscan <`, `nextscan ==`, `nextscan 105` |
| `scanresults [limit]` | Print top candidate addresses from current scan |
| `scanreset` | Reset memory scanner and clear candidate list |
| `structs` | List all registered struct templates, sizes, and field counts |
| `struct <name> <addr>` | Parse and print formatted struct fields at target memory address |
| `defstruct <c_code...>` | Dynamically define and register a new C struct declaration |
| `inferiors` / `processes` | List all active debug sessions and PIDs (`inferiors`) |
| `inferior <id\|pid>` | Switch active debugging session and workspace tab (`inferior 2`, `inferior 12347`) |
| `process <id\|pid>` | Switch active session (alias for `inferior`) |
| `dumpstate` | Format and copy full CPU state snapshot to clipboard |
| `py <code...>` | Directly evaluate Python 3 statement or expression. Ex: `py print(hex(edb.get_reg('rip')))` |
| `lua <code...>` | Directly evaluate Lua 5.4 statement or expression. Ex: `lua print(string.format('0x%x', edb.get_reg('rip')))` |
| `help` | Print list of all registered built-in and plugin CLI commands |

---

## 9. DWARF Source-Level Debugging Guide

`edb-next` provides native, high-performance integration with DWARF debug info (`libdw`), bridging raw assembly instructions with original C/C++ source code.

### 9.1 Compiler Options & DWARF Extraction
When building binaries with source access, compile with `-g` or `-g3`:
```bash
gcc -g -O0 -no-pie target.c -o target
```
Upon launching or attaching to the target, `core/DwarfParser` extracts `.debug_info`, `.debug_line`, and `.debug_str` sections, building a fast lookup cache for bidirectional `Address <-> (File:Line:Column)` mapping.

### 9.2 Mixed ASM/Source View (`Ctrl+Shift+S`)
By default, the `DisassemblyView` displays pure machine disassembly.
- Press **Ctrl+Shift+S** or right-click -> **"Toggle Mixed Source/ASM"** to enable mixed mode.
- The disassembly engine groups instructions by their corresponding source line and renders a sleek, dark-green banner above each basic block showing the source file, line number, and original code line (e.g. `target.c:18: if (n <= 1) return n;`).
- This dramatically accelerates vulnerability audits by correlating raw assembly with high-level program logic.

### 9.3 Standalone Source Browser (`Alt+S`)
The central code area features a dedicated **Source View** tab:
1. **Focus**: Press **Alt+S** to switch focus directly to the source browser.
2. **File Selection**: The top combo box lists all C/C++ source files participating in the compilation unit.
3. **Line Indicators**:
   - **Instruction Pointer**: The active CPU `RIP` is highlighted with an emerald arrow (`➔`) in the margin.
   - **Breakpoints**: Lines with active breakpoints show a bright red dot (`●`).
4. **Source Breakpoints**: Double-click any line number in the margin to toggle a breakpoint. `edb-next` automatically translates the line to the corresponding instruction start address in memory.

### 9.4 Source-Level Stepping
- **Source Step Over**: Executes instructions continuously until `RIP` exits the address range covered by the current source line.
- **Source Step Into**: When encountering a source line containing a function call, steps into the first source line of the called function.

---

## 10. Embedded Scripting Automation Guide (Python 3 & Lua 5.4)

`edb-next` features an industry-leading dual-engine embedded scripting architecture unified under `IScriptEngine` and `ScriptEngineManager`.

### 10.1 Dual-Engine Architecture Rationale
- **Python 3 Engine**: Geared towards complex vulnerability research and exploit generation, integrating seamlessly with the rich security ecosystem (`pwntools`, `z3`, `scapy`, `requests`, etc.).
- **Lua 5.4 Engine**: Zero-overhead, microsecond-latency condition evaluation, perfectly suited for high-frequency breakpoint hooks executed thousands of times per second.

### 10.2 Interactive Script Console (`Alt+P`)
Press **Alt+P** to open the dedicated **Script Console** tab in the bottom drawer:
- **Language Switcher**: Toggle dynamically between `Python 3` and `Lua 5.4`.
- **History Navigation**: Use **Up / Down** arrow keys to recall previous commands.
- **Run External Script Files (`▶ Run File...`)**: One-click execution of `.py` or `.lua` files from disk.
- **Dark Geek Styling**: Monospace font with cyan prompts, soft-white outputs, and red exception tracebacks.

### 10.3 Python 3 Automation & `edb` API Reference
The embedded Python 3 runtime exports the built-in `edb` module:

| Python API | Return Type | Description |
| :--- | :--- | :--- |
| `edb.get_regs()` | `dict` | Retrieve all register values as a dictionary |
| `edb.get_reg(name)` | `int \| None` | Get register value (case-insensitive, e.g. `'rip'`, `'rax'`) |
| `edb.set_reg(name, val)` | `bool` | Set register value |
| `edb.read_memory(addr, size)` | `bytes` | Read raw bytes from target virtual memory |
| `edb.write_memory(addr, data)`| `bool` | Write Python `bytes` to target virtual memory |
| `edb.set_breakpoint(addr, sym="")` | `bool` | Set software breakpoint at address |
| `edb.remove_breakpoint(addr)` | `bool` | Remove breakpoint at address |
| `edb.step_into()` / `edb.step_over()` | `None` | Step into / Step over single instruction |
| `edb.step_source()` | `None` | Step over one source line |
| `edb.resume()` / `edb.pause()` | `None` | Resume / Interrupt target execution |
| `edb.resolve_symbol(name)` | `int \| None` | Resolve symbol name to address |
| `edb.eval(expr)` | `int \| None` | Evaluate debug expression (e.g. `"rax + 0x20"`) |
| `edb.pid()` / `edb.tid()` | `int` | Get target PID / active TID |
| `edb.state()` | `str` | Get session state (`"Running"`, `"Paused"`, `"Stopped"`) |
| `edb.log(msg)` | `None` | Write message to host debug log |

#### Python Script Example: Automated Buffer Decryption
```python
import edb

rip = edb.get_reg("rip")
print(f"[*] Current RIP: {hex(rip)}")

buf_addr = edb.resolve_symbol("secret_buffer")
if buf_addr:
    raw = edb.read_memory(buf_addr, 32)
    decrypted = bytes([b ^ 0x5A for b in raw])
    print(f"[+] Decrypted: {decrypted}")
```

### 10.4 Lua 5.4 Fast Condition Hooks & API Reference
In Lua 5.4, the global `edb` table provides symmetric APIs, and `print()` output is captured directly into the console:

```lua
local rip = edb.get_reg("rip")
local rax = edb.get_reg("rax")
print(string.format("[Lua Hook] RIP=0x%x, RAX=0x%x", rip, rax))

if rax > 1000 then
    print("[Lua] Threshold hit! Hooking return address...")
    local rsp = edb.get_reg("rsp")
    local ret_bytes = edb.read_memory(rsp, 8)
end
```

### 10.5 Running External Script Files & Automated Unpacking
1. Write a script `unpack.py`:
   ```python
   import edb, time
   oep = edb.resolve_symbol("main") or 0x401000
   edb.set_breakpoint(oep, "OEP")
   edb.resume()
   while edb.state() == "Running":
       time.sleep(0.01)
   print("[+] Reached OEP! Dumping unpacked text section...")
   payload = edb.read_memory(0x400000, 0x10000)
   with open("/tmp/dumped_payload.bin", "wb") as f:
       f.write(payload)
   print("[+] Dump completed successfully!")
   ```
2. Click **▶ Run File...** in Script Console and select `unpack.py` for unattended unpacking and payload dumping.

### 10.6 Inline CommandBar Script Evaluation (`py ...` / `lua ...`)
Evaluate one-liners directly from the bottom CommandBar without switching tabs:
- Python: `py print("Hex RAX:", hex(edb.get_reg('rax')))`
- Lua: `lua print('PID is: ' .. edb.pid())`

### 10.7 Script-Driven Breakpoint Actions & Silent Hooking Hands-on

When reversing obfuscated binaries, malware, or complex multithreaded network daemons, setting interactive breakpoints frequently interrupts time-sensitive communication or anti-analysis watchdogs.

By combining Breakpoint Manager **Script Actions** with the `return False` (Python) / `return false` (Lua) protocol, `edb-next` transforms breakpoints into non-intrusive microsecond-level runtime hooks (dynamic probes):

#### Scenario 1: Python Buffer Decryption & Silent Telemetry
Set a breakpoint at the entry point of a cryptographic function (e.g. `decrypt_payload`, where `RDI` holds the buffer address and `RSI` holds the byte length). In Breakpoint Manager, bind the following Python script:
```python
import edb

buf_addr = edb.get_reg("rdi")
length = min(edb.get_reg("rsi") or 0, 64)

if buf_addr and length > 0:
    data = edb.read_memory(buf_addr, length)
    edb.log(f"[Crypto Hook] Decrypted payload preview: {data.hex()}")

# Returning False bypasses the pause: single-steps over original opcode and resumes at full speed!
return False
```

#### Scenario 2: Lua High-Frequency Dynamic Patching
Set a breakpoint on a frequently invoked authentication or license check routine, and bind this ultra-fast Lua script:
```lua
-- Microsecond-level zero-overhead Lua execution
local uid = edb.get_reg("rdi")

if uid ~= 0 then
    -- Patch UID argument dynamically to 0 (root) in registers
    edb.set_reg("rdi", 0)
    edb.log(string.format("[Lua Patch] Altered UID %d to 0 (root)!", uid))
end

-- Returning false ensures silent execution without halting the UI or threads
return false
```
If manual inspection is required only when an anomaly occurs (e.g. `uid == 1000`), simply omit `return false` or `return true`, and `edb-next` will pause execution cleanly and navigate the disassembly view to the target.

---

## 11. Modern C++20 Plugin Development Guide

Plugins are compiled as standard Linux shared objects (`.so`) using the `IPlugin` and `IPluginContext` contracts.

### 11.1 Writing a Plugin Header (`MyPlugin.hpp`)
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

### 11.2 Implementing Plugin Logic (`MyPlugin.cpp`)
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

### 11.3 CMake Build Definition (`CMakeLists.txt`)
```cmake
cmake_minimum_required(VERSION 3.16)
project(MyPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

set(EDB_NEXT_ROOT "/path/to/edb-next")
include_directories(${EDB_NEXT_ROOT} ${EDB_NEXT_ROOT}/core ${EDB_NEXT_ROOT}/ui)

add_library(my_plugin SHARED MyPlugin.cpp)
set_target_properties(my_plugin PROPERTIES PREFIX "")
target_link_libraries(my_plugin PRIVATE Qt6::Widgets Qt6::Core)
```

### 11.4 Loading and Using the Plugin
- **Auto-Loading**: Copy `my_plugin.so` to `~/.config/edb-next/plugins/`.
- **Manual Loading**: In the GUI, select `Plugins -> Manage Plugins...` and click **Load Plugin...**.
- **Execution**: The action appears under the **Plugins** menu, and `my_ping` is available in the bottom CommandBar.

---

## 12. Keyboard Shortcut Cheat Sheet

| Hotkey / Interaction | Description | Category |
| :--- | :--- | :--- |
| **F9** | Continue Execution | Execution |
| **F7** | Step Into | Execution |
| **F8** | Step Over | Execution |
| **Shift+F11** | Step Out of Function | Execution |
| **F4** | Run to Selection | Execution |
| **Ctrl+F2** | Restart Session | Execution |
| **\*** *(Numpad / Key)* | **Origin**: Follow / Center on Current RIP | Disassembly |
| **Ctrl+\*** | Set RIP (New Origin) | Execution |
| **F2** | Toggle Software Breakpoint | Breakpoints |
| **Enter** | Follow Branch | Disassembly |
| **Esc / Backspace** | Go Back in History (Disasm & Dump) | Navigation |
| **Alt+Left / Alt+Right** | Navigate History Back / Forward (Disasm & Dump) | Navigation |
| **Register Table: + / -** | Increment / Decrement Register (+1 / -1) | Registers |
| **Register Table: 0 / ~** | Instant Zero Out / Bitwise Invert (~val) | Registers |
| **Register Table: Enter** | Edit Register Value Modal | Registers |
| **Right-Click GPR -> Follow in Stack** | Navigate Register Stack Pointer to Quadrant 4 | Linking |
| **Right-Click GPR -> Copy As...** | Copy As Hex, Decimal, or Dereferenced String/Bytes | Registers |
| **Stack View: Enter** | Smart Follow (Code to Disasm, Data to Dump) | Stack Analysis |
| **Stack View: Space** | Modify QWORD Value In-Place | Stack Analysis |
| **Stack View: Ctrl+G** | Go to Stack Virtual Address | Stack Navigation |
| **Hex Dump: Enter / Dbl-Click** | Modify Bytes In-Place (prefilled current byte, supports spaced/compact hex) | Memory Patching |
| **Right-Click -> Follow in Dump 1~4** | Route Address / Value to Target Dump Tab (Dump 1 ~ Dump 4) | View Linking |
| **Right-Click Disasm -> Disable / Enable BP** | Temporarily suspend or resume breakpoint without deletion | Breakpoints |
| **Right-Click Dump -> Follow QWORD** | Follow QWORD in Dump / Disassembly / Stack | Memory Analysis |
| **Right-Click Dump -> View as Struct** | Instant Struct Layout Decoding in Tab 22 Type Viewer | Struct Analysis |
| **Right-Click Stack -> Follow in Disasm** | Follow `[Return Address]` to Call Site | Stack Analysis |
| **Space** | Continuous Assemble (Auto-advancing with NOP fill) | Patching |
| **; (Semicolon)** | Add / Edit Comment | Reverse Engineering |
| **: (Colon)** | Set / Edit User Label (`🏷`) | Reverse Engineering |
| **Ctrl+B** | Toggle Bookmark (`★`) | Reverse Engineering |
| **Ctrl+Alt+S** | Search for All Referenced Strings | Reverse Engineering |
| **Ctrl+Alt+C** | Search for All Intermodular Calls | Reverse Engineering |
| **X** | Show Cross References (XREFs) | Reverse Engineering |
| **Ctrl+E** | Modify Hex Bytes | Memory Patching |
| **Ctrl+P** | Patch Manager & Disk Export | Patching & Unpacking |
| **Ctrl+S** | Save Project Database | Project Persistence |
| **Ctrl+D** | Dump CPU State Snapshot | State Export |
| **Shift+S** | Toggle Stack View | Layout |
| **Alt+C** | Focus CPU Disassembly | View Switch |
| **Alt+S** | Focus Source View (SourceView) | View Switch |
| **Ctrl+Shift+S** | Toggle Mixed ASM/Source View | View Switch |
| **Alt+P** | Focus Script Console (Python/Lua) | Automation |
| **Alt+D** | Switch to Memory Hex Dump | View Switch |
| **Alt+K** | Switch to Call Stack Drawer | View Switch |
| **Alt+B** | Switch to Breakpoints Drawer | View Switch |
| **Alt+M** | Switch to Memory Regions Drawer | View Switch |
| **Alt+E** | Switch to Symbol Viewer | View Switch |
| **Alt+L** | Switch to Debug System Log | View Switch |
| **Shift+F7/F8/F9** | Pass Signal Step / Run | Signal Handling |

---

## 13. Conclusion

This guide outlines the end-to-end operational workflows in `edb-next`, from source compilation and 4-quadrant dynamic debugging to DWARF source mapping, dual-engine Python/Lua scripting automation, in-target remote syscall injection, physical ELF patching to disk, and C++20 plugin extension.
