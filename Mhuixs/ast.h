#ifndef AST_H
#define AST_H

#include "bignum.h"
#include "lexer.h"

/**
 * 抽象语法树（AST）节点定义
 * 
 * 支持控制流语句的语法树表示
 */

/* AST节点类型 */
typedef enum {
    AST_EXPRESSION,     /* 表达式 */
    AST_ASSIGNMENT,     /* 赋值语句 */
    AST_IF,             /* if语句 */
    AST_FOR,            /* for循环 */
    AST_WHILE,          /* while循环 */
    AST_DO_WHILE,       /* do-while循环 */
    AST_BLOCK,          /* 语句块 */
    AST_IMPORT          /* import语句 */
} ASTNodeType;

/* 前向声明 */
struct ASTNode;

/* 表达式节点 */
typedef struct {
    char *expression;   /* 表达式字符串 */
} ASTExpression;

/* 赋值节点 */
typedef struct {
    char *variable;     /* 变量名 */
    char *expression;   /* 表达式字符串 */
} ASTAssignment;

/* if语句节点 */
typedef struct {
    char *condition;              /* 条件表达式 */
    struct ASTNode *then_block;   /* then分支 */
    struct ASTNode *else_block;   /* else分支（可选） */
} ASTIf;

/* for循环节点 */
typedef struct {
    char *variable;               /* 循环变量 */
    char *start_expr;             /* 起始值表达式 */
    char *end_expr;               /* 结束值表达式 */
    char *step_expr;              /* 步长表达式（可选） */
    struct ASTNode *body;         /* 循环体 */
} ASTFor;

/* while循环节点 */
typedef struct {
    char *condition;              /* 条件表达式 */
    struct ASTNode *body;         /* 循环体 */
} ASTWhile;

/* do-while循环节点 */
typedef struct {
    struct ASTNode *body;         /* 循环体 */
    char *condition;              /* 条件表达式 */
} ASTDoWhile;

/* 语句块节点 */
typedef struct {
    struct ASTNode **statements;  /* 语句数组 */
    int count;                    /* 语句数量 */
    int capacity;                 /* 容量 */
} ASTBlock;

/* import语句节点 */
typedef struct {
    char *package_name;           /* 包名 */
} ASTImport;

/* AST节点 */
typedef struct ASTNode {
    ASTNodeType type;
    union {
        ASTExpression expression;
        ASTAssignment assignment;
        ASTIf if_stmt;
        ASTFor for_stmt;
        ASTWhile while_stmt;
        ASTDoWhile do_while_stmt;
        ASTBlock block;
        ASTImport import_stmt;
    } data;
} ASTNode;

/**
 * 创建表达式节点
 */
ASTNode* ast_create_expression(const char *expr);

/**
 * 创建赋值节点
 */
ASTNode* ast_create_assignment(const char *var, const char *expr);

/**
 * 创建if节点
 */
ASTNode* ast_create_if(const char *condition, ASTNode *then_block, ASTNode *else_block);

/**
 * 创建for节点
 */
ASTNode* ast_create_for(const char *var, const char *start, const char *end, const char *step, ASTNode *body);

/**
 * 创建while节点
 */
ASTNode* ast_create_while(const char *condition, ASTNode *body);

/**
 * 创建do-while节点
 */
ASTNode* ast_create_do_while(ASTNode *body, const char *condition);

/**
 * 创建语句块节点
 */
ASTNode* ast_create_block(void);

/**
 * 向语句块添加语句
 */
int ast_block_add_statement(ASTNode *block, ASTNode *stmt);

/**
 * 创建import节点
 */
ASTNode* ast_create_import(const char *package_name);

/**
 * 销毁AST节点
 */
void ast_destroy(ASTNode *node);

/**
 * 执行AST节点
 */
int ast_execute(ASTNode *node, void *ctx, void *func_registry, void *pkg_manager, int precision, char *result_str, size_t max_len);

#endif /* AST_H */
