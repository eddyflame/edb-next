# edb-next

<div align="center">

<h3>下一代 Linux 原生图形化二进制逆向工程与动态调试平台</h3>

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg?style=flat-square&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![Qt](https://img.shields.io/badge/Qt-5.15%2B-brightgreen.svg?style=flat-square&logo=qt)](https://www.qt.io/)
[![Platform](https://img.shields.io/badge/Platform-Linux%20x86__64-orange.svg?style=flat-square&logo=linux)](https://www.kernel.org/)
[![License](https://img.shields.io/badge/License-GPLv3-green.svg?style=flat-square)](LICENSE)
[![CI](https://img.shields.io/badge/CI-Passing-success.svg?style=flat-square&logo=github-actions)](.github/workflows/ci.yml)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat-square)](doc/CONTRIBUTING_zh.md)

**[English](README.md) | [简体中文](README_zh.md)**

</div>

---

## 项目简介 (Overview)

**edb-next** 是一套专为 Linux x86_64 平台设计的现代化、高性能图形化二进制逆向分析与动态调试工具。

基于 **C++20**、**Qt 5.15+** 与 **Capstone 反汇编引擎** 从零重构构建，`edb-next` 旨在终结 Linux 生态长期缺少顶级原生 GUI 调试器的痛点。它不仅深度吸收了 Windows 平台逆向标杆 **x64dbg** 备受好评的四象限工作流与极客操作手感，更立足于 Linux 内核特性，创新实现了目标空间远程系统调用注入、ELF 物理磁盘二进制落盘、异步非阻塞事件驱动循环以及 Glibc ptmalloc 堆内存全景剖析。

```text
┌───────────────────────────────────────┬───────────────────────────────────────┐
│     象限 1: 反汇编视图 (Disassembly)    │       象限 2: 寄存器视图 (Registers)    │
│  - Capstone 语法高亮与当前 RIP 指示     │  - 16 大通用寄存器十六进制呈现与变动标红 │
│  - F2 断点 / Enter 跟入 / Esc 历史瞬退  │  - 智能解引用推导 (符号/栈指针/字符串)   │
│  - Space 就地汇编 / X 交叉引用弹窗      │  - EFLAGS 翡翠绿一键翻转徽章条 / SSE向量│
├───────────────────────────────────────┼───────────────────────────────────────┤
│     象限 3: 多路转储 (Multi-Dump)      │       象限 4: 专有 64 位栈 (StackView) │
│  - Dump 1 ~ Dump 4 独立多标签内存转储  │  - 纯 8 字节 QWORD 对齐排布            │
│  - Ctrl+E 十六进制就地编辑与 NOP 填充  │  - => RSP 栈顶高亮与相对偏移自动推导    │
│  - 18 大高级分析抽屉 (堆/ROP/CFG/外呼) │  - 双击智能路由 (跳反汇编 / 跳 Dump)    │
└───────────────────────────────────────┴───────────────────────────────────────┘
```

---

## 核心架构特色与创新亮点

- 🚀 **零死锁与永不冻结的多线程事件循环**：独立后台线程 `EventLoopThread` 以 `waitpid(WNOHANG)` 模式循环轮询，通过 Qt 信号槽安全派发；前台同步系统操作时自动触发原子挂起（`suspended_`），彻底根除界面假死与死锁。
- ⚡ **目标地址空间远程系统调用注入 (`executeRemoteSyscall`)**：通过原子置换临时 `0x0F 0x05` (`syscall`) 指令，直接在被调试目标内部原生执行 `SYS_mprotect`（修改任意虚拟页为 RWX 读写执行）、`SYS_mmap`（动态分配独立内存供 Shellcode / Trampoline 注入）与 `SYS_munmap`。
- 💾 **一键物理 ELF 磁盘文件落盘 (`patchFileToDisk`)**：自动解算虚拟内存地址到 ELF 物理段文件偏移（$VAddr \to FileOffset$），将内存补丁直接覆写生成可独立运行的脱壳/破解版 ELF 二进制，赋予 `0755` 执行权限。
- 🗄️ **逆向分析成果无感持久化 (`.edb_db`)**：标准化 JSON 数据库持久化存储所有注释、书签、高级条件断点、动态监视表、内存补丁与随手记草稿；重载相同目标时毫秒级自动恢复。
- 🎯 **经典 4 象限黄金工作台**：反汇编、寄存器、4路独立转储（Dump 1~4）以及专有 64 位 QWORD 栈视图四维同屏联动。
- 🔍 **原生 Linux 深度内省与漏洞利用工具**：内置 Glibc ptmalloc 堆链解析器（`malloc_chunk` 与 `A|M|P` 标志）、ROP Gadget 滑动窗口搜寻与 Python `p64(...)` 利用脚本导出、交互式基本块控制流图 (CFG)、跨模块动态库 API 外呼搜索、以及 `/proc/<pid>/fd/` 句柄分类。
- 📖 **DWARF 源码级调试与反汇编混合渲染**：基于 `libdw` 原生解析 `.debug_info` 与 `.debug_line`，实现地址与源码行号双向瞬时映射；支持反汇编与原始 C/C++ 源码混合排版（`Ctrl+Shift+S`），内置独立源码浏览器 `SourceView`（`Alt+S`），支持源码行双击断点与源码级单步。
- 🐍 **嵌入式 Python 3 & Lua 5.4 双脚本自动化引擎**：原生嵌入 CPython 3 与 Lua 5.4 解释器，统一由 `ScriptEngineManager` 调度；内置 `edb` 模块向脚本全面暴露内存读写、寄存器控制、断点管理、单步执行与表达式求值，配备独立暗黑极客 Script Console（`Alt+P`）与 CommandBar 行内执行（`py <code...>` / `lua <code...>`）；支持在任意断点绑定脚本动作，结合 `return False` / `return false` 契约实现微秒级无感动态打桩（Silent Hooking）。
- 🏷️ **C++ 符号智能反混淆 (Demangling)**：原生集成 Itanium ABI `abi::__cxa_demangle`，全局符号浏览器、调用栈、反汇编行指示、寄存器与栈区智能解引用全线呈现清晰的 `calculate_fib(int)`，搜索过滤双向匹配，悬停保留原始 Mangled 名。
- 🎯 **转储区细粒度硬件读写监视点 (Hardware Watchpoints Context Menu)**：在 HexDump 单元格右键一键部署 1/2/4/8 字节硬件写监视点（Write Watchpoint）、硬件读写监视点与硬件执行断点，单元格深红背景醒目高亮指示活动断点。
- ⌨️ **常驻 x64dbg 风格 CommandBar 命令行**：底栏极客 CLI 控制台，内置 `bp`, `bph`, `r`, `d`, `u`, `step`, `eval`, `py`, `lua`, `mprotect`, `alloc`, `dumpstate` 等指令，并向插件全面开放扩展接口。
- 🧩 **现代 C++20 解耦插件网关**：基于纯虚契约 `IPlugin` 与网关 `IPluginContext`，支持动态 `.so` 热加载、菜单注入、命令行扩展与断点监听钩子。

---

## 功能对比全景矩阵

| 特性维度 | 原版 edb (Linux) | x64dbg (Windows) | edb-next (现代化 Linux 重构) |
| :--- | :---: | :---: | :---: |
| **语言规范** | C++11 | C++14/17 | **现代 C++20 标准** |
| **事件循环并发** | 0ms QTimer 轮询 (易死锁假死) | 复杂同步事件 | **非阻塞 EventLoopThread (0% 界面冻结)** |
| **工作台布局** | 单一底部抽屉 (反复切Tab) | 经典四象限布局 | **4-Quadrant 黄金工作流** |
| **内存转储能力** | 单一 Hex Dump | 标配 Dump 1~5 | **4路独立 MultiDumpWidget (Dump 1~4)** |
| **只读内存修改** | 报错拒绝写入 | VirtualProtect 模拟 | **原生远程系统调用注入 (`SYS_mprotect`)** |
| **脱壳补丁落盘** | 无此功能 (仅内存补丁) | 导出 Patched EXE | **创新 `patchFileToDisk` (直接导出 ELF)** |
| **源码级调试** | 仅纯反汇编 | 需外部工具 | **内置 DWARF 源码映射与混合渲染 (`libdw`)** |
| **自动化脚本引擎**| 无内嵌脚本 | 需第三方插件 | **原生 Python 3 & Lua 5.4 双脚本引擎 (`Alt+P`)** |
| **项目成果持久化** | 退出全盘丢失 | 标配 `.dd64` 数据库 | **`.edb_db` JSON 项目自动恢复** |
| **命令行交互** | 无交互 CLI | 标配底栏命令行 | **x64dbg 风格 CommandBar 极客交互栏** |
| **Linux 堆分析** | 插件支持较旧 | 不适用 (Windows) | **原生 Glibc ptmalloc 分析器 (Tab 9)** |
| **漏洞利用辅助** | 基础 ROP 插件 | 需第三方插件 | **内置 ROP 工具箱与 Python `p64()` 导出** |

---

## 快速上手 (Quick Start)

### 1. 安装构建依赖 (Ubuntu / Debian)
```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    qtbase5-dev \
    libqt5widgets5 \
    libcapstone-dev \
    libdw-dev \
    libelf-dev \
    python3-dev \
    liblua5.4-dev
```

### 2. 源码编译构建
```bash
git clone https://github.com/your-username/edb-next.git
cd edb-next

# 配置并启用全核并发编译
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### 3. 运行全量验证测试套件
```bash
./build/test_core       # 核心测试 (断点、单步、多线程、ELF解析等)
./build/test_dwarf      # DWARF 源码级调试与行号双向映射测试
./build/test_advanced   # 进阶测试 (补丁落盘、Trace、CFG、远程系统调用注入等)
./build/test_scripting  # Python 3 & Lua 5.4 嵌入式双引擎测试
./build/test_exit       # 析构安全压力测试
```

### 4. 启动调试器
```bash
./build/edb_next
```

---

## 项目工程文档目录 (Documentation)

项目全套双语文档已统一归档至 `doc/` 目录下：

| 文档名称 | 中文版 (Chinese) | 英文版 (English) | 文档定位与主要内容 |
| :--- | :--- | :--- | :--- |
| **软件架构与工程设计说明书 (SDD)** | [doc/DESIGN_zh.md](doc/DESIGN_zh.md) | [doc/DESIGN_en.md](doc/DESIGN_en.md) | 深度梳理底层机制、设计方案、状态机、数据结构与技术演进规划 |
| **完全使用教程与插件开发指南** | [doc/TUTORIAL_zh.md](doc/TUTORIAL_zh.md) | [doc/TUTORIAL_en.md](doc/TUTORIAL_en.md) | 详细功能操作手册、快捷键速查表与现代 C++20 插件开发全流程实战 |
| **开源贡献规范** | [doc/CONTRIBUTING_zh.md](doc/CONTRIBUTING_zh.md) | [doc/CONTRIBUTING_en.md](doc/CONTRIBUTING_en.md) | 代码风格要求、Git 提交规范与 Pull Request 流程规范 |

---

## 全局常用快捷键速查表

| 快捷键 | 功能说明 | 快捷键 | 功能说明 |
| :--- | :--- | :--- | :--- |
| **F9** | Continue (全速运行) | **Enter** | Follow Branch (跟随分支/跟入) |
| **F7** | Step Into (单步步入) | **Esc / Backspace** | Go Back (沿历史栈瞬时回退) |
| **F8** | Step Over (单步步过) | **Space** | Assemble (就地内联汇编) |
| **Shift+F11** | Step Out (跳出当前函数) | **; (分号)** | Add Comment (添加/修改指令注释) |
| **F4** | Run to Selection (运行到光标) | **Ctrl+B** | Toggle Bookmark (打下/取消书签) |
| **Ctrl+F2** | Restart (重启会话) | **X** | Show Cross References (交叉引用) |
| **Ctrl+\*** | Set RIP (设置指令指针) | **Ctrl+E** | Modify Hex Bytes (就地编辑内存) |
| **F2** | Toggle Breakpoint (切换断点) | **Ctrl+P** | Patch Manager (补丁管理与落盘) |
| **Alt+P** | Script Console (Python/Lua 控制台) | **Alt+S** | Focus Source View (聚焦源码浏览器) |
| **Ctrl+Shift+S** | Toggle Mixed ASM/Source (混合渲染切换) | **Alt+C** | Focus CPU / Disassembly (聚焦反汇编) |
| **Ctrl+S** | Save Project (.edb_db 项目保存) | **Ctrl+D** | Dump CPU State (导出机器状态快照) |
| **Shift+S** | Toggle Stack View (折叠/展开栈) | **Shift+F7/F8/F9** | 透传信号执行 (Pass Signal Step/Run) |

---

## 插件二次开发示例

`edb-next` 提供轻量解耦的 C++20 插件网关，官方参考插件位于 [plugins/SamplePlugin/](plugins/SamplePlugin/)：

```cpp
#include "core/IPlugin.hpp"
#include "core/IPluginContext.hpp"

class MyPlugin : public QObject, public edb_next::IPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EDB_NEXT_PLUGIN_IID)
    Q_INTERFACES(edb_next::IPlugin)

public:
    bool initialize(edb_next::IPluginContext* ctx) override {
        // 向底栏 CommandBar 注册自定义极客命令
        ctx->registerCommand("my_ping", [](const std::vector<std::string>& args) {
            // 执行业务命令逻辑
        }, "my_ping - 示例扩展指令");

        // 挂接断点触发监听钩子
        ctx->registerDebugEventListener([](const edb_next::DebugEvent& ev) {
            // 响应调试事件
        });
        return true;
    }
    void shutdown() override {}
};
```
完整插件开发步骤详见 [doc/TUTORIAL_zh.md](doc/TUTORIAL_zh.md) 或 [doc/TUTORIAL_en.md](doc/TUTORIAL_en.md)。

---

## 参与贡献 (Contributing)

我们非常欢迎来自开源社区的 PR、Bug 报告和功能建议！在提交代码前，请先查阅 [贡献指南](doc/CONTRIBUTING_zh.md)。

---

## 开源许可证 (License)

本项目遵循 [GNU General Public License v3.0 (GPL-3.0)](LICENSE) 开源许可证。
