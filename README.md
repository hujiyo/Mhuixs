<img src=".logo/Mhuixs-logo.png" height="130px" />

# Mhuixs

> 内存数据结构库 · HOOK 统一引用

## 1. 这是什么

Mhuixs 是一个**基于内存的数据库**，核心只有两个概念：

- **数据结构**：LIST、TABLE、BITMAP、KVALOT —— 都是纯粹的内存结构，一个文件一个文件打磨出来的
- **HOOK**：指向数据结构的统一引用。**任何能被 HOOK 引用的东西，才叫数据结构**

设计原点（2024.11）：

> 数据是一片蓝海，HOOK 是鱼钩，各种数据结构是不同类型的鱼。
> 只要通过 HOOK 就能引用数据并操作它；
> 数据结构之间也能通过索引（同样是 HOOK）互相引用。

HOOK 同时承担三件事：**防止数据失联**（有名可查）、**权限控制收口**、**压缩与落盘的基本单位**。

---

## 2. 当前状态（v0.1.0 / 2026-10）

项目正处于**重建期**，当前聚焦回最初的内核。上一轮减重剥离了后来叠加、且相互冲突的部分：

**已剥离**

| 层 | 内容 | 原因 |
|---|---|---|
| 语言层 | lexer / parser / ast / evaluator / compiler / bytecode / vm | 后加的，且内部存在两套互斥的解释器后端 |
| 网络层 | netplug、queue | 依赖未链接的 libuv，且从未跑通 |
| 用户组 | usergroup、bcrypt、crypt、iami | 与前两者冲突，且初始化早已被注释掉 |

**已保留（内核）**

| 模块 | 文件 |
|---|---|
| 数据结构 | `lib/list.c`、`lib/tblh.c`、`lib/bitmap.c`、`lib/kvalh.c`、`lib/bignum.c` |
| HOOK 与注册表 | `lib/hook.c`、`registry.c` |
| 命令层 | `execute.c` |
| 基础设施 | `lib/env.c`、`lib/getid.c`、`lib/merr.c`、`lib/hash.c`、`lib/mstring.h`、`lib/bitcpy.c` |

**v0.1.0 的界定**：内核自洽、能交互使用的第一个版本。具体是：

- 编译零错误，三套回归测试全过
- **六种数据结构全部可以被 HOOK 引用**（含 TABLE / KVALOT）
- 内存所有权处处明确（容器拥有元素；注册表拥有 HOOK），有对照测量作证
- 能交互使用：`./mhuixs` 进入 REPL，命令驱动，退出时自动存盘
- HOOK 与数据可完整落盘并恢复

尚未包含（见"已知限制"）：网络、多用户与权限隔离、脚本语言、原子落盘、命令历史。

| 数据结构 | BHS 原生支持 | 能挂到 HOOK |
|---|---|---|
| NUMBER / STRING | ✅ | ✅ |
| LIST | ✅ | ✅ |
| BITMAP | ✅ | ✅ |
| TABLE | ✅ | ✅ |
| KVALOT | ✅ | ✅ |

---

## 3. 命令层

Mhuixs **不做自己的编程语言**。操作方式是**一行一条命令**，空格分隔。

这是刻意的边界：没有变量、没有 `if`/`while`/`for`、没有函数定义、没有表达式求值。
需要循环和判断的调用方，用 C / Python / 或 AI 现场生成的代码来写。
好处是命令可白名单、可审计、天然不是图灵完备——对 AI 调用方尤其重要。

```
create <list|bitmap|kvalot> <name>      创建数据结构并挂钩
create table <name> <field:type> [...]  创建表，如 id:int name:str
drop <name>                             摘除钩子
hooks                                   列出所有钩子
type <name>                             查看钩子指向的类型

rpush/lpush/lpop/rpop <name> [<v>...]   LIST 操作
llen/lget/lset <name> ...               LIST 操作

bset/bget/bcount/bsize <name> ...       BITMAP 操作

kset <name> <key> <value>               KVALOT 写入（键已存在则覆盖）
kget/kdel/kexists/klen <name> ...       KVALOT 操作

tadd <name> [<v>...]                    TABLE 追加一行
tget/tset <name> <row> <col> [...]      TABLE 读写单元格，col 可用列名
tdel <name> <row>                       TABLE 删除一行
trows/tfields <name>                    TABLE 行数 / 字段列表
```

