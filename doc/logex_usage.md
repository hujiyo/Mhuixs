# Logex 使用指南

## 概述

Logex 现在是一个统一的脚本解释器和虚拟机，类似于 Python 的 `python` 命令。

## 使用方式

### 1. 交互式 REPL 模式

```bash
logex
```

启动交互式环境，支持多行编辑：

```
> let x = 10
> let y = 20
> x + y
> :run
= 30
```

**REPL 命令**：
- `:run` 或 `:r` - 编译并执行当前输入
- `:clear` 或 `:c` - 清空输入缓冲区
- `:vars` - 显示所有变量
- `:funcs` - 显示所有函数
- `:vm` - 切换到 VM 模式（默认）
- `:direct` - 切换到直接解释模式
- `:help` 或 `:h` - 显示帮助
- `:quit` 或 `:q` - 退出

### 2. 执行源代码文件

```bash
logex script.lgx
```

直接执行 Logex 源代码文件（`.lgx`）。

**示例**：
```bash
# 创建测试文件
cat > test.lgx << 'EOF'
let x = 10
let y = 20
let sum = x + y
sum
EOF

# 执行
logex test.lgx
# 输出: 30
```

### 3. 执行字节码文件

```bash
logex script.ls
```

执行预编译的字节码文件（`.ls`）。

**工作流**：
```bash
# 1. 用 gll 编译源码为字节码
gll script.lgx -o script.ls

# 2. 用 logex 执行字节码
logex script.ls
```

### 4. 详细模式

```bash
logex script.ls -v
```

以详细模式执行字节码，显示加载和执行信息。

**输出示例**：
```
加载字节码: script.ls
✓ 字节码加载成功
开始执行...

30

✓ 执行完成
```

### 5. 显示帮助

```bash
logex -h
# 或
logex --help
```

## 完整示例

### 示例 1：REPL 交互

```bash
$ logex
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

> let mylist = list()
> let mylist = rpush(mylist, 100)
> let mylist = rpush(mylist, 200)
> llen(mylist)
>

--- 编译并执行 ---
= 2

> :vars

=== 变量列表 ===
mylist = [LIST]

> :quit

再见！
```

### 示例 2：执行源文件

```bash
# 创建源文件
cat > fibonacci.lgx << 'EOF'
let a = 0
let b = 1
let n = 10
let i = 0

# 计算第 n 个斐波那契数
let temp = 0
EOF

# 执行
logex fibonacci.lgx
```

### 示例 3：编译和执行字节码

```bash
# 编译
gll fibonacci.lgx -o fibonacci.ls

# 查看字节码（可选）
gll fibonacci.lgx -o fibonacci.ls -d

# 执行字节码
logex fibonacci.ls

# 详细模式执行
logex fibonacci.ls -v
```

## 与其他工具的对比

### Python 风格
```bash
python              # REPL
python script.py    # 执行源码
python script.pyc   # 执行字节码
```

### Logex 风格
```bash
logex               # REPL
logex script.lgx    # 执行源码
logex script.ls     # 执行字节码
```

## 工具链

完整的 Logex 工具链包括：

1. **logex** - 统一的解释器和虚拟机
   - REPL 模式
   - 执行源码
   - 执行字节码

2. **gll** - 编译器
   - 将 `.lgx` 编译为 `.ls`
   - 支持反汇编

## 命令行选项

```
logex [选项] [文件]

选项:
  -v, --verbose    详细模式（仅用于字节码执行）
  -h, --help       显示帮助信息

文件:
  无               启动 REPL 模式
  *.lgx            执行源代码文件
  *.ls             执行字节码文件
```

## 退出代码

- `0` - 成功
- `1` - 错误（编译错误、运行时错误、文件未找到等）

## 环境变量

（暂无，未来可能添加）

## 配置文件

（暂无，未来可能添加 `~/.logexrc`）

## 常见问题

### Q: 如何区分源码和字节码？
A: Logex 通过文件扩展名自动识别：
- `.lgx` - 源代码
- `.ls` - 字节码

### Q: 可以直接执行字节码吗？
A: 可以！`logex script.ls` 会直接加载并执行字节码。

### Q: REPL 模式下如何执行多行代码？
A: 直接输入多行，然后输入空行或 `:run` 命令执行。

### Q: 如何查看编译后的字节码？
A: 使用 `gll script.lgx -o script.ls -d` 查看反汇编输出。

## 更多信息

- 内置函数文档: `doc/BUILTIN_FUNCTIONS.md`
- 字节码 VM 文档: `doc/BYTECODE_VM.md`
- REPL 文档: `doc/LOGEX_REPL.md`
- 包开发指南: `doc/PACKAGE_GUIDE.md`
