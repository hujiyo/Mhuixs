<img src=".logo/Mhuixs-logo.png" height="130px" />

# Mhuixs

> 内存数据结构库 · HOOK 统一引用

## 1. 这是什么

Mhuixs 是一个**基于内存的数据库**，核心只有两个概念：

- **数据结构**：LIST、TABLE、BITMAP、KVALOT —— 都是纯粹的内存结构
- **HOOK**：指向数据结构的统一引用。**任何能被 HOOK 引用的东西，才叫数据结构**

设计原点（2024.11）：

> 数据是一片蓝海，HOOK 是鱼钩，各种数据结构是不同类型的鱼。
> 只要通过 HOOK 就能引用数据并操作它；
> 数据结构之间也能通过索引（同样是 HOOK）互相引用。

HOOK 同时承担三件事：**防止数据失联**（有名可查）、**权限控制收口**、**压缩与落盘的基本单位**。

---

## 2. 当前状态（v0.2.0 / 2026-10）

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
| 会话层 | `session.c` |
| 网络 | `lib/sock.c`、`proto.c`、`server.c` |
| 基础设施 | `lib/env.c`、`lib/getid.c`、`lib/merr.c`、`lib/hash.c`、`lib/mstring.h`、`lib/bitcpy.c` |

**v0.2.0 的界定**：在 v0.1.0（内核自洽、能交互使用）之上，加上了
**能被别的进程使用**这一条 —— 一个常驻服务端，加一份文档化的线格式。

- 编译零错误（两个平台），七套回归测试全过
- **六种数据结构全部可以被 HOOK 引用**（含 TABLE / KVALOT）
- 内存所有权处处明确（容器拥有元素；注册表拥有 HOOK），有对照测量作证
- 能交互使用：`./mhuixs` 进入 REPL，命令驱动，退出时自动存盘
- HOOK 与数据可完整落盘并恢复
- **能作为服务端跑**：`./mhuixs server`，别的语言按 MHP/1 协议连上来下命令
- 参考客户端在 `client/python/mhuixs.py`

尚未包含（见"已知限制"）：多连接并发、多用户与权限分级、脚本语言、命令历史。

| 数据结构 | BHS 原生支持 | 能挂到 HOOK | 能经网络操作 |
|---|---|---|---|
| NUMBER / STRING | ✅ | ✅ | ✅ |
| LIST | ✅ | ✅ | ✅ |
| BITMAP | ✅ | ✅ | ✅ |
| TABLE | ✅ | ✅ | ✅ |
| KVALOT | ✅ | ✅ | ✅ |

---

## 3. 命令层

Mhuixs 的操作方式是**一行一条命令**，空格分隔。

这是刻意的边界：没有变量、没有 `if`/`while`/`for`、没有函数定义、没有表达式求值。
需要循环和判断的调用方，用 C / Python / 或 AI 现场生成的代码来写。
好处是命令可白名单、可审计、天然不是图灵完备——对 AI 调用方尤其重要。

```
create <list|bitmap|kvalot> <name>      创建数据结构并挂钩
create table <name> <field:type> [...]  创建表，如 id:int name:str
drop <name>                             摘除钩子（并释放其数据）
hooks                                   列出所有钩子，带内容摘要
info <name>                             查看某个钩子的内容
type <name>                             只看钩子指向的类型

rpush/lpush/lpop/rpop <name> [<v>...]   LIST 操作
llen/lget/lset/lrem <name> ...          LIST 操作

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

`hooks` 带内容摘要，`info` 展开细节：

```
mhuixs> hooks
OK 4 hook(s):
fruits           list     len=3
flags            bitmap   bits=64 ones=3
cache            kvalot   keys=2
users            table    rows=3 fields=3

mhuixs> info users
users : table
  字段 3 个: id(number) name(string) age(number)
  记录 3 行
  [0] 1 "alice" 25
  [1] 2 "bob" 30
  [2] 3 "carol" (empty)

mhuixs> info flags
flags : bitmap
  共 64 位，其中 1 有 3 个
  前 64 位: 0001000000100000000000000000000000000000000000000000000000000001
```

`info` 最多展示 20 条，更多的会提示还有多少未显示。

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
make               # 编译
./mhuixs           # 进入交互模式（REPL）
./mhuixs job.mhx   # 脚本模式：执行命令文件后退出
./mhuixs server    # 服务端模式：监听端口，供其它程序连接
make test          # 回归测试（七套）：基础库 / 持久化往返 / HOOK 生命周期 /
                   #   命令层输出 / 会话层 / 协议编解码 / 网络集成
```

### 交互模式

直接运行 `./mhuixs` 进入 REPL，一行一条命令：

