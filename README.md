<img src=".logo/Mhuixs-logo.png" height="130px" />

# Mhuixs 数据库

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/hujiyo/Mhuixs)

> Hook Engine • Mhuixs Database

## 1. 介绍

Mhuixs 是一个**基于内存的数据库**，将数据库引擎与脚本语言深度融合。Hook是Mhuixs的核心接口程序。

**核心理念**：
- **HOOK 注册系统**：数据库核心，数据持久化管理
- **Logex 原生操作语言**：内置脚本解释器，直接操作数据结构
- **BHS 统一数据类型**：任意精度数值、字符串、LIST、TABLE、BITMAP 的统一封装

**当前状态** (2026.01.19):
- ✅ **Logex 内置化完成**：13 个核心函数已内置
- ✅ **数据结构完善**：LIST/TABLE/BITMAP/KVALOT 已实现并集成
- ✅ **客户端 muixclt**：支持完整的 NAQL 语法解析和网络通信
- 📋 **MCP 协议支持**：规划中

---

## 2. 核心架构

### 架构层次

```
Mhuixs 数据库核心
  ├── HOOK 注册系统
  │   ├── 权限管理（owner/group/other）
  │   ├── 对象注册/注销/查找
  │   └── 生命周期管理
  │
  ├── 原生数据结构
  │   ├── LIST（双端队列）
  │   ├── TABLE（关系型表）
  │   ├── BITMAP（位图）
  │   ├── KVALOT（键值对池）
  │   └── STREAM（流数据）
  │
  ├── BHS 统一类型（bhs/basic_handle_struct）
  │   ├── NUMBER（任意精度数值）
  │   ├── STRING（字符串）
  │   ├── LIST（列表指针）
  │   ├── TABLE（表指针）
  │   └── BITMAP（位图指针）
  │
  └── Logex 解释器（执行模块）
       ├── 内置函数
       │   ├── LIST 操作：list, lpush, rpush, lpop, rpop, lget, llen
       │   ├── TYPE 转换：num, str, bmp
       │   └── BITMAP 操作：bset, bget, bcount
       │
       └── 外部包
           └── math：sin, cos, sqrt, π, e 等
```

## 3. Logex 脚本语言

### 核心特性

Logex 是 Mhuixs 的操作语言：

- ✅ **任意精度数值计算**（BHS 实现）
- ✅ **完整的布尔逻辑运算**
- ✅ **变量机制和表达式求值**
- ✅ **内置 13 个核心函数**（LIST/TYPE/BITMAP）
- ✅ **包管理系统**（动态加载 .so 扩展）
- ✅ **递归下降解析器**
- ✅ **交互式 REPL**

> **名称由来**: Logex = Logic + Expression

### 内置函数

#### LIST 操作 (7个)
```javascript
let mylist = list()              // 创建空列表
let mylist = rpush(mylist, 100)  // 右侧插入
let mylist = lpush(mylist, 50)   // 左侧插入
let val = lpop(mylist)           // 左侧弹出
let val = rpop(mylist)           // 右侧弹出
let item = lget(mylist, 0)       // 获取元素
let size = llen(mylist)          // 列表长度
```

#### TYPE 转换 (3个)
```javascript
let n = num("123.456")   // 字符串 → 数字
let s = str(789)         // 数字 → 字符串
let b = bmp(255)         // 数字 → 位图
```

#### BITMAP 操作 (3个)
```javascript
let bm = bmp(0)
let bm = bset(bm, 10, 1)        // 设置位
let bit = bget(bm, 10)          // 获取位
let count = bcount(bm, 0, 100)  // 统计位数
```

### 外部包（需要 import）

```javascript
// 数学函数包
import math
let result = math.sin(math.pi / 2)  // 1
let root = math.sqrt(16)            // 4
```

### 完整示例

```javascript
# 示例 1：LIST 操作
let mylist = list()
let mylist = rpush(mylist, 100)
let mylist = rpush(mylist, 200)
let mylist = rpush(mylist, 300)

let size = llen(mylist)      # size = 3
let first = lget(mylist, 0)  # first = 100
let val = lpop(mylist)       # val = 100

# 示例 2：类型转换
let n = num("123.456")
let result = n + 100         # result = 223.456
let s = str(result)          # s = "223.456"

# 示例 3：BITMAP 操作
let bm = bmp(0)
let bm = bset(bm, 0, 1)
let bm = bset(bm, 5, 1)
let bm = bset(bm, 10, 1)
let count = bcount(bm, 0, 20)  # count = 3

# 示例 4：数学运算（需要 import）
import math
let angle = math.pi / 4
let sin_val = math.sin(angle)
let cos_val = math.cos(angle)
```

**详细文档**: [BUILTIN_FUNCTIONS.md](doc/BUILTIN_FUNCTIONS.md)

---

## 4. 项目文件结构

