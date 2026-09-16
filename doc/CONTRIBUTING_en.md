# Contributing to edb-next

Thank you for your interest in contributing to **edb-next**! We warmly welcome community contributors to help make edb-next the leading open-source binary reverse engineering and dynamic debugging platform on Linux.

---

## 1. Code of Conduct
- Be respectful, open, and collaborative.
- Do not submit malicious payloads, crack tools targeting proprietary systems without permission, or copyrighted code.

---

## 2. Branching & Git Workflow

### 2.1 Branching Strategy
- **`main` / `master`**: Production-ready branch. All commits must pass continuous integration.
- **`develop`**: Integration branch for upcoming minor releases.
- **`feature/<name>`**: Feature branches.
- **`fix/<issue-id>`**: Bugfix branches.

### 2.2 Commit Conventions
We recommend Conventional Commits:
- `feat: add ARM64 register context interface`
- `fix: resolve race condition in remote syscall execution`
- `docs: update tutorial for plugin creation`
- `refactor: optimize string scanner memory footprint`
- `test: add unit test for hardware watchpoint DR6 flags`

---

## 3. Coding Standards

- **Language Standard**: Strict **C++20** (avoid obsolete C-style macros and raw pointer ownership).
- **Memory Safety**: Follow RAII idioms. Raw pointers should only represent non-owning observers. Dynamic allocations must use `std::unique_ptr` or `std::shared_ptr`.
- **Formatting**: Run `clang-format` using the project's `.clang-format` before submitting:
  ```bash
  clang-format -i core/*.cpp core/*.hpp ui/*.cpp ui/*.hpp
  ```
- **Naming Conventions**:
  - Types / Classes / Structs: PascalCase (`LinuxDebugEngine`, `RegisterContext`)
  - Functions / Methods: camelCase (`executeRemoteSyscall`, `readMemory`)
  - Private Member Variables: Trailing underscore (`pid_`, `memFd_`)
  - Enums / Constants: PascalCase (`SessionState::Paused`)

---

## 4. Testing & Verification

Before submitting a Pull Request, build the codebase and verify that all test suites pass cleanly:

```bash
# 1. Compile all targets
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 2. Run core regression suite
./build/test_core

# 3. Run advanced features regression suite
./build/test_advanced

# 4. Run teardown stress suite
./build/test_exit
```

If introducing new features or fixing bugs, please append unit tests in `tests/test_core.cpp` or `tests/test_advanced.cpp`.

---

## 5. Pull Request Process

1. **Fork** the repository on GitHub;
2. Create your branch from `main`: `git checkout -b feature/my-feature`;
3. Commit your changes and ensure local tests pass;
4. Push to your fork: `git push origin feature/my-feature`;
5. Open a Pull Request on GitHub, describing the changes and test results;
6. Await code review and ensure GitHub Actions CI checks pass.

Thank you for contributing to edb-next!
