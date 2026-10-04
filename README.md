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

## 2. 当前状态（2026-10）

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
| 基础设施 | `lib/env.c`、`lib/getid.c`、`lib/merr.c`、`lib/hash.c`、`lib/mstring.h`、`lib/bitcpy.c` |

当前内核可编译、可运行，并通过 5 项主链路自检。**六种数据结构全部可以被 HOOK 引用**：

| 数据结构 | BHS 原生支持 | 能挂到 HOOK |
|---|---|---|
| NUMBER / STRING | ✅ | ✅ |
| LIST | ✅ | ✅ |
| BITMAP | ✅ | ✅ |
| TABLE | ✅ | ✅ |
| KVALOT | ✅ | ✅ |

支持 HOOK 落盘与恢复（含 TABLE / KVALOT 的完整内容）。

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
./mhuixs      # 运行：模块初始化 + 主链路自检 + 命令层演示
make test     # 回归测试（两个）：基础库所有权与深拷贝 / 持久化往返
```

预期输出（节选）：

```
==== Mhuixs 内核启动 ====
  ENV 模块就绪
  Logger 模块就绪
  ID 分配器就绪
  注册表就绪

---- 主链路自检 ----
  [1] 任意精度数值     100 + 200 = 300
  [2] HOOK 注册        'mhuixs_selfcheck' 注册成功（当前 HOOK 数=1）
  [3] HOOK 查找        'mhuixs_selfcheck' 找到，名字=mhuixs_selfcheck
  [4] 重名保护         重复注册被拒绝 (ret=1)
  [5] 权限检查         root 可读=1，其他用户可读=1

---- 命令层演示 ----
  mhuixs> create list fruits
         OK created list 'fruits'
  ...

==== 自检结果：全部通过 ====
```

数据目录由 `src/Mhuixs.config` 的 `MhuixsHomePath` 指定，HOOK 注册表落盘为 `<MhuixsHomePath>/registry.dat`。

---

## 5. 目录结构

```
Mhuixs/
├── src/
│   ├── Mhuixs.c          # 内核入口：模块初始化 + 自检 + 命令层演示
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
├── doc/                  # 哈希表相关的设计文档
└── test/
    ├── test_lib_status.c            # LIST 所有权与深拷贝 / BITMAP / TABLE（make test）
    ├── test_persist_roundtrip.c     # TABLE / KVALOT 落盘-恢复往返（make test）
    ├── test_list_memory.c           # LIST 内存回收检查（手动，仅 Windows）
    └── test_hash_performance.c      # 哈希表性能（手动）
```

---

## 6. 内存与所有权模型

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

## 7. 已知限制

- **组权限当前对所有人生效**：用户组模块剥离后，`get_primary_gid_by_uid()` 恒返回 0，`HOOK_login()` 也把 `hook->group` 设为 0，于是任何 caller 都被判定为"同组"。在用户组模块回归前，**不要依赖组权限做隔离**。详见 `src/lib/hook.c`。
- **`mstr_cstr()` 返回的指针不带 `\0`**：不能直接配 `printf("%s")` / `strcmp` 用，会读到相邻未初始化内存（症状是字符串后面多出乱码，且时有时无）。要用 `mstr_to_cstr()`（需 free）或 `%.*s` + `mstrlen()`。详见 `src/lib/mstring.h`。
- **`drop` 只从注册表摘除 HOOK，不释放 HOOK 对象与它持有的数据**：`reg_unregister_hook` 只做 `hash_remove`。反复 drop / 重建同名钩子会持续增长内存。
- `lib/pkg.c` 依赖网络字节序（`arpa/inet.h`），暂未纳入构建。
- 注册表落盘不是原子操作：直接写 `registry.dat`，中途失败会留下截断的文件。建议改为写临时文件再重命名。
- 深拷贝没有环检测：若数据里出现自引用（结构套自己），`bignum_copy` 会无限递归。当前没有产生这种结构的路径。

---

## 8. 设计边界（重要）

Mhuixs 不实现自己的编程语言。操作以命令形式提供，语言层面的循环、判断、函数交给调用方。
这样做的理由是：命令可白名单、可审计、非图灵完备；而造一门语言意味着与 Lua / Python 竞争，
投入巨大且价值有限。详见 `src/execute.c` 顶部注释。

---

## 9. 参与与交流

- **Email**: Mhuxis@outlook.com | Mhuxis.db@gmail.com
- **GitHub**: [hujiyo/Mhuixs](https://github.com/hujiyo/Mhuixs)

---

**本 README 最后更新**: 2026.10.05（重建期）
