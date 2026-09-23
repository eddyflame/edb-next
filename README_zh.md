# edb-next

<div align="center">

<h3>下一代 Linux 原生图形化二进制逆向工程与动态调试平台</h3>

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg?style=flat-square&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/23)
[![Qt](https://img.shields.io/badge/Qt-6.4%2B-brightgreen.svg?style=flat-square&logo=qt)](https://www.qt.io/)
[![Platform](https://img.shields.io/badge/Platform-Linux%20x86__64-orange.svg?style=flat-square&logo=linux)](https://www.kernel.org/)
[![License](https://img.shields.io/badge/License-GPLv3-green.svg?style=flat-square)](LICENSE)
[![CI](https://img.shields.io/badge/CI-Passing-success.svg?style=flat-square&logo=github-actions)](.github/workflows/ci.yml)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat-square)](doc/CONTRIBUTING_zh.md)

**[English](README.md) | [简体中文](README_zh.md)**

</div>

---

## 项目简介 (Overview)

**edb-next** 是一套专为 Linux x86_64 平台设计的现代化、高性能图形化二进制逆向分析与动态调试工具。

基于 **C++23**、**Qt 6.4+** 与 **Capstone / Zydis 双反汇编引擎** 构建，`edb-next` 深度吸收了 Windows 平台逆向标杆 **x64dbg** 备受好评的四象限工作流与极客操作手感，并立足 Linux 内核特性，打造了反应式事件循环、原生 C 伪代码反编译、时间旅行调试、Z3 符号执行、无头 DAP 协议服务与物理 ELF 补丁落盘等现代化特性。

```text
┌───────────────────────────────────────┬───────────────────────────────────────┐
│     象限 1: 反汇编视图 (Disassembly)    │       象限 2: 寄存器视图 (Registers)    │
│  - One Dark 语法高亮与当前 RIP 指示     │  - 16 大通用寄存器十六进制呈现与变动标红 │
│  - 动态分支预测与操作数解引用链式预览条 │  - GPR 极速 +1/-1 微调与 Follow in Stack│
│  - F2 断点 / Enter 跟入 / Esc 历史瞬退  │  - EFLAGS 翡翠绿一键翻转徽章条 / SSE向量│
│  - F5 原生 C 伪代码反编译器 (Decompiler)│  - TimeTravelWidget 时间旅行轴(Ctrl+F7) │
├───────────────────────────────────────┼───────────────────────────────────────┤
│     象限 3: 多路转储 (Multi-Dump)      │       象限 4: 专有 64 位栈 (StackView) │
│  - Dump 1 ~ Dump 4 独立多标签内存转储  │  - 纯 8 字节 QWORD 对齐排布            │
│  - Alt+Left/Right 历史前后穿梭导航     │  - [Return Address] 亮琥珀金返回点识别 │
│  - Ctrl+E 十六进制就地编辑 / TypeViewer│  - => RSP 栈顶高亮与双击智能路由        │
└───────────────────────────────────────┴───────────────────────────────────────┘
```

---

## 核心亮点 (Key Highlights)

- 🚀 **四象限黄金工作流与现代化工效**：反汇编、寄存器、多路独立转储 (Dump 1~4) 与 64 位栈区同屏四维联动；提供 x64dbg 风格语法高亮、动态分支预测与解引用预览条、5 轨调用/跳转控制流关系线，以及连续就地汇编（`Space`）。
- ⚡ **微秒级反应式内核事件循环 (`pidfd` + `epoll`)**：采用 Linux 5.3+ 原生 `pidfd` 与 `epoll` 架构，配合 `eventfd` 自唤醒机制，达成 0% 空闲 CPU 占用与微秒级响应；利用 `pidfd_getfd` 实现对目标进程文件描述符与套接字的非侵入式零死锁内省。
- 🧩 **原生 C 语言伪代码反编译与符号分析**：现代化 C++23 原生反编译器 (`DecompilerEngine`)，将反汇编提升为 AST 结构化控制流并在 `F5` 视图呈现双向映射；集成 SSA Micro-IR 与 Z3 SMT 求解器，支持分支到达性自动求解与污点追踪。
- ⏳ **时间旅行调试 (TTD) 与隐匿内存监控**：确定性执行帧回溯与寄存器/内存写差分追踪，支持单步倒流（`Ctrl+F7`）、逆向全速运行（`Ctrl+Shift+F9`）与时间轴拖拽；基于 Linux `userfaultfd` 实现零 `0xCC` 隐匿写监视点与脏页监听。
- 💾 **远程系统调用注入与物理 ELF 补丁落盘**：突破只读内存限制，在目标进程内部原子注入执行 `SYS_mprotect`（修改内存保护为 RWX）与 `SYS_mmap`；一键解算虚拟地址到 ELF 物理段偏移，直接覆写导出可独立执行的脱壳/修补版 ELF 二进制。
- 🔌 **双脚本自动化与无头 DAP 协议生态**：原生内嵌 Python 3 与 Lua 5.4 双自动化脚本引擎 (`Alt+P`)，支持断点自动化打桩（Silent Hooking）；内置微软 DAP (Debug Adapter Protocol) 协议服务 (`--dap`)，无缝集成 VS Code 与 Neovim。
- 🗄️ **ACID 增量工程存储引擎**：基于嵌入式 SQLite3 + Zstandard (`libzstd`) 高压缩率事务数据库 (`.edb_db`)，毫秒级持久化/加载海量注释、标签、书签、断点与补丁，并对旧版 JSON 项目工程提供 100% 透明升级。

> **技术实现细节与系统架构设计**，请参阅完整的设计说明书：[doc/DESIGN_zh.md](doc/DESIGN_zh.md)。

---

## 项目工程文档 (Documentation)

项目全套文档已统一归档至 `doc/` 目录下：

| 文档名称 | 中文版 (Chinese) | 英文版 (English) | 文档定位与主要内容 |
| :--- | :--- | :--- | :--- |
| **软件架构与工程设计说明书 (SDD)** | [doc/DESIGN_zh.md](doc/DESIGN_zh.md) | [doc/DESIGN_en.md](doc/DESIGN_en.md) | 深度梳理底层机制、设计方案、状态机、数据结构与技术演进规划 |
| **完全使用教程与插件开发指南** | [doc/TUTORIAL_zh.md](doc/TUTORIAL_zh.md) | [doc/TUTORIAL_en.md](doc/TUTORIAL_en.md) | 详细功能操作手册、快捷键速查表与现代 C++20 插件开发全流程实战 |
| **开源贡献规范** | [doc/CONTRIBUTING_zh.md](doc/CONTRIBUTING_zh.md) | [doc/CONTRIBUTING_en.md](doc/CONTRIBUTING_en.md) | 代码风格要求、Git 提交规范与 Pull Request 流程规范 |

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

### 2. 源码编译构建
```bash
git clone https://github.com/your-username/edb-next.git
cd edb-next

# 配置并启用多核并发编译
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### 3. 运行全量验证测试套件
```bash
./build/test_core           # 核心测试 (断点、单步、多线程、ELF解析等)
./build/test_dwarf          # DWARF 源码级调试与行号双向映射测试
./build/test_advanced       # 进阶测试 (补丁落盘、Trace、CFG、远程系统调用注入等)
./build/test_scripting      # Python 3 & Lua 5.4 嵌入式双引擎测试
./build/test_exit           # 析构安全压力测试
./build/test_nextgen        # 验证 pidfd 反应式循环、目标 FD 内省、libclang AST、SQLite3+zstd 及 C++23 单子
./build/test_p4_advanced_re # 验证 SSA Micro-IR、Z3 符号执行、原生反编译、TTD 回溯、DAP 服务与 eBPF
./build/test_p5_ultimate    # 验证 userfaultfd 隐匿缺页、BTF 紧凑类型系统、DR6 归因与 PageGuard 自动降级、反反调试及 NEON 向量化
```

### 4. 启动调试器

#### 方式 A：直接运行本地构建产物
```bash
./build/edb_next
```

#### 方式 B：使用独立便携 AppImage（推荐，免安装）
下载发布版或本地构建生成的单文件 AppImage，赋予执行权限后即可在各大主流 Linux 发行版（Ubuntu、Debian、Fedora、Arch Linux 等）直接运行：
```bash
chmod +x edb-next-x86_64.AppImage
./edb-next-x86_64.AppImage
```

> **提示 (ptrace 调试权限)**：
> - 启动新程序调试（Spawn / Open Binary）完全免 root，开箱即用。
> - 若需附加（Attach）到已存在的非子进程，受 Linux Yama LSM 安全策略影响，建议使用 `sudo ./edb-next-x86_64.AppImage` 启动，或临时设置宿主机内核参数：`sudo sysctl -w kernel.yama.ptrace_scope=0`。

#### 本地一键构建 AppImage
```bash
# 宿主机直接构建并打包
./scripts/build_appimage.sh

# 或在 Ubuntu 22.04 LTS (glibc 2.35) Docker 容器中打包（获得最高的跨发行版兼容性）
./scripts/docker_build_appimage.sh
```

---

## 全局常用快捷键速查表

| 快捷键 | 功能说明 | 快捷键 | 功能说明 |
| :--- | :--- | :--- | :--- |
| **F9** | Continue (全速运行) | **Enter** | Follow Branch (分支跟随) / 栈智能跟入 |
| **F7** | Step Into (单步步入) | **Esc / Backspace** | Go Back (沿历史栈瞬时回退) |
| **F8** | Step Over (单步步过) | **\*** *(小键盘 / 键)* | **Origin**: 居中显示当前执行指针 RIP |
| **Shift+F11** | Step Out (跳出当前函数) | **Space** | 连续就地汇编 (自动步进下一条 + NOP 补齐) |
| **F4** | Run to Selection (运行到光标) | **; (分号)** | Add Comment (添加/修改指令注释) |
| **F5** | **原生 C 反编译** (刷新/呼出) | **: (冒号)** | Set / Edit Label (自定义用户标签 `🏷`) |
| **Ctrl+F7** | **Step Back** (时间旅行单步倒流) | **Ctrl+B** | Toggle Bookmark (打下/取消书签) |
| **Ctrl+Shift+F9** | **Reverse Continue** (逆向全速运行) | **X** | Show Cross References (交叉引用) |
| **Ctrl+F2** | Restart (重启会话) | **Ctrl+E** | Modify Hex Bytes (就地编辑内存) |
| **Ctrl+\*** | Set RIP (强制重设当前执行指针) | **Ctrl+P** | Patch Manager (补丁管理与落盘) |
| **F2** | Toggle Breakpoint (切换断点) | **Alt+S** | Focus Source View (聚焦源码浏览器) |
| **Ctrl+Alt+S** | Search All Strings (搜索引用字符串) | **Alt+C** | Focus CPU / Disassembly (聚焦反汇编) |
| **Ctrl+Alt+C** | Search All Calls (搜索跨模块调用) | **Ctrl+D** | Dump CPU State (导出机器状态快照) |
| **Alt+P** | Script Console (Python/Lua 控制台) | **Shift+F7/F8/F9** | 透传信号执行 (Pass Signal Step/Run) |
| **Ctrl+Shift+S** | Toggle Mixed ASM/Source (混合渲染切换) | **栈视图: `Space` / `Ctrl+G`** | 就地改写 QWORD / 跳转栈地址 |
| **Ctrl+S** | Save Project (.edb_db 项目保存) | **Follow in Dump 1~4** | 多标签定向转储路由 |
| **Shift+S** | Toggle Stack View (折叠/展开栈) | **寄存器: `+` / `-` / `0`** | GPR 快速增减/清零 |
| **转储: `Enter` / 双击** | 就地极速改写内存字节 | | |

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
完整插件开发步骤详见 [doc/TUTORIAL_zh.md](doc/TUTORIAL_zh.md)。

---

## 参与贡献 (Contributing)

我们非常欢迎来自开源社区的 PR、Bug 报告和功能建议！在提交代码前，请先查阅 [贡献指南](doc/CONTRIBUTING_zh.md)。

---

## 开源许可证 (License)

本项目遵循 [GNU General Public License v3.0 (GPL-3.0)](LICENSE) 开源许可证。
