# AGENTS.md

给未来的自己（和被派来干活的 AI）看的工程约定。只记「不可再分的事实」：
命令、约定、陷阱。讨论过程的来龙去脉看 `doc/`。

---

## 一句话

Mhuixs 是一个基于内存的数据结构库：**数据结构**（LIST/TABLE/BITMAP/KVALOT/数值/字符串）
由 **HOOK** 统一引用，通过**命令**操作。不做脚本语言、不做网络。

## 常用命令

```bash
cd src
make              # 编译
./mhuixs          # 交互模式（REPL）；退出自动存盘
./mhuixs demo     # 主链路自检明细 + 命令演示，跑完退出
./mhuixs -h       # 用法
make test         # 三套回归测试：基础库 / 持久化往返 / HOOK 生命周期
```

## 数据与配置在哪

仓库里**没有**运行配置（`src/Mhuixs.config.example` 只是模板，不被程序读取）。

程序启动时按顺序找配置，找不到就自动生成一份：

1. `<可执行文件同目录>/Mhuixs.config` —— 部署时在此覆盖
2. `~/.mhuixs/Mhuixs.config` —— 用户配置，不存在则自动生成

数据落在 `~/.mhuixs/data/registry.dat`（配置里的 `MhuixsHomePath`，不存在会自动创建）。

## 代码结构

```
src/
  Mhuixs.c        入口：模块初始化 + REPL + demo + 自检
  execute.c       命令层：一行一条命令，mhx_execute() 是唯一入口
  registry.c      注册表：HOOK 增删查 + 权限 + 落盘（原子写）
  lib/
    bignum.c      BHS 统一类型：所有数据类型都装在这里
    list.c        分块双向列表（Block + 居中/分裂/合并）
    tblh.c        关系表（逻辑/物理行索引分离）
    kvalh.c       键值对（自建哈希桶）
    bitmap.c      位图
    hook.c        HOOK：名字 + 权限 + obj
    hash.c        Robin Hood 哈希表（注册表和各容器在用）
    mstring.h     带长度头的字符串（**注意：不带 \0**）
    env.c         配置加载
    getid.c       ID 分配器
    merr.c        错误码与日志
    bitcpy.c      位级拷贝
    pkg.c         MUIX 打包协议（**未纳入构建**，依赖 arpa/inet.h）
```

## 所有权规则（最容易违反，务必先读）

容器**拥有**它存放的元素。细则写在 `src/lib/list.h` 的注释里（LIST/TABLE/KVALOT 同规则）：

- **存入即交出所有权**：`list_rpush` / `tadd` / `kset` 之后，不要再释放那个对象
- **取出即拿回所有权**：`list_lpop` / `list_rpop` / `list_rm_index` 的返回值归调用方
- **只看不拿**：`list_get_index` / `tget` / `kget` 返回借用指针
- **复制是深拷贝**：`list_copy` / `table_copy` / `kvalot_copy`
- **注册表拥有 HOOK**，以及 `hook_set_bhs` 深拷贝进去的对象

一句话：**谁创建谁销毁，存进容器就交出，从容器取出就拿回。**
不引入引用计数，理由见 `doc/reference-model.md`。

## 新增一种数据类型要改 4 处

漏一处就会在某个路径上出错（尤其序列化，容易静默丢数据）：

1. `lib/bignum.h` —— 前向声明 `struct XXX;`、union 加成员、声明 `bignum_from_*` / `bignum_get_*`
2. `lib/bignum.c` —— 实现上面两个函数，并在 **`bignum_copy` / `bignum_destroy` /
   `bignum_free` / `bignum_to_string`** 里各加一个分支
3. `registry.c` —— **`bhs_serialize` 和 `bhs_deserialize` 各加一个显式分支**。
   **不要让它落到 else 兜底里**：兜底会把 union 里的指针字节当数据写出去，
   读回来就是一个 type 正确、指针是垃圾的对象，一碰就崩。
4. `execute.c` —— 命令实现 + 分发表 + `cmd_info` 的分支 + `mhx_help_text()`

## 已知陷阱（都是踩过的）

