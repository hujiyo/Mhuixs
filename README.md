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

当前内核可编译、可运行，并通过 5 项主链路自检，支持 HOOK 落盘与恢复。

---

## 3. 编译与运行

```bash
cd src
make
./mhuixs
```

预期输出：

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

  已保存 1 个 HOOK 到磁盘

==== 自检结果：全部通过 ====
```

数据目录由 `src/Mhuixs.config` 的 `MhuixsHomePath` 指定，HOOK 注册表落盘为 `<MhuixsHomePath>/registry.dat`。

---

## 4. 目录结构

```
Mhuixs/
├── src/
│   ├── Mhuixs.c          # 内核入口：模块初始化 + 主链路自检
│   ├── registry.c/h      # 注册表：统一管理 HOOK 与权限
│   ├── Makefile
│   ├── Mhuixs.config     # 运行配置
│   └── lib/              # 基础库
│       ├── list.c/h      # LIST 列表
│       ├── tblh.c/h      # TABLE 表
│       ├── bitmap.c/h    # BITMAP 位图
│       ├── kvalh.c/h     # KVALOT 键值对
│       ├── bignum.c/h    # BHS 统一类型 + 任意精度数值
│       ├── hook.c/h      # HOOK
│       ├── hash.c/h      # Robin Hood 哈希表
│       ├── mstring.h     # 字符串
│       ├── getid.c/h     # ID 分配器
│       ├── env.c/h       # 环境配置
│       ├── merr.c/h      # 错误码与日志
│       ├── bitcpy.c/h    # 位级拷贝
│       ├── pkg.c/h       # MUIX 打包协议（暂挂起，待网络层回归）
│       └── logo.c/h      # 启动标识
├── doc/                  # 设计文档（部分内容对应已剥离的模块，待整理）
└── test/                 # 测试代码（同上）
```

---

## 5. 已知限制

- **组权限当前对所有人生效**：用户组模块剥离后，`get_primary_gid_by_uid()` 恒返回 0，`HOOK_login()` 也把 `hook->group` 设为 0，于是任何 caller 都被判定为"同组"。在用户组模块回归前，**不要依赖组权限做隔离**。详见 `src/lib/hook.c`。
- `lib/pkg.c` 依赖网络字节序（`arpa/inet.h`），暂未纳入构建。
- `doc/` 与 `test/` 中部分内容对应已剥离的模块，需要后续清理。

---

## 6. 参与与交流

- **Email**: Mhuxis@outlook.com | Mhuxis.db@gmail.com
- **GitHub**: [hujiyo/Mhuixs](https://github.com/hujiyo/Mhuixs)

---

**本 README 最后更新**: 2026.10.05（重建期）