```
Mhuixs-root/
├── src/                       # 核心数据库代码
│   ├── lib/                   # 基础库
│   │   ├── list.c/h          # LIST 数据结构
│   │   ├── tblh.c/h          # TABLE 数据结构
│   │   ├── bitmap.c/h        # BITMAP 数据结构
│   │   ├── kvalh.cpp/hpp     # KVALOT 数据结构
│   │   └── mstring.h         # 字符串工具
│   │
│   ├── share/                 # 共享模块
│   │   └── obj.h             # BHS/bhs 定义
│   │
│   ├── hub/                   # 服务端核心（开发中）
│   │   ├── hook.cpp/hpp      # HOOK 注册系统
│   │   ├── registry.cpp/hpp  # 注册表管理
│   │   └── usergroup.cpp/hpp # 用户权限管理
│   │
│   ├── bhs.c/h            # BHS 核心实现
│   ├── builtin.c/h           # 内置函数
│   ├── evaluator.c/h         # Logex 解释器
│   ├── lexer.c/h             # 词法分析器
│   ├── parser.c/h            # 语法分析器
│   ├── context.c/h           # 变量上下文
│   ├── function.c/h          # 外部包注册系统
│   ├── package.c/h           # 包管理器
│   └── package/              # 外部包目录
│       └── math_package.c    # 数学函数包
│
├── test/                      # 测试文件
│   ├── test_builtin.c        # 内置函数测试
│   ├── test_builtin_simple.c # 简化测试
│   └── test_builtin.logex    # Logex 测试脚本
│
├── doc/                       # 文档目录
│   ├── BUILTIN_FUNCTIONS.md  # 内置函数文档
│   ├── NAQL.txt              # NAQL 语法文档
│   ├── PACKAGE_GUIDE.md      # 包开发指南
│   └── README_old.md         # 旧版 README
│
└── muixclt/                   # 客户端（已完成）
    ├── muixclt.c             # 主程序
    ├── lexer.c               # NAQL 词法分析器
    ├── pkg.c                 # MUIX 包协议
    └── netlink.c             # 网络通信
```

### 开发进度

- ✅ **核心数据结构**: LIST, TABLE, BITMAP, KVALOT, STREAM 已实现
- ✅ **BHS 统一类型**: 已完成，支持所有数据类型
- ✅ **Logex 内置化**: 13 个核心函数已内置，测试通过
- ✅ **包管理系统**: 支持动态加载 .so 扩展
- ✅ **muixclt 客户端**: NAQL 解析器、网络通信已完成
- 🚧 **HOOK 注册系统**: 基础框架已有，待完善
- 🚧 **服务端 hub**: 正在开发
- 📋 **TABLE 操作函数**: 待内置化
- 📋 **MCP 协议支持**: 规划中

---

## 5. 目标与特色

Mhuixs 定位为 **AI Agent 数据库**，具备以下核心特色：

### 数据库特性
- ✅ **混合数据模型**：同时支持关系型（TABLE）和非关系型（KVALOT）
- ✅ **多种数据结构**：LIST、TABLE、BITMAP、KVALOT、STREAM
- ✅ **基于内存**：使用索引、哈希表快速定位数据
- ✅ **HOOK 注册系统**：统一的对象管理和权限控制
- 🚧 **数据压缩策略**：lv0-lv5 分级压缩（规划中）

### 脚本语言特性
- ✅ **内置核心函数**：LIST/TYPE/BITMAP 操作
- ✅ **任意精度计算**：BHS 支持超大数值和高精度小数
- ✅ **包扩展系统**：通过 .so 动态加载扩展功能
- ✅ **简洁语法**：接近自然语言，易于 AI 理解和生成

### AI Agent 友好
- 📋 **MCP 协议支持**（规划中）：全面兼容 AI Agent
- ✅ **NAQL 查询语言**：接近自然语言的查询语法
- ✅ **权限分级管理**：owner/group/other 三级权限
- ✅ **脚本化操作**：AI 可直接生成 Logex 脚本操作数据库

### 性能与安全
- ✅ **内存优先**：极速数据访问
- ✅ **权限隔离**：基于 HOOK 的权限系统
- 🚧 **TLS 加密**：客户端支持 TLS 连接
- 📋 **持久化**：数据持久化到磁盘（规划中）

---

## 6. NAQL 查询语言

**NAQL**: NAture-language Query Language

旨在设计一种最接近口语的、最简单、给 AI 可以直接现场学会的数据查询语言。

### 语法特点
- ✅ 接近自然语言的语法设计
- ✅ 简洁明了的命令结构
- ✅ 支持复杂的数据查询和操作
- ✅ 专为 AI 优化的语法规则

### 示例

```sql
# TABLE 操作
HOOK TABLE users;
FIELD ADD id i4 PKEY;
FIELD ADD name str NOTNULL;
ADD 1 'Alice' 25;
GET WHERE id == 1;

# KVALOT 操作
HOOK KVALOT cache;
SET user:1 'Alice';
GET user:1;

# 控制语句（客户端本地执行）
$counter = 0;
FOR i 1 10 1;
    ADD $i 'user$i';
    $counter = $counter + 1;
END;
```

**详细文档**: [NAQL 基础语法](doc/NAQL.txt)

---

## 7. 快速开始

### 编译 Logex 解释器

