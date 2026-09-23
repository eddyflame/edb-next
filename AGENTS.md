# AGENTS.md - Developer Guidelines for edb-next

Mandatory instructions for AI agents and developers. Follow by default without user prompts.

---

## 1. Automated Workflow & Commit Rules

### 1.1 Autonomous Execution Cycle
On feature, bug fix, or code update:
1. **Implement & Test**: Write code and unit tests.
2. **Synchronize Docs**: Update design documents (`doc/DESIGN_*.md`) and user manuals/tutorials (`doc/TUTORIAL_*.md`) for new features, architectural changes, or usage instructions. Keep `README.md` and `README_zh.md` concise as high-level project introductions—never pile granular feature lists into the README unless it is a major milestone overview. Use relative markdown links only; never expose local machine paths.
3. **Build & Verify (Code Changes Only)**: When C++ code or build scripts are modified, compile (`make -C build -j8`) and run relevant/all test suites (`test_core`, `test_advanced`, `test_dwarf`, `test_exit`, `test_scripting`). **Skip compilation and tests for documentation-only (`docs:`) or non-code updates.**
4. **Commit Together**: Stage and commit code, tests, and documentation together in a single logical commit. Never ask user permission to commit or update docs.

### 1.2 Atomic Conventional Commits (Code + Docs Bundled)
* **Bundle Code and Documentation**: Every feature or fix commit **MUST include its corresponding code changes, tests, and documentation updates together**. Do not split code and documentation into separate commits; keep git history concise and clean.
* **One Logical Topic Per Commit**: Each commit must represent exactly one logical unit (feature, fix, or refactor) along with its docs and tests for clean review and trivial `git revert`.
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

## 3. Architecture Boundaries

### 3.1 Headless Core (`core/`)
* **Strictly UI-Independent**: Never include `<QWidget>`, `<QPainter>`, or any UI/graphics header. Core must remain 100% headless-testable.

### 3.2 Graphical Interface (`ui/`)
* **Decoupled Presentation**: Depend on `core/` via public interfaces; never leak GUI state into `core/`.
* **Ergonomics & Aesthetics**: Follow x64dbg/IDA inspired dark aesthetics, high information density, and responsive micro-interactions.

---

## 4. Verification Protocol

Before committing code changes (**skip for docs-only / non-code changes**):
1. `make -C build -j8`
2. Run relevant tests, or full suite before major code commits:
   `QT_QPA_PLATFORM=offscreen ./build/test_core && ./build/test_dwarf && ./build/test_exit && ./build/test_scripting && ./build/test_advanced`
