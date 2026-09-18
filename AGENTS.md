# AGENTS.md - Workspace Rules & Developer Guidelines for edb-next

This document defines the mandatory engineering workflows, coding standards, and architectural conventions for AI agents and developers working on the `edb-next` codebase. All instructions herein must be followed by default without requiring repeated user prompts.

---

## 1. Automated Workflow Policies (Mandatory by Default)

### 1.1 Automatic Git Commit & Documentation Synchronization
* **Automatic Execution**: Whenever a feature is developed, a bug is fixed, or any functional/UI code is modified, the agent **MUST automatically**:
  1. **Update Documentation**: Synchronize both English ([README.md](file:///home/eddy/myplace/project/edb-next/README.md)) and Chinese ([README_zh.md](file:///home/eddy/myplace/project/edb-next/README_zh.md)), as well as any relevant documentation in `doc/`, capturing the newly added capabilities, keyboard shortcuts, UI elements, or API changes.
  2. **Build & Verify**: Compile cleanly with `make -C build -j4` and execute relevant automated test suites (e.g., `test_core`, `test_advanced`, `test_dwarf`, `test_exit`, `test_scripting`).
  3. **Stage & Commit**: Stage all modified and newly created files (`git add ...`) and commit them to Git with structured Conventional Commit messages.
* **No Manual Reminders**: The user should never need to ask to "commit changes" or "update docs". Execute this complete cycle autonomously before reporting task completion.

### 1.2 Conventional Commit Standard
Commit messages must strictly follow the Conventional Commits format:
```text
<type>(<scope>): <concise description in imperative mood>

[optional body explaining rationale and technical details]
```
* **Types**: `feat`, `fix`, `docs`, `refactor`, `test`, `perf`, `ci`, `chore`.
* **Scopes**: `ui`, `core`, `disasm`, `dump`, `stack`, `scanner`, `engine`, `dwarf`, `scripting`, `test`.
* **Example**:
  ```text
  fix(ui): restore call flow lines in disassembly mark column and align with x64dbg aesthetics
  ```

---

## 2. Google C++ Style Guide & Coding Standards

All C++ code in `edb-next` is built against the **C++20** standard and must adhere to the **Google C++ Style Guide** with project-aligned modern idioms.

### 2.1 Naming Conventions
* **Types / Classes / Structs / Enums**: `PascalCase` (e.g., `DisassemblyView`, `InstructionInspector`, `SessionState`).
* **Functions & Methods**: `camelCase` (e.g., `setupUi()`, `drawFlowLines()`, `extractBranchTarget()`).
* **Variables & Parameters**: `camelCase` (e.g., `targetAddr`, `selRow`, `maxVisibleAddr`).
* **Class Member Variables**: `camelCase` with a trailing underscore (e.g., `session_`, `currentInstructions_`, `displayRows_`).
* **Constants & Enums**: Prefix with `k` for constants (`kDefaultMaxRails`, `kRegisterColor`) or `PascalCase` for enum enumerators (`SessionState::Paused`, `MnemonicClass::Call`).
* **Namespaces**: `lower_snake_case` (e.g., `namespace edb_next`).

### 2.2 Modern C++ Idioms & Memory Safety
* **Zero Raw Owning Pointers**: Always use RAII and smart pointers (`std::unique_ptr`, `std::shared_ptr`, `std::weak_ptr`). Never use `malloc`/`free` or naked `new`/`delete` for resource lifecycle.
* **Const-Correctness**: Aggressively mark methods `const`, variables `const` / `constexpr`, and functions `noexcept` where applicable. Use `[[nodiscard]]` on queries that return values that must not be ignored (e.g., `addressAtRow()`, `isRowVisible()`).
* **Modern Standard Library**: Prefer `std::string_view`, `std::optional`, `std::span`, `std::ranges`, and `<algorithm>` over raw C arrays or manual pointer loops.
* **Exception & Error Handling**: Avoid unhandled exceptions. Return `std::optional<T>`, `std::expected<T, E>`, or explicit `bool ok` alongside descriptive error outputs.
* **Header Hygiene**: Use `#pragma once` at the top of every header file. Include headers in logical order: standard library headers (`<memory>`, `<vector>`), third-party/Qt headers (`<QPainter>`, `<QTableWidget>`), and project headers (`"Types.hpp"`).

---

## 3. Architecture & Project-Specific Guidelines

`edb-next` is strictly partitioned into two decoupled layers:

### 3.1 Headless Core (`core/`)
* **Scope**: Linux kernel interaction (`ptrace`, `/proc`), DWARF 4/5 symbol extraction (`libdw`), Capstone disassembly wrappers, memory scanner, ROP searcher, script engine bridges (Python/Lua), and breakpoint state machines.
* **Constraint**: **STRICTLY UI-INDEPENDENT**. Files in `core/` must NEVER include `<QWidget>`, `<QPainter>`, or any UI-related headers. They must remain fully testable and operational in headless CLI and automated testing environments.

### 3.2 Graphical Interface (`ui/`)
* **Scope**: Qt6 GUI views (`MainWindow`, `DisassemblyView`, `MemoryHexView`, `RegistersView`, `StackView`, `SessionTabWidget`, `TypeViewerDialog`, `WatchView`).
* **Ergonomics**: Deeply inspired by **x64dbg**, **OllyDbg**, and **IDA Pro**. Use rich dark aesthetics (One Dark palette), clear visual feedback, micro-animations, and high information density.

### 3.3 Disassembly View & Mark Column Flow Lines Standard
The **Mark Column** (Column 0, width: 75px) is a signature feature of `edb-next`:
1. **Layout Isolation**:
   * Left Area (`0 ~ 26px`): Breakpoint dots (`●`), Current RIP arrow (`➔`), Bookmark star (`★`).
   * Right Area (`28 ~ 72px`): 5 concurrent collision-free vertical rails for control flow lines.
2. **Color Hierarchy**:
   * **Function Calls (`call`, `callq`)**: **Neon Cyan (`#00e5ff`)**.
   * **Unconditional Jumps (`jmp`, `jmpq`)**: **Golden Yellow (`#ffd54f`)**.
   * **Conditional Forward Jumps (`jcc`)**: **Amber Orange (`#ff9800`)**.
   * **Conditional Backward Jumps / Loops (`loop`, backward `jcc`)**: **Coral Red (`#ff5252`)**.
   * **Dynamic RIP Branch Status**: **Emerald Green (`#00e676`)** if branch is taken; **Slate Gray (`#90a4ae`)** if not taken.
3. **Line Continuity & Viewport Filtering**:
   * **Never truncate calls**: All calls must route along full vertical rails.
   * **Out-of-viewport indicators**: Lines to addresses above visible range must connect all the way to the top border (`y = 5`) with an upward arrow (`▲`); lines below must connect to the bottom border (`y = height - 5`) with a downward arrow (`▼`).
   * **Viewport Filtering**: Only lines where at least the source row or target row is visible in the active viewport should be drawn. Completely offscreen jumps must not render phantom lines crossing the screen.
4. **Interactive Highlights**:
   * Selection: 2.4px line width with 5.5px semi-transparent glow aura (Pass 2 rendering).
   * In-view target: Highlighted dashed rounded rectangle bounding the destination cell in the Mark column.
   * Tooltips & Navigation: Mark column must set rich tooltips (`CALL ➔ <target>`), and support `Enter` / double-click branch follow and `Esc` / `Backspace` / `Alt+Left` history return.

---

## 4. Verification & Testing Protocol

Before committing any changes:
1. **Compilation**: Run `make -C build -j4` without warnings.
2. **Headless Execution**: Ensure all test targets pass with `QT_QPA_PLATFORM=offscreen`:
   ```bash
   ./build/test_core
   ./build/test_dwarf
   ./build/test_exit
   ./build/test_scripting
   ./build/test_advanced
   ```
3. **Zero Regression**: Verify that existing features, CI scripts (`.github/workflows/ci.yml`), and AppImage packaging (`scripts/build_appimage.sh`) remain functional.