| 陷阱 | 症状 | 正确做法 |
|---|---|---|
| `mstr_cstr()` **不带 `\0`** | 字符串后多出乱码；乱码非法时会把整段 UTF-8 冲乱，看起来像编码坏了 | 用 `mstr_to_buf()` / `mstr_to_cstr()`，或 `%.*s` + `mstrlen()` |
| 分块容器未使用区域没清零 | 遍历整个 capacity 时读到野指针 → 段错误 | `calloc`，扩容后 `memset` 新增区域；不变式「要么有效对象要么 NULL」 |
| Windows `rename` 不覆盖已存在文件 | 原子改名失败 | 用 `MoveFileEx(..., MOVEFILE_REPLACE_EXISTING)` |
| `-std=c99` 下 glibc 不暴露 POSIX 函数 | Linux 上 `implicit declaration of readlink/fileno` | 特性宏放 **Makefile**（`-D_POSIX_C_SOURCE=200809L`），**不要放头文件** —— 包含顺序不保证 |
| 反序列化时「写到一半」和「合法空值」都返回 NULL | 文件被截断却报成功，数据静默丢失 | 加**标志字节**把两者分开（见 `registry.c` 的 `read_cell`、`has_bhs`） |
| 注释把 bug 说成设计 | 读代码的人会跳过 | 判断依据看**写盘侧的约定**，别信读盘侧的注释 |

## 平台

已在 **Windows（MSYS2 / MinGW-w64, gcc）+ Linux（Ubuntu 24.04, gcc）** 两个平台实测通过。

- MinGW 也定义 `_WIN32`，所以 Windows 上只编译 `_WIN32` 分支；
  `#else` 里的 Linux 代码在 Windows 上**不会被编译**。改完两边都要跑一次。
- `lib/getid.c` 无条件 `#include <pthread.h>`：Linux 原生可用，MSYS2 由 mingw 提供，
  但 MSVC / 原生 MinGW 会编译失败。
- 编译完成会打印目标平台（`$(CC) -dumpmachine`），借此确认自己在编哪一份。

## 测试

`make test` 跑三套（都在 `test/` 下，新增测试记得加进 Makefile 的 `test` 目标）：

| 文件 | 覆盖 |
|---|---|
| `test_lib_status.c` | LIST 所有权与深拷贝、嵌套 LIST、BITMAP、TABLE |
| `test_persist_roundtrip.c` | TABLE / KVALOT 落盘-恢复往返 |
| `test_registry_lifecycle.c` | HOOK 注册→挂数据→注销 的释放检查 |

另有 `test_list_memory.c`（手动运行，仅 Windows）：反复创建/释放观察进程工作集，
用来验证容器真的回收了元素。**改内存所有权相关的代码就靠它。**

## 提交

- **一个提交只做一件事**；相关但可分开撤销的改动要拆开提交
- 用本机 git 配置（`hujiyo`），不要 `-c user.name=` 临时覆盖
- 关键动作（push / 迁移）后用 `git ls-remote` 核实，**不要相信退出码**

**不要提交**：编译产物（`*.o` / `mhuixs` / `*.exe`）、`Mhuixs.config`、
`.workbuddy/`、密钥。

## 仓库环境

本仓库的 ref 后端为 **reftable**（2026-10-05 从 `files` 迁移）。

背景：老式 `files` 后端依赖「写 .lock → rename」，在 Windows 上这个窗口
可能被实时扫描类软件拦截，出现「`git push` 返回成功但 ref 没落盘」。
reftable 用追加式 ref 表 + 原子切换，从机制上消除了这个窗口。

相关命令：

```bash
git refs migrate --ref-format=reftable   # 迁移（可回退 --ref-format=files）
git config --get extensions.refStorage   # 查看当前后端
```

**「不迷信退出码」这条纪律仍然有效**，与后端无关：关键操作后一律用
`git ls-remote` / `git rev-parse` 核实真实状态。

## 当前状态与边界

**v0.1.0**：内核自洽、可交互使用。已实现的：六种数据结构全部可被 HOOK 引用、
命令层、REPL、原子落盘、损坏文件留档。

**刻意不做**（不是没做完，是决定不做，别再提）：

- **脚本语言**。操作方式是一行一条命令，没有变量 / `if` / `while` / 函数 /
  表达式求值。需要循环和判断的调用方自己写（C / Python / AI 生成的代码）。
  理由：命令可白名单、可审计、非图灵完备；造语言等于与 Lua / Python 竞争。
- **网络、多用户、权限隔离**。当前权限只有 owner/other 两档有效
  （组权限因用户组模块已剥离而恒通过）。

设计取舍的完整论证见 `doc/reference-model.md`。
