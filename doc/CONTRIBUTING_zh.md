# edb-next 贡献指南 (Contributing Guide)

感谢您对 **edb-next** 项目的关注与支持！我们热忱欢迎社区贡献者一同将 edb-next 打造成 Linux 平台顶尖的开源二进制逆向工程与动态调试平台。

---

## 1. 行为准则 (Code of Conduct)
- 相互尊重、开放交流，对技术问题保持严谨与友善。
- 严禁提交带有恶意 Payload、恶意破解特征或侵犯第三方版权的代码。

---

## 2. 开发环境与分支规范 (Development & Git Flow)

### 2.1 分支策略
- **`master` / `main`**：稳定发布主分支，所有代码必须通过自动化 CI 测试套件。
- **`develop`**：日常集成开发分支。
- **`feature/<name>`**：新特性开发分支。
- **`fix/<issue-id>`**：问题修复分支。

### 2.2 提交信息规范 (Git Commit Convention)
推荐使用语义化 Commit 规范：
- `feat: add ARM64 register context interface`
- `fix: resolve race condition in remote syscall execution`
- `docs: update tutorial for plugin creation`
- `refactor: optimize string scanner memory footprint`
- `test: add unit test for hardware watchpoint DR6 flags`

---

## 3. 代码风格与规范 (Coding Standards)

- **C++ 标准**：严格遵守 **C++20** 标准（禁用过时的 C 风格宏与裸指针管理）。
- **内存安全**：全面使用 RAII 模式，裸指针仅用于非拥有关系的观察者（Observer），资源所有权采用 `std::unique_ptr` 或 `std::shared_ptr`。
- **代码格式化**：项目根目录包含 `.clang-format` 配置文件，提交前请格式化代码：
  ```bash
  clang-format -i core/*.cpp core/*.hpp ui/*.cpp ui/*.hpp
  ```
- **命名规范**：
  - 类名与结构体：大驼峰（`LinuxDebugEngine`, `RegisterContext`）；
  - 函数与方法：小驼峰（`executeRemoteSyscall`, `readMemory`）；
  - 私有成员变量：带下划线后缀（`pid_`, `memFd_`）；
  - 常量与枚举项：大驼峰（`SessionState::Paused`）。

---

## 4. 本地测试与质量门禁 (Testing & Verification)

在发起 Pull Request 之前，必须在本地编译并通过全部三大测试套件：

```bash
# 1. 编译全部目标
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 2. 运行基础回归测试套件
./build/test_core

# 3. 运行进阶特性全量验证套件
./build/test_advanced

# 4. 运行窗口析构与进程销毁压力测试
./build/test_exit
```
若有新增功能，请在 `tests/test_core.cpp` 或 `tests/test_advanced.cpp` 中同步编写对应的单元测试用例。

---

## 5. Pull Request 流程

1. **Fork** 本仓库到您的 GitHub 账号；
2. 从 `main` 分支切出您的特性分支：`git checkout -b feature/my-new-feature`；
3. 编写代码并确保通过本地全量测试；
4. 提交代码并推送至您的 Fork 仓库：`git push origin feature/my-new-feature`；
5. 在 GitHub 上创建 Pull Request，填写变更说明与测试结果；
6. 维护团队审核通过并等待 GitHub Actions CI 检查全绿后合并入主干。

再次感谢您的贡献！
