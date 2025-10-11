# 库导入功能使用指南

## 📚 概述

Logex 现在支持**模块化的库导入系统**，允许用户通过简单的 `import` 命令导入用C语言实现的函数库，并直接在表达式中调用这些函数。

## 🎯 核心特性

- ✅ **简洁的导入语法**：`import math` 即可导入数学函数库
- ✅ **直接函数调用**：导入后无需命名空间前缀，直接使用 `sin(x)`, `sqrt(x)` 等
- ✅ **自动常量注册**：导入时自动添加数学常量 `pi`, `e`, `phi`
- ✅ **22+ 数学函数**：三角、对数、指数、取整、比较等常用函数
- ✅ **可扩展架构**：轻松添加新的库模块

## 📖 使用示例

### 基础使用

```bash
expr > import math
已导入 math 库 (22 个函数)

expr > sqrt(16)
= 4

expr > sin(0)
= 0

expr > pi
= 3.141592653589793115997963468544185161590576171875

expr > e
= 2.718281828459045090795598298427648842334747314453125
```

### 在表达式中使用

```bash
expr > sqrt(16) + sqrt(9)
= 7

expr > max(1, 5, 3, 9, 2)
= 9

expr > abs(-10) * 2
= 20
```

### 与变量结合

```bash
expr > import math
已导入 math 库 (22 个函数)

expr > let r = 5
r = 5

expr > let area = pi * r * r
area = 78.539816339744827899949086713604629039764404296875

expr > area
= 78.539816339744827899949086713604629039764404296875
```

### 三角函数计算

```bash
expr > import math
已导入 math 库 (22 个函数)

expr > let angle = 1.57
angle = 1.57

expr > sin(angle)
= 0.798487112623490258300762434373609721660614013671875

expr > cos(0)
= 1

expr > tan(0)
= 0
```

## 📋 可用函数列表

使用 `funcs` 命令查看所有已注册的函数：

```bash
expr > funcs
可用函数列表：
  sin - 正弦函数 sin(x)
  cos - 余弦函数 cos(x)
  tan - 正切函数 tan(x)
  asin - 反正弦函数 asin(x)
  acos - 反余弦函数 acos(x)
  atan - 反正切函数 atan(x)
  atan2 - 两参数反正切 atan2(y,x)
  exp - 自然指数函数 e^x
  ln - 自然对数 ln(x)
  log - 对数 log(x) 或 log(x,base)
  log10 - 常用对数 log10(x)
  log2 - 二进制对数 log2(x)
  sqrt - 平方根 √x
  cbrt - 立方根 ∛x
  floor - 向下取整 floor(x)
  ceil - 向上取整 ceil(x)
  round - 四舍五入 round(x)
  trunc - 截断取整 trunc(x)
  abs - 绝对值 |x|
  sign - 符号函数 sign(x)
  max - 最大值 max(a,b,...)
  min - 最小值 min(a,b,...)
```

## 🔧 函数详细说明

### 三角函数

| 函数 | 说明 | 示例 |
|------|------|------|
| `sin(x)` | 正弦函数（x为弧度） | `sin(0)` → `0` |
| `cos(x)` | 余弦函数（x为弧度） | `cos(0)` → `1` |
| `tan(x)` | 正切函数（x为弧度） | `tan(0)` → `0` |
| `asin(x)` | 反正弦函数，返回弧度 | `asin(1)` → `1.57...` |
| `acos(x)` | 反余弦函数，返回弧度 | `acos(1)` → `0` |
| `atan(x)` | 反正切函数，返回弧度 | `atan(1)` → `0.785...` |
| `atan2(y,x)` | 两参数反正切 | `atan2(1, 1)` → `0.785...` |

### 指数与对数

| 函数 | 说明 | 示例 |
|------|------|------|
| `exp(x)` | e的x次幂 | `exp(1)` → `2.718...` |
| `ln(x)` | 自然对数（以e为底） | `ln(e)` → `1` |
| `log(x)` | 常用对数（以10为底） | `log(100)` → `2` |
| `log(x, base)` | 以指定底数的对数 | `log(8, 2)` → `3` |
| `log10(x)` | 常用对数（以10为底） | `log10(1000)` → `3` |
| `log2(x)` | 二进制对数（以2为底） | `log2(16)` → `4` |

### 幂与根

| 函数 | 说明 | 示例 |
|------|------|------|
| `sqrt(x)` | 平方根 | `sqrt(16)` → `4` |
| `cbrt(x)` | 立方根 | `cbrt(27)` → `3` |

### 取整函数

| 函数 | 说明 | 示例 |
|------|------|------|
| `floor(x)` | 向下取整 | `floor(3.7)` → `3` |
| `ceil(x)` | 向上取整 | `ceil(3.2)` → `4` |
| `round(x)` | 四舍五入 | `round(3.5)` → `4` |
| `trunc(x)` | 截断取整（向零取整） | `trunc(-3.9)` → `-3` |

### 其他函数

| 函数 | 说明 | 示例 |
|------|------|------|
| `abs(x)` | 绝对值 | `abs(-5)` → `5` |
| `sign(x)` | 符号函数（返回-1, 0, 1） | `sign(-5)` → `-1` |
| `max(a,b,...)` | 最大值（可变参数） | `max(1,5,3)` → `5` |
| `min(a,b,...)` | 最小值（可变参数） | `min(1,5,3)` → `1` |

