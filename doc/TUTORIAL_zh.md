# edb-next 官方完全使用教程与插件开发指南

> **项目名称**：edb-next (Next-Generation Linux Binary Debugger & Reverse Engineering Platform)  
> **适用版本**：v1.0.0+  
> **适用平台**：Linux x86_64  
> **开发语言**：C++20 / 纯 Qt 6.4+ (GCC 13+) / Capstone Engine

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
   - 4.4 断点绑定 Python/Lua 脚本动作与微秒级无感打桩 (Script Actions)
   - 4.5 内存页保护断点与零 0xCC 隐匿执行断点 (Page-Guard Breakpoints)
   - 4.6 POSIX 信号拦截、放行与透传执行
   - 4.7 底部动态分支预测 (Dynamic Branch Prediction)
   - 4.8 代码执行覆盖率 (Hit Trace) 与时间旅行单步回溯 (Run Trace)
   - 4.9 CPU 机器状态全景快照导出 (StateDumper)
   - 4.10 动态库全自动拦截、热重载与延迟待决断点 (Shared Libraries & Pending Breakpoints)
   - 4.11 多进程 Follow-Fork 模式与子进程跟踪实战 (Follow-Fork & Inferiors)
   - 4.12 多线程独立冻结与解冻实战 (Thread Freeze / Thaw & Isolated Stepping)
   - 4.13 动态内存特征差分扫描器实战 (Differential Memory Scanner - CheatEngine style)
   - 4.14 复合数据类型重建与结构体布局可视化实战 (Type Viewer & Struct Layout)
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
9. [DWARF 源码级调试实战指南 (Source-Level Debugging)](#9-dwarf-源码级调试实战指南-source-level-debugging)
   - 9.1 编译选项与 DWARF 调试符号提取
   - 9.2 反汇编与源码混合排版模式 (`Ctrl+Shift+S`)
   - 9.3 独立源码文件浏览器与行号断点 (`Alt+S`)
   - 9.4 源码级单步步过与步入
10. [嵌入式脚本自动化引擎实战指南 (Python 3 & Lua 5.4)](#10-嵌入式脚本自动化引擎实战指南-python-3--lua-54)
   - 10.1 为什么采用 Python 与 Lua 双引擎体系
   - 10.2 交互式 Script Console 终端 (`Alt+P`)
   - 10.3 Python 3 自动化逆向脚本编写与 `edb` API 参考
   - 10.4 Lua 5.4 极速条件 Hook 编写与 API 参考
   - 10.5 运行外部脚本文件与批量脱壳/内存转储实战
   - 10.6 CommandBar 行内快速脚本求值 (`py ...` / `lua ...`)
   - 10.7 断点绑定脚本自动化动作与无感打桩实战 (Script-Driven Breakpoint Hooking)
11. [现代化 C++20 插件编写与使用全指南 (Plugin Development)](#11-现代化-c20-插件编写与使用全指南-plugin-development)
   - 11.1 插件架构与工作原理
   - 11.2 插件工程目录与 CMakeLists.txt 规范
   - 11.3 编写插件头文件 (`.hpp`)
   - 11.4 编写插件实现源码 (`.cpp`)
   - 11.5 编译生成插件共享库 (`.so`)
   - 11.6 插件安装、加载与实机测试
12. [常用快捷键一览表 (Cheat Sheet)](#12-常用快捷键一览表-cheat-sheet)
13. [结语](#13-结语)

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
    qt6-base-dev \
    qt6-tools-dev \
    libgl1-mesa-dev \
    libcapstone-dev \
    libdw-dev \
    libelf-dev \
    python3-dev \
    liblua5.4-dev
```

#### Fedora / RHEL 安装命令：
```bash
sudo dnf install -y \
    gcc-c++ \
    cmake \
    git \
    pkgconf-pkg-config \
    qt6-qtbase-devel \
    capstone-devel
```

#### Arch Linux 安装命令：
```bash
sudo pacman -S --needed \
    base-devel \
    cmake \
    git \
    pkgconf \
    qt6-base \
    capstone
```

---

### 1.2 编译构建操作

```bash
# 1. 切换至项目根目录
cd /path/to/edb-next

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
- `build/test_dwarf`：DWARF 源码级调试与行号双向映射测试套件；
- `build/test_advanced`：高阶逆向特性全量回归测试套件；
- `build/test_scripting`：Python 3 & Lua 5.4 嵌入式双引擎测试套件；
- `build/test_exit`：窗口析构防崩溃压力测试套件。

---

### 1.3 运行全量自动化验证套件

在正式调试目标前，可以运行测试套件验证当前系统环境各项底层机制（如 ptrace 权限、内存映射与系统调用注入）是否正常：

```bash
# 运行基础核心测试 (覆盖断点状态机、单步、多会话、ELF解析、调用栈、堆分析、汇编器等)
./build/test_core

# 运行 DWARF 源码解析与行号双向映射测试
./build/test_dwarf

# 运行进阶特性测试 (覆盖偏好配置、补丁落盘、插件网关、Trace引擎、CFG、远程系统调用等)
./build/test_advanced

# 运行 Python 3 & Lua 5.4 双脚本自动化引擎测试
./build/test_scripting

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
- **x64dbg 风格反汇编语法着色 (Syntax Highlighting)**：
  指令列由定制委托 `InstructionHighlightDelegate` 接管，全面采用现代 One Dark / x64dbg 高信息密度色彩体系：
  - **`CALL`**：粗体暖金黄色（`#E5C07B`），醒目标注子函数调用点；
  - **`JMP`**：暖橙色（`#D19A66`），无条件分支清晰明朗；
  - **`Jcc`**（`JE`, `JNE`, `JZ`, `JNZ`, `JG`, `JL`, `JA`, `JB`, `JAE`, `JBE` 等）：珊瑚粉红色（`#E06C75`），突出决策分流节点；
  - **`RET` / `RETN`**：粗体紫罗兰品红（`#C678DD`），函数返回尾声一目了然；
  - **`SYSCALL` / `SYSENTER` / `INT` / `UD2` / `HLT`**：粗体深绯红（`#E06C75`），系统调用与内核态陷入警戒；
  - **`PUSH` / `POP`**：青碧色（`#56B6C2`），快速辨识栈平衡操作；
  - **`CMP` / `TEST`**：金黄暗色（`#E5C07B`），标志位计算点；
  - **`NOP`**：斜体暗灰色（`#5C6370`）；
  - **通用/段/控制寄存器**（`RAX`..`R15`, `EAX`..`R15D`, `RSP`, `RBP`, `RIP`, `CR0`..`CR4` 等）：亮天蓝（`#61AFEF`）；
  - **内存解引用定界符**（方括号 `[...]`）：柔嫩草绿色（`#98C379`）；
  - **十六进制与数值立即数**（`0x...` 与数字）：橙粉色（`#D19A66`）。
- **富文本动态分支预测与内存操作数求值预览**：
  紧邻反汇编视图底部的常驻状态预览条，结合 Capstone 指令结构与当前 CPU 真实物理状态，在单步或光标悬停时提供极具价值的推导：
  - **动态分支预测 (Dynamic Branch Prediction)**：结合 `EFLAGS`（`ZF`, `SF`, `OF`, `CF`, `PF`）实时推断当前 `Jcc` 指令是否即将发生跳转：
    - 即将跳转：翡翠绿 `Branch Taken: YES (ZF=1)`；
    - 不发生跳转：玫瑰红 `Branch Taken: NO (ZF=0)`；
  - **内存操作数解引用链式解析**：针对 `[rbp - 0x14]` 或 `[rax + rcx*4 + 0x20]` 等间接寻址，实时计算物理内存有效地址，并读取目标内存中的 8 字节数值，链式直观呈现为：
    `[rbp - 0x14] => 0x7fffffffe00c => 0x00000001`；
  - **目标符号跨模块引用解析**：自动解析调用与跳转的目标符号（如 `call <calculate_fib>` 或 `call <main+147>`）。
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
- **强制改变执行指针 (Set Origin / Set RIP)**：
  - 右键任意反汇编行，选择 **"Set New Origin Here (Set RIP)"**（或快捷键 **Ctrl+\***）；
  - 调试器将原子修改物理 CPU 的 RIP 寄存器并写回内核，强制目标从该行开始执行，秒级绕过注册校验或强行进入漏洞代码路径。

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
- **极客快捷数值微调 (+1 / -1)**：
  - 右键任意通用寄存器（RAX~R15），在右键菜单中直接点击 **`+1 (Increment)`** 或 **`-1 (Decrement)`**；
  - 调试器无需弹出任何修改对话框，直接原子下发 `ptrace(PTRACE_SETREGS)` 将内核寄存器值增减 1，极大简化了逆向循环计数器与条件标志的手工干涉流程。
- **快速追踪至栈视图 (Follow in Stack)**：
  - 右键任意存放栈地址的寄存器（如 RSP, RBP 或计算出的局部变量地址），选择 **`Follow in Stack`**；
  - 象限 4 栈视图将立即平滑定位到对应栈槽，方便瞬时观察函数帧与局部变量。
- **多格式复制子菜单 (Copy As...)**：
  - 右键寄存器展开 **`Copy As...`**：
    - `Hex (0x...)`：复制为标准 64 位十六进制格式（如 `0x00007FFFFFFFD7D0`）；
    - `Decimal`：复制为有符号与无符号十进制数值；
    - `Dereferenced String/Bytes`：将寄存器作为内存指针自动解引用，读取对应的 ASCII 字符串或 Hex 字节流至剪贴板。
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
- **转储区历史导航栈 (Dump Navigation History Stack)**：
  - 完整继承 x64dbg 人体工程学导航体验：每次通过 `Goto Address`、`Follow in Dump` 或指针跳转时，自动记录历史地址；
  - 按下快捷键 **Alt+Left** 或 **Backspace**：瞬时原路后退（Back）至上一处转储地址；
  - 按下快捷键 **Alt+Right**：历史前进（Forward）；
  - 在复杂多级数据结构或指针链逆向时，来回穿梭如丝般顺滑。
- **单元格跨视图穿梭联动 (Follow QWORD in...)**：
  - 在 Hex Dump 视图中选中任意字节，右键提供：
    - **`Follow QWORD in Dump`**：读取从该字节起始的 8 字节 QWORD 并作为目标地址在转储区跳转；
    - **`Follow QWORD in Disassembly`**：将 8 字节 QWORD 作为代码地址在反汇编视图定位；
    - **`Follow QWORD in Stack`**：将 8 字节 QWORD 作为栈地址在象限 4 栈视图定位。
- **结构体布局分析直达 (View as Struct (Type Viewer)...)**：
  - 右键任意内存单元格，选择 **`View as Struct (Type Viewer)...`**；
  - 调试器瞬时激活底部抽屉 **Tab 22: Type Viewer**，并将当前光标所在物理内存地址自动填充至地址输入框，一键完成从原始十六进制字节到结构体高维语义字段的视觉映射。
- **十六进制就地热补丁**：
  - 选中欲修改的字节，按下快捷键 **Ctrl+E**；
  - 实时输入新的十六进制字节序列并确认，修改立即生效于目标内存并被 `PatchManager` 自动记录；
  - 右键支持 **"Fill with Zeros"**（全填 0）与 **"Fill with NOPs"**（全填 0x90）。
- **内存转储区右键直下硬件监视点与单元格高亮**：
  - 在 Hex Dump 视图中选中任意字节单元格，右键打开 **"Breakpoint"** 子菜单；
  - 一键设置 **"Set Hardware Write Watchpoint"**（1 / 2 / 4 / 8 字节写监视点）或 **"Set Hardware Read/Write Watchpoint"**（1 / 2 / 4 / 8 字节读写监视点），亦支持设置硬件执行断点或切换 0xCC 软件断点；
  - 转储区自动检测活动断点地址，命中或注册断点的内存单元格以**深红背景 (`QColor(160, 40, 40, 160)`) 与高亮白字**醒目着色，鼠标悬浮即提示 `Breakpoint active at 0x...`，断点状态一目了然。
- **内存转储区页保护断点 (Page-Guard / 软内存监视点)**：
  - 在 Hex Dump 视图中选中任意字节或范围，右键打开 **"Breakpoint -> Page-Guard Breakpoint"**；
  - 支持一键下发 **"No Access (PROT_NONE)"**（拦截一切读写执行）、**"Write Only (PROT_READ)"**（拦截写入，读与执行全速放行）、**"Execute Only (PROT_EXEC)"** 或输入自定义字节跨度；
  - 受 Page-Guard 监视的内存单元格以**金琥珀色背景 (`QColor(180, 110, 20, 160)`) 与高亮白字**醒目呈现，所在 4KB 页面其余区域以淡金柔和着色，鼠标悬停即显示 `Page-Guard Watched: 0x...`；
  - 彻底打破 CPU DR0~DR3 仅 4 处的物理限制，可无限设置软内存监视点。
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
- **函数返回地址智能识别与亮琥珀金高亮**：
  - 栈引擎实时推导当前栈上每个 8 字节 QWORD 的语义归属；
  - 若某个 QWORD 落在具有可执行权限的代码段内，且其上一指令槽位为 `CALL`，调试器在描述列呈现醒目的亮琥珀金标签 **`[Return Address] <函数名+偏移>`**（例如 `[Return Address] __libc_start_main+0x80`）；
  - 帮助逆向人员一眼辨识函数调用链层级，在攻防分析中瞬间捕捉栈溢出漏洞导致的返回地址覆盖篡改；
  - 右键提供 **`Follow in Disassembly`**、**`Follow in Dump`**、**`Copy Address`** 与 **`Copy QWORD Value`**。
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

### 4.4 断点绑定 Python/Lua 脚本动作与微秒级无感打桩 (Script Actions)

`edb-next` 支持在任意软件断点或硬件断点上绑定原生自动化脚本代码，支持 **Python 3** 或 **Lua 5.4** 双引擎：
- **配置脚本动作**：
  - 切换至 Tab 4 **Breakpoint Manager**，选中断点后点击顶部 **"Edit Script..."**，或直接右键断点选择 **"Edit Script Action..."**；
  - 在弹出的交互式脚本对话框中选择编程语言（`Python` 或 `Lua`），录入自动化脚本逻辑。
- **核心无感打桩契约 (Silent Hooking Protocol)**：
  - **静默放行 (Bypass)**：若脚本执行结束并显式返回布尔假值——Python 中为 `return False`，Lua 中为 `return false`，调试引擎在完成脚本动作后**绝不挂起 UI、不暂停任何线程**，而是通过内部高精状态机自动调度“单步越过断点原指令并恢复全速运行”！这实现了微秒级的动态无感打桩探针，无需重启进程或重新编译二进制即可任意嗅探、修改程序行为；
  - **常规中断 (Pause)**：若脚本显式返回 `True` / `true`，或未显式返回假值，调试器在执行完脚本后将正常挂起被调试进程，并自动定位反汇编视图至命中行；
  - **异常安全防护**：若脚本存在语法错误或运行时抛出未捕获异常，调试引擎自动将完整的异常栈追踪（Traceback）转储至控制台与调试日志，并安全挂起目标供逆向人员介入，绝不导致宿主崩溃；
  - **项目持久化支持**：所有绑定的脚本代码及语言配置均完整序列化存储在 `.edb_db` 项目工程库中，重载工程即刻还原。

### 4.5 内存页保护断点与零 0xCC 隐匿执行断点 (Page-Guard Breakpoints)

在面对包含代码校验和（CRC32/Hash）自检测的加固二进制，或者需要同时监控数十处内存缓冲区却受限于 CPU DR0~DR3 仅 4 处硬件寄存器的场景下，`edb-next` 提供了工业级的内存页保护软断点系统：

- **设置页保护断点**：
  - **HexDump 视图快捷部署**：在 Hex Dump 单元格上右键展开 **"Breakpoint -> Page-Guard Breakpoint"**，选择监视类型：
    - **No Access (`PROT_NONE`)**：全面拦截该内存地址的读、写与执行访问；
    - **Write Only (`PROT_READ`)**：拦截写入操作（软写监视点），允许正常读取与指令执行；
    - **Execute Only (`PROT_EXEC`)**：拦截读写操作，允许正常指令执行；
    - **Custom Page-Guard Range...**：输入任意字节跨度（支持跨 4KB 页面无缝覆盖）。
  - **底栏极客命令行一键下发**：
    - `pageguard 0x7ffff7fbc100 8 ro`（在指定地址设置 8 字节读监视点，写操作触发断点）；
    - `pageguard 0x401020 1 none`（设置 0xCC-Free 隐匿执行断点）；
    - `guards` 或 `pageguards` 查看当前所有受监控的页保护断点；
    - `unpageguard 0x7ffff7fbc100` 清除指定地址的页保护断点。
- **色彩与状态渲染**：
  - 受 Page-Guard 监视的单元格以**金琥珀色背景 (`QColor(180, 110, 20, 160)`) 与高亮白字**醒目显示；
  - 鼠标悬浮 Tooltip 呈现 `Page-Guard Watched: 0x...`；所在 4KB 页面以柔和淡金底色提示页保作用域。
- **微秒级假阳性透明放行机制 (False-Positive Bypass)**：
  - 操作系统虚拟内存权限以 4KB 页面为最小管理粒度。若目标进程访问了**同一 4KB 页面上的其他合法变量**，调试引擎通过内置内核级状态机自动执行“撤除保护 -> 单步执行一条指令 -> 恢复页保 -> 全速恢复运行”，全过程在微秒内无感完成，逆向人员不会遭遇频繁卡顿与误报！
- **反反调试代码自校验绕过 (CRC32/Hash Bypass)**：
  - 对代码函数入口设置 Page-Guard，目标内存中**完全保留原始机器指令字节码（零 0xCC 修改）**。加固壳的代码段自校验循环扫描读取指令时，页面允许读取直接放行，校验和完美匹配；当 CPU 指令指针 RIP 执行至该地址时，内核硬件级陷入断点，彻底粉碎目标反调试防线！

---

### 4.6 POSIX 信号拦截、放行与透传执行

- **全局信号策略配置**：
  - 打开 **Options -> Preferences -> Signals**；
  - 针对 1~64 号 Linux POSIX 信号（如 SIGSEGV、SIGTRAP、SIGINT、SIGPIPE），精确配置其拦截（Intercept）与放行（Pass）策略。
- **透传执行动作**：
  - 当目标遭遇信号停下时，若需要将信号交由程序自身的异常处理函数（Signal Handler）处理，使用透传快捷键：
    - **Shift+F7**：Step Into Pass Signal
    - **Shift+F8**：Step Over Pass Signal
    - **Shift+F9**：Run Pass Signal

---

### 4.7 底部动态分支预测 (Dynamic Branch Prediction)

在调试条件跳转指令（如 `je`, `jne`, `jg`, `jle`）时，主工作台底部常驻有动态预测状态条：
- `InstructionInspector` 实时解算当前条件跳转依赖的 EFLAGS 标志位（ZF, SF, OF, CF）；
- 实时给出高亮提示：
  - 翠绿色提示：**`[JUMP TAKEN] (Branch will be followed to 0x...)`**
  - 暗灰色提示：**`[JUMP NOT TAKEN] (Execution will fall through)`**
- 无需心算标志位，一眼即可预知分支走向。

---

### 4.8 代码执行覆盖率 (Hit Trace) 与时间旅行单步回溯 (Run Trace)

切换至 Tab 13 **Trace** 选项卡：
- **Hit Trace (代码覆盖率)**：
  - 勾选启用 Hit Trace，目标程序运行过程中，所有被 CPU 执行过的代码行都会在反汇编视图中标记为淡绿色高亮；
  - 点击 **"Clear Hit Trace"** 可随时重置统计，便于分析特定算法分支是否被触发。
- **Run Trace 与时间旅行历史穿梭 (Time-Travel)**：
  - 勾选启用 Run Trace，系统将自动记录单步调试走过的每一帧历史；
  - 界面提供 **`< Step Back`** 与 **`Step Forward >`** 按钮；
  - 点击 **`< Step Back`**，反汇编光标与寄存器视图将瞬间穿越回上一帧，并标红呈现与当前帧的变动差分，实现时间旅行式逆向回溯。

---

### 4.9 CPU 机器状态全景快照导出 (StateDumper)

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

### 4.10 动态库全自动拦截、热重载与延迟待决断点 (Shared Libraries & Pending Breakpoints)

在分析带有插件架构、动态加壳或者延迟通过 `dlopen()` 加载核心 `.so` 模块的现代程序时，传统调试器无法在模块加载前下断点，且加载后往往需要手动刷新符号。`edb-next` 基于 Linux glibc `_r_debug` Rendezvous 协议彻底解决了这一痛点：

#### 1. 延迟待决断点 (Pending Breakpoints - `bpp <symbol>`)
当目标尚未加载某个插件库时（例如 `plugin_calc.so`），其导出函数（如 `plugin_calc_magic`）的虚拟地址在当前进程中尚不存在：
- 在底栏命令行执行：
  ```text
  bpp plugin_calc_magic
  ```
  或在普通断点命令 `bp plugin_calc_magic` 找不到符号时，确认将其转换为待决断点；
- 在 **Breakpoints** 抽屉（Tab 5）中，待决断点将以醒目的青色 `[Pending]` 标识与琥珀色状态呈现；
- 一旦目标在后续执行中调用 `dlopen()` 将该 `.so` 映射入内存，`edb-next` 的 Rendezvous 引擎将**瞬间自动捕捉并解析出其重定位基址**，无缝将其升级绑定为硬件/软件断点；
- 当目标代码执行进入 `plugin_calc_magic` 时，断点精确命中并暂停，供逆向人员审查第一现场！

#### 2. 模块加载捕获开关 (`catch load` / `catch dlopen`)
- 默认情况下，调试器对动态库装载与符号热重载过程采取**微秒级无感透明放行**（自动更新内部符号表与 DWARF，目标不暂停）；
- 若需要在任意 `.so` 刚映射入内存的瞬间暂停目标（例如观察其 `.init` / `.init_array` 初始化构造函数执行）：
  ```text
  catch load       # 开启模块加载拦截
  catch dlopen     # 别名，效果相同
  ```
- 再次输入该命令即可快速切回无感静默模式。

#### 3. 已加载共享库查询 (`modules` / `libs` / `solist`)
- 在底栏命令行随时输入 `modules`、`libs` 或 `solist`，控制台将格式化打印出当前已装载的所有共享库列表（包含基地址、库名称、绝对物理路径与动态段头指针）。

---

### 4.11 多进程 Follow-Fork 模式与子进程跟踪实战 (Follow-Fork & Inferiors)

在逆向多进程服务器（如 Nginx、多工作进程服务）、自动化沙箱解包器或 CTF Pwn 赛题中，目标程序频繁调用 `fork()` 或 `vfork()` 衍生出子进程。`edb-next` 提供了全套强大的 Follow-Fork 控制与多 Inferior 管理能力：

#### 1. 配置 Follow-Fork 模式 (`follow-fork [parent|child|both]`)
在底栏 CommandBar 控制台中，逆向人员可以随时查询与动态切换跟踪策略：
```text
show follow-fork-mode          # 查看当前遵循模式 (Parent / Child / Both)
follow-fork parent             # 默认模式：继续跟踪父进程，子进程脱钩自由运行
follow-fork child              # 切换模式：脱离父进程，将会话焦点转移至新派生的子进程
follow-fork both               # 双调试模式：保留父进程会话，同时自动为子进程创建独立会话
set follow-fork-mode <mode>    # GDB 风格别名命令，效果相同
```

#### 2. 分支事件捕获断下 (`catch fork` / `catch vfork`)
若希望在目标执行 `fork()` 的物理瞬间精确暂停，审查当时的寄存器与调用栈参数：
- 在底栏命令行执行：
  ```text
  catch fork         # 开启 fork 事件拦截
  catch vfork        # 开启 vfork 事件拦截
  ```
- 目标调用 `fork()` 时，进程将在 fork 系统调用返回瞬间被内核精确挂起，并在底栏状态行与系统日志中呈现：
  ```text
  [Fork Event] Process 12345 forked child 12347 (mode: Parent)
  ```
- 此时用户可从容查看现场、分析子进程 PID、设置断点，随后按 **F9**（`run`）继续。

#### 3. 多目标进程列表与工作区穿梭 (`inferiors` / `inferior <id|pid>`)
在 `follow-fork both` 模式下，每次目标派生子进程，`edb-next` 的会话管理器将自动在主工作区派生出一个新的工作区标签页（例如 `Child [PID: 12347]`），其拥有独立的寄存器、反汇编、内存转储与调用栈视图。
- **查询所有活动会话与进程**：
  ```text
  inferiors          # 打印所有会话 ID、PID、运行状态与目标程序
  processes          # 别名，输出相同
  ```
  输出示例：
  ```text
  === Active Debug Sessions (Inferiors) ===
    ID: 1 | PID: 12345 | Name: nginx_master | State: Paused | Path: /usr/sbin/nginx
  * ID: 2 | PID: 12347 | Name: Child [PID: 12347] | State: Running | Path: /usr/sbin/nginx
  ```
- **切换活动会话焦点**：
  ```text
  inferior 1         # 通过会话 ID 切换至父进程
  inferior 12347     # 或直接通过 PID 切换至子进程
  process 12347      # 别名，效果相同
  ```
  主界面将平滑将当前工作区标签页与底栏命令行的上下文对齐至指定进程，多进程协同调试从未如此清晰从容！

---

### 4.12 多线程独立冻结与解冻实战 (Thread Freeze / Thaw & Isolated Stepping)

在多线程高并发程序（如数据库引擎、网络服务器、加固壳多工作线程）调试中，当我们在某个断点暂停并开始单步步过或步入时，其他线程常常在后台并发推进，甚至触发新的断点或修改当前函数正在审查的全局变量，造成严重的心智负担和“竞态破坏”。

`edb-next` 支持细粒度的轻量级线程独立冻结与解冻（Freeze / Thaw）以及隔离单步步进：

#### 1. Threads 视图状态感知与一键控制 (Tab 10)
切换至底部抽屉 **Threads** 标签页（Tab 10）：
- **全景 8 列信息呈现**：展示当前进程的所有轻量级线程（TID、线程名称、运行状态、**冻结状态**、当前 RIP 指针、函数符号、RSP 栈顶、活动焦点标识）；
- **冰蓝状态指示**：处于冻结状态的线程将呈现醒目的冰蓝色 `❄ FROZEN` 状态徽标并整行高亮；
- **顶部快捷工具栏**：
  - **`❄ Freeze / Thaw`**：一键切换选中线程的冻结状态；
  - **`❄ Freeze Others`**：一键冻结除当前活动焦点线程外的所有其他并发线程；
  - **`🔥 Thaw All`**：一键解冻所有线程，恢复全并发推进。
- **右键上下文菜单**：在任一线程条目右键，均可快速选择 `Freeze Thread` / `Thaw Thread` 或 `Freeze All Other Threads`。

#### 2. 隔离单步步进 (Isolated Stepping)
- **典型实战场景**：调试某个产生死锁或数据竞争的工作线程。
- **操作方式**：
  1. 在 Threads 视图中点击 **`❄ Freeze Others`**（或在命令行输入 `freeze all`）；
  2. 此时除当前正在调试的线程外，其余所有后台 Worker 线程均被操作系统信号与调试引擎调度器双重锁定；
  3. 按 **F7**（单步步入）或 **F8**（单步步过），调试器将严格仅驱动当前活动线程执行指令，其余线程绝对静止，确保变量与执行现场不被破坏；
  4. 审查完毕后，点击 **`🔥 Thaw All`**（或在命令行输入 `thaw all`），恢复全量多线程自由推进。

#### 3. CommandBar 命令行极客操作
除了图形界面操作，底栏命令行提供了迅捷的 CLI 指令：
```text
threads            # 打印当前所有线程列表、RIP、符号与 [FROZEN] 状态
thread <tid>       # 快速将活动调试焦点切换至指定 TID
freeze <tid>       # 冻结指定 TID 的线程
freeze all         # 冻结除当前活动焦点外的所有其他线程
thaw <tid>         # 解冻指定 TID 的线程
thaw all           # 解冻所有线程
```

---

### 4.13 动态内存特征差分扫描器实战 (Differential Memory Scanner - CheatEngine style)

在安全分析、CTF 攻防、外挂逆向以及漏洞挖掘中，目标程序中的敏感变量（如加密密钥缓冲区、游戏金币/生命值、解密标志位、用户权限等级）往往动态驻留于堆（Heap）、栈（Stack）或未初始化数据段（BSS）中。静态特征码搜索无法捕获动态数值的变化。

`edb-next` 内置了工业级、CheatEngine 风格的多轮差分内存扫描器（Tab 21: "Memory Scanner"），支持从数百万候选地址中快速过滤收敛：

#### 1. 扫描器操作面板全景 (Tab 21)
切换至底部抽屉 **Memory Scanner**（Tab 21）：
- **参数控制栏**：
  - **Value（目标值）**：输入待检索数值（支持十进制如 `100`、十六进制如 `0x1337`、浮点数如 `3.1415`、字符串文本或 Hex 字节流如 `48 89 ?? 55`）；
  - **Delta (+/-)**：配合“Increased By...”或“Decreased By...”输入特定变动差值；
  - **Data Type（数据类型）**：支持 `Int32 (4 Bytes)`、`Int64 (8 Bytes)`、`Int16 (2 Bytes)`、`Int8 (1 Byte)`、`Float (Single)`、`Double`、`String (Text)`、`Hex Bytes (ByteArray)`；
  - **Scan Type（扫描类型）**：
    - `Exact Value`：精准匹配输入值；
    - `Increased Value`：数值变大（`>`）；
    - `Decreased Value`：数值变小（`<`）；
    - `Changed Value`：数值发生任何变动（`!=`）；
    - `Unchanged Value`：数值保持未变（`==`）；
    - `Increased By...`：精准增加 Delta；
    - `Decreased By...`：精准减少 Delta；
    - `Unknown Initial Value`：全量抓取所有候选基准。
  - **Writable Memory Only**：默认勾选，仅扫描可读写数据段（`rw-p`），耗时仅数十毫秒；
  - **Alignment（内存对齐）**：支持 4 字节默认对齐、1 字节无对齐、2 字节或 8 字节对齐。

#### 2. 多轮差分收敛实战流程 (Multi-Pass Convergence)
- **第 1 轮扫描 (First Scan)**：
  1. 在 Value 输入当前观察到的初始值（例如当前生命值 `100`）；
  2. 点击 **`🔍 First Scan`**（或在 CommandBar 输入 `scan 100`）；
  3. 系统瞬间检索全部内存并展示数万个候选基准；
- **第 2 轮收敛 (Next Scan)**：
  1. 按 F9 让目标程序继续运行并发生数值变动（例如受到攻击掉血变为 `85`，或者只知道“数值变小了”）；
  2. 程序暂停后，选择 `Decreased Value`（或直接输入精确新值 `85`）；
  3. 点击 **`⚡ Next Scan`**（或在 CommandBar 输入 `nextscan <` 或 `nextscan 85`）；
  4. 候选地址数量断崖式骤降至数十个；
- **第 3 轮最终锁定**：
  1. 再次让目标运行并改变数值，点击 `⚡ Next Scan`；
  2. 候选列表精准收敛至唯一定位目标地址！

#### 3. 候选地址交互与原位内存修改
- **颜色高亮感知**：变大数值以清新草绿色标注（`+15`），变小数值以浅红色标注（`-15`）；
- **联动转储**：双击任一行候选，主工作区 Hex Dump 自动跳转至该物理地址；
- **右键上下文菜单**：
  - `Follow in Hex Dump` / `Follow in Disassembly`；
  - `Copy Address`；
  - **`Edit / Write Value...`**：直接弹窗输入新值，向目标进程物理覆写该变量（例如将血量直接修改为 `999999`），改完后自动刷新候选当前值！

#### 4. CommandBar 命令行极客操作
```text
scan <value|unknown> [type]   # 发起首次扫描。例：scan 100 int32、scan 0x1337 int64、scan "admin" str
nextscan <compare> [val]      # 执行下一轮差分收敛。例：nextscan >、nextscan <、nextscan ==、nextscan 105、nextscan + 10
scanresults [limit]           # 打印当前前 N 个收敛结果详情
scanreset                     # 重置扫描器状态并清空候选表
```

---

### 4.14 复合数据类型重建与结构体布局可视化实战 (Type Viewer & Struct Layout)

在网络协议分析、Linux 内核模块逆向以及复杂 C/C++ 业务逻辑逆向中，内存中的数据绝非零散的孤立字节，而是根据结构体定义精心对齐排布的复合数据。传统的十六进制编辑器无法直观呈现字段边界，逆向人员需要反复肉眼计算偏移并手工比对数据类型。

`edb-next` 引入了 CheatEngine 风格的结构体剖析与复合数据类型重构工作台（Tab 22: "Type Viewer"），支持导入 C 语法结构体并在实时内存上完成字段切分、类型解析与交互跳转：

#### 1. 结构体布局分析工作台全景 (Tab 22)
切换到底部抽屉 **Type Viewer**（Tab 22）：
- **顶部工具栏**：
  - **Struct 下拉菜单**：快速选择已注册的结构体模板（内置 `timespec`、`timeval`、`sockaddr_in`、`list_head`、`io_vec` 等）；
  - **`➕ Define Struct...` 按钮**：弹出 C 语言语法声明编辑器，支持直接粘贴 C 头文件中的结构体源码并瞬时完成解析注册；
  - **Address 地址输入框**：输入待解析的内存基地址，原生支持寄存器与动态算式表达式（如 `rsp`、`rbp - 0x40`、`0x7fffffffd7d0`）；
  - **`🔬 Inspect` 按钮**：从目标进程读取指定结构体尺寸的内存数据并格式化刷新各个字段；
  - **`🔄 Refresh` 按钮**：在程序单步或恢复运行后，重新读取当前地址的内存变动；
  - **Size 标签**：自动指示当前结构体的总字节大小与内存对齐基数（如 `Size: 56 bytes (align 8)`）。

#### 2. 六维字段表格呈现与指针解引用穿梭
- **`Offset`（偏移列）**：清晰显示每个字段相对于结构体首地址的十六进制偏移（如 `+0x000`、`+0x008`、`+0x010`），自动展示 System V AMD64 ABI 填充对齐空隙（Padding）；
- **`Field Name` / `Type` / `Size`**：呈现字段变量名、数据类型（如 `char[16]`、`int32`、`void*`）以及所占物理字节大小；
- **`Raw Hex`**：展示该字段在目标进程内存中的原始字节序列（以小端序呈现）；
- **`Value / Dereference`（取值与解引用列）**：
  - 数值型字段同步显示十进制与十六进制；
  - 单字节字段展示数字及可读 ASCII 字符（如 `42 ('*')`）；
  - 字符数组展示双引号转义文本（如 `"PlayerOne"`）；
  - **指针跳转穿梭**：指针字段以醒目青色下划线标注（如 `0x00007fffffffe100`），在单元格上**双击**即可直接一键穿梭：
    - 若指向代码段，自动跳转至反汇编视图（Disassembly）；
    - 若指向数据段/堆栈，自动在多路转储视图（MultiDump）中精准定位该物理内存；
- **右键上下文菜单**：
  - **`Edit Field Value...`**：弹出字段就地编辑对话框，直接向目标进程物理地址写入新的整数、浮点数或文本内容，写完后自动刷新展示；
  - `Follow in Hex Dump` / `Follow in Disassembly`：联动主工作区跳转；
  - `Copy Field Value`：将格式化数值直接复制到系统剪贴板。

#### 3. 动态自定义结构体定义 (C 语言语法解析)
点击 `➕ Define Struct...` 或在 CommandBar 执行 `defstruct`，可以直接输入标准 C 声明：
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
系统内部解析器自动根据 x86_64 体系规则计算自然对齐（如 `short` 自动对齐至 2 字节偏移、`int` 自动对齐至 4 字节偏移、`long` / 指针自动对齐至 8 字节偏移），确保与 GCC/Clang 编译生成的实际二进制二进制完全吻合！

#### 4. CommandBar 命令行快速操作
```text
structs                       # 列出当前所有已注册的结构体模板名称、总尺寸与字段数
struct <name> <addr_or_expr>  # 解析并打印指定内存地址上的结构体字段展开详情
defstruct <c_code...>         # 在命令行直接注册新的 C 语言结构体定义
```

---

## 5. 高级逆向分析工具箱实战 (Advanced Reverse Engineering)

在主工作台左下角，内置了 22 个按需切换的高级分析抽屉：

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
  - **Dynamic Tags**：展开所有 `DT_NEEDED` 依赖的 `.so` 动态库；
  - **Loaded Shared Libraries (`_r_debug`)（第 5 标签页）**：实时追踪 Linux glibc `link_map`，呈现所有已加载动态库的装载基址、名称、物理文件路径与动态段地址；支持双击任意行直达反汇编或内存转储。

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

### 5.9 全局符号浏览器与 C++ 符号反混淆 (SymbolViewer - Tab 6 / Alt+E)
- 按下快捷键 **Alt+E**（或切换至 Tab 6 **Symbols**）：
  - 自动列出目标二进制及所有加载的共享库中的所有导出、局部及动态符号；
  - **原生 Itanium ABI C++ 反混淆**：所有修饰符号（如 `_Z13calculate_fibi`、`_ZNSt7__cxx11...`）全自动反混淆为清爽易读的函数原型（如 `calculate_fib(int)`）；
  - **悬浮 Tooltip 溯源**：鼠标悬停于符号名称单元格，即弹出原始 Mangled 字符串与符号地址，逆向比对零困扰；
  - **双向模糊过滤**：在顶部搜索框输入 Mangled 原名或 Demangled 函数名均可即时高亮匹配；
  - **双击瞬达**：双击任意函数符号，反汇编视图立即对齐至该函数入口点。

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
| `pageguard <addr> [sz] [type]` | `guard` | 设置页保护软内存断点（type: `none`/`ro`/`xo`）。例：`guard 0x401000 8 ro` |
| `unpageguard <addr>` | `unguard` | 移除指定地址上的页保护断点。例：`unguard 0x401000` |
| `pageguards` | `guards` | 打印当前所有激活的页保护断点清单 |
| `modules` / `libs` / `solist` | `libs` | 打印目标进程当前所有已加载的共享库及基址与路径 |
| `catch load` / `catch dlopen` | `catch load` | 切换是否在目标加载/卸载共享库时中断暂停执行 |
| `bpp <symbol>` | `bpp` | 设定延迟待决断点 (Pending Breakpoint)，在目标动态加载该符号时自动绑定 |
| `follow-fork [mode]` | `follow-fork` | 查询或设置 fork 遵循模式（`parent` / `child` / `both`）。例：`follow-fork both` |
| `set follow-fork-mode <mode>` | `set follow-fork-mode` | GDB 风格设置 fork 遵循模式 |
| `show follow-fork-mode` | `show follow-fork-mode`| 打印当前生效的 fork 遵循模式 |
| `catch fork` / `catch vfork` | `catch fork` | 切换是否在目标派生子进程时刻中断暂停被调试进程 |
| `inferiors` / `processes` | `inferiors` | 打印当前受控的所有目标调试会话 (Inferiors) 状态清单 |
| `inferior <id\|pid>` | `inferior` | 切换当前活动调试会话与工作区焦点。例：`inferior 2`、`inferior 12347` |
| `process <id\|pid>` | `process` | 切换活动会话（`inferior` 别名） |
| `threads` | `threads` | 打印当前进程全部轻量级线程列表、RIP、符号与冻结状态 |
| `thread <tid>` | `thread` | 切换当前活动线程焦点至指定 TID |
| `freeze <tid\|all>` | `freeze` | 冻结指定 TID 线程或一键冻结所有非当前焦点线程 |
| `thaw <tid\|all>` | `thaw` | 解冻指定 TID 线程或解冻全部线程 |
| `scan <val> [type]` | `scan` | 发起首次内存扫描（CheatEngine 风格）。例：`scan 100 int32`、`scan unknown` |
| `nextscan <cmp> [val]` | `nextscan` | 执行下一轮差分收敛。例：`nextscan >`、`nextscan <`、`nextscan ==`、`nextscan 105` |
| `scanresults [limit]` | `scanresults` | 打印当前扫描结果列表的前 N 个条目 |
| `scanreset` | `scanreset` | 重置内存扫描器状态并清空候选表 |
| `structs` | `structs` | 列出当前所有已注册的复合结构体模板、总大小与字段数 |
| `struct <name> <addr>` | `struct` | 按结构体模板解析并打印目标内存地址各个字段展开详情 |
| `defstruct <c_code...>` | `defstruct` | 在命令行动态注册新的 C 语言结构体声明 |
| `dumpstate` | `dps` | 导出当前 CPU 完整快照并复制到剪贴板 |
| `py <code...>` | `py` | 直接执行 Python 3 语句或代码块求值。例：`py print(hex(edb.get_reg('rip')))` |
| `lua <code...>` | `lua` | 直接执行 Lua 5.4 语句或代码块求值。例：`lua print(string.format('0x%x', edb.get_reg('rip')))` |
| `help` | `?` | 打印出当前所有内置及外部插件注册的 CLI 命令列表 |

---

## 9. DWARF 源码级调试实战指南 (Source-Level Debugging)

`edb-next` 内置对 DWARF 调试符号的高性能原生支持，支持纯汇编与原始 C/C++ 源码之间的双向无缝映射。

### 9.1 编译选项与 DWARF 调试符号提取
当调试自己编译或拥有源码的二进制程序时，请在 GCC / Clang 编译时添加 `-g` 或 `-g3` 参数：
```bash
gcc -g -O0 -no-pie my_target.c -o my_target
```
在 `edb-next` 中加载该二进制时，底核 `core/DwarfParser` 将基于 `libdw` 自动提取 `.debug_info`、`.debug_abbrev`、`.debug_line` 与 `.debug_str` 等调试节区，构建起编译单元与所有源文件、函数及代码行号的高速查找缓存树。

### 9.2 反汇编与源码混合排版模式 (`Ctrl+Shift+S`)
在反汇编窗口 `DisassemblyView` 中，默认仅呈现纯汇编指令序列。
- 按下快捷键 **Ctrl+Shift+S**，或在反汇编区域点击右键菜单 **"Toggle Mixed Source/ASM"**；
- 反汇编引擎将自动将指令按源码行归类，并在对应汇编基本块上方渲染精致的**暗黑青绿色源码横幅**（例如：`main.c:24: if (user_id == 0) {`）；
- 逆向分析人员可在审查底层 CPU 指令逻辑的同时，直接对照原始高级语言业务逻辑，逆向理解效率提升数倍！

### 9.3 独立源码文件浏览器与行号断点 (`Alt+S`)
`edb-next` 在主代码展示区引入了独立的 **Source View** 标签页：
1. **一键聚焦**：按下快捷键 **Alt+S**，代码区自动切换至源码视图；
2. **源文件选择**：顶部下拉框自动列出当前二进制编译所包含的所有 C/C++ 源文件（如 `main.c`, `utils.c` 等）；
3. **行号状态指示**：
   - **执行指针指示**：当前 CPU 的 `RIP` 所在源码行以显眼的青蓝色粗体高亮，并在行号栏标注 `➔` 指示符；
   - **断点指示**：已下断点的源码行在行号栏标注亮红色圆点 `●`；
4. **源码行号双击断点**：在任意源码行的行号区域**双击左键**，即可针对该行下达源码断点，系统自动将源码行转换为底层机器代码物理首地址下断！再次双击即可清除断点。

### 9.4 源码级单步步过与步入
在源码调试模式下：
- **源码步过 (Source Step Over)**：系统执行连续单步，直至 CPU `RIP` 离开当前源码行所覆盖的所有机器指令区间，实现高级语言语句级的平滑步过；
- **源码步入 (Source Step Into)**：遇到包含函数调用的源码行时，直接步入目标子函数的源码第一行。

---

## 10. 嵌入式脚本自动化引擎实战指南 (Python 3 & Lua 5.4)

`edb-next` 提供业界领先的双脚本自动化引擎架构，统一由 `IScriptEngine` 与 `ScriptEngineManager` 协调调度。

### 10.1 为什么采用 Python 与 Lua 双引擎体系
- **Python 3 引擎**：面向复杂逆向、漏洞利用开发与安全生态联动。可直接利用 `pwntools`, `z3`, `scapy`, `requests`, `numpy` 等庞大的 Python 生态；
- **Lua 5.4 引擎**：面向极致轻量、高频断点命中与微秒级条件判定。Lua 解释器启动与执行开销几乎为零，非常适合在每秒触发上万次的紧凑循环中充当高速判定 Hook。

### 10.2 交互式 Script Console 终端 (`Alt+P`)
在主界面按下快捷键 **Alt+P**，底部工作台将自动切入 **Script Console** 选项卡：
- **Engine 语言切换**：顶部下拉框支持随时在 `Python 3` 与 `Lua 5.4` 之间自由切换；
- **命令行输入与历史穿梭**：底栏单行输入框支持使用键盘 **Up / Down** 箭头键穿梭最近执行的历史命令；
- **运行外部脚本文件 (`▶ Run File...`)**：点击可直接选择并运行磁盘上的 `.py` 或 `.lua` 脚本；
- **色彩渲染**：用户指令以青色高亮，执行输出以柔软灰白呈现，异常与 Traceback 报错以珊瑚红标出。

### 10.3 Python 3 自动化逆向脚本编写与 `edb` API 参考
在 Python 环境中，原生内置模块 `edb` 自动导入，提供了操作调试会话的完整 API 接口：

| Python API | 返回值 | 功能说明 |
| :--- | :--- | :--- |
| `edb.get_regs()` | `dict` | 获取全部寄存器字典（如 `{'rip': 0x401000, 'rax': 0x0, ...}`） |
| `edb.get_reg(name)` | `int \| None` | 获取指定寄存器的整数值（大小写不敏感，支持 `$rax` 或 `rax`） |
| `edb.set_reg(name, val)` | `bool` | 设置指定寄存器的数值 |
| `edb.read_memory(addr, size)` | `bytes` | 读取指定目标虚拟地址的二进制字节串 |
| `edb.write_memory(addr, data)`| `bool` | 将 Python `bytes` 数据覆写至目标虚拟地址 |
| `edb.set_breakpoint(addr, symbol="")` | `bool` | 在指定地址下达断点 |
| `edb.remove_breakpoint(addr)` | `bool` | 移除指定地址上的断点 |
| `edb.step_into()` / `edb.step_over()` | `None` | 单步步入 / 步过一条机器指令 |
| `edb.step_source()` | `None` | 步过一条源码行 |
| `edb.resume()` / `edb.pause()` | `None` | 继续全速运行 / 中断暂停被调试进程 |
| `edb.resolve_symbol(name)` | `int \| None` | 解析符号名称为物理虚拟地址 |
| `edb.eval(expr)` | `int \| None` | 计算调试表达式（如 `"rax + 0x20"`、`"[rsp+8]"`） |
| `edb.pid()` / `edb.tid()` | `int` | 获取目标进程 PID / 当前活动线程 TID |
| `edb.state()` | `str` | 获取当前会话状态（`"Running"`, `"Paused"`, `"Stopped"`） |
| `edb.log(msg)` | `None` | 向宿主系统日志视图输出一条信息 |

#### Python 实战示例：扫描内存并提取解密 Flag
```python
import edb

# 获取当前程序指针与基地址
rip = edb.get_reg("rip")
print(f"[*] Current RIP: {hex(rip)}")

# 解析目标函数符号
target_addr = edb.resolve_symbol("secret_buffer")
if target_addr:
    data = edb.read_memory(target_addr, 32)
    # 进行 XOR 0x5A 解密
    decrypted = bytes([b ^ 0x5A for b in data])
    print(f"[+] Decrypted buffer: {decrypted}")
else:
    print("[-] Symbol not found")
```

### 10.4 Lua 5.4 极速条件 Hook 编写与 API 参考
在 Lua 5.4 环境中，全局表 `edb` 注入了完全对称的高性能 API，且系统重写了标准 `print()` 函数，所有输出自动重定向至控制台：

```lua
-- Lua 极速循环检测与条件触发
local rip = edb.get_reg("rip")
local rax = edb.get_reg("rax")
print(string.format("[Lua] Hook hit at 0x%x, RAX = 0x%x", rip, rax))

if rax > 1000 then
    print("[Lua] Threshold exceeded! Setting breakpoint at return address...")
    local rsp = edb.get_reg("rsp")
    local ret_bytes = edb.read_memory(rsp, 8)
    -- 处理返回地址
end
```

### 10.5 运行外部脚本文件与批量脱壳/内存转储实战
1. 编写独立脚本文件 `unpack.py`：
   ```python
   import edb, time

   oep = edb.resolve_symbol("main") or 0x401000
   edb.set_breakpoint(oep, "OEP")
   edb.resume()

   # 等待断点命中
   while edb.state() == "Running":
       time.sleep(0.01)

   print("[+] Reached OEP! Dumping memory...")
   payload = edb.read_memory(0x400000, 0x10000)
   with open("/tmp/dumped_text.bin", "wb") as f:
       f.write(payload)
   print("[+] Dump completed successfully!")
   ```
2. 在 Script Console 顶部点击 **▶ Run File...**，选中 `unpack.py`，即可全自动实现一键值守式脱壳与内存导出！

### 10.6 CommandBar 行内快速脚本求值 (`py ...` / `lua ...`)
无需专门切换到控制台标签页，逆向人员可在底栏常驻命令行中直接执行单行脚本：
- 执行 Python 表达式：`py print("Hex RAX:", hex(edb.get_reg('rax')))`
- 执行 Lua 语句：`lua print('PID is: ' .. edb.pid())`
执行结果与可能产生的异常 Traceback 将即时打印在系统日志与状态栏中。

### 10.7 断点绑定脚本自动化动作与无感打桩实战 (Script-Driven Breakpoint Hooking)

在逆向分析恶意软件或调试高并发网络服务时，常常需要针对关键系统调用或业务函数进行动态参数脱敏、解密捕获或无感修改。传统断点会频繁中断程序执行，破坏并发时序。

结合断点管理器的 **Script Action** 与 `return False` / `return false` 契约，可实现真正的零干扰动态打桩：

#### 场景 1：Python 动态解密关键缓冲区并无感放行
在加密处理函数入口（如 `encrypt_buffer`，假定 `RDI` 为缓冲区指针，`RSI` 为长度）下达断点，并在断点管理器中为其绑定如下 Python 脚本：
```python
import edb

buf_addr = edb.get_reg("rdi")
length = min(edb.get_reg("rsi") or 0, 64)

if buf_addr and length > 0:
    raw = edb.read_memory(buf_addr, length)
    edb.log(f"[Crypto Probe] Outgoing packet ({length} bytes): {raw.hex()}")

# 显式返回 False：静默单步放行，目标全速运行不暂停！
return False
```

#### 场景 2：Lua 极速高频条件计数器与动态提权打桩
在频繁调用的权限鉴权函数中下达断点，绑定如下轻量级 Lua 脚本：
```lua
-- Lua 5.4 超轻量微秒级执行
local uid = edb.get_reg("rdi")

if uid ~= 0 then
    -- 动态篡改 RDI 为 0 (Root 权限) 并记录日志
    edb.set_reg("rdi", 0)
    edb.log(string.format("[Lua Patch] Escalated UID %d to root (0)!", uid))
end

-- 显式返回 false：目标进程微秒级放行全速执行，不产生任何 UI 顿挫
return false
```
若仅在特定敏感条件（如 `uid == 1000`）发生时希望人工接管，则条件命中时不执行 `return false`（或执行 `return true`），调试器即刻精准挂起目标并定位反汇编视图！

---

## 11. 现代化 C++20 插件编写与使用全指南 (Plugin Development)

`edb-next` 拥有一套现代、安全、解耦的 C++20 插件体系。插件被编译为标准的 Linux 共享库（`.so`），由宿主通过 `QPluginLoader` 动态装载。

### 11.1 插件架构与工作原理

插件只需要实现纯虚接口契约 [IPlugin](core/IPlugin.hpp)，并通过 [IPluginContext](core/IPluginContext.hpp) 能力网关与宿主进行交互：
- **能力网关隔离**：插件无法直接破坏调试器的核心私有指针，必须通过 `context->activeSession()` 进行内存读写、寄存器访问与会话控制；
- **能力扩展维度**：
  1. **主菜单注入**：向主窗口 `Plugins` 菜单挂载自定义动作与弹窗；
  2. **CLI 命令行注册**：向底栏 CommandBar 注册自定义命令；
  3. **事件总线监听**：挂接断点触发（Breakpoint Hit）与会话状态改变（Session State Changed）钩子；
  4. **视图右键注入**：向反汇编、寄存器、内存 Dump 或栈视图注入右键菜单项；
  5. **偏好设置扩展**：向 `PreferencesDialog` 注入专属配置选项卡。

---

### 11.2 插件工程目录与 CMakeLists.txt 规范

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

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

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
    Qt6::Widgets
    Qt6::Core
)
```

---

### 11.3 编写插件头文件 (`MyPlugin.hpp`)

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

### 11.4 编写插件实现源码 (`MyPlugin.cpp`)

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

### 11.5 编译生成插件共享库 (`.so`)

使用 CMake 进行编译：
```bash
cd MyPlugin/
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```
构建完成后，将在构建目录下生成插件动态库文件：`my_plugin.so`。

---

### 11.6 插件安装、加载与实机测试

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

## 12. 常用快捷键一览表 (Cheat Sheet)

| 快捷键 / 交互操作 | 功能描述 | 对应分类 |
| :--- | :--- | :--- |
| **F9** | Continue (继续全速运行) | 调试执行 |
| **F7** | Step Into (单步步入) | 调试执行 |
| **F8** | Step Over (单步步过) | 调试执行 |
| **Shift+F11** | Step Out (跳出当前函数) | 调试执行 |
| **F4** | Run to Selection (运行到光标所在行) | 调试执行 |
| **Ctrl+F2** | Restart (重启会话) | 调试执行 |
| **Ctrl+\*** | Set New Origin Here (强制重设当前 RIP) | 调试执行 |
| **F2** | Toggle Breakpoint (切换软件断点) | 断点控制 |
| **Enter** | Follow Branch (跟随分支跳转/函数跟入) | 反汇编导航 |
| **Esc / Backspace** | Go Back (沿反汇编/转储历史栈瞬时回退) | 历史导航 |
| **Alt+Left / Alt+Right** | Navigate History (反汇编与转储历史后退 / 前进) | 历史导航 |
| **右键寄存器 -> +1 / -1** | GPR 极速加 1 / 减 1 (Increment / Decrement) | 寄存器微调 |
| **右键寄存器 -> Follow in Stack** | 快速追踪寄存器栈指针至象限 4 栈视图 | 视图联动 |
| **右键寄存器 -> Copy As...** | 多格式复制寄存器值 (Hex / Decimal / String) | 数据提取 |
| **右键转储 -> Follow QWORD** | 智能跨视图追踪 QWORD (Dump / Disasm / Stack) | 内存分析 |
| **右键转储 -> View as Struct** | 一键携带当前地址直达 Tab 22 Type Viewer 解析 | 结构体分析 |
| **右键栈单元 -> Follow in Disasm** | 追踪函数返回地址 `[Return Address]` 至调用点 | 调用栈分析 |
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
| **Alt+S** | 快速聚焦源码浏览器窗口 (SourceView) | 视图切换 |
| **Ctrl+Shift+S** | 切换反汇编与源码混合排版模式 (Mixed Mode) | 视图切换 |
| **Alt+P** | 快速打开并聚焦 Script Console (Python/Lua) | 自动化脚本 |
| **Alt+D** | 快速切换至内存 Dump 窗口 | 视图切换 |
| **Alt+K** | 快速切换至调用栈 (Call Stack) 抽屉 | 视图切换 |
| **Alt+B** | 快速切换至断点列表 (Breakpoints) 抽屉 | 视图切换 |
| **Alt+M** | 快速切换至内存分页段表 (Memory Regions) 抽屉 | 视图切换 |
| **Alt+E** | 快速切换至全局符号浏览器 (Symbols) 抽屉 | 视图切换 |
| **Alt+L** | 快速切换至系统日志控制台 (Log) 抽屉 | 视图切换 |
| **Shift+F7/F8/F9** | 透传 POSIX 信号执行 (Pass Signal Step/Run) | 信号控制 |

---

## 13. 结语

本教程系统化梳理了从零编译、四象限日常逆向、DWARF 源码级调试、嵌入式 Python/Lua 双脚本自动化控制、高阶漏洞分析、系统调用注入与 ELF 补丁磁盘落盘，直至 C++20 插件编写的全套操作规范。您可以充分发挥 `edb-next` 强大的底核控制力与极速交互体验，轻松攻克复杂 Linux 原生二进制逆向与漏洞利用分析难题！