值语法：`123` → NUMBER，`"hello"` → STRING（引号强制），`hello` → STRING（自动回退）。
KVALOT 的键一律按字符串处理。

运行效果：

```
  mhuixs> create table users id:int name:str age:int
         OK created table 'users' with 3 field(s)
  mhuixs> tadd users 1 alice 25
         OK tadd users, rows=1
  mhuixs> tget users 0 name
         OK "alice"
  mhuixs> kset cache user:1 bob
         OK kset cache, keys=1
  mhuixs> kget cache user:1
         OK "bob"
  mhuixs> hooks
         OK 4 hook(s):
         fruits           list
         flags            bitmap
         cache            kvalot
         users            table
```

---

## 4. 编译与运行

```bash
cd src
make          # 编译
./mhuixs      # 进入交互模式（REPL）
make test     # 回归测试（三套）：基础库 / 持久化往返 / HOOK 生命周期
```

### 交互模式

直接运行 `./mhuixs` 进入 REPL，一行一条命令：

```
==== Mhuixs 0.1.0 ====
  ENV 模块就绪
  Logger 模块就绪
  ID 分配器就绪
  注册表就绪
  无已持久化的 HOOK（首次运行或数据为空）
  主链路自检通过

输入命令，:help 查看用法，:quit 退出

mhuixs> create table users id:int name:str age:int
OK created table 'users' with 3 field(s)
mhuixs> tadd users 1 alice 25
OK tadd users, rows=1
mhuixs> tget users 0 name
OK "alice"
mhuixs> create kvalot cache
OK created kvalot 'cache'
mhuixs> kset cache user:1 bob
OK kset cache, keys=1
mhuixs> kget cache user:1
OK "bob"
mhuixs> hooks
OK 2 hook(s):
cache            kvalot
users            table
mhuixs> :quit

已保存 2 个 HOOK 到磁盘
```

交互元命令：`:help` 查看全部命令，`:save` 立即存盘，`:quit` 退出（退出时自动保存）。

### 其他入口

```bash
./mhuixs demo       # 主链路自检明细 + 命令层演示，跑完退出
./mhuixs -h         # 用法说明
```

数据目录由 `src/Mhuixs.config` 的 `MhuixsHomePath` 指定，HOOK 注册表落盘为 `<MhuixsHomePath>/registry.dat`。启动时自动恢复，退出时自动保存。

---

## 5. 平台支持

**已在两个平台实测通过：**

| 平台 | 编译器 | 结果 |
|---|---|---|
| Windows（MSYS2 / MinGW-w64） | gcc 15.2.0 (`x86_64-w64-mingw32`) | make 零错误；三套测试全过；REPL 正常 |
| Linux（Ubuntu 24.04 LTS，x86_64） | gcc 13.3.0 (`x86_64-linux-gnu`) | make 零错误；三套测试全过；REPL 正常 |

Linux 侧实测（2026-10-05）：编译、三套回归测试、REPL 交互、
六种数据结构的挂载与持久化跨重启，全部正常。
两平台的编译告警集合**完全一致**（`-Wsign-compare` 35 处、`-Wunused-variable` 5 处、
`-Wtype-limits` 3 处等，均为既有代码风格问题，不影响功能）。

**数据文件可跨平台搬运**：Windows 写出的 `registry.dat`（含 LIST / BITMAP /
KVALOT / TABLE）在 Linux 上能完整恢复。原因是序列化只用定宽整数、不做结构体
整体 dump。注意这条结论的前提是**同为小端序、同为 LP64 模型**；
若将来支持大端或 32 位平台需要重新验证。

