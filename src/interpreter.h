#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "error.h"
#include "context.h"
#include "function.h"
#include "package.h"
#include "ast_v2.h"
#include "vm.h"
#include "compiler.h"

/**
 * Logex 解释器统一接口
 * 
 * 提供清晰的解释器架构：
 * 源代码 -> 词法分析 -> 语法分析 -> AST -> 执行 -> 结果
 * 
 * 特性：
 * - 统一的错误处理
 * - 清晰的执行流程
 * - 易于扩展的架构
 * - 完整的上下文管理
 */

/* 解释器状态 */
typedef struct {
    Context *context;           /* 运行时上下文 */
    FunctionRegistry *funcs;    /* 函数注册表 */
    PackageManager *packages;   /* 包管理器 */
    LogexError error;           /* 错误信息 */
    int precision;              /* 数值精度 */
    
    /* 字节码 VM 支持 */
    VM *vm;                     /* 虚拟机实例 */
    Compiler *compiler;         /* 编译器实例 */
    int use_vm;                 /* 是否使用 VM 模式（1=VM, 0=直接解释） */
} Interpreter;

/* 执行结果 */
typedef struct {
    enum {
        RESULT_NONE,            /* 无结果（语句） */
        RESULT_VALUE,           /* 有值（表达式） */
        RESULT_ASSIGNMENT,      /* 赋值结果 */
        RESULT_IMPORT,          /* 导入结果 */
        RESULT_ERROR            /* 错误 */
    } type;
    char value[512];            /* 结果值字符串 */
    char message[256];          /* 消息（如导入成功信息） */
} InterpreterResult;

/**
 * 创建解释器实例
 * 
 * @return 解释器实例，失败返回 NULL
 */
Interpreter* interpreter_create(void);

/**
 * 销毁解释器实例
 * 
 * @param interp 解释器实例
 */
void interpreter_destroy(Interpreter *interp);

/**
 * 执行源代码（多行模式）
 * 
 * @param interp 解释器实例
 * @param source 源代码（可以是多行）
 * @param filename 文件名（可选，用于错误报告）
 * @param result 执行结果
 * @return 0 表示成功，非 0 表示失败
 */
int interpreter_execute(Interpreter *interp, const char *source, const char *filename, InterpreterResult *result);

/**
 * 设置执行模式
 * 
 * @param interp 解释器实例
 * @param use_vm 1=使用 VM 模式（编译+执行），0=直接解释模式
 */
void interpreter_set_vm_mode(Interpreter *interp, int use_vm);

/**
 * 执行文件
 * 
 * @param interp 解释器实例
 * @param filename 文件名
 * @param result 执行结果
 * @return 0 表示成功，非 0 表示失败
 */
int interpreter_execute_file(Interpreter *interp, const char *filename, InterpreterResult *result);

/**
 * 获取错误信息
 * 
 * @param interp 解释器实例
 * @return 错误信息指针
 */
const LogexError* interpreter_get_error(const Interpreter *interp);

/**
 * 清除错误信息
 * 
 * @param interp 解释器实例
 */
void interpreter_clear_error(Interpreter *interp);

/**
 * 设置数值精度
 * 
 * @param interp 解释器实例
 * @param precision 精度值
 */
void interpreter_set_precision(Interpreter *interp, int precision);

/**
 * 获取数值精度
 * 
 * @param interp 解释器实例
 * @return 精度值
 */
int interpreter_get_precision(const Interpreter *interp);

/**
 * 导入包
 * 
 * @param interp 解释器实例
 * @param package_name 包名
 * @return 导入的函数数量，失败返回 -1
 */
int interpreter_import_package(Interpreter *interp, const char *package_name);

/**
 * 获取变量值
 * 
 * @param interp 解释器实例
 * @param name 变量名
 * @param value 输出缓冲区
 * @param max_len 缓冲区最大长度
 * @return 0 表示成功，非 0 表示失败
 */
int interpreter_get_variable(const Interpreter *interp, const char *name, char *value, size_t max_len);

/**
 * 设置变量值
 * 
 * @param interp 解释器实例
 * @param name 变量名
 * @param value 变量值
 * @return 0 表示成功，非 0 表示失败
 */
int interpreter_set_variable(Interpreter *interp, const char *name, const char *value);

/**
 * 列出所有变量
 * 
 * @param interp 解释器实例
 * @param buffer 输出缓冲区
 * @param max_len 缓冲区最大长度
 * @return 0 表示成功，非 0 表示失败
 */
int interpreter_list_variables(const Interpreter *interp, char *buffer, size_t max_len);

/**
 * 列出所有函数
 * 
 * @param interp 解释器实例
 * @param buffer 输出缓冲区
 * @param max_len 缓冲区最大长度
 * @return 0 表示成功，非 0 表示失败
 */
int interpreter_list_functions(const Interpreter *interp, char *buffer, size_t max_len);

/**
 * 清空所有变量
 * 
 * @param interp 解释器实例
 */
void interpreter_clear_variables(Interpreter *interp);

/* 兼容性接口（标记为 deprecated） - 移除以避免冲突 */

#endif /* INTERPRETER_H */
