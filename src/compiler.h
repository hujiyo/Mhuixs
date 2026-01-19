/**
 * GLL - Logex 编译器
 * 将 .lgx 源文件编译为 .ls 字节码文件
 */

#ifndef COMPILER_H
#define COMPILER_H

#include "bytecode.h"
#include "lexer.h"
#include "ast.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 编译器上下文 */
typedef struct {
    BytecodeProgram *program;     /* 字节码程序 */
    Lexer *lexer;                 /* 词法分析器 */
    char error_msg[256];          /* 错误信息 */
    int has_error;                /* 是否有错误 */
    
    /* 符号表（变量名 -> 常量池索引） */
    struct {
        char *name;
        uint32_t const_idx;
    } *symbols;
    int symbol_count;
    int symbol_capacity;
    
    /* 跳转标签栈（用于 break/continue） */
    struct {
        uint32_t loop_start;
        uint32_t loop_end;
    } *loop_stack;
    int loop_depth;
    
} Compiler;

/* 编译器初始化和清理 */
Compiler* compiler_create(void);
void compiler_destroy(Compiler *comp);

/* 编译源文件 */
int compiler_compile_file(Compiler *comp, const char *source_file, const char *output_file);
int compiler_compile_string(Compiler *comp, const char *source, const char *source_name);

/* 错误处理 */
const char* compiler_get_error(Compiler *comp);

#ifdef __cplusplus
}
#endif

#endif /* COMPILER_H */
