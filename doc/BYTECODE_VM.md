# Logex 字节码虚拟机架构

## 概述

Logex 采用**字节码虚拟机**架构，将源代码编译为字节码后执行，提供更好的性能和可扩展性。

---

## 架构设计

```
.lgx 源文件
    ↓
  GLL 编译器
    ↓
.ls 字节码文件
    ↓
 Logex VM 虚拟机
    ↓
  执行结果
```

### 核心组件

1. **GLL 编译器** (`gll`)
   - 将 `.lgx` 源文件编译为 `.ls` 字节码
   - 词法分析 → 语法分析 → 字节码生成
   - 支持常量池优化

2. **字节码格式** (`.ls`)
   - 魔数：`LSGX` (0x4C534758)
   - 文件头：版本、常量池大小、代码段大小
   - 常量池：数字、字符串、标识符
   - 代码段：指令序列

3. **Logex VM 虚拟机** (`logex_vm`)
   - 基于栈的虚拟机
   - 操作数栈：1024 深度
   - 支持内置函数直接调用
   - 运行时环境：变量上下文、包管理

---

## 字节码指令集

### 栈操作 (0-19)

| 指令 | 操作码 | 说明 |
|------|--------|------|
| `NOP` | 0 | 空操作 |
| `PUSH_NUM` | 1 | 压入数字常量 |
| `PUSH_STR` | 2 | 压入字符串常量 |
| `PUSH_VAR` | 3 | 压入变量值 |
| `POP` | 4 | 弹出栈顶 |
| `DUP` | 5 | 复制栈顶 |
| `SWAP` | 6 | 交换栈顶两元素 |

### 算术运算 (20-39)

| 指令 | 操作码 | 说明 |
|------|--------|------|
| `ADD` | 20 | 加法 |
| `SUB` | 21 | 减法 |
| `MUL` | 22 | 乘法 |
| `DIV` | 23 | 除法 |
| `MOD` | 24 | 取模 |
| `POW` | 25 | 幂运算 |
| `NEG` | 26 | 取负 |

### 比较运算 (40-49)

| 指令 | 操作码 | 说明 |
|------|--------|------|
| `EQ` | 40 | == |
| `NE` | 41 | != |
| `LT` | 42 | < |
| `LE` | 43 | <= |
| `GT` | 44 | > |
| `GE` | 45 | >= |

### 逻辑运算 (50-59)

| 指令 | 操作码 | 说明 |
|------|--------|------|
| `AND` | 50 | 逻辑与 ^ |
| `OR` | 51 | 逻辑或 v |
| `NOT` | 52 | 逻辑非 ! |
| `XOR` | 53 | 异或 ⊽ |
| `IMPLIES` | 54 | 蕴含 → |
| `IFF` | 55 | 等价 ↔ |

### 变量操作 (60-69)

| 指令 | 操作码 | 说明 |
|------|--------|------|
| `STORE_VAR` | 60 | 存储变量 |
| `LOAD_VAR` | 61 | 加载变量 |
| `DEL_VAR` | 62 | 删除变量 |

### 控制流 (70-89)

| 指令 | 操作码 | 说明 |
|------|--------|------|
| `JMP` | 70 | 无条件跳转 |
| `JMP_IF_FALSE` | 71 | 条件跳转（假） |
| `JMP_IF_TRUE` | 72 | 条件跳转（真） |
| `CALL` | 73 | 函数调用 |
| `RETURN` | 74 | 返回 |
| `BREAK` | 77 | 跳出循环 |
| `CONTINUE` | 78 | 继续循环 |

### 内置函数 (90-109)

| 指令 | 操作码 | 函数 |
|------|--------|------|
| `CALL_LIST` | 91 | list() |
| `CALL_LPUSH` | 92 | lpush() |
| `CALL_RPUSH` | 93 | rpush() |
| `CALL_LPOP` | 94 | lpop() |
| `CALL_RPOP` | 95 | rpop() |
| `CALL_LGET` | 96 | lget() |
| `CALL_LLEN` | 97 | llen() |
| `CALL_NUM` | 98 | num() |
| `CALL_STR` | 99 | str() |
| `CALL_BMP` | 100 | bmp() |
| `CALL_BSET` | 101 | bset() |
| `CALL_BGET` | 102 | bget() |
| `CALL_BCOUNT` | 103 | bcount() |

### 其他 (200+)

| 指令 | 操作码 | 说明 |
|------|--------|------|
| `LINE` | 200 | 行号信息（调试） |
| `HALT` | 201 | 停止执行 |

---

## 使用方法

### 1. 编译源文件

```bash
# 编译 .lgx 为 .ls
gll example.lgx -o example.ls

# 编译并反汇编
gll example.lgx -o example.ls -d
```

### 2. 执行字节码

```bash
# 执行 .ls 字节码
logex_vm example.ls

# 详细模式
logex_vm example.ls -v
```

### 3. 完整流程示例

```bash
# 创建源文件 test.lgx
cat > test.lgx << 'EOF'
let x = 10
let y = 20
let sum = x + y
sum
EOF

# 编译
gll test.lgx -o test.ls

# 执行
logex_vm test.ls
# 输出: 30
```

