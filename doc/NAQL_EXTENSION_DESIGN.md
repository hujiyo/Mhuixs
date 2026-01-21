# NAQL 扩展设计文档

## 概述

本文档记录了将 NAQL（NAture-language Query Language）集成到 Logex 字节码虚拟机的设计方案。

**核心优势**：BHS 统一数据类型让 Logex 和 NAQL 无缝集成。

---

## 架构设计

### 整体流程

```
NAQL 源码
    ↓
Lexer（识别 NAQL Token）
    ↓
Parser（构建 NAQL AST）
    ↓
Compiler（生成 OP_DB_* 字节码）
    ↓
VM（执行数据库操作）
    ↓
调用 lib/tblh.c, lib/kvalh.cpp（BHS 接口）
```

---

## BHS 统一类型的威力

### 为什么 BHS 让扩展变得简单？

**BHS（Basic Handle Struct）** 是 Mhuixs 的统一数据类型，封装了：
- NUMBER（任意精度数值）
- STRING（字符串）
- LIST（列表指针）
- TABLE（表指针）
- BITMAP（位图指针）

### 关键优势

#### 1. **统一的数据传递**

```c
// AST 节点中直接使用 BHS
typedef struct {
    char *operation;
    BHS **values;        // 统一使用 BHS 数组！
    int value_count;
} ASTTableOp;
```

**好处**：
- ✅ 不需要为每种类型定义不同的字段
- ✅ 编译器生成的字节码统一
- ✅ VM 执行时类型检查统一

#### 2. **Logex 和 NAQL 混用**

```javascript
// Logex 变量
let user_id = 1
let user_name = "Alice"

// NAQL 创建表
HOOK TABLE users;
FIELD ADD id i4 PKEY;
FIELD ADD name str NOTNULL;

// 混合使用 - BHS 自动转换！
ADD user_id user_name;  // Logex 变量传给 NAQL
```

**原理**：
- Logex 的变量存储为 BHS
- NAQL 的参数接受 BHS
- 无需类型转换！

#### 3. **字节码层统一**

```c
// 编译器生成
OP_PUSH_VAR user_id      // 压入 BHS
OP_PUSH_VAR user_name    // 压入 BHS
OP_DB_TABLE TABLE_ADD    // 直接使用栈上的 BHS
```

**好处**：
- ✅ VM 栈统一存储 BHS
- ✅ 操作码不需要区分类型
- ✅ 内存管理统一

#### 4. **库函数接口统一**

```c
// lib/tblh.h - 已经使用 BHS* 接口
int add_record(TABLE* table, BHS** values, size_t num);
BHS* get_value(TABLE* table, size_t idx_x, size_t idx_y);
int set_value(TABLE* table, size_t idx_x, size_t idx_y, BHS* content);
```

**好处**：
- ✅ NAQL 直接调用现有 C API
- ✅ 无需包装层
- ✅ 性能最优

---

## 已完成的工作

### ✅ 第 1 步：Lexer 扩展

**文件**：`src/lexer.h`, `src/lexer.c`

**添加的 Token**：
- NAQL 操作关键字：`HOOK`, `TABLE`, `KVALOT`, `FIELD`, `ADD`, `GET`, `SET`, `DEL`, `WHERE` 等
- NAQL 数据类型：`i1`, `i2`, `i4`, `i8`, `str`, `bool`, `blob` 等
- NAQL 约束：`PKEY`, `FKEY`, `UNIQUE`, `NOTNULL`, `DEFAULT` 等
- 分号：`;`（NAQL 语句结束符）

**关键函数**：
```c
static TokenType check_keyword(const char *value) {
    // 识别 100+ NAQL 关键字
    if (strcmp(value, "HOOK") == 0) return TOK_HOOK;
    if (strcmp(value, "TABLE") == 0) return TOK_TABLE;
    // ...
}
```

### ✅ 第 2 步：AST 扩展

**文件**：`src/ast.h`

**添加的节点类型**：
```c
typedef enum {
    // Logex 节点
    AST_EXPRESSION, AST_ASSIGNMENT, AST_IF, AST_FOR, ...
    
    // NAQL 节点
    AST_HOOK_CREATE,    // HOOK TABLE users;
    AST_FIELD_ADD,      // FIELD ADD id i4 PKEY;
    AST_TABLE_ADD,      // ADD 1 'Alice' 25;
    AST_KVALOT_SET,     // SET key value;
    // ...
} ASTNodeType;
```

**关键数据结构**：
```c
/* NAQL TABLE 操作节点 - 使用 BHS！ */
typedef struct {
    char *operation;
    BHS **values;        // 统一使用 BHS 数组
    int value_count;
    char *condition;
    int index;
} ASTTableOp;
```

---

## 待完成的工作

### 🔲 第 3 步：Parser 扩展

**目标**：解析 NAQL 语法，构建 AST

**需要实现的函数**：
```c
ASTNode* parse_hook_statement(Parser *parser);
ASTNode* parse_field_statement(Parser *parser);
ASTNode* parse_table_add(Parser *parser);
ASTNode* parse_kvalot_set(Parser *parser);
```

**示例解析**：
```c
// 解析：HOOK TABLE users;
ASTNode* parse_hook_statement(Parser *parser) {
    expect(TOK_HOOK);
    Token obj_type = expect(TOK_TABLE);  // 或 TOK_KVALOT
    Token obj_name = expect(TOK_IDENTIFIER);
    expect(TOK_SEMICOLON);
    
    return ast_create_hook("CREATE", obj_type.value, obj_name.value);
}
```

### 🔲 第 4 步：Compiler 扩展

