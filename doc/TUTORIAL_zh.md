# edb-next 官方完全使用教程与插件开发指南

> **项目名称**：edb-next (Next-Generation Linux Binary Debugger & Reverse Engineering Platform)  
> **适用版本**：v1.0.0+  
> **适用平台**：Linux x86_64  
> **开发语言**：C++20 / Qt 5.15+ / Capstone Engine

---

## 目录

1. [环境准备与编译构建 (Build & Setup)](#1-环境准备与编译构建-build--setup)
   - 1.1 系统依赖与开发工具链
   - 1.2 编译构建操作
   - 1.3 运行全量自动化验证套件
2. [快速入门与会话管理 (Quick Start & Sessions)](#2-快速入门与会话管理-quick-start--sessions)
   - 2.1 启动图形化调试器
   - 2.2 加载目标程序 (Open Target)
   - 2.3 配置命令行参数与工作目录 (Launch Arguments)
   - 2.4 附加已有进程 (Attach to Process)
   - 2.5 重启、脱钩与销毁会话
3. [核心 4 象限黄金工作台深度实操 (4-Quadrant Workspace)](#3-核心-4-象限黄金工作台深度实操-4-quadrant-workspace)
   - 3.1 象限 1：反汇编视图 (DisassemblyView)
   - 3.2 象限 2：寄存器视图 (RegisterView)
   - 3.3 象限 3：4 路内存转储容器 (MultiDumpWidget)
   - 3.4 象限 4：专有 64 位调用栈 (Dedicated StackView)
4. [高级执行控制与调试技巧 (Advanced Debugging)](#4-高级执行控制与调试技巧-advanced-debugging)
   - 4.1 步入、步过、跳出与运行到光标
   - 4.2 软件断点、硬件断点与递归下降条件断点
   - 4.3 命中间隔 (Ignore Count) 与仅日志断点 (Log Only)
   - 4.4 POSIX 信号拦截、放行与透传执行
   - 4.5 底部动态分支预测 (Dynamic Branch Prediction)
   - 4.6 代码执行覆盖率 (Hit Trace) 与时间旅行单步回溯 (Run Trace)
   - 4.7 CPU 机器状态全景快照导出 (StateDumper)
5. [高级逆向分析工具箱实战 (Advanced Reverse Engineering)](#5-高级逆向分析工具箱实战-advanced-reverse-engineering)
   - 5.1 Glibc ptmalloc 堆内存深度解析 (HeapView)
   - 5.2 ROP Gadget 漏洞挖掘与 Python Payload 导出 (ROPToolView)
   - 5.3 交互式基本块控制流有向图 (CFGGraphView)
   - 5.4 跨模块共享库 API 外呼快速定位 (IntermodularCallsView)
   - 5.5 指令序列与操作码特征检索 (OpcodeSearcherView)
   - 5.6 深度 ELF 文件结构与动态依赖解析 (BinaryInfoView)
   - 5.7 Linux 进程环境与软链接句柄内省 (ProcessPropertiesView)
   - 5.8 随手记 Notes 分析草稿本 (NotesView)
6. [突破只读内存、热补丁与物理 ELF 磁盘落盘 (Patching & Disk Export)](#6-突破只读内存热补丁与物理-elf-磁盘落盘-patching--disk-export)
   - 6.1 突破内存只读屏障：远程系统调用注入 (remoteMprotect)
   - 6.2 目标地址空间动态分配内存 (remoteMmap)
   - 6.3 内存字节就地编辑与快速 NOP 填充
   - 6.4 集中补丁管理中心 (PatchManagerDialog)
   - 6.5 一键脱壳/破解后物理可执行文件落盘 (patchFileToDisk)
7. [项目工程数据库无感持久化 (.edb_db)](#7-项目工程数据库无感持久化-edb_db)
8. [x64dbg 风格常驻极客命令行控制台 (CommandBar CLI)](#8-x64dbg-风格常驻极客命令行控制台-commandbar-cli)
9. [现代化 C++20 插件编写与使用全指南 (Plugin Development)](#9-现代化-c20-插件编写与使用全指南-plugin-development)
   - 9.1 插件架构与工作原理
   - 9.2 插件工程目录与 CMakeLists.txt 规范
   - 9.3 编写插件头文件 (`.hpp`)
   - 9.4 编写插件实现源码 (`.cpp`)
   - 9.5 编译生成插件共享库 (`.so`)
   - 9.6 插件安装、加载与实机测试

---

## 1. 环境准备与编译构建 (Build & Setup)

`edb-next` 针对现代 64 位 Linux 平台设计，采用现代 C++20 标准与 Qt 5.15+ 构建，依赖极度精简且高度标准化。

### 1.1 系统依赖与开发工具链

支持主流 64 位 Linux 发行版（Ubuntu 20.04/22.04/24.04、Debian 11/12、Fedora 36+、Arch Linux 等）。

#### Ubuntu / Debian 安装命令：
```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    qtbase5-dev \
    libqt5widgets5 \
    libcapstone-dev
```

#### Fedora / RHEL 安装命令：
```bash
sudo dnf install -y \
    gcc-c++ \
    cmake \
    git \
    pkgconf-pkg-config \
    qt5-qtbase-devel \
    capstone-devel
```

#### Arch Linux 安装命令：
```bash
sudo pacman -S --needed \
    base-devel \
    cmake \
    git \
    pkgconf \
    qt5-base \
    capstone
```

---

### 1.2 编译构建操作

```bash
# 1. 切换至项目根目录
cd /home/eddy/myplace/project/edb-debugger/edb-next

# 2. 配置 CMake 构建系统 (推荐使用 Release 模式获取最高执行性能)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 3. 启用全核并发编译
cmake --build build -j$(nproc)
```

编译成功完成后，`build/` 目录将输出以下核心产物：
- `build/edb_next`：**主程序图形化可执行文件**；
- `build/libedb_core.a`：底层调试核心静态库；
- `build/plugins/sample_plugin.so`：官方参考扩展插件；
- `build/test_core`：核心能力全量回归测试套件；
- `build/test_advanced`：高阶逆向特性全量回归测试套件；
- `build/test_exit`：窗口析构防崩溃压力测试套件。

---

### 1.3 运行全量自动化验证套件

在正式调试目标前，可以运行测试套件验证当前系统环境各项底层机制（如 ptrace 权限、内存映射与系统调用注入）是否正常：

```bash
# 运行基础核心测试 (覆盖断点状态机、单步、多会话、ELF解析、调用栈、堆分析、汇编器等)
./build/test_core

# 运行进阶特性测试 (覆盖偏好配置、补丁落盘、插件网关、Trace引擎、CFG、远程系统调用等)
./build/test_advanced

# 运行析构防崩溃压力测试
./build/test_exit
```
若全部测试输出 `>>> ALL UNIT TESTS PASSED SUCCESSFULLY! <<<`，表明调试内核运行环境完全就绪。

---

## 2. 快速入门与会话管理 (Quick Start & Sessions)

### 2.1 启动图形化调试器

在终端直接执行：
```bash
./build/edb_next
```
启动后主窗口将以现代暗黑极客主题展开，核心呈现经典四象限布局与底部常驻命令行交互栏。

---

### 2.2 加载目标程序 (Open Target)

1. 点击菜单栏 **File -> Open Target...**（或按下全局快捷键 **Ctrl+O**）；
2. 在文件弹窗中选择欲逆向调试的 64 位 Linux ELF 二进制程序（例如 `./build/test_target`）；
3. 系统将自动：
   - 派生子进程并调用 `PTRACE_TRACEME` 挂钩；
   - 原生调用 `personality(ADDR_NO_RANDOMIZE)` **一键禁用 ASLR**，确保基地址固定便于分析；
   - `ElfParser` 解析 `.symtab`、`.dynsym` 符号与各节区；
   - 左上反汇编视图自动跳转对齐至程序入口点（`main` 或 `_start`）；
   - 右上寄存器、右下栈视图与左下内存转储区瞬间联动刷新。

---

### 2.3 配置命令行参数与工作目录 (Launch Arguments)

如果被调试程序需要传递特定命令行参数（`argv`）或指定工作目录（`cwd`）：
1. 菜单栏选择 **Options -> Launch Arguments...**；
2. 在弹窗中配置：
   - **Arguments**：支持传入多个参数（如 `-v --config /tmp/test.conf`）；
   - **Working Directory**：配置目标进程启动时的 `cwd`；
3. 点击 **OK** 保存。下次启动或重启（`Ctrl+F2`）时将自动携带该参数执行。

---

### 2.4 附加已有进程 (Attach to Process)

如果要调试后台正在运行的守护进程或服务：
1. 点击菜单栏 **File -> Attach to Process...**；
2. 输入目标进程的 PID；
3. `edb-next` 将调用 `PTRACE_ATTACH` 中断目标进程，同步抓取其所有活动线程的寄存器状态并就地挂起，反汇编光标自动停在当前中断指令处。

---

### 2.5 重启、脱钩与销毁会话

- **重启会话 (Restart)**：按下快捷键 **Ctrl+F2**（或菜单 `Debug -> Restart`），调试器将优雅销毁旧进程并以相同参数瞬间重新拉起全新进程；
- **脱钩释放 (Detach)**：菜单 `Debug -> Detach`，清除所有植入的软件/硬件断点，将进程控制权交还操作系统全速运行；
- **强行销毁 (Terminate)**：菜单 `Debug -> Terminate`，向目标投递 `SIGKILL` 彻底终止。

---

## 3. 核心 4 象限黄金工作台深度实操 (4-Quadrant Workspace)

`edb-next` 彻底抛弃了原版反复切 Tab 的单抽屉设计，采用对标 x64dbg 的四象限同屏联动工作流：

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

### 3.1 象限 1：反汇编视图 (DisassemblyView)

- **行号与视觉标识**：
  - **当前执行行 (RIP)**：背景以深青色高亮，行首带有醒目翠绿粗体指示箭头 **`➔`**；
  - **断点行**：整行呈现深红背景；若当前 RIP 恰好停在断点上，则呈现复合强调色。
- **键盘极速分支导航 (Branch Navigation)**：
  - 在任何 `CALL`、`JMP`、`Jcc` 指令行上按下 **Enter** 键，光标将瞬间跳至目标分支地址，并将当前位置自动压入导航历史栈；
  - 按下 **Esc** 或 **Backspace**（或 **Alt+Left**），瞬间原路回退到跳转前的位置；按 **Alt+Right** 重新前进。彻底解决在多层嵌套函数逆向时迷失上下文的痛点。
- **代码交叉引用 (Code XREFs)**：
  - 光标停在某个函数或指令行，按下快捷键 **X**；
  - 弹出 **XRef Dialog**，列出所有调用或跳转到该地址的代码行（含 `CALL`、`JMP`、`LEA [rip+disp]`）；双击任一项直接瞬移至调用处。
- **原生内联汇编 (Inline Assembler)**：
  - 光标停在欲修改的指令行，按下快捷键 **Space**（空格键）；
  - 弹出汇编对话框，输入标准 Intel 汇编指令（如 `xor eax, eax` 或 `mov rdi, 1`）；
  - 勾选 **"Auto Fill with NOPs"**，汇编器将基于 GNU `as` + `objcopy` 动态编译，并在指令长度不足时自动用 `0x90` 铺满，确保后续指令地址完全对齐。
- **强制改变执行指针 (Set RIP)**：
  - 右键任意反汇编行，选择 **"Set New Origin Here (Set RIP)"**（或快捷键 **Ctrl+\***）；
  - 调试器将原子修改物理 CPU 的 RIP 寄存器并写回内核，强制目标从该行开始执行。

---

### 3.2 象限 2：寄存器视图 (RegisterView)

- **通用寄存器表 (GPRs)**：
  - 表格包含 4 列：`Register`（寄存器名称）、`Hex Value`（十六进制）、`Comment / Dereference`（智能推导）、`Decimal`（十进制）；
  - **数值变动高亮**：单步执行时，发生变动的寄存器数值立即标红；
  - **智能解引用推导**：
    - 自动匹配就近函数符号与偏移（如 `<calculate_fib+0x12>`）；
    - 栈顶指针感知：自动标注 `=> [RSP] (Stack Top)`、`=> [RBP] (Base Pointer)`；
    - 内存字符串探测：如果寄存器指向有效字符串，直接打印预览（如 `"Hello world"`）；
    - 二级指针链推导：直观呈现 `-> 0x555555555120 <main>`。
  - **就地双击修改**：双击数值直接输入新的十六进制数写入物理寄存器。
- **EFLAGS 互动翻转徽章条**：
  - 顶部常驻 `CF`, `PF`, `AF`, `ZF`, `SF`, `TF`, `IF`, `DF`, `OF` 按钮；
  - 翡翠绿色代表置位（1），暗灰色代表清零（0）；
  - **鼠标单击徽章即可物理翻转该标志位**（例如破解跳转前单击 `ZF`，使其直接取反）。
- **SSE / AVX XMM 向量面板**：
  - 切换到 `FPU / SSE` 选项卡，同时呈现 16 个 128 位 XMM 寄存器；
  - 每行同时展开为 128 位十六进制、4 个 32 位浮点数与 2 个 64 位双精度浮点数，支持双击修改。

---

### 3.3 象限 3：4 路内存转储容器 (MultiDumpWidget)

- **多标签页独立并行监视 (Dump 1 ~ Dump 4)**：
  - 彻底告别单一转储区限制，拥有 4 个互相独立的内存标签页；
  - 每个 Dump 页拥有完全独立的起始地址、滚动游标与历史记录；
  - 在反汇编、寄存器或栈视图右键点击 **"Follow in Dump"**，数据将自动流入当前选中的 Dump 标签页。
- **十六进制就地热补丁**：
  - 选中欲修改的字节，按下快捷键 **Ctrl+E**；
  - 实时输入新的十六进制字节序列并确认，修改立即生效于目标内存并被 `PatchManager` 自动记录；
  - 右键支持 **"Fill with Zeros"**（全填 0）与 **"Fill with NOPs"**（全填 0x90）。
- **内存段物理转储 (.bin)**：
  - 切换至 Tab 5 **Memory Regions**（内存区域表）；
  - 右键任意内存段（如堆段、数据段或动态库），选择 **"Dump Region to File (.bin)..."**，秒级导出完整内存快照。

---

### 3.4 象限 4：专有 64 位调用栈 (Dedicated StackView)

- **工业级 8 字节 QWORD 单独成行**：
  - **Address 列**：青色展示物理虚拟栈地址（如 `0x00007fffffffd7d0`）；
  - **Value 列**：高亮呈现 64 位整字值；**双击智能路由**：若数值是代码段地址，反汇编视图自动跳转跟随；若是指向数据的指针，Dump 视图自动跳转跟随；
  - **Offset 列**：动态计算相对当前 RSP 的偏移，当前栈顶整行翠绿高亮标注 **`=> RSP`**，后续行依次计算 `+0x08`, `+0x10`；当遇到 RBP 时自动标注 `[RBP]` 及 `[RBP-0x08]`；
  - **Symbol / Comment 列**：自动解析返回地址对应的函数符号及局部字符串。
- **快捷操作条**：
  - 点击 **`[RSP]`** 按钮：一键瞬间归位回栈顶；
  - 点击 **`[RBP]`** 按钮：一键跳转至基址指针；
  - 按下快捷键 **Shift+S**：一键快速展开/折叠右下栈视图，将垂直空间让给反汇编。

---

## 4. 高级执行控制与调试技巧 (Advanced Debugging)

### 4.1 步入、步过、跳出与运行到光标

| 动作 | 快捷键 | 技术机制与行为 |
| :--- | :---: | :--- |
| **Continue (继续运行)** | **F9** | 全速恢复目标运行，直到命中下一个断点或信号 |
| **Step Into (单步步入)** | **F7** | 物理单步执行当前一条指令，若遇到 `call` 则跟入函数体内部 |
| **Step Over (单步步过)** | **F8** | 单步执行当前指令；若遇到 `call`，自动在函数返回的下一行打下临时断点全速越过 |
| **Step Out (单步跳出)** | **Shift+F11** | 安全解析 CallStack Frame #1 的返回地址并打下临时断点，运行直到跳出当前函数 |
| **Run to Selection (运行到光标)**| **F4** | 在当前选中的反汇编行打下一次性临时断点并恢复全速运行，命中后自动销毁该断点 |
| **Pause (暂停)** | 菜单 / 工具栏 | 异步向目标投递中断请求，立即将所有线程挂起 |

---

### 4.2 软件断点、硬件断点与递归下降条件断点

- **普通软件断点 (INT3 0xCC)**：
  - 在反汇编行直接双击，或按下 **F2** 切换断点；
  - 底层自动管理指令首字节替换，恢复时自动通过先单步后恢复状态机越过。
- **硬件断点 (Hardware Breakpoint DR0~DR3)**：
  - 右键反汇编行，展开 **"Hardware Breakpoint"** 子菜单；
  - 支持设置 **Execute (执行)**、**Write 1/2/4/8 Bytes (写监视)**、**Read/Write 1/2/4/8 Bytes (读写监视)**；
  - 硬件断点无需修改目标内存字节码，完全由 CPU 调试寄存器硬件拦截，专门用于破解只读内存或自校验壳。
- **递归下降条件断点 (Conditional Breakpoint)**：
  - 切换至 Tab 4 **Breakpoint Manager**，在断点上右键选择 **"Set Condition..."**；
  - 输入表达式，支持寄存器、常数、指针解引用与逻辑比较：
    - `rdi == 7`
    - `[rbp - 8] > 0`
    - `rax != 0 && rbx <= 100`
  - 只有当表达式求值为 True 时调试器才会挂起 UI；若为 False 则静默放行。

---

### 4.3 命中间隔 (Ignore Count) 与仅日志断点 (Log Only)

- **命中间隔 (Ignore Count)**：
  - 在断点右键选择 **"Set Ignore Count..."**，输入数字（如 `500`）；
  - 调试器将忽略前 500 次命中，第 501 次时才会挂起，极度适合调试深层循环或高频 API 调用。
- **仅日志断点 (Log Only / Trace Point)**：
  - 在断点右键勾选 **"Log Only"** 并输入格式化串（如 `Hit func! RAX={rax}, RDI={rdi}`）；
  - 断点命中时**绝对不暂停目标执行**，而是自动将求值结果静默打印至系统调试日志中，相当于在不修改源码的情况下动态注入 `printf`！

---

### 4.4 POSIX 信号拦截、放行与透传执行

- **全局信号策略配置**：
  - 打开 **Options -> Preferences -> Signals**；
  - 针对 1~64 号 Linux POSIX 信号（如 SIGSEGV、SIGTRAP、SIGINT、SIGPIPE），精确配置其拦截（Intercept）与放行（Pass）策略。
- **透传执行动作**：
  - 当目标遭遇信号停下时，若需要将信号交由程序自身的异常处理函数（Signal Handler）处理，使用透传快捷键：
    - **Shift+F7**：Step Into Pass Signal
    - **Shift+F8**：Step Over Pass Signal
    - **Shift+F9**：Run Pass Signal

---

### 4.5 底部动态分支预测 (Dynamic Branch Prediction)

在调试条件跳转指令（如 `je`, `jne`, `jg`, `jle`）时，主工作台底部常驻有动态预测状态条：
- `InstructionInspector` 实时解算当前条件跳转依赖的 EFLAGS 标志位（ZF, SF, OF, CF）；
- 实时给出高亮提示：
  - 翠绿色提示：**`[JUMP TAKEN] (Branch will be followed to 0x...)`**
  - 暗灰色提示：**`[JUMP NOT TAKEN] (Execution will fall through)`**
- 无需心算标志位，一眼即可预知分支走向。

---

### 4.6 代码执行覆盖率 (Hit Trace) 与时间旅行单步回溯 (Run Trace)

切换至 Tab 13 **Trace** 选项卡：
- **Hit Trace (代码覆盖率)**：
  - 勾选启用 Hit Trace，目标程序运行过程中，所有被 CPU 执行过的代码行都会在反汇编视图中标记为淡绿色高亮；
  - 点击 **"Clear Hit Trace"** 可随时重置统计，便于分析特定算法分支是否被触发。
- **Run Trace 与时间旅行历史穿梭 (Time-Travel)**：
  - 勾选启用 Run Trace，系统将自动记录单步调试走过的每一帧历史；
  - 界面提供 **`< Step Back`** 与 **`Step Forward >`** 按钮；
  - 点击 **`< Step Back`**，反汇编光标与寄存器视图将瞬间穿越回上一帧，并标红呈现与当前帧的变动差分，实现时间旅行式逆向回溯。

---

### 4.7 CPU 机器状态全景快照导出 (StateDumper)

在任何调试中断时刻，按下快捷键 **Ctrl+D**（或菜单 `Debug -> Dump CPU State (DumpState)`）：
- 系统将瞬间生成当前时刻的精细报告，输出至系统日志并自动复制到剪贴板：
  ```text
  ================ CPU Machine State Snapshot ================
  Target Process : PID 10645 (Paused)
  Instruction Ptr: 0x0000555555555297 <calculate_fib>

  [General Purpose Registers]
  RAX = 0x0000000000000000   RBX = 0x1234567890abcdef   RCX = 0x00007ffff7fc5880
  RDX = 0x00007fffffff8e78   RSI = 0x00007fffffff8e68   RDI = 0x0000000000000007
  RBP = 0x00007fffffff8e60   RSP = 0x00007fffffff8e58   R8  = 0x0000000000000000
  R9  = 0x00007ffff7fc9040   R10 = 0x00007ffff7fc59a8   R11 = 0x0000000000000246
  R12 = 0x00007fffffff8e68   R13 = 0x0000555555555297   R14 = 0x0000555555557df8
  R15 = 0x00007ffff7ffd040

  [Flags (RFLAGS: 0x00000246)]
  CF=0  PF=1  AF=0  ZF=1  SF=0  TF=0  IF=1  DF=0  OF=0

  [Disassembly Context]
     0x555555555291: 66 2e 0f 1f 84 00...  nop word ptr cs:[rax + rax]
  => 0x555555555297: f3 0f 1e fa           endbr64  <calculate_fib>
     0x55555555529b: 55                    push rbp
     0x55555555529c: 48 89 e5              mov rbp, rsp
     0x55555555529f: 48 83 ec 18           sub rsp, 0x18

  [Stack Top (RSP: 0x00007fffffff8e58)]
     [RSP+0x00]: 0x00005555555553db <main+0x98>
     [RSP+0x08]: 0x0000000700000002
  ============================================================
  ```

---

## 5. 高级逆向分析工具箱实战 (Advanced Reverse Engineering)

在主工作台左下角，内置了 18 个按需切换的高级分析抽屉：

### 5.1 Glibc ptmalloc 堆内存深度解析 (HeapView - Tab 9)
- 点击 Tab 9 **Heap**，自动定位进程中的 `[heap]` 虚拟段；
- 按照 glibc 内存布局解析 `malloc_chunk` 链表；
- 清晰列出每一个堆块的：起始地址、用户数据区指针、大小（Size）、状态（`Allocated` / `Free` / `Top Chunk`）以及标志位（`A|M|P`）；
- **双击任何堆块**，Dump 视图瞬间跳转至该堆块的用户数据区，堆漏洞挖掘直观高效。

---

### 5.2 ROP Gadget 漏洞挖掘与 Python Payload 导出 (ROPToolView - Tab 11)
- 切换至 Tab 11 **ROP Tool**，点击 **"Scan Gadgets"**；
- 逆向滑动窗口快速提取代码段中所有以 `ret`、`syscall`、`int 0x80` 结尾的短指令序列；
- 智能分类检索：`StackPivot`（栈迁移）、`Syscall`（系统调用）、`MemoryWrite`（写内存）、`Arithmetic`（算术运算）；
- 选中任意 Gadget，点击 **"Copy Python Payload"**，自动生成格式化利用代码并复制到剪贴板：
  ```python
  # 0x000055555555501a : pop rdi ; ret
  payload += p64(0x000055555555501a)
  ```

---

### 5.3 交互式基本块控制流有向图 (CFGGraphView - Tab 14)
- 切换至 Tab 14 **CFG Graph**，点击 **"Generate CFG"**；
- 自动分析当前函数的控制流拓扑并渲染为有向图：
  - 绿色连线：条件满足跳转的分支（True）；
  - 红色连线：条件不满足直行的分支（False / Fall-through）；
  - 蓝色连线：无条件跳转（Unconditional Jump）；
- 支持鼠标拖拽平移、滚轮缩放；双击基本块节点，主反汇编视图瞬间对齐至该块首地址。

---

### 5.4 跨模块共享库 API 外呼快速定位 (IntermodularCallsView - Tab 18)
- 切换至 Tab 18 **Intermodular Calls**，点击 **"Scan Calls"**；
- 快速列出主程序代码对外部动态链接库（如 `libc.so.6`）的所有 PLT/GOT 函数外呼（如 `printf`, `malloc`, `free`, `socket`）；
- 支持在顶部文本框输入 API 名称进行模糊过滤；双击任一行直达调用现场。

---

### 5.5 指令序列与操作码特征检索 (OpcodeSearcherView - Tab 19)
- 切换至 Tab 19 **Opcode Searcher**；
- 下拉选框预置了常见的漏洞利用指令特征：
  - `JMP <reg>`
  - `CALL <reg>`
  - `PUSH <reg>; RET`
  - `POP <reg>; RET`
  - `Syscall / Interrupt`
- 支持输入自定义正则过滤；双击结果直接联动反汇编定位。

---

### 5.6 深度 ELF 文件结构与动态依赖解析 (BinaryInfoView - Tab 17)
- 深度解析并树状呈现：
  - **ELF Header**：架构、入口点、程序头/节头偏移；
  - **Program Headers (Segments)**：段权限（R/W/X）、物理偏移与虚拟内存映射；
  - **Section Headers**：`.text`, `.rodata`, `.data`, `.bss`, `.plt`, `.got` 大小与地址；
  - **Dynamic Tags**：展开所有 `DT_NEEDED` 依赖的 `.so` 动态库。

---

### 5.7 Linux 进程环境与软链接句柄内省 (ProcessPropertiesView - Tab 8)
- 遍历并解析目标进程在 `/proc/<pid>/` 下的运行状态：
  - 完整的命令行启动参数（`cmdline`）；
  - 完整的环境变量键值对表（`environ`）；
  - 遍历 `/proc/<pid>/fd/` 并解引用软链接，智能识别普通文件、网络套接字（Socket）、管道（Pipe）与伪终端（PTY）。

---

### 5.8 随手记 Notes 分析草稿本 (NotesView - Tab 15)
- 专为逆向分析师集成的 Monospace 随笔编辑器；
- 点击 **"Insert RIP"**：自动插入当前调试指令地址；
- 点击 **"Insert Timestamp"**：插入当前时间；
- 支持保存至工程数据库或一键导出为标准 Markdown（`.md`）分析报告。

---

## 6. 突破只读内存、热补丁与物理 ELF 磁盘落盘 (Patching & Disk Export)

在传统逆向工具中，修改了内存字节后往往无法写回磁盘文件，且面对只读段（如 `.rodata` 或无写权限的 `.text`）时直接报错。`edb-next` 打造了完整的**分析、突破、修改、落盘**闭环工作流：

### 6.1 突破内存只读屏障：远程系统调用注入 (remoteMprotect)

当需要在只读段修改代码或打补丁时：
1. 切换至 Tab 5 **Memory Regions**；
2. 找到目标内存段，右键选择 **"Change Page Permissions (mprotect)..."**；
3. 勾选 **Read**、**Write**、**Execute** 并确认；
4. `edb-next` 底层自动通过 `executeRemoteSyscall` 在目标进程中注入执行 `SYS_mprotect`，就地将虚拟页权限修改为 **RWX**，彻底打破保护限制！

*(亦可通过底部 CLI 命令行执行：`mprotect 0x555555555000 4096 7`)*

---

### 6.2 目标地址空间动态分配内存 (remoteMmap)

在需要注入一段独立的 Shellcode 或构造 Hook 跳转中继（Trampoline）时：
1. 内存区域表右键选择 **"Allocate Target Memory (mmap)..."**，输入分配大小（如 `4096` 字节）；
2. 目标进程空间将动态分配出一块独立的、具备可执行权限的匿名内存页，并返回其虚拟地址；
3. 随后即可使用内存编辑功能向该地址写入自定义代码。

*(亦可通过底部 CLI 命令行执行：`alloc 4096 7`)*

---

### 6.3 内存字节就地编辑与快速 NOP 填充

1. 在反汇编视图或 Hex Dump 视图中，按下快捷键 **Ctrl+E**；
2. 直接输入欲修改的十六进制字节；或者按下 **Space** 输入汇编指令，系统将自动汇编并覆盖写入；
3. 右键可随时点击 **"Fill with NOPs"** 将选定指令全部填充为 `0x90`。

---

### 6.4 集中补丁管理中心 (PatchManagerDialog)

1. 按下快捷键 **Ctrl+P**（或菜单 `Debug -> Manage Patches...`）；
2. 弹窗列出本次逆向会话中产生的所有内存修改记录；
3. 每条记录清晰对比：修改地址、所属段、原始字节（Original Bytes）与补丁字节（Patched Bytes）；
4. 支持选中单项点击 **"Revert Patch"**（撤销补丁恢复原貌），或 **"Reapply Patch"**（重新应用）。

---

### 6.5 一键脱壳/破解后物理可执行文件落盘 (patchFileToDisk)

1. 在 **Patch Manager** 弹窗中，点击右下角 **"Patch File to Disk"** 按钮；
2. 指定输出文件路径（例如 `/path/to/target_cracked`）；
3. `edb-next` 将自动调用 `patchFileToDisk` 算法：
   - 读取原二进制的 ELF `Program Headers` (`PT_LOAD`)；
   - 自动计算公式 $\text{FileOffset} = VAddr - Segment.p\_vaddr + Segment.p\_offset$；
   - 将全部内存补丁原汁原味地覆写回磁盘二进制文件，并赋予 `0755` 物理可执行权限；
4. 此时新生成的可执行文件即可脱离调试器，在原生 Linux 终端下独立运行！

---

## 7. 项目工程数据库无感持久化 (.edb_db)

为了彻底根除“退出调试器后辛苦写下的注释、书签与断点全部丢失”的行业痛点，`edb-next` 打造了标准化 JSON 逆向数据库架构：

1. **持久化范畴**：
   - 所有在反汇编行编写的指令注释（快捷键 **;**）；
   - 所有打下的金黄色五角星书签（快捷键 **Ctrl+B**）；
   - 所有软件与硬件断点配置（含触发条件表达式、命中间隔、仅日志格式串）；
   - 动态监视表达式列表（Watches）；
   - 内存补丁修改历史记录；
   - Notes 标签页中的分析草稿随笔。
2. **保存项目数据库**：
   - 菜单选择 `File -> Save Project Database`（快捷键 **Ctrl+S**）；
   - 系统将在目标程序同级目录下生成 `<binary_name>.edb_db` 文件。
3. **全自动无感恢复**：
   - 下次使用 `edb-next` 重新打开相同的二进制程序时，系统会自动探测同目录下是否存在同名 `.edb_db` 文件；
   - 若存在，无需任何手动点选，**毫秒级全自动无感恢复全量分析环境**！

---

## 8. x64dbg 风格常驻极客命令行控制台 (CommandBar CLI)

主界面最底部常驻交互式命令行控制台。支持使用键盘上下箭头穿梭历史命令：

| 命令语法 | 简写 / 别名 | 功能说明与操作示例 |
| :--- | :--- | :--- |
| `bp <addr \| symbol>` | `bp` | 在指定地址或函数名下软件断点。例：`bp main`、`bp 0x401000` |
| `bph <addr>` | `bph` | 在指定物理地址下硬件执行断点。例：`bph 0x401000` |
| `bc <addr>` | `bc` | 清除指定地址上的断点。例：`bc 0x401000` |
| `bd <addr>` | `bd` | 禁用指定地址上的断点。例：`bd 0x401000` |
| `be <addr>` | `be` | 启用指定地址上的断点。例：`be 0x401000` |
| `r <reg> <val>` | `r` | 修改指定寄存器的数值。例：`r rax 0x1337`、`r rdi 0` |
| `d <addr>` | `d` | 让当前活动 Dump 标签页跳转至指定地址。例：`d 0x7fffffffd7d0` |
| `u <addr \| symbol>` | `u` | 让反汇编视图跳转至指定地址或函数。例：`u calculate_fib` |
| `step` | `s` | 单步步入（等同于 F7） |
| `stepo` | `so` | 单步步过（等同于 F8） |
| `ret` | `rto` | 单步跳出当前函数（等同于 Shift+F11） |
| `run` | `g` | 继续运行（等同于 F9） |
| `eval <expression>` | `?` | 动态求值复杂表达式。例：`eval rax + 0x20`、`eval [rbp-8]` |
| `mprotect <addr> <size> <prot>`| `mprot`| 动态修改目标内存权限（prot: 7=RWX, 5=RX, 3=RW, 1=RO）。例：`mprotect 0x555555555000 4096 7` |
| `alloc <size> [prot]` | `malloc`| 在目标进程动态分配内存页。例：`alloc 4096 7` |
| `free <addr> <size>` | `free` | 释放目标进程中动态分配的内存。例：`free 0x7ffff7fbc000 4096` |
| `dumpstate` | `dps` | 导出当前 CPU 完整快照并复制到剪贴板 |
| `help` | `?` | 打印出当前所有内置及外部插件注册的 CLI 命令列表 |

---

## 9. 现代化 C++20 插件编写与使用全指南 (Plugin Development)

`edb-next` 拥有一套现代、安全、解耦的 C++20 插件体系。插件被编译为标准的 Linux 共享库（`.so`），由宿主通过 `QPluginLoader` 动态装载。

### 9.1 插件架构与工作原理

插件只需要实现纯虚接口契约 [IPlugin](file:///home/eddy/myplace/project/edb-debugger/edb-next/core/IPlugin.hpp)，并通过 [IPluginContext](file:///home/eddy/myplace/project/edb-debugger/edb-next/core/IPluginContext.hpp) 能力网关与宿主进行交互：
- **能力网关隔离**：插件无法直接破坏调试器的核心私有指针，必须通过 `context->activeSession()` 进行内存读写、寄存器访问与会话控制；
- **能力扩展维度**：
  1. **主菜单注入**：向主窗口 `Plugins` 菜单挂载自定义动作与弹窗；
  2. **CLI 命令行注册**：向底栏 CommandBar 注册自定义命令；
  3. **事件总线监听**：挂接断点触发（Breakpoint Hit）与会话状态改变（Session State Changed）钩子；
  4. **视图右键注入**：向反汇编、寄存器、内存 Dump 或栈视图注入右键菜单项；
  5. **偏好设置扩展**：向 `PreferencesDialog` 注入专属配置选项卡。

---

### 9.2 插件工程目录与 CMakeLists.txt 规范

建议将新插件创建为独立工程目录，例如 `MyPlugin/`：

```text
MyPlugin/
├── CMakeLists.txt              # CMake 构建定义
├── MyPlugin.hpp                # 插件声明头文件
└── MyPlugin.cpp                # 插件业务实现
```

#### 标准 `CMakeLists.txt` 内容编写：
```cmake
cmake_minimum_required(VERSION 3.16)
project(MyPlugin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui)

# 引入 edb-next 核心头文件路径
set(EDB_NEXT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
include_directories(
    ${EDB_NEXT_ROOT}
    ${EDB_NEXT_ROOT}/core
    ${EDB_NEXT_ROOT}/ui
)

# 声明为 SHARED 动态共享库
add_library(my_plugin SHARED
    MyPlugin.cpp
)

set_target_properties(my_plugin PROPERTIES
    PREFIX ""                     # 去除 lib 前缀，输出 my_plugin.so
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

target_link_libraries(my_plugin
    PRIVATE
    Qt5::Widgets
    Qt5::Core
)
```

---

### 9.3 编写插件头文件 (`MyPlugin.hpp`)

```cpp
#pragma once

#include "core/IPlugin.hpp"
#include "core/IPluginContext.hpp"
#include <QObject>
#include <QMenu>
#include <QWidget>

namespace my_plugin {

class MyPlugin : public QObject, public edb_next::IPlugin {
    Q_OBJECT
    // 关键宏：声明 Qt 插件元数据与接口契约
    Q_PLUGIN_METADATA(IID EDB_NEXT_PLUGIN_IID)
    Q_INTERFACES(edb_next::IPlugin)

public:
    MyPlugin() = default;
    ~MyPlugin() override = default;

    // 1. 定义插件元数据
    [[nodiscard]] edb_next::PluginMetadata metadata() const override {
        return edb_next::PluginMetadata{
            .id = "my_awesome_plugin",
            .name = "My Awesome Reverse Engineering Tool",
            .version = "1.0.0",
            .author = "Security Researcher",
            .description = "Demonstrates custom menu, CLI command, and breakpoint hook."
        };
    }

    // 2. 生命周期接口
    bool initialize(edb_next::IPluginContext* context) override;
    void shutdown() override;

    // 3. 界面与功能挂接接口
    QMenu* createMenu(QWidget* parent = nullptr) override;
    std::vector<QAction*> contextMenuItems(edb_next::ContextMenuTarget target, QWidget* parent = nullptr) override;
    QWidget* createOptionsPage(QWidget* parent = nullptr) override;

private:
    edb_next::IPluginContext* ctx_{nullptr};
};

} // namespace my_plugin
```

---

### 9.4 编写插件实现源码 (`MyPlugin.cpp`)

```cpp
#include "MyPlugin.hpp"
#include <QMessageBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QCheckBox>

namespace my_plugin {

bool MyPlugin::initialize(edb_next::IPluginContext* context) {
    ctx_ = context;
    if (!ctx_) return false;

    // A. 向 CommandBar 注册自定义极客命令行指令: my_search <hex_pattern>
    ctx_->registerCommand("my_search", [this](const std::vector<std::string>& args) {
        if (args.empty()) {
            ctx_->logMessage("[MyPlugin] Usage: my_search <pattern>");
            return;
        }
        ctx_->logMessage(QString("[MyPlugin] Executing custom search for pattern: %1")
                         .arg(QString::fromStdString(args[0])));
        
        // 可通过 ctx_->activeSession() 访问当前调试引擎读取内存
        auto session = ctx_->activeSession();
        if (session && session->state() == edb_next::SessionState::Paused) {
            uint64_t rip = session->registers().rip();
            ctx_->logMessage(QString("[MyPlugin] Current RIP is at 0x%1").arg(rip, 0, 16));
        }
    }, "my_search <pattern> - Custom memory search command provided by MyPlugin");

    // B. 挂接断点拦截钩子 (Hook Breakpoint Hit)
    ctx_->registerDebugEventListener([this](const edb_next::DebugEvent& ev) {
        if (ev.reason == edb_next::StopReason::Breakpoint) {
            ctx_->logMessage(QString("[MyPlugin Hook] Target hit breakpoint at: 0x%1!")
                             .arg(QString::fromStdString(ev.address.toHex())));
        }
    });

    ctx_->logMessage("[MyPlugin] My Awesome Plugin initialized successfully!");
    return true;
}

void MyPlugin::shutdown() {
    if (ctx_) {
        ctx_->logMessage("[MyPlugin] Shutting down cleanly.");
        ctx_ = nullptr;
    }
}

// C. 注入主菜单栏 (自动添加至主窗口 [Plugins] 菜单下)
QMenu* MyPlugin::createMenu(QWidget* parent) {
    auto* menu = new QMenu("My Awesome Tool", parent);

    auto* actHello = menu->addAction("Scan Secret Memory...");
    connect(actHello, &QAction::triggered, [this, parent]{
        QMessageBox::information(parent, "MyPlugin", "Scanning target memory for sensitive strings...");
        if (ctx_) {
            ctx_->logMessage("[MyPlugin] User initiated Scan Secret Memory.");
        }
    });

    return menu;
}

// D. 注入反汇编视图右键菜单
std::vector<QAction*> MyPlugin::contextMenuItems(edb_next::ContextMenuTarget target, QWidget* parent) {
    std::vector<QAction*> actions;
    if (target == edb_next::ContextMenuTarget::Disassembly) {
        auto* act = new QAction("MyPlugin: Check ROP Safety", parent);
        connect(act, &QAction::triggered, [this]{
            if (ctx_) {
                ctx_->logMessage("[MyPlugin] ROP Safety check triggered on current instruction.");
            }
        });
        actions.push_back(act);
    }
    return actions;
}

// E. 注入偏好设置配置页
QWidget* MyPlugin::createOptionsPage(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel("<b>My Awesome Plugin Options</b>"));
    layout->addWidget(new QCheckBox("Enable deep memory heuristic inspection", page));
    layout->addStretch();
    page->setWindowTitle("Awesome Tool");
    return page;
}

} // namespace my_plugin
```

---

### 9.5 编译生成插件共享库 (`.so`)

使用 CMake 进行编译：
```bash
cd MyPlugin/
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```
构建完成后，将在构建目录下生成插件动态库文件：`my_plugin.so`。

---

### 9.6 插件安装、加载与实机测试

`edb-next` 支持两种插件加载方式：

#### 方式 1：自动加载（推荐）
将生成的 `my_plugin.so` 文件复制到用户的插件默认配置目录下：
```bash
mkdir -p ~/.config/edb-next/plugins
cp build/plugins/my_plugin.so ~/.config/edb-next/plugins/
```
启动 `./build/edb_next`，程序在初始化阶段将自动扫描该目录并自动加载插件。

#### 方式 2：图形界面手动热加载
1. 打开主界面菜单栏 **Plugins -> Manage Plugins...**；
2. 弹出的 **Plugin Manager** 对话框中，点击 **"Load Plugin..."** 按钮；
3. 选择刚才编译出的 `my_plugin.so`；
4. 插件将瞬间热装载入当前调试器！

#### 验证插件功能：
1. 查看菜单栏 **Plugins**：可以看到新增了二级子菜单 **`My Awesome Tool -> Scan Secret Memory...`**；
2. 点击反汇编视图右键：可以看到新增了 **`MyPlugin: Check ROP Safety`** 上下文选项；
3. 在底部 **CommandBar** 输入指令：
   ```text
   my_search secret_token
   ```
   回车执行，在 Tab 16 **Log** 视图中即可看到插件打印出的响应信息与当前 RIP 地址！

---

## 10. 常用快捷键一览表 (Cheat Sheet)

| 快捷键 | 功能描述 | 对应分类 |
| :--- | :--- | :--- |
| **F9** | Continue (继续全速运行) | 调试执行 |
| **F7** | Step Into (单步步入) | 调试执行 |
| **F8** | Step Over (单步步过) | 调试执行 |
| **Shift+F11** | Step Out (跳出当前函数) | 调试执行 |
| **F4** | Run to Selection (运行到光标所在行) | 调试执行 |
| **Ctrl+F2** | Restart (重启会话) | 调试执行 |
| **Ctrl+\*** | Set New Origin Here (强制设置当前 RIP) | 调试执行 |
| **F2** | Toggle Breakpoint (切换软件断点) | 断点控制 |
| **Enter** | Follow Branch (跟随分支跳转/函数跟入) | 反汇编导航 |
| **Esc / Backspace** | Go Back (沿导航历史栈瞬时回退) | 反汇编导航 |
| **Alt+Left / Right** | Navigate History (历史前进 / 后退) | 反汇编导航 |
| **Space** | Assemble (就地呼出内联汇编框) | 代码修补 |
| **; (分号)** | Add / Edit Comment (为指令添加/编辑注释) | 逆向分析 |
| **Ctrl+B** | Toggle Bookmark (打下/清除黄色五角星书签) | 逆向分析 |
| **X** | Show Cross References (呼出代码交叉引用弹窗) | 逆向分析 |
| **Ctrl+E** | Modify Hex Bytes (就地十六进制编辑内存) | 内存修补 |
| **Ctrl+P** | Patch Manager (集中补丁管理与文件磁盘落盘) | 补丁与脱壳 |
| **Ctrl+S** | Save Project Database (保存 .edb_db 工程数据库) | 工程管理 |
| **Ctrl+D** | Dump CPU State (一键导出 CPU 机器状态快照) | 状态导出 |
| **Shift+S** | Toggle Stack View (快速展开/折叠右下调用栈) | 界面布局 |
| **Alt+C** | 快速聚焦 CPU 反汇编窗口 | 视图切换 |
| **Alt+D** | 快速切换至内存 Dump 窗口 | 视图切换 |
| **Alt+K** | 快速切换至调用栈 (Call Stack) 抽屉 | 视图切换 |
| **Alt+B** | 快速切换至断点列表 (Breakpoints) 抽屉 | 视图切换 |
| **Alt+M** | 快速切换至内存分页段表 (Memory Regions) 抽屉 | 视图切换 |
| **Alt+E** | 快速切换至全局符号浏览器 (Symbols) 抽屉 | 视图切换 |
| **Alt+L** | 快速切换至系统日志控制台 (Log) 抽屉 | 视图切换 |
| **Shift+F7/F8/F9** | 透传 POSIX 信号执行 (Pass Signal Step/Run) | 信号控制 |

---

## 11. 结语

本教程系统化梳理了从零编译、四象限日常逆向、高阶漏洞分析、系统调用注入与 ELF 补丁磁盘落盘，直至 C++20 插件编写的全套操作规范。您可以充分发挥 `edb-next` 强大的底核控制力与极速交互体验，轻松攻克复杂 Linux 原生二进制逆向与漏洞利用分析难题！