### 数学常量

| 常量 | 说明 | 值 |
|------|------|------|
| `pi` | 圆周率 π | `3.14159...` |
| `e` | 自然常数 e | `2.71828...` |
| `phi` | 黄金比例 φ | `1.61803...` |

## 💡 实用案例

### 案例1：计算圆的面积

```bash
expr > import math
已导入 math 库 (22 个函数)

expr > let radius = 10
radius = 10

expr > let area = pi * radius * radius
area = 314.1592653589793115997963468544185161590576171875

expr > area
= 314.1592653589793115997963468544185161590576171875
```

### 案例2：计算三角形的高

```bash
expr > import math
已导入 math 库 (22 个函数)

expr > let angle = 0.785398  # 45度（弧度）
angle = 0.785398

expr > let base = 10
base = 10

expr > let height = base * sin(angle)
height = 7.0710546641691023324966196645982563495635986328125

expr > height
= 7.0710546641691023324966196645982563495635986328125
```

### 案例3：统计数据分析

```bash
expr > import math
已导入 math 库 (22 个函数)

expr > let data1 = 5
data1 = 5

expr > let data2 = 10
data2 = 10

expr > let data3 = 3
data3 = 3

expr > max(data1, data2, data3)
= 10

expr > min(data1, data2, data3)
= 3
```

## 🏗️ 架构设计

### 模块结构

```
┌─────────────────────────────────────┐
│         calculator.c                │
│  (维护 FunctionRegistry)            │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│         evaluator.c                 │
│  - 解析 import 语句                 │
│  - 解析函数调用 func(args)          │
│  - 调用 function_call()             │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│         function.c                  │
│  (函数注册和调用机制)               │
│  - FunctionRegistry                 │
│  - function_register()              │
│  - function_lookup()                │
│  - function_call()                  │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│    package/math_package.c           │
│  (动态加载的数学包实现)             │
│  - package_init()                   │
│  - package_get_info()               │
│  - 22+ 数学函数实现                 │
└─────────────────────────────────────┘
```

### 扩展新库的步骤

1. **创建库文件**：`mylib.h` 和 `mylib.c`
2. **实现函数**：遵循 `NativeFunction` 接口
3. **注册函数**：在库的初始化函数中调用 `function_register()`
4. **修改 evaluator.c**：在 `eval_statement()` 中添加新库的导入逻辑
5. **更新 Makefile**：添加新库的编译规则

示例代码框架：

```c
// mylib.c
#include "function.h"
#include "bignum.h"

/* 示例函数：double(x) = x * 2 */
static int my_double(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) return -1;
    
    BigNum two;
    bignum_from_string("2", &two);
    
    return bignum_mul(&args[0], &two, result);
}

/* 注册所有函数 */
int mylib_register_all(FunctionRegistry *registry) {
    int count = 0;
    count += (function_register(registry, "double", my_double, 1, 1, "将数值翻倍") == 0);
    return count;
}
```

## 🎓 设计理念

### 为什么不用命名空间？

传统编程语言（如Python）使用 `math.sin(x)` 这样的命名空间语法，但我们选择了更**数学家友好**的方式：

```python
# 编程风格（Python）
import math
math.sin(x)

# 数学家风格（我们的设计）
import math
sin(x)  # 直接使用，更自然！
```

**原因**：
1. ✅ 数学家习惯直接写 `sin(x)`，而不是 `math.sin(x)`
2. ✅ 减少冗余，提升可读性
3. ✅ 更接近数学表达式的自然形式
4. ✅ 命令行交互更快捷

### 函数调用 vs 语言内置

我们选择通过C语言实现函数库，而不是在语言层面实现 `if`, `for` 等控制流：

**优势**：
- ⚡ **高性能**：C语言实现的数学函数运行更快
- 🔧 **易扩展**：添加新函数只需C代码，无需修改解析器
- 🎯 **专注性**：保持语言核心简洁，复杂功能通过库提供
- 🔒 **稳定性**：核心语法不变，功能通过库扩展

## 📝 命令参考

### 新增命令

| 命令 | 说明 |
|------|------|
| `import math` | 导入数学函数库 |
| `funcs` | 列出所有已注册的函数 |

### 原有命令

| 命令 | 说明 |
|------|------|
| `help` | 显示帮助信息 |
| `vars` | 列出所有变量 |
| `clear` | 清空所有变量 |
| `exit` / `quit` | 退出程序 |

## 🐛 已知问题

1. **UTF-8符号支持**：`π` 符号目前无法直接识别，请使用 `pi` 代替
2. **精度显示**：默认精度为100位小数，可能导致显示过长

## 🚀 未来计划

- [ ] 支持更多库（统计学、线性代数等）
- [ ] 支持用户自定义函数（脚本式定义）
- [ ] 改进UTF-8符号支持
- [ ] 添加函数帮助系统（`help(sin)`）
- [ ] 支持函数重载（不同参数数量）

## 📚 参考资料

- [ARCHITECTURE.md](./ARCHITECTURE.md) - 架构设计文档
- [README.md](./README.md) - 项目总览
- [function.h](./function.h) - 函数注册机制接口
- [package/math_package.c](./package/math_package.c) - 数学包实现

---

**设计理念**：让数学表达式更自然，让计算器更强大！✨