**目标**：将 NAQL AST 编译为字节码

**字节码格式**：
```
OP_DB_TABLE <subop> <arg_count>
```

**子操作码定义**：
```c
typedef enum {
    TABLE_FIELD_ADD = 0,
    TABLE_ADD_RECORD,
    TABLE_GET_RECORD,
    TABLE_SET_VALUE,
    TABLE_DEL_RECORD,
    TABLE_GET_WHERE,
} TableSubOp;
```

**编译示例**：
```c
void compile_table_add(Compiler *comp, ASTTableOp *node) {
    // ADD 1 'Alice' 25;
    // 生成字节码：
    for (int i = 0; i < node->value_count; i++) {
        // 将 BHS 值压栈
        compile_bhs_value(comp, node->values[i]);
    }
    bytecode_emit_u32(comp->program, OP_DB_TABLE, TABLE_ADD_RECORD);
    bytecode_emit_u32(comp->program, node->value_count);
}
```

### 🔲 第 5 步：VM 扩展

**目标**：执行 NAQL 字节码

**VM 执行示例**：
```c
case OP_DB_TABLE: {
    uint8_t subop = read_u8(vm);
    uint32_t arg_count = read_u32(vm);
    
    switch (subop) {
        case TABLE_ADD_RECORD: {
            // 从栈上弹出 BHS 值
            BHS **values = malloc(sizeof(BHS*) * arg_count);
            for (int i = arg_count - 1; i >= 0; i--) {
                values[i] = vm_pop(vm);  // 弹出 BHS
            }
            
            // 调用 C API
            TABLE *table = get_current_table(vm);
            add_record(table, values, arg_count);
            
            free(values);
            break;
        }
    }
    break;
}
```

### 🔲 第 6 步：测试

**测试用例**：
```javascript
// test_naql.lgx

// 创建表
HOOK TABLE users;
FIELD ADD id i4 PKEY;
FIELD ADD name str NOTNULL;
FIELD ADD age i4;

// 添加记录
ADD 1 'Alice' 25;
ADD 2 'Bob' 30;

// 查询
GET 0;              // 获取第一行
GET WHERE id == 2;  // 条件查询

// 混合 Logex
let new_id = 3;
let new_name = "Charlie";
ADD new_id new_name 28;
```

---

## 字节码操作码设计

### 已预留的操作码

```c
/* bytecode.h 中已定义 */
OP_DB_HOOK = 120,     /* HOOK 操作 */
OP_DB_TABLE,          /* TABLE 操作 */
OP_DB_KVALOT,         /* KVALOT 操作 */
OP_DB_LIST,           /* LIST 操作 */
OP_DB_BITMAP,         /* BITMAP 操作 */
OP_DB_STREAM,         /* STREAM 操作 */
```

### 子操作码扩展

```c
/* 新增：bytecode.h */

/* OP_DB_HOOK 子操作 */
typedef enum {
    HOOK_CREATE = 0,
    HOOK_SWITCH,
    HOOK_DELETE,
    HOOK_CLEAR,
} HookSubOp;

/* OP_DB_TABLE 子操作 */
typedef enum {
    TABLE_FIELD_ADD = 0,
    TABLE_FIELD_DEL,
    TABLE_FIELD_SWAP,
    TABLE_ADD_RECORD,
    TABLE_GET_RECORD,
    TABLE_SET_VALUE,
    TABLE_DEL_RECORD,
    TABLE_GET_WHERE,
} TableSubOp;

/* OP_DB_KVALOT 子操作 */
typedef enum {
    KVALOT_SET = 0,
    KVALOT_GET,
    KVALOT_DEL,
    KVALOT_EXISTS,
} KvalotSubOp;
```

---

## 混合语法示例

### 示例 1：Logex 控制流 + NAQL 操作

```javascript
// 批量插入数据
HOOK TABLE users;
FIELD ADD id i4 PKEY;
FIELD ADD name str NOTNULL;

for i in range(1, 100) {
    let name = str(i);
    ADD i name;  // NAQL 使用 Logex 变量
}
```

### 示例 2：NAQL 查询 + Logex 处理

```javascript
// 查询并处理结果
let result = GET WHERE id > 50;
let count = llen(result);  // Logex 内置函数

if count > 0 {
    // 处理结果
}
```

### 示例 3：完整的数据库操作

```javascript
// 创建用户表
HOOK TABLE users;
FIELD ADD id i4 PKEY;
FIELD ADD name str NOTNULL;
FIELD ADD email str UNIQUE;
FIELD ADD age i4;

// 创建缓存
HOOK KVALOT cache;

// 添加用户
ADD 1 'Alice' 'alice@example.com' 25;
ADD 2 'Bob' 'bob@example.com' 30;

// 缓存用户信息
SET 'user:1' 'Alice';
SET 'user:2' 'Bob';

// 查询
let user = GET WHERE id == 1;
let cached = GET 'user:1';

// 统计
let total = GET COUNT;
```

---

## 性能优势

### 1. **零拷贝数据传递**

```c
// Logex 变量 -> NAQL 操作
let value = 100;
ADD value;  // 直接传递 BHS 指针，无需拷贝
```

### 2. **统一的内存管理**

```c
// VM 统一管理 BHS 生命周期
BHS *value = vm_pop(vm);
add_record(table, &value, 1);
// VM 负责释放 value
```

### 3. **编译优化**

```c
// 编译器可以优化 BHS 操作
OP_PUSH_VAR user_id
OP_PUSH_VAR user_name
OP_DB_TABLE TABLE_ADD  // 一次调用，多个参数
```