```
==== Mhuixs 0.2.0 ====
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

### 脚本模式

像 `python script.py` 那样，把命令文件直接交给它：

```bash
./mhuixs job.mhx      # 执行命令文件后退出
./mhuixs - < job.mhx  # 等价形式：从 stdin 逐行读命令
```

`job.mhx` 一行一条命令，空行与 `#` 注释跳过：

```
# 初始化演示数据
create list fruits
rpush fruits apple banana
llen fruits
```

与交互模式的命令语义完全相同，差别只在呈现：

- **stdout 只剩命令输出**：提示符不打印，启动/收尾信息走 stderr，可以放心 `> result.txt`
- 元命令可用：`:format json`（结果变成 JSON 流）、`:save`、`:quit`
- **出错即停**：第一条失败的命令中止脚本，退出码 1，报错带文件名与行号；打不开文件退出码 2
- 跑完（或 `:quit`）自动存盘，与交互模式同一语义——脚本跑到哪，就存到哪

仍然没有变量、条件、循环：脚本模式只是把「一行一条命令」的输入源从键盘换成
文件，不是脚本语言（见第 9 节设计边界）。

### 服务端模式

让**别的进程、别的语言**来下命令：

```bash
MHUIXS_TOKEN=$(openssl rand -hex 16) ./mhuixs server
```

监听地址与端口取自配置的 `bind`（默认 `127.0.0.1`）和 `port`（默认 18185）。
访问令牌取自配置的 `authtoken`，或环境变量 `MHUIXS_TOKEN`（后者优先，
临时起一次服务可以不让令牌落进配置文件）。

**没有令牌就拒绝启动**。这不是洁癖：协议是明文的，一个没有门槛的 Mhuixs
等于把数据摊在网上。连上来之后第一条请求必须是 `AUTH <令牌>`，不对就断开。

用自带的 Python 客户端试一下（它同时是这份协议的参考实现）：

```bash
python client/python/mhuixs.py --token 你的令牌 \
  'create list todo' 'rpush todo milk egg' 'llen todo'
# OK created list 'todo'
# OK rpush todo, len=2
# OK 2
```

#### 线格式（MHP/1）

请求是**参数数组**，每个参数带长度前缀：

```
*3\r\n$5\r\nrpush\r\n$4\r\ntodo\r\n$4\r\nmilk\r\n
```

响应有三种：

```
+OK\r\n                        认证成功
-NOAUTH invalid token\r\n      出错，服务端随后断开
$57\r\n{"ok":true,...}\r\n     一切正常结果（JSON 文档）
```

要点：

- **参数带长度，不是拼成一行**。值是任意字节，可以含空格、换行、引号 ——
  文本划不了这个界，长度前缀才能。这就是所谓二进制安全。
- `$` 元素按内容自动判定数字还是字符串（等价于不加引号）；`+` 元素强制当
  字符串（等价于加引号），所以 `rpush x "123"` 那种语义不会丢。
- 响应体就是命令层那套 JSON（`ok` / `code` / `kind` / `value` / `text`），
  错误码取值与文本模式完全一致。程序只该认这些字段，不要去解析文案。

#### 已知边界

- **一次只服务一个连接**（v0.2.0 是有意为之）。第二个客户端不会收到拒绝，
  而是排在监听队列里等着 —— 所以它看起来像"没反应"，其实是排队。
- **明文传输**。令牌能挡住误连，挡不住抓包。只在本机或可信网络里用；
  要跨公网请套隧道。
- **单个参数上限 255 字节，且不能含 NUL 字节**。这是命令层把参数当 C 字符串
  处理决定的，不是分帧层的限制。超了会明确报错，不会静默截断。
- `:quit` 只断开当前连接，服务继续；`:shutdown` 才让服务停下来（停机前存盘）。

### 其他入口

```bash
./mhuixs demo       # 主链路自检明细 + 命令层演示，跑完退出
./mhuixs -h         # 用法说明
```

数据目录和配置**不在仓库里**：首次启动会自动在用户目录生成
`~/.mhuixs/Mhuixs.config`，并把数据目录也建在 `~/.mhuixs/data`。
启动时从 `registry.dat` 恢复，退出时自动保存。

配置查找顺序（`src/Mhuixs.config.example` 是模板，只作说明）：

1. `<可执行文件所在目录>/Mhuixs.config` —— 部署时可在此覆盖
2. `~/.mhuixs/Mhuixs.config` —— 用户配置；不存在则自动生成

想改配置，直接编辑 `~/.mhuixs/Mhuixs.config`；删掉它会重新生成一份默认的。
配置里写的 `MhuixsHomePath` 目录如果不存在，会自动递归创建。

