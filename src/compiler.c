/**
 * GLL - Logex 编译器实现
 */

#include "compiler.h"
#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define INITIAL_SYMBOL_CAPACITY 64

/* 创建编译器 */
Compiler* compiler_create(void) {
    Compiler *comp = (Compiler*)calloc(1, sizeof(Compiler));
    if (!comp) return NULL;
    
    comp->program = bytecode_create();
    if (!comp->program) {
        free(comp);
        return NULL;
    }
    
    comp->symbols = (typeof(comp->symbols))calloc(INITIAL_SYMBOL_CAPACITY, sizeof(*comp->symbols));
    comp->symbol_capacity = INITIAL_SYMBOL_CAPACITY;
    comp->symbol_count = 0;
    
    comp->loop_stack = NULL;
    comp->loop_depth = 0;
    
    comp->has_error = 0;
    comp->error_msg[0] = '\0';
    
    return comp;
}

/* 销毁编译器 */
void compiler_destroy(Compiler *comp) {
    if (!comp) return;
    
    if (comp->program) {
        bytecode_destroy(comp->program);
    }
    
    for (int i = 0; i < comp->symbol_count; i++) {
        free(comp->symbols[i].name);
    }
    free(comp->symbols);
    free(comp->loop_stack);
    
    free(comp);
}

/* 设置错误 */
static void compiler_error(Compiler *comp, const char *msg) {
    comp->has_error = 1;
    snprintf(comp->error_msg, sizeof(comp->error_msg), "%s", msg);
}

/* 查找或添加符号 */
static uint32_t compiler_add_symbol(Compiler *comp, const char *name) {
    /* 查找已存在的符号 */
    for (int i = 0; i < comp->symbol_count; i++) {
        if (strcmp(comp->symbols[i].name, name) == 0) {
            return comp->symbols[i].const_idx;
        }
    }
    
    /* 添加新符号 */
    uint32_t const_idx = bytecode_add_const_identifier(comp->program, name);
    comp->symbols[comp->symbol_count].name = strdup(name);
    comp->symbols[comp->symbol_count].const_idx = const_idx;
    comp->symbol_count++;
    
    return const_idx;
}

/* 编译表达式 */
static int compile_expression(Compiler *comp, ASTNode *node);

/* 编译二元运算 */
static int compile_binary_op(Compiler *comp, ASTNode *node) {
    /* 编译左操作数 */
    if (compile_expression(comp, node->data.binary_op.left) != 0) {
        return -1;
    }
    
    /* 编译右操作数 */
    if (compile_expression(comp, node->data.binary_op.right) != 0) {
        return -1;
    }
    
    /* 发射运算符指令 */
    switch (node->data.binary_op.op) {
        case '+': bytecode_emit(comp->program, OP_ADD); break;
        case '-': bytecode_emit(comp->program, OP_SUB); break;
        case '*': bytecode_emit(comp->program, OP_MUL); break;
        case '/': bytecode_emit(comp->program, OP_DIV); break;
        case '%': bytecode_emit(comp->program, OP_MOD); break;
        case '^': bytecode_emit(comp->program, OP_AND); break;
        case 'v': bytecode_emit(comp->program, OP_OR); break;
        case 'x': bytecode_emit(comp->program, OP_XOR); break;
        default:
            compiler_error(comp, "Unknown binary operator");
            return -1;
    }
    
    return 0;
}

/* 编译字面量 */
static int compile_literal(Compiler *comp, ASTNode *node) {
    if (node->type == AST_NUMBER) {
        /* 添加数字常量到常量池 */
        uint32_t idx = bytecode_add_const_number(comp->program, 
                                                  node->data.number.digits,
                                                  node->data.number.decimal_pos,
                                                  node->data.number.is_negative);
        bytecode_emit_u32(comp->program, OP_PUSH_NUM, idx);
    } else if (node->type == AST_STRING) {
        /* 添加字符串常量到常量池 */
        uint32_t idx = bytecode_add_const_string(comp->program, node->data.string);
        bytecode_emit_u32(comp->program, OP_PUSH_STR, idx);
    }
    
    return 0;
}

