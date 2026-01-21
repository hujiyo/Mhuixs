# Logex 测试指南

## 编译

```bash
cd src
make clean
make
```

## 测试 1：REPL 模式

```bash
./logex
```

预期输出：
```
╔════════════════════════════════════════════════════════════╗
║           Logex REPL - 多行编辑模式                       ║
...
╚════════════════════════════════════════════════════════════╝

当前模式: VM (编译+执行)

>
```

测试命令：
```
> let x = 10
> let y = 20
> x + y
>

--- 编译并执行 ---
= 30

> :quit
```

## 测试 2：执行源代码文件

创建测试文件：
```bash
cat > test.lgx << 'EOF'
let a = 100
let b = 200
let sum = a + b
sum
EOF
```

执行：
```bash
./logex test.lgx
```

预期输出：
```
300
```

## 测试 3：编译源码为字节码

```bash
./logex test.lgx -o test.ls
```

预期输出：
```
编译: test.lgx -> test.ls
✓ 编译成功
```

检查文件：
```bash
ls -lh test.ls
file test.ls
```

## 测试 4：执行字节码

```bash
./logex test.ls
```

预期输出：
```
300
```

## 测试 5：详细模式执行字节码

```bash
./logex test.ls -v
```

预期输出：
```
加载字节码: test.ls
✓ 解释器创建成功
✓ 字节码加载成功
开始执行...

300

✓ 执行完成
```

## 测试 6：编译并反汇编

```bash
./logex test.lgx -o test.ls -d
```

预期输出：
```
编译: test.lgx -> test.ls
✓ 编译成功

=== Logex Bytecode Disassembly ===
Magic: 0x4C534758
Version: 1
Source: test.lgx
Constants: 5
Code Size: 11

=== Constant Pool ===
[0] NUMBER: 100 (decimal_pos=0, negative=0)
[1] IDENTIFIER: a
[2] NUMBER: 200 (decimal_pos=0, negative=0)
[3] IDENTIFIER: b
[4] IDENTIFIER: sum

=== Code ===
0000: PUSH_NUM [0]        # 压入 100
0001: STORE_VAR [1]       # 存储到 a
0002: PUSH_NUM [2]        # 压入 200
0003: STORE_VAR [3]       # 存储到 b
0004: LOAD_VAR [1]        # 加载 a
0005: LOAD_VAR [3]        # 加载 b
0006: ADD                 # a + b
0007: STORE_VAR [4]       # 存储到 sum
0008: LOAD_VAR [4]        # 加载 sum
0009: HALT                # 停止
```

## 测试 7：帮助信息

```bash
./logex -h
```

预期输出：
```
Logex - 统一的脚本解释器、虚拟机和编译器

用法:
  logex                         交互式 REPL 模式
  logex <script.lgx>            执行源代码文件
  logex <script.ls> [-v]        执行字节码文件
  logex <input.lgx> -o <out.ls> 编译源码为字节码

选项:
  -o <file>         指定输出字节码文件（编译模式）
  -d                反汇编输出（编译模式，调试用）
  -v, --verbose     详细模式（执行字节码时）
  -h, --help        显示此帮助信息

示例:
  logex                         # 启动 REPL
  logex test.lgx                # 执行源码
  logex test.ls -v              # 详细模式执行字节码
  logex test.lgx -o test.ls     # 编译为字节码
  logex test.lgx -o test.ls -d  # 编译并反汇编
```

## 测试 8：LIST 操作（REPL）

```bash
./logex
```

```
> let mylist = list()
> let mylist = rpush(mylist, 10)
> let mylist = rpush(mylist, 20)
> let mylist = rpush(mylist, 30)
> llen(mylist)
>

--- 编译并执行 ---
= 3

> :vars

=== 变量列表 ===
mylist = [LIST]

> :quit
```

## 测试 9：复杂表达式

创建测试文件：
```bash
cat > complex.lgx << 'EOF'
let x = 10
let y = 20
let z = 30
let result = (x + y) * z
result
EOF
```

编译并执行：
```bash
./logex complex.lgx -o complex.ls
./logex complex.ls
```

预期输出：
```
编译: complex.lgx -> complex.ls
✓ 编译成功
900
```

## 测试 10：错误处理

### 语法错误
```bash
echo "let x = " > error.lgx
./logex error.lgx
```

应该显示编译错误。

### 运行时错误
```bash
echo "let x = 10 / 0" > error.lgx
./logex error.lgx
```

应该显示运行时错误。

### 文件不存在
```bash
./logex notexist.lgx
```

应该显示文件错误。

## 完整测试脚本

```bash
#!/bin/bash

echo "=== 测试 Logex ==="

# 测试 1: 编译
echo "1. 测试编译..."
cat > test.lgx << 'EOF'
let x = 100
let y = 200
x + y
EOF

./logex test.lgx -o test.ls
if [ $? -eq 0 ]; then
    echo "✓ 编译成功"
else
    echo "✗ 编译失败"
    exit 1
fi

# 测试 2: 执行字节码
echo "2. 测试执行字节码..."
result=$(./logex test.ls)
if [ "$result" = "300" ]; then
    echo "✓ 执行字节码成功: $result"
else
    echo "✗ 执行字节码失败: $result"
    exit 1
fi

# 测试 3: 执行源码
echo "3. 测试执行源码..."
result=$(./logex test.lgx)
if [ "$result" = "300" ]; then
    echo "✓ 执行源码成功: $result"
else
    echo "✗ 执行源码失败: $result"
    exit 1
fi

# 测试 4: 反汇编
echo "4. 测试反汇编..."
./logex test.lgx -o test.ls -d > /dev/null
if [ $? -eq 0 ]; then
    echo "✓ 反汇编成功"
else
    echo "✗ 反汇编失败"
    exit 1
fi

# 清理
rm -f test.lgx test.ls

echo ""
echo "=== 所有测试通过！ ==="
```

## 性能测试

创建大文件：
```bash
cat > perf.lgx << 'EOF'
let sum = 0
let i = 0
# 循环 1000 次
EOF

# 添加 1000 行
for i in {1..1000}; do
    echo "let sum = sum + $i" >> perf.lgx
done

echo "sum" >> perf.lgx
```

测试编译时间：
```bash
time ./logex perf.lgx -o perf.ls
```

测试执行时间：
```bash
time ./logex perf.ls
```

## 故障排除

### 问题：编译失败
- 检查是否有 `compiler.h` 和 `bytecode.h`
- 检查 Makefile 依赖是否正确

### 问题：执行失败
- 检查字节码文件是否存在
- 检查字节码文件是否损坏
- 使用 `-v` 查看详细信息

### 问题：REPL 无响应
- 检查是否正确输入命令
- 尝试 `:help` 查看帮助
- 使用 `:quit` 退出

## 总结

所有测试通过后，`logex` 应该能够：

✅ 启动 REPL 模式  
✅ 执行源代码文件  
✅ 执行字节码文件  
✅ 编译源码为字节码  
✅ 反汇编字节码  
✅ 处理错误  
✅ 显示帮助信息  

恭喜！你现在有了一个完整的、统一的 Logex 工具！🎉
