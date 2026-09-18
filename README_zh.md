# edb-next

<div align="center">

<h3>下一代 Linux 原生图形化二进制逆向工程与动态调试平台</h3>

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg?style=flat-square&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
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

基于 **C++20**、**Qt 6.4+** 与 **Capstone 反汇编引擎** 从零重构构建，`edb-next` 旨在终结 Linux 生态长期缺少顶级原生 GUI 调试器的痛点。它不仅深度吸收了 Windows 平台逆向标杆 **x64dbg** 备受好评的四象限工作流与极客操作手感，更立足于 Linux 内核特性，创新实现了目标空间远程系统调用注入、ELF 物理磁盘二进制落盘、异步非阻塞事件驱动循环以及 Glibc ptmalloc 堆内存全景剖析。

```text
┌───────────────────────────────────────┬───────────────────────────────────────┐
│     象限 1: 反汇编视图 (Disassembly)    │       象限 2: 寄存器视图 (Registers)    │
│  - One Dark 语法高亮与当前 RIP 指示     │  - 16 大通用寄存器十六进制呈现与变动标红 │
│  - 动态分支预测与操作数解引用链式预览条 │  - GPR 极速 +1/-1 微调与 Follow in Stack│
│  - F2 断点 / Enter 跟入 / Esc 历史瞬退  │  - EFLAGS 翡翠绿一键翻转徽章条 / SSE向量│
├───────────────────────────────────────┼───────────────────────────────────────┤
│     象限 3: 多路转储 (Multi-Dump)      │       象限 4: 专有 64 位栈 (StackView) │
│  - Dump 1 ~ Dump 4 独立多标签内存转储  │  - 纯 8 字节 QWORD 对齐排布            │
│  - Alt+Left/Right 历史前后穿梭导航     │  - [Return Address] 亮琥珀金返回点识别 │
│  - Ctrl+E 十六进制就地编辑 / TypeViewer│  - => RSP 栈顶高亮与双击智能路由        │
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
- 🛡️ **内存页保护断点与零 0xCC 隐匿执行断点 (Page-Guard / Anti-Anti-Debugging)**：突破 x86_64 硬件寄存器仅 4 处的物理极限，基于 `PROT_NONE` / `PROT_READ` 虚拟页保护提供无限槽位软监视点；对代码段实施零 `0xCC` 注入的纯内存断点，完美绕过加固壳与混淆样本的 CRC32/Hash 代码段自检测反调试；内置内核级假阳性透明放行状态机，微秒级越过同页其他变量访问。
- 📦 **动态库加载全自动拦截与热重载 (_r_debug Rendezvous & Pending Breakpoints)**：深度接入 Linux glibc `_r_debug` Rendezvous 协议与 `_dl_debug_state` 内部陷阱，全自动捕获运行时 `dlopen()` 与 `dlclose()` 共享库装载事件；动态差分 `link_map`、合并 ELF 符号表并扩展 DWARF 源码映射；提供待决延迟断点（Pending Breakpoints，`bpp <symbol>`），在新模块装载瞬间自动绑定物理断点，支持 `catch load` / `catch dlopen` 模块加载中断与 `BinaryInfoView` 动态库实时面板。
- 🌿 **多进程 Follow-Fork 与子进程会话树 (Follow-Fork & Multi-Process Debugging)**：基于 Linux 内核 `PTRACE_O_TRACEFORK`/`TRACEVFORK` 与 Tracer 亲和性机制，支持 `Parent`（保持父进程）、`Child`（切换至子进程）与 `Both`（父子多进程独立会话树同屏协同）三态跟踪策略；支持 `catch fork` 物理断下拦截；底栏集成 `inferiors` 与 `inferior <id|pid>` 指令，无缝管理多进程工作区标签页。
- ❄️ **多线程独立冻结与解冻及隔离单步步进 (Thread Freeze / Thaw & Isolated Stepping)**：支持单个或批量轻量级线程（TID）独立冻结与解冻，在 `ThreadsView`（Tab 10）全景呈现 8 列信息与冰蓝 `❄ FROZEN` 状态高亮；提供 `❄ Freeze Others` 与 `🔥 Thaw All` 快捷控制；支持隔离单步（Isolated Stepping），彻底消除高并发 Worker 线程干扰；CLI 支持 `freeze <tid|all>` 与 `thaw <tid|all>`。
- 🔍 **CheatEngine 级动态内存特征差分扫描器 (Differential Memory Scanner)**：专为动态密钥定位与外挂变量收敛打造，在底部抽屉提供专属 **Memory Scanner** 面板（Tab 21）；原生支持 8 种数据类型（Int8~64、Float、Double、String、Hex 通配符）；支持多轮差分收敛（增大、减小、变动、未变、增减指定 Delta）；聚焦可读写段流式扫描，单轮耗时仅数十毫秒；候选列表支持增绿减红变动指示、双击联动 Hex Dump 以及原位直接修改目标内存；CLI 全面支持 `scan`, `nextscan`, `scanresults`, `scanreset`。
- 🧬 **复合数据类型重构与结构体布局可视化 (Type Viewer & Struct Layout)**：专为复杂对象反向解析与网络协议还原设计，在底部抽屉提供专属 **Type Viewer** 工作台（Tab 22）；原生支持 C 语言标准语法解析与 System V AMD64 ABI 自然对齐/填充计算；覆盖 14 种基础数据类型与定长数组；预置 `timespec`、`timeval`、`sockaddr_in`、`list_head`、`io_vec` 等 Linux 核心结构体；支持字段相对偏移与原始 Hex 呈现，青色高亮指针字段并支持双击直接解引用追踪（Jump to Hex Dump / Disassembly），支持右键原位修改内存；CLI 支持 `structs`、`struct <name> <addr>`、`defstruct <c_code...>`。
- 🎨 **x64dbg 风格现代化逆向工效与语法着色系统**：定制指令重绘委托 `InstructionHighlightDelegate`，提供 CALL/JMP/Jcc/RET/SYSCALL/PUSH/POP/CMP/TEST/NOP/寄存器/寻址括号/立即数细粒度语义高亮；富文本动态分支预测（`Branch Taken: YES / NO`）与内存操作数链式求值预览（`[rbp - 0x14] => 0x... => val`）；专有栈视图函数返回地址智能识别与亮琥珀金标签（`[Return Address] <symbol>`）；寄存器极速 `+1`/`-1` 微调、`Follow in Stack` 与多格式复制子菜单；Hex Dump 历史导航栈（`Alt+Left` / `Backspace` / `Alt+Right`）、多维 QWORD 穿梭与一键直达结构体解析（`View as Struct...`）；`Ctrl+*` Set Origin (Set RIP)。
- ⚡ **x64dbg / edb 风格反汇编 Mark 列控制流与调用关系线系统**：在反汇编视口 Mark 列（宽 75px）集成 5 轨贪心多通道避让控制流连线。多维度色系标识：函数调用（CALL）采用**霓虹青蓝（Neon Cyan `#00e5ff`）**、无条件跳转（JMP）采用**明亮金黄（Golden Yellow `#ffd54f`）**、向后循环分支（Loop）采用**珊瑚红（Coral Red `#ff5252`）**、向前条件分支（Jcc）采用**琥珀橙（Amber Orange `#ff9800`）**，结合当前 RIP 动态分支预测评估（成立呈现翡翠绿，不成立呈现沉着灰蓝）。纵向线沿独立轨道完整贯通至视口边界（越界指示箭头 `▲` / `▼`），智能视口可见性过滤杜绝离屏虚影杂线；双通道选中高亮发光光晕（Pass 2 Glow Aura）与目标落点边框；Mark 列单元格富文本悬停解析提示，支持 `Enter` 或双击即刻追踪分支调用与历史栈快速返回。
- ⌨️ **常驻 x64dbg 风格 CommandBar 命令行**：底栏极客 CLI 控制台，内置 `bp`, `bph`, `r`, `d`, `u`, `step`, `eval`, `py`, `lua`, `mprotect`, `alloc`, `dumpstate`, `pageguard`, `guards`, `follow-fork`, `inferiors`, `structs`, `struct`, `defstruct` 等指令，并向插件全面开放扩展接口。
- 🧩 **现代 C++20 解耦插件网关**：基于纯虚契约 `IPlugin` 与网关 `IPluginContext`，支持动态 `.so` 热加载、菜单注入、命令行扩展与断点监听钩子。