```bash
cd src
gcc -o logex main.c evaluator.c lexer.c bhs.c builtin.c context.c \
    function.c lib/list.c lib/bitmap.c lib/tblh.c -I. -Ilib -Ishare -lm
```

### 运行示例

```bash
# 交互式 REPL
./logex

# 执行脚本
./logex script.logex
```

### 测试内置函数

```bash
# 编译测试程序
gcc -o test_builtin test_builtin_simple.c builtin.c bhs.c \
    lib/list.c lib/bitmap.c lib/tblh.c -I. -Ilib -Ishare -lm

# 运行测试
./test_builtin
```

---

## 8. 参与与交流

欢迎对数据库、AI Agent、编程语言方向感兴趣的朋友加入，一起完成 Mhuixs！

- **Email**: Mhuxis@outlook.com | Mhuxis.db@gmail.com | Mhuxis.db@outlook.com
- **WeChat**: wx17601516389
- **GitHub**: [hujiyo/Mhuixs](https://github.com/hujiyo/Mhuixs)

---

## 9. 重要里程碑

- **2024.10.17**: 项目启动
- **2024.12.20**: muixclt 客户端完成
- **2025.10.07**: Logex 整合规划
- **2026.01.19**: Logex 内置化完成，13 个核心函数已集成
- **本次更新**: 2026.01.19

---

## 10. NAQL 协议详解

### MUIX 包协议

用于客户端与服务器间的可靠数据传输：

```
包结构: [MUIX(4字节)] + [长度(4字节)] + [$(1字节)] + [用户数据]
- 魔数: 'M''U''I''X'
- 长度: 大端字节序，表示用户数据长度
- 结束符: '$'
- 最大包大小: 4KB
```

### HUJI 命令协议

NAQL 语句转换为二进制命令格式：

```
命令格式: [HUJI(4字节)] + [编号(4字节)] + [参数流]
参数流: [参数数目(1字节)] + [@] + [参数1长度] + [@] + [参数1] + [@] + ...
```

**命令编号体系**:
- 1-50: 基础语法（GET, HOOK, DESC 等）
- 51-70: 事务控制（MULTI, EXEC, ASYNC）
- 71-90: 控制语句（IF, WHILE, FOR - 本地处理）
- 101-150: TABLE 操作（FIELD, ADD, SET 等）
- 151-200: KVALOT 操作（EXISTS, SET, GET 等）
- 201-280: STREAM/LIST 操作
- 281-330: BITMAP 操作
- 361-370: 变量管理（本地处理）

---

## 11. 致谢

### 致敬开源社区

```
感谢开源社区的无私奉献和优秀工作，让 Mhuixs 能够站在巨人的肩膀上发展。
我在这里祝你们心想事成、事业有成、阖家幸福、幸福安康。

————HuJiYo 2026

Thanks to the open-source community. It is their selfless dedication and 
excellent work that have enabled Mhuixs to develop by standing on the 
shoulders of giants. Here, I wish you all the best in your endeavors, 
success in your careers, happiness for your families, and health and well-being.

————HuJiYo 2026
```

### 特别感谢

1. **AI 助手**: Claude、GPT、Qwen、Deepseek、DouBao 系列模型（~QvQ~）
2. **开发者**: 感谢坚持不放弃的自己（doge）！
3. **社区**: 所有关注和支持 Mhuixs 的朋友们

---

## 12. 重要说明

### 架构说明

**请注意以下关键点，避免误判**：

1. **HOOK 是核心，BHS 是别名**
   - `HOOK` 注册系统是 Mhuixs 数据库的核心
   - `BHS`/`bhs`/`basic_handle_struct` 是同一个结构体的不同名称
   - HOOK 体现"大海捞针"的数据库理念

2. **Logex 是 Mhuixs 的一部分**
   - Logex 不是独立项目，是 Mhuixs 的原生操作语言
   - 核心函数已内置化，无需 `import list` 或 `import type`
   - 只有 `math` 等非数据库功能才需要 import

3. **内置函数 vs 外部包**
   - **内置函数**（13个）：`list()`, `lpush()`, `num()`, `str()`, `bmp()` 等，直接可用
   - **外部包**：`import math` 后才能使用 `sin()`, `cos()` 等

4. **已完成 vs 规划中**
   - ✅ **已完成**: LIST/BITMAP/TYPE 内置函数、BHS 统一类型、包管理系统
   - 🚧 **开发中**: HOOK 注册系统完善、服务端 hub
   - 📋 **规划中**: TABLE 操作内置化、MCP 协议、数据持久化

### 文档索引

- **内置函数文档**: [BUILTIN_FUNCTIONS.md](doc/BUILTIN_FUNCTIONS.md)
- **NAQL 语法**: [NAQL.txt](doc/NAQL.txt)
- **包开发指南**: [PACKAGE_GUIDE.md](doc/PACKAGE_GUIDE.md)
- **客户端文档**: [muixclt README](muixclt/README.md)

---

**本 README 最后更新**: 2026.01.19  
**当前版本**: Logex 内置化完成版

---

