# Logex 解释器架构重构计划

## 当前架构问题分析

### 1. 错误处理问题
- **不统一的错误返回**：有些函数返回 `EVAL_ERROR`，有些返回 `-1`，有些返回 `NULL`
- **错误信息不明确**：大量使用 "语法错误、变量未定义或函数未定义" 这样的模糊错误
- **缺乏错误上下文**：不知道错误发生在哪一行、哪个位置
- **错误处理分散**：错误处理逻辑散落在各个模块中

### 2. 执行流程问题
- **混合架构**：部分使用 AST（控制流），部分直接求值（表达式）
- **双重解析**：`evaluator.c` 和 `parser.c` 都在做解析工作
- **职责不清**：`eval_statement` 同时做词法、解析和执行
- **难以扩展**：添加新语法需要修改多个地方

### 3. 代码组织问题
- **模块耦合**：各模块之间相互依赖严重
- **接口不统一**：不同模块使用不同的参数传递方式
- **缺乏文档**：关键函数缺少清晰的注释和文档

## 新架构设计

### 1. 统一的错误处理系统

#### 错误类型定义 (error.h)
```c
typedef enum {
    ERR_NONE = 0,           // 无错误
    ERR_SYNTAX,             // 语法错误
    ERR_TYPE,               // 类型错误
    ERR_NAME,               // 名称错误（变量/函数未定义）
    ERR_VALUE,              // 值错误
    ERR_RUNTIME,            // 运行时错误
    ERR_IMPORT,             // 导入错误
    ERR_MEMORY,             // 内存错误
    ERR_DIV_ZERO,           // 除零错误
} ErrorType;

typedef struct {
    ErrorType type;          // 错误类型
    char message[256];       // 错误消息
    const char *source;      // 源代码
    int line;                // 行号
    int column;              // 列号
    int length;              // 错误标记长度
} LogexError;

// 错误处理函数
void error_init(LogexError *err);
void error_set(LogexError *err, ErrorType type, const char *msg, 
               const char *source, int line, int col, int len);
void error_print(const LogexError *err);  // 类似 Python 的错误显示
char* error_format(const LogexError *err); // 格式化错误信息
```

#### 错误显示格式（类似 Python）
```
SyntaxError: unexpected token '+'
  File "<stdin>", line 2
    5 + + 3
        ^
```

### 2. 清晰的执行流程

#### 标准执行流程
```
源代码 -> 词法分析(Lexer) -> Token流 -> 语法分析(Parser) -> AST -> 执行器(Executor) -> 结果
                ↓                          ↓                      ↓
            错误处理                    错误处理               错误处理
```

#### 模块划分
1. **lexer.c/h** - 词法分析器（只负责分词）
2. **parser.c/h** - 语法分析器（只负责构建 AST）
3. **ast.c/h** - AST 定义和操作
4. **executor.c/h** - AST 执行器（只负责执行）
5. **error.c/h** - 统一错误处理
6. **context.c/h** - 运行时上下文
7. **interpreter.c/h** - 顶层接口

### 3. 统一的接口设计

#### 顶层接口 (interpreter.h)
```c
// 解释器状态
typedef struct {
    Context *context;           // 运行时上下文
    FunctionRegistry *funcs;    // 函数注册表
    PackageManager *packages;   // 包管理器
    LogexError error;           // 错误信息
} Interpreter;

// 初始化和清理
Interpreter* interpreter_create(void);
void interpreter_destroy(Interpreter *interp);

// 执行接口
int interpreter_eval(Interpreter *interp, const char *source, char *result, size_t max_len);
int interpreter_exec_file(Interpreter *interp, const char *filename);

// 错误处理
LogexError* interpreter_get_error(Interpreter *interp);
void interpreter_clear_error(Interpreter *interp);
```

### 4. 位置追踪

#### Token 增强
```c
typedef struct {
    TokenType type;
    char value[BIGNUM_MAX_DIGITS];
    int line;       // 行号
    int column;     // 列号
    int length;     // token 长度
} Token;
```

#### AST 节点增强
```c
typedef struct {
    ASTNodeType type;
    int line;       // 起始行
    int column;     // 起始列
    int end_line;   // 结束行
    int end_column; // 结束列
    // ... 其他字段
} ASTNode;
```

## 重构步骤

### 阶段 1：错误处理系统
1. 创建 `error.h` 和 `error.c`
2. 定义错误类型和结构
3. 实现错误格式化和打印函数

### 阶段 2：位置追踪
1. 增强 Lexer 添加位置信息
2. 增强 Token 结构
3. 增强 AST 节点结构

### 阶段 3：分离职责
1. 移除 `evaluator.c` 中的解析代码
2. 将所有解析逻辑移到 `parser.c`
3. 创建统一的 `executor.c` 执行 AST

### 阶段 4：顶层接口
1. 创建 `interpreter.h` 和 `interpreter.c`
2. 实现统一的解释器接口
3. 重构 `calculator.c` 使用新接口

### 阶段 5：测试和文档
1. 编写单元测试
2. 编写集成测试
3. 更新文档

## 兼容性保证

- 保持原有功能不变
- 保持原有语法不变
- 逐步重构，每个阶段都可以编译运行
- 保留旧接口作为兼容层（标记为 deprecated）

## 预期效果

1. **清晰的错误信息**
   ```
   NameError: variable 'x' is not defined
     File "<stdin>", line 1
       print(x)
             ^
   ```

2. **更好的可维护性**
   - 每个模块职责单一
   - 接口清晰统一
   - 易于添加新特性

3. **更好的可扩展性**
   - 新语法只需修改 Parser 和 AST
   - 新功能只需添加新的 AST 节点类型
   - 优化器可以在 AST 层面工作

4. **更好的错误处理**
   - 精确的错误定位
   - 清晰的错误消息
   - 完整的错误上下文