---

## 功能对比全景矩阵

| 特性维度 | 原版 edb (Linux) | x64dbg (Windows) | edb-next (现代化 Linux 重构) |
| :--- | :---: | :---: | :---: |
| **语言规范** | C++11 | C++14/17 | **现代 C++20 标准** |
| **事件循环并发** | 0ms QTimer 轮询 (易死锁假死) | 复杂同步事件 | **非阻塞 EventLoopThread (0% 界面冻结)** |
| **工作台布局** | 单一底部抽屉 (反复切Tab) | 经典四象限布局 | **4-Quadrant 黄金工作流** |
| **反汇编与工效着色** | 纯等宽黑白文本 | 丰富色彩与调用/返回高亮 | **x64dbg 风格语法委托 + 5轨调用与跳转控制流关系线 (霓虹青蓝调用 / 金黄跳转 / 珊瑚红循环) + 动态分支预测/解引用预览 + Ctrl+* Set Origin** |
| **转储与调用栈工效** | 基础线性转储 | 历史栈与返回地址识别 | **转储历史前进/后退 (Alt+Left/Right) + [Return Address] 识别 + 结构体直达** |
| **寄存器快捷微调** | 弹窗手工输十六进制 | 快捷增减 | **GPR 快捷 +1/-1 + Follow in Stack + 多格式复制子菜单** |
| **内存转储能力** | 单一 Hex Dump | 标配 Dump 1~5 | **4路独立 MultiDumpWidget (Dump 1~4)** |
| **只读内存修改** | 报错拒绝写入 | VirtualProtect 模拟 | **原生远程系统调用注入 (`SYS_mprotect`)** |
| **脱壳补丁落盘** | 无此功能 (仅内存补丁) | 导出 Patched EXE | **创新 `patchFileToDisk` (直接导出 ELF)** |
| **源码级调试** | 仅纯反汇编 | 需外部工具 | **内置 DWARF 源码映射与混合渲染 (`libdw`)** |
| **自动化脚本引擎**| 无内嵌脚本 | 需第三方插件 | **原生 Python 3 & Lua 5.4 双脚本引擎 (`Alt+P`)** |
| **项目成果持久化** | 退出全盘丢失 | 标配 `.dd64` 数据库 | **`.edb_db` JSON 项目自动恢复** |
| **命令行交互** | 无交互 CLI | 标配底栏命令行 | **x64dbg 风格 CommandBar 极客交互栏** |
| **Linux 堆分析** | 插件支持较旧 | 不适用 (Windows) | **原生 Glibc ptmalloc 分析器 (Tab 9)** |
| **动态库热加载 / dlopen 拦截** | 需手动刷新符号 | 支持 DLL 事件 | **原生 glibc _r_debug 协议自动捕获 + Pending 待决断点** |
| **多进程 Follow-Fork / 子进程跟踪** | 仅单进程跟踪 | 支持多进程附加 | **原生 PTRACE_EVENT_FORK 捕获 + Parent/Child/Both 三态会话树 + inferiors 穿梭** |
| **多线程独立冻结 / 隔离单步** | 仅查看线程列表 | 支持暂停/恢复线程 | **原生 SYS_tgkill 信号阻断 + 调度掩码 + 冰蓝高亮 + 隔离单步** |
| **动态内存差分扫描 (CheatEngine 级)** | 基础静态特征搜索 | 需挂接外部 CE | **原生 8 种数据类型 + 多轮差分收敛 + 增减 Delta + 原位覆写 (Tab 21)** |
| **复合结构体解析与布局可视化** | 无此功能 | 需复杂插件扩展 | **原生 C 语言语法解析 + AMD64 ABI 对齐 + 字段原位编辑 + 解引用穿梭 (Tab 22)** |
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
    qt6-base-dev \
    qt6-tools-dev \
    libgl1-mesa-dev \
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
| **F9** | Continue (全速运行) | **Enter** | Follow Branch (分支跟随) / 栈智能跟入 |
| **F7** | Step Into (单步步入) | **Esc / Backspace** | Go Back (沿历史栈瞬时回退) |
| **F8** | Step Over (单步步过) | **\*** *(小键盘 / 键)* | **Origin**: 居中显示当前执行指针 RIP |
| **Shift+F11** | Step Out (跳出当前函数) | **Space** | 连续就地汇编 (自动步进下一条 + NOP 补齐) |
| **F4** | Run to Selection (运行到光标) | **; (分号)** | Add Comment (添加/修改指令注释) |
| **Ctrl+F2** | Restart (重启会话) | **: (冒号)** | Set / Edit Label (自定义用户标签 `🏷`) |
| **Ctrl+\*** | Set RIP (强制重设当前执行指针) | **Ctrl+B** | Toggle Bookmark (打下/取消书签) |
| **F2** | Toggle Breakpoint (切换断点) | **X** | Show Cross References (交叉引用) |
| **Ctrl+Alt+S** | Search All Strings (搜索引用字符串) | **Ctrl+E** | Modify Hex Bytes (就地编辑内存) |
| **Ctrl+Alt+C** | Search All Calls (搜索跨模块调用) | **Ctrl+P** | Patch Manager (补丁管理与落盘) |
| **Alt+P** | Script Console (Python/Lua 控制台) | **Alt+S** | Focus Source View (聚焦源码浏览器) |
| **Ctrl+Shift+S** | Toggle Mixed ASM/Source (混合渲染切换) | **Alt+C** | Focus CPU / Disassembly (聚焦反汇编) |
| **Ctrl+S** | Save Project (.edb_db 项目保存) | **Ctrl+D** | Dump CPU State (导出机器状态快照) |
| **Shift+S** | Toggle Stack View (折叠/展开栈) | **Shift+F7/F8/F9** | 透传信号执行 (Pass Signal Step/Run) |
| **寄存器: `+` / `-` / `0`** | GPR 快速增减/清零 | **栈视图: `Space` / `Ctrl+G`** | 就地改写 QWORD / 跳转栈地址 |
| **转储: `Enter` / 双击** | 就地极速改写内存字节 | **Follow in Dump 1~4** | 多标签定向转储路由 |

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
