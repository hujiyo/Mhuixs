# Logex REPL - 多行编辑模式

## 概述

新版 Logex REPL 采用**多行编辑 + 编译执行**模式，提供更好的交互体验和执行性能。

---

## 核心特性

### 1. 多行编辑模式

```
> let x = 10          # 回车：换行继续编辑
> let y = 20          # 回车：换行继续编辑
> x + y               # 回车：换行继续编辑
>                     # 空行回车：编译并执行
--- 编译并执行 ---
= 30
```

### 2. 编译 + 执行架构

```
多行输入 → GLL 编译器 → 字节码 → VM 虚拟机 → 结果
```

**优势**：
- ✅ 编辑时不执行，避免误操作
- ✅ 支持复杂的多行代码
- ✅ 字节码执行，性能更好
- ✅ 可以在执行前检查语法

---

## 使用方法

### 启动 REPL

```bash
cd src
make
./logex_repl
```

### 交互示例

#### 示例 1：基本算术

```
> let a = 100
> let b = 200
> a + b
> :run

--- 编译并执行 ---
= 300
```

#### 示例 2：LIST 操作

```
> let mylist = list()
> let mylist = rpush(mylist, 10)
> let mylist = rpush(mylist, 20)
> let mylist = rpush(mylist, 30)
> llen(mylist)
>

--- 编译并执行 ---
= 3
```

#### 示例 3：类型转换

```
> let n = num("123.456")
> let s = str(789)
> n + num("100")
> :run

--- 编译并执行 ---
= 223.456
```

---

## REPL 命令

| 命令 | 说明 |
|------|------|
| `:run` 或 `:r` | 编译并执行当前输入 |
| 空行 + 回车 | 编译并执行（当有内容时） |
| `:clear` 或 `:c` | 清空当前输入缓冲区 |
| `:vars` | 显示所有变量 |
| `:funcs` | 显示所有函数 |
| `:vm` | 切换到 VM 模式（默认） |
| `:direct` | 切换到直接解释模式 |
| `:help` 或 `:h` | 显示帮助 |
| `:quit` 或 `:q` | 退出 REPL |

---

## 执行模式

### VM 模式（默认）

```
> :vm
已切换到 VM 模式（编译+执行）
```

**特点**：
- 先编译为字节码
- 再由 VM 执行
- 性能更好
- 支持优化

### 直接解释模式

```
> :direct
已切换到直接解释模式
```

**特点**：
- 直接解析执行
- 无需编译
- 兼容旧代码

---

## 工作流程

### 1. 输入阶段

```
> let x = 10          # 添加到缓冲区
1> let y = 20         # 行号递增
2> x + y              # 继续添加
3>                    # 空行触发执行
```

### 2. 编译阶段

```
--- 编译并执行 ---
[内部] 源码 → Lexer → Parser → Compiler → 字节码
```

### 3. 执行阶段

```
[内部] 字节码 → VM 加载 → VM 执行 → 结果
= 30
```

### 4. 清理阶段

```
缓冲区清空，准备下一次输入
```

---

## 与旧版对比

| 特性 | 旧版 Logex | 新版 REPL |
|------|-----------|----------|
| 输入方式 | 单行立即执行 | 多行编辑后执行 |
| 执行方式 | 直接解释 | 编译 + VM |
| 性能 | 较慢 | 较快 |
| 编辑体验 | 不便 | 友好 |
| 错误检查 | 运行时 | 编译时 + 运行时 |
| 适用场景 | 简单计算 | 复杂脚本 |

---

## 高级用法

### 1. 查看变量

```
> let x = 10
> let y = 20
> :run
> :vars

=== 变量列表 ===
x = 10
y = 20
```

### 2. 查看函数

```
> import math
> :funcs

=== 函数列表 ===
[内置函数]
list, lpush, rpush, lpop, rpop, lget, llen
num, str, bmp, bset, bget, bcount

[外部包函数]
math.sin, math.cos, math.sqrt, ...
```

### 3. 清空输入

```
> let x = 10
> let y = 20
> let z = 30
> :clear
缓冲区已清空

> # 重新开始
```

### 4. 模式切换

```
> :vm
已切换到 VM 模式（编译+执行）

> let x = 10
> x * 2
> :run
= 20

> :direct
已切换到直接解释模式

> let y = 5
> y + 3
> :run
= 8
```

---

## 编译和安装

### 编译

```bash
cd src
make                    # 编译新版 REPL
make run                # 编译并运行

# 或编译所有工具
make all                # logex_repl
make vm_tools           # gll + logex_vm
```

### 安装到系统

```bash
sudo make install
# 安装到 /usr/local/bin/
# - logex (新版 REPL)
# - gll (编译器)
# - logex_vm (虚拟机)
```

---

## 快捷键（计划中）

未来版本将支持：

- **Ctrl+Enter**: 执行当前输入
- **Ctrl+C**: 清空当前输入
- **Ctrl+D**: 退出 REPL
- **↑/↓**: 历史记录导航
- **Tab**: 自动补全

---

## 示例会话

```
╔════════════════════════════════════════════════════════════╗
║           Logex REPL - 多行编辑模式                       ║
║                                                            ║
║  • 回车键：换行继续编辑                                   ║
║  • 输入 :run 或空行后回车：编译并执行                     ║
║  • :clear - 清空当前输入                                  ║
║  • :vars  - 显示所有变量                                  ║
║  • :help  - 显示帮助                                      ║
║  • :quit  - 退出                                          ║
║                                                            ║
║  模式：编译 → 字节码 → VM 执行                           ║
╚════════════════════════════════════════════════════════════╝

当前模式: VM (编译+执行)

> # 计算圆的面积
> let radius = 5
> let pi = 3.14159
> let area = pi * radius * radius
> area
>

--- 编译并执行 ---
= 78.53975

> # 使用 LIST
> let numbers = list()
> let numbers = rpush(numbers, 10)
> let numbers = rpush(numbers, 20)
> let numbers = rpush(numbers, 30)
> llen(numbers)
> :run

--- 编译并执行 ---
= 3

> :vars

=== 变量列表 ===
radius = 5
pi = 3.14159
area = 78.53975
numbers = [LIST]

> :quit

再见！
```

---

## 技术细节

### 架构

```
logex_repl.c
    ↓
interpreter.c (use_vm = 1)
    ↓
compiler.c → bytecode.c → vm.c
    ↓
builtin.c + bhs.c + context.c
```

### 内存管理

- 多行缓冲区：8KB
- VM 栈深度：1024
- 共享上下文：避免重复创建

### 性能优化

- 编译一次，多次执行（如果需要）
- 字节码缓存（未来）
- JIT 编译（计划中）

---

## 故障排除

### 问题：编译错误

```
> let x = 10 +
> :run
错误: Compile error: Unexpected end of expression
```

**解决**：检查语法，补全表达式

### 问题：运行时错误

```
> let x = 10 / 0
> :run
错误: Runtime error: Division by zero
```

**解决**：修正逻辑错误

### 问题：缓冲区满

```
错误: 输入过长
缓冲区已清空
```

**解决**：分批执行，或增加 `MAX_INPUT_SIZE`

---

## 总结

新版 Logex REPL 提供了：

✅ **多行编辑**：像写脚本一样编辑代码  
✅ **编译执行**：先检查语法，再执行  
✅ **VM 性能**：字节码执行更快  
✅ **友好交互**：命令丰富，操作简单  
✅ **模式切换**：VM 和直接解释随意切换  

这是一个现代化的、专业的脚本语言 REPL！