### 落盘是原子的

保存时先写 `registry.dat.tmp`，刷到磁盘后再整体改名覆盖 `registry.dat`。
这样目标文件在任何时刻要么是旧的完整内容、要么是新的完整内容，
不存在"写到一半"的中间态 —— 断电、OOM、Ctrl-C 都不会把已有数据毁掉。

实测（Ubuntu，注入"写完文件头就 `_exit`"模拟中途崩溃）：

| | 旧行为（直接写目标文件） | 现行为（临时文件 + 改名） |
|---|---|---|
| 崩溃前 | 113 B, md5 `2bc80fdb…` | 113 B, md5 `2bc80fdb…` |
| 崩溃后 | **12 B, md5 变了** | **113 B, md5 不变** |
| 重启后 | 恢复失败，0 个 hook，数据全丢 | 恢复 1 个 hook，内容完好 |

崩溃会留下一个 `registry.dat.tmp`，无碍：下次保存以 `wb` 打开会直接截断它，
不会累积，也不会被加载。

### 快照式持久化：只在关库时写

落盘只有两个时机：**关库**（`:quit` / Ctrl-D）和手动 `:save`。
运行期间不写盘（`tadd` / `kset` 只动内存），所以不需要增量落盘，也不需要日志。

**崩溃、Ctrl-C、关窗口、`kill -9` 不在设计考虑范围内**：进程直接终止，
本次会话的改动全部丢失。这是快照模型的既定语义，不是待修的缺陷；
想留住进度就中途敲 `:save`。

上面那套「原子改名」保护的是**保存动作本身**不被写成半截文件，
暂时不提供崩溃恢复。

---

## 5. 平台支持

**已在两个平台实测通过：**

| 平台 | 编译器 | 结果 |
|---|---|---|
| Windows（MSYS2 / MinGW-w64） | gcc 15.2.0 (`x86_64-w64-mingw32`) | make 零错误；七套测试全过；REPL / 服务端正常 |
| Linux（Ubuntu 24.04 LTS，x86_64） | gcc 13.3.0 (`x86_64-linux-gnu`) | make 零错误；七套测试全过；REPL / 服务端正常 |

Linux 侧实测（2026-10-13）：编译、七套回归测试、REPL 交互，
以及**起服务端并用 Python 客户端走完整链路**（认证 → 建表写入查询 →
存盘 → 停机 → 重启后数据仍在），六种数据结构的挂载与持久化跨重启，全部正常。
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
| 已写平台分支（网络） | `lib/sock.c`、`lib/sock.h` | Winsock 与 BSD socket 的差异收口在这里：初始化、关闭、错误码、收发签名、`SO_REUSEADDR` 的语义分歧 |
| 依赖 POSIX 兼容层 | `lib/getid.c` | 无条件 `#include <pthread.h>`。Linux 原生可用；MSYS2 下由 mingw 提供；但**用 MSVC 或原生 MinGW 会编译失败** |

### 编译告警里的一个坑（已修）

`-std=c99` 下 glibc 会定义 `__STRICT_ANSI__`，**默认不暴露 POSIX 函数**——
即使包含了 `<unistd.h>`，`readlink()` / `fileno()` 也没有声明，
编译器只能按 `int` 去理解返回值（实际是 `ssize_t`）。
这个问题只在 Linux 上能看到（Windows 上那些代码在 `#else` 分支里，根本没被编译）。

修法是在 **Makefile** 里加 `-D_POSIX_C_SOURCE=200809L`（非 Windows 时）。
放头文件里是靠不住的 —— `registry.h` 第一行就是 `#include <stdio.h>`，
等轮到 `env.h` 定义宏时 `stdio.h` 已经进来了，`fileno` 仍然没有声明。
特性宏必须在任何系统头文件之前定义，只有构建脚本能保证这一点。
修好之后，两平台的编译告警集合完全一致。

### 配置与数据放在哪（曾导致 Linux 开箱不可用）

`src/Mhuixs.config` 曾经被提交进仓库，里面写死了 `MhuixsHomePath D:\Mhuixs_data`，
而且数据目录不存在就拒绝启动 —— 结果在 Linux 上拿到源码后完全跑不起来
（这条只在 Linux 实测时才暴露，Windows 上看不出来）。

现在改成：**配置文件不进仓库**（它是机器相关的），
首次启动在 `~/.mhuixs/Mhuixs.config` 生成一份，
数据目录 `~/.mhuixs/data` 一并自动创建。仓库里只留 `Mhuixs.config.example` 作模板。

---

## 6. 目录结构

