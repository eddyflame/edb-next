# AGENTS.md - Developer Guidelines for edb-next

Mandatory instructions for AI agents and developers. Follow by default without user prompts.

---

## 1. Automated Workflow & Commit Rules

### 1.1 Autonomous Execution Cycle
On every feature, bug fix, or code update, the agent **MUST autonomously**:
1. **Update Docs**: Synchronize [README.md](file:///home/eddy/myplace/project/edb-next/README.md) and [README_zh.md](file:///home/eddy/myplace/project/edb-next/README_zh.md) (and `doc/` if applicable).
2. **Build & Test**: Compile (`make -C build -j4`) and run all test suites (`test_core`, `test_advanced`, `test_dwarf`, `test_exit`, `test_scripting`).
3. **Commit**: Stage and commit changes. Never ask user permission to commit or update docs.

### 1.2 Atomic Conventional Commits (Single Responsibility)
* **One Feature/Fix Per Commit**: Each commit must represent exactly one logical change for clean review and trivial `git revert`. Split complex tasks into sequential layered commits (e.g. `feat(core): ...`, then `feat(ui): ...`).
* **Format**: `<type>(<scope>): <imperative summary>`
  * **Types**: `feat`, `fix`, `docs`, `refactor`, `test`, `perf`, `chore`.
  * **Scopes**: `ui`, `core`, `disasm`, `dump`, `stack`, `scanner`, `engine`, `dwarf`, `scripting`, `test`.

---

## 2. Google C++ Style & Modern C++20 Standards

* **Naming**:
  * Types/Classes/Enums: `PascalCase`
  * Functions/Methods/Variables: `camelCase`
  * Class Members: `member_` (trailing underscore)
  * Constants: `kConstantName`
  * Namespaces: `lower_snake_case` (`edb_next`)
* **Memory & Safety**:
  * Zero raw owning pointers; strictly RAII (`std::unique_ptr`, `std::shared_ptr`).
  * Const-correctness everywhere; mark queries `[[nodiscard]]` and `noexcept` where applicable.
  * Modern std: prefer `std::string_view`, `std::span`, `std::ranges`, `std::optional`.
* **Headers**: `#pragma once` on all headers. Include standard headers first, then Qt/third-party, then project headers.

---

## 3. Architecture Guidelines

### 3.1 Headless Core (`core/`)
* **Strictly UI-Independent**: Never include `<QWidget>`, `<QPainter>`, or any UI header. Core must remain 100% headless-testable.

### 3.2 UI & Disassembly Mark Column (`ui/`)
* **Ergonomics**: x64dbg/IDA inspired One Dark theme, micro-animations, high information density.
* **Mark Column (Col 0, width 75px)**:
  * `0 ~ 26px`: Icons (Breakpoint `●`, RIP `➔`, Bookmark `★`).
  * `28 ~ 72px`: 5 collision-free vertical rails for flow lines.
* **Flow Line Colors**:
  * Call: Neon Cyan (`#00e5ff`) — never truncated, full rail.
  * Unconditional JMP: Golden Yellow (`#ffd54f`).
  * Forward Jcc: Amber Orange (`#ff9800`).
  * Backward Jcc / Loop: Coral Red (`#ff5252`).
  * Dynamic RIP Branch: Emerald Green (`#00e676`) if taken, Slate Gray (`#90a4ae`) if not.
* **Rendering Rules**:
  * Viewport filter: render only if source or target row is visible in viewport.
  * Out-of-viewport: connect to border (`y=5` top, `y=h-5` bottom) with arrow (`▲`/`▼`).
  * Selection: 2.4px line with 5.5px semi-transparent glow aura + target bounding box.
  * Navigation: Rich tooltip (`CALL ➔ <target>`), double-click or `Enter` to follow, `Esc`/`Backspace`/`Alt+Left` to return.

---

## 4. Verification Protocol

Before committing:
1. `make -C build -j4`
2. `QT_QPA_PLATFORM=offscreen ./build/test_core && ./build/test_dwarf && ./build/test_exit && ./build/test_scripting && ./build/test_advanced`