/* 编译变量引用 */
static int compile_variable(Compiler *comp, ASTNode *node) {
    uint32_t idx = compiler_add_symbol(comp, node->data.identifier);
    bytecode_emit_u32(comp->program, OP_LOAD_VAR, idx);
    return 0;
}

/* 编译函数调用 */
static int compile_function_call(Compiler *comp, ASTNode *node) {
    const char *func_name = node->data.func_call.name;
    int arg_count = node->data.func_call.arg_count;
    
    /* 编译参数（从左到右压栈） */
    for (int i = 0; i < arg_count; i++) {
        if (compile_expression(comp, node->data.func_call.args[i]) != 0) {
            return -1;
        }
    }
    
    /* 检查是否是内置函数 */
    OpCode builtin_op = OP_NOP;
    if (strcmp(func_name, "list") == 0) builtin_op = OP_CALL_LIST;
    else if (strcmp(func_name, "lpush") == 0) builtin_op = OP_CALL_LPUSH;
    else if (strcmp(func_name, "rpush") == 0) builtin_op = OP_CALL_RPUSH;
    else if (strcmp(func_name, "lpop") == 0) builtin_op = OP_CALL_LPOP;
    else if (strcmp(func_name, "rpop") == 0) builtin_op = OP_CALL_RPOP;
    else if (strcmp(func_name, "lget") == 0) builtin_op = OP_CALL_LGET;
    else if (strcmp(func_name, "llen") == 0) builtin_op = OP_CALL_LLEN;
    else if (strcmp(func_name, "num") == 0) builtin_op = OP_CALL_NUM;
    else if (strcmp(func_name, "str") == 0) builtin_op = OP_CALL_STR;
    else if (strcmp(func_name, "bmp") == 0) builtin_op = OP_CALL_BMP;
    else if (strcmp(func_name, "bset") == 0) builtin_op = OP_CALL_BSET;
    else if (strcmp(func_name, "bget") == 0) builtin_op = OP_CALL_BGET;
    else if (strcmp(func_name, "bcount") == 0) builtin_op = OP_CALL_BCOUNT;
    
    if (builtin_op != OP_NOP) {
        /* 内置函数 */
        bytecode_emit_u32(comp->program, builtin_op, arg_count);
    } else {
        /* 外部包函数 */
        uint32_t name_idx = bytecode_add_const_identifier(comp->program, func_name);
        bytecode_emit_ref(comp->program, OP_CALL_PACKAGE, name_idx, arg_count);
    }
    
    return 0;
}

/* 编译表达式 */
static int compile_expression(Compiler *comp, ASTNode *node) {
    if (!node) return -1;
    
    switch (node->type) {
        case AST_NUMBER:
        case AST_STRING:
            return compile_literal(comp, node);
            
        case AST_IDENTIFIER:
            return compile_variable(comp, node);
            
        case AST_BINARY_OP:
            return compile_binary_op(comp, node);
            
        case AST_FUNCTION_CALL:
            return compile_function_call(comp, node);
            
        default:
            compiler_error(comp, "Unknown expression type");
            return -1;
    }
}

/* 编译赋值语句 */
static int compile_assignment(Compiler *comp, ASTNode *node) {
    /* 编译右侧表达式 */
    if (compile_expression(comp, node->data.assignment.value) != 0) {
        return -1;
    }
    
    /* 存储到变量 */
    uint32_t idx = compiler_add_symbol(comp, node->data.assignment.var_name);
    bytecode_emit_u32(comp->program, OP_STORE_VAR, idx);
    
    return 0;
}