代码的跨平台情况分三类：

| 类别 | 文件 | 说明 |
|---|---|---|
| 已写平台分支 | `registry.c`、`lib/env.c`、`lib/env.h`、`Mhuixs.c` | 互斥锁、路径分隔符、取可执行文件路径都有 `_WIN32` 分支 |
| 依赖 POSIX 兼容层 | `lib/getid.c` | 无条件 `#include <pthread.h>`。Linux 原生可用；MSYS2 下由 mingw 提供；但**用 MSVC 或原生 MinGW 会编译失败** |
| 未纳入构建 | `lib/pkg.c/h` | 依赖 `arpa/inet.h`（Linux 专有）。网络层回归时再处理 |

### 编译告警里的一个坑（已修）

`-std=c99` 下 glibc 会定义 `__STRICT_ANSI__`，**默认不暴露 POSIX 函数**——
即使包含了 `<unistd.h>`，`readlink()` 也没有声明，编译器只能按 `int` 去理解
它的返回值（实际是 `ssize_t`）。这个问题只在 Linux 上能看到
（Windows 上那段代码在 `#else` 分支里，根本没被编译）。
修法是在 `lib/env.h` 顶部请求 `_POSIX_C_SOURCE`。

### 已知的跨平台缺口

`src/Mhuixs.config` 里 `MhuixsHomePath` 写的是 Windows 绝对路径
（`D:\Mhuixs_data`），且数据目录不存在时会拒绝启动 —— 意味着
**Linux 上拿到源码后开箱不可用**，需要先改配置并手动建目录。
详见第 8 章"已知限制"。

---

## 6. 目录结构

```
Mhuixs/
├── src/
│   ├── Mhuixs.c          # 入口：模块初始化 + 交互模式(REPL) + demo + 自检
│   ├── execute.c/h       # 命令执行层：一行一条命令
│   ├── registry.c/h      # 注册表：统一管理 HOOK、权限、落盘
│   ├── Makefile
│   ├── Mhuixs.config     # 运行配置
│   └── lib/              # 基础库
│       ├── list.c/h      # LIST 列表
│       ├── tblh.c/h      # TABLE 表
│       ├── bitmap.c/h    # BITMAP 位图
│       ├── kvalh.c/h     # KVALOT 键值对
│       ├── bignum.c/h    # BHS 统一类型 + 任意精度数值（含各类型的桥接）
│       ├── hook.c/h      # HOOK
│       ├── hash.c/h      # Robin Hood 哈希表
│       ├── mstring.h     # 字符串
│       ├── getid.c/h     # ID 分配器
│       ├── env.c/h       # 环境配置
│       ├── merr.c/h      # 错误码与日志
│       ├── bitcpy.c/h    # 位级拷贝
│       ├── pkg.c/h       # MUIX 打包协议（暂挂起，待网络层回归）
│       └── logo.c/h      # 启动标识
├── doc/                  # 哈希表相关的设计文档、引用模型
└── test/
    ├── test_lib_status.c            # LIST 所有权与深拷贝 / BITMAP / TABLE（make test）
    ├── test_persist_roundtrip.c     # TABLE / KVALOT 落盘-恢复往返（make test）
    ├── test_registry_lifecycle.c    # HOOK 注册→挂数据→注销 的释放检查（make test）
    ├── test_list_memory.c           # LIST 内存回收检查（手动，仅 Windows）
    └── test_hash_performance.c      # 哈希表性能（手动）
```

---

## 7. 内存与所有权模型

容器（LIST / TABLE / KVALOT）**拥有**它们存放的元素。规则在 `src/lib/list.h` 里写死了：

- **存入即交出所有权**：`list_rpush` / `tadd` / `kset` 之后，不要再释放那个对象
- **取出即拿回所有权**：`list_lpop` / `list_rpop` / `list_rm_index` 的返回值归调用方，用完要 `bignum_destroy`
- **只看不拿（借用）**：`list_get_index` / `tget` / `kget` 返回内部指针，不要释放
- **复制是深拷贝**：`list_copy` / `table_copy` / `kvalot_copy` 产出的新结构完全独立