```
Mhuixs/
├── src/
│   ├── Mhuixs.c          # 入口：模块初始化 + 交互/脚本/服务端/demo 四种模式 + 自检
│   ├── execute.c/h       # 命令执行层：一行一条命令，参数进来结果出去
│   ├── session.c/h       # 会话层：元命令 + 命令执行，三个前端共用
│   ├── proto.c/h         # MHP/1 线格式编解码（纯函数，无 socket 依赖）
│   ├── server.c/h        # 网络前端：监听、accept、按帧收发
│   ├── registry.c/h      # 注册表：统一管理 HOOK、权限、落盘
│   ├── Makefile
│   ├── Mhuixs.config.example  # 配置模板（只作说明；运行配置在 ~/.mhuixs/）
│   └── lib/              # 基础库
│       ├── list.c/h      # LIST 列表
│       ├── tblh.c/h      # TABLE 表
│       ├── bitmap.c/h    # BITMAP 位图
│       ├── kvalh.c/h     # KVALOT 键值对
│       ├── bignum.c/h    # BHS 统一类型 + 任意精度数值（含各类型的桥接）
│       ├── hook.c/h      # HOOK
│       ├── hash.c/h      # Robin Hood 哈希表
│       ├── sock.c/h      # 跨平台 socket 薄封装（POSIX / Winsock）
│       ├── mstring.h     # 字符串
│       ├── getid.c/h     # ID 分配器
│       ├── env.c/h       # 环境配置
│       ├── merr.c/h      # 错误码与日志
│       ├── bitcpy.c/h    # 位级拷贝
│       └── logo.c/h      # 启动标识（客户端时期遗留，当前无调用点）
├── doc/                  # 哈希表相关的设计文档、引用模型
├── client/
│   └── python/mhuixs.py  # Python 客户端，也是 MHP/1 的参考实现
└── test/
    ├── test_lib_status.c            # LIST 所有权与深拷贝 / BITMAP / TABLE（make test）
    ├── test_persist_roundtrip.c     # TABLE / KVALOT 落盘-恢复往返（make test）
    ├── test_registry_lifecycle.c    # HOOK 注册→挂数据→注销 的释放检查（make test）
    ├── test_execute_json.c          # 命令层输出：文本逐字节不变 + JSON 结构（make test）
    ├── test_session.c               # 会话层：元命令分派 + 格式是会话私有状态（make test）
    ├── test_protocol.c              # 协议编解码：半包/粘包/二进制安全（make test）
    ├── test_server_net.c            # 网络集成：真 socket 真分帧（make test）
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
- **服务端一次只服务一个连接**：有意为之，见第 4 节。第二个客户端会排队等待而不是被拒绝。多连接并发要改的是会话层那一处锁接缝（`session.c` 的 `mhx_exec_lock`），命令实现不用动。
- **服务端是明文协议**：令牌只挡误连，不挡抓包。要么只监听本机，要么自己套隧道。
- **单个参数上限 255 字节且不能含 NUL**：命令层把参数当 C 字符串处理，这是那一层决定的，不是分帧层的。超限会明确报错。
- **准入即全权**：令牌对得上就连上来，连上来就是 owner。per-hook 权限位当前不参与判定（上面那条组权限恒通过是同一个原因），不要以为它管事。
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
- **2026.10.05** 补上 `drop` / `reg_destroy` 的释放（所有权写进代码，不只写在文档里）；
  配置移出仓库，首次启动在 `~/.mhuixs` 下生成 → **v0.1.1**
- **2026.10.05** 加固数据安全与呈现 → **v0.1.2**：落盘改原子写；
  加载失败时留档 `.corrupt` 而不是被空注册表覆盖；反序列化不再把"读坏了"
  当成"合法空值"；小数不再被截成整数；demo 不再覆盖用户同名钩子
- **2026.10.06** 原生脚本模式 → **v0.1.4**：`mhuixs 文件` 像跑 python 一样
  直接执行命令文件（`mhuixs -` 读 stdin）；stdout 只剩命令输出，
  出错即停带行号，跑完自动存盘
- **2026.10.13** 服务端化 → **v0.2.0**：抽出会话层（三个前端共用元命令，
  输出格式不再是一个进程级全局开关）、定下 MHP/1 线格式（参数带长度、
  二进制安全）、`mhuixs server` 监听并把命令开放给别的进程，
  Python 参考客户端落地。走的不是"把旧网络层捡回来"那条路：不引第三方库，
  只加一层跨平台 socket 薄封装

---

## 11. 参与与交流

- **Email**: Mhuxis@outlook.com | Mhuxis.db@gmail.com
- **GitHub**: [hujiyo/Mhuixs](https://github.com/hujiyo/Mhuixs)