---

## 字节码文件格式

### 文件结构

```
+------------------+
| 文件头 (Header)  |  (sizeof(BytecodeHeader))
+------------------+
| 常量池           |  (const_count * Constant)
+------------------+
| 代码段           |  (code_size * Instruction)
+------------------+
```

### 文件头 (BytecodeHeader)

```c
struct BytecodeHeader {
    uint32_t magic;           // 魔数 0x4C534758 "LSGX"
    uint32_t version;         // 版本号 (当前为 1)
    uint32_t const_count;     // 常量池大小
    uint32_t code_size;       // 代码段大小
    uint32_t entry_point;     // 入口点
    uint32_t flags;           // 标志位
    char source_file[256];    // 源文件名
};
```

### 常量池条目

```c
// 类型
enum ConstType {
    CONST_NUMBER,      // 数字常量
    CONST_STRING,      // 字符串常量
    CONST_IDENTIFIER,  // 标识符
};

// 条目
struct Constant {
    ConstType type;
    union {
        char *str;                    // 字符串/标识符
        struct {
            char *digits;             // 数字字符串
            int decimal_pos;          // 小数点位置
            int is_negative;          // 是否负数
        } num;
    } value;
};
```

### 指令格式

```c
struct Instruction {
    OpCode opcode;        // 操作码
    union {
        int64_t i64;      // 整数操作数
        double f64;       // 浮点操作数
        uint32_t u32;     // 无符号整数
        struct {
            uint32_t idx; // 常量池索引
            uint32_t len; // 长度/参数数量
        } ref;
    } operand;
};
```

---

## 示例：编译过程

### 源代码 (example.lgx)

```javascript
let x = 10
let y = 20
let sum = x + y
sum
```

### 生成的字节码 (反汇编)

```
=== Logex Bytecode Disassembly ===
Magic: 0x4C534758
Version: 1
Source: example.lgx
Constants: 5
Code Size: 11

=== Constant Pool ===
[0] NUMBER: 10 (decimal_pos=0, negative=0)
[1] IDENTIFIER: x
[2] NUMBER: 20 (decimal_pos=0, negative=0)
[3] IDENTIFIER: y
[4] IDENTIFIER: sum

=== Code ===
0000: PUSH_NUM [0]        # 压入 10
0001: STORE_VAR [1]       # 存储到 x
0002: PUSH_NUM [2]        # 压入 20
0003: STORE_VAR [3]       # 存储到 y
0004: LOAD_VAR [1]        # 加载 x
0005: LOAD_VAR [3]        # 加载 y
0006: ADD                 # x + y
0007: STORE_VAR [4]       # 存储到 sum
0008: LOAD_VAR [4]        # 加载 sum
0009: HALT                # 停止
```

---

## 虚拟机执行流程

### 栈式执行

```
指令: PUSH_NUM [0]
栈: [10]

指令: STORE_VAR [1]  (x)
栈: [10]
变量: {x: 10}

指令: PUSH_NUM [2]
栈: [10, 20]

指令: STORE_VAR [3]  (y)
栈: [10, 20]
变量: {x: 10, y: 20}

指令: LOAD_VAR [1]  (x)
栈: [10, 20, 10]

指令: LOAD_VAR [3]  (y)
栈: [10, 20, 10, 20]

指令: ADD
栈: [10, 20, 30]

指令: STORE_VAR [4]  (sum)
栈: [10, 20, 30]
变量: {x: 10, y: 20, sum: 30}

指令: LOAD_VAR [4]  (sum)
栈: [10, 20, 30, 30]

指令: HALT
结果: 30
```

---

## 性能优势

### 相比解释器

1. **编译一次，多次执行**
   - 源码只需编译一次
   - 字节码可重复执行

2. **优化空间**
   - 常量折叠
   - 死代码消除
   - 跳转优化

3. **执行效率**
   - 无需重复解析
   - 指令紧凑
   - 栈式计算高效

### 相比原生代码

1. **跨平台**
   - 字节码平台无关
   - 虚拟机适配不同平台

2. **安全性**
   - 沙箱执行
   - 资源限制

3. **灵活性**
   - 动态加载
   - 热更新

---

## 扩展方向

### 1. 优化器

- 常量折叠
- 死代码消除
- 内联优化

### 2. JIT 编译

- 热点代码识别
- 动态编译为机器码
- 性能提升 10-100 倍

### 3. 调试支持

- 断点
- 单步执行
- 变量查看
- 调用栈追踪

### 4. 数据库操作

- TABLE 操作字节码
- KVALOT 操作字节码
- 事务支持

---

## 文件命名规范

- **源文件**: `.lgx` (Logex)
- **字节码**: `.ls` (Logex Script)
- **编译器**: `gll` (GLL - G for Logex Language)
- **虚拟机**: `logex_vm`

---

## 总结

Logex 字节码虚拟机架构提供了：

✅ **性能**: 编译一次，多次执行  
✅ **可扩展**: 易于添加新指令  
✅ **可调试**: 反汇编、单步执行  
✅ **跨平台**: 字节码平台无关  
✅ **安全**: 沙箱执行环境  

这是一个专业、现代的脚本语言实现方案！