要验证回收是否正常，跑 `test/test_list_memory.c`（见文件头部的编译命令）。
实测 80 万次分配，工作集变化 +0.1 MB 以内。

**为什么不用引用计数**、HOOK 与 BHS 的分工、以及将来若要支持"引用共享"该怎么走，
见 [`doc/reference-model.md`](doc/reference-model.md)。

---

## 8. 已知限制

- **组权限当前对所有人生效**：用户组模块剥离后，`get_primary_gid_by_uid()` 恒返回 0，`HOOK_login()` 也把 `hook->group` 设为 0，于是任何 caller 都被判定为"同组"。在用户组模块回归前，**不要依赖组权限做隔离**。详见 `src/lib/hook.c`。
- **`mstr_cstr()` 返回的指针不带 `\0`**：不能直接配 `printf("%s")` / `strcmp` 用，会读到相邻未初始化内存（症状是字符串后面多出乱码，且时有时无）。要用 `mstr_to_cstr()`（需 free）或 `%.*s` + `mstrlen()`。详见 `src/lib/mstring.h`。
- **`src/Mhuixs.config` 写死了 Windows 路径，且数据目录不存在就拒绝启动**：`MhuixsHomePath D:\Mhuixs_data` 在 Linux 上不是有效路径，`env_init()` 直接失败、程序退出。所以 Linux 上必须先改配置并手动建目录才能用。两处都值得修：配置该用平台无关的路径，数据目录该在首次运行时自动创建。
- `lib/pkg.c` 依赖网络字节序（`arpa/inet.h`），暂未纳入构建。
- 注册表落盘不是原子操作：直接写 `registry.dat`，中途失败会留下截断的文件。建议改为写临时文件再重命名。
- 深拷贝没有环检测：若数据里出现自引用（结构套自己），`bignum_copy` 会无限递归。当前没有产生这种结构的路径。
- 权限系统只有 owner/other 两档真正生效（组权限因用户组模块剥离而恒通过）。
- 交互模式没有命令历史、没有 Tab 补全（有意从简，先把可用的最小形态做出来）。
- 注册表锁在 Windows 上是可重入的临界区，POSIX 分支用的是默认互斥锁。
  已有代码刻意避免在持锁时再取锁，但换平台时值得复查一遍。

---

## 9. 设计边界（重要）

Mhuixs 不实现自己的编程语言。操作以命令形式提供，语言层面的循环、判断、函数交给调用方。
这样做的理由是：命令可白名单、可审计、非图灵完备；而造一门语言意味着与 Lua / Python 竞争，
投入巨大且价值有限。详见 `src/execute.c` 顶部注释。

---

## 10. 里程碑

- **2024.10** 项目启动，最初设想：内存数据结构 + HOOK 统一引用
- **2024.12** 初版服务端骨架（run_queue / ret_queue + 命令格式）
- **2025.01** 纯 C 数据结构库成型（`datstrc/`：table / kvalot / list / bitmap / stack / queue / stream）
- **2025.07** 迁往 C++，扩展为服务端 + 客户端 + 语言层的完整构想
- **2026.01** 从 C++ 迁回 C；Logex 语言层集中落地后陷入方向迷失
- **2026.10.05** **清理重建**：剥离语言层 / 网络层 / 用户组，回到内核；
  补齐 TABLE 与 KVALOT 到 HOOK 的桥接；确立所有权模型并修掉一批泄漏；
  补上命令层与交互模式 → **v0.1.0**

---

## 11. 参与与交流

- **Email**: Mhuxis@outlook.com | Mhuxis.db@gmail.com
- **GitHub**: [hujiyo/Mhuixs](https://github.com/hujiyo/Mhuixs)

---

**本 README 最后更新**: 2026.10.05 · **v0.1.0**（内核可用，进入重建期后的第一个版本）