/* 编译 if 语句 */
static int compile_if_statement(Compiler *comp, ASTNode *node) {
    /* 编译条件 */
    if (compile_expression(comp, node->data.if_stmt.condition) != 0) {
        return -1;
    }
    
    /* 条件跳转（假则跳过 then 分支） */
    uint32_t jump_to_else = bytecode_current_pos(comp->program);
    bytecode_emit_u32(comp->program, OP_JMP_IF_FALSE, 0);  /* 占位 */
    
    /* 编译 then 分支 */
    if (compile_expression(comp, node->data.if_stmt.then_branch) != 0) {
        return -1;
    }
    
    /* 跳过 else 分支 */
    uint32_t jump_to_end = bytecode_current_pos(comp->program);
    bytecode_emit_u32(comp->program, OP_JMP, 0);  /* 占位 */
    
    /* 修补跳转到 else */
    uint32_t else_pos = bytecode_current_pos(comp->program);
    bytecode_patch_jump(comp->program, jump_to_else, else_pos);
    
    /* 编译 else 分支 */
    if (node->data.if_stmt.else_branch) {
        if (compile_expression(comp, node->data.if_stmt.else_branch) != 0) {
            return -1;
        }
    }
    
    /* 修补跳转到结束 */
    uint32_t end_pos = bytecode_current_pos(comp->program);
    bytecode_patch_jump(comp->program, jump_to_end, end_pos);
    
    return 0;
}

/* 编译语句 */
static int compile_statement(Compiler *comp, ASTNode *node) {
    if (!node) return 0;
    
    switch (node->type) {
        case AST_ASSIGNMENT:
            return compile_assignment(comp, node);
            
        case AST_IF_STATEMENT:
            return compile_if_statement(comp, node);
            
        case AST_EXPRESSION_STMT:
            return compile_expression(comp, node->data.expr_stmt);
            
        default:
            return compile_expression(comp, node);
    }
}

/* 编译字符串源码 */
int compiler_compile_string(Compiler *comp, const char *source, const char *source_name) {
    if (!comp || !source) return -1;
    
    /* 设置源文件名 */
    if (source_name) {
        strncpy(comp->program->header.source_file, source_name, 
                sizeof(comp->program->header.source_file) - 1);
    }
    
    /* 初始化词法分析器 */
    Lexer lexer;
    LogexError error;
    lexer_init(&lexer, source, source_name, &error);
    comp->lexer = &lexer;
    
    /* 解析并编译每个语句 */
    Parser parser;
    parser_init(&parser, &lexer);
    
    while (lexer_current_type(&lexer) != TOK_END) {
        ASTNode *stmt = parser_parse_statement(&parser);
        if (!stmt || parser_has_error(&parser)) {
            compiler_error(comp, parser_get_error(&parser));
            if (stmt) ast_destroy(stmt);
            return -1;
        }
        
        if (compile_statement(comp, stmt) != 0) {
            ast_destroy(stmt);
            return -1;
        }
        
        ast_destroy(stmt);
    }
    
    /* 添加 HALT 指令 */
    bytecode_emit(comp->program, OP_HALT);
    
    return 0;
}

/* 编译文件 */
int compiler_compile_file(Compiler *comp, const char *source_file, const char *output_file) {
    if (!comp || !source_file || !output_file) return -1;
    
    /* 读取源文件 */
    FILE *fp = fopen(source_file, "r");
    if (!fp) {
        compiler_error(comp, "Cannot open source file");
        return -1;
    }
    
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char *source = (char*)malloc(size + 1);
    fread(source, 1, size, fp);
    source[size] = '\0';
    fclose(fp);
    
    /* 编译 */
    int ret = compiler_compile_string(comp, source, source_file);
    free(source);
    
    if (ret != 0) {
        return -1;
    }
    
    /* 保存字节码 */
    return bytecode_save(comp->program, output_file);
}

/* 获取错误信息 */
const char* compiler_get_error(Compiler *comp) {
    return comp ? comp->error_msg : "Unknown error";
}
