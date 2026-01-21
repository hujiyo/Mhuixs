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
    /* Logex 节点 */
    AST_EXPRESSION,     /* 表达式 */
    AST_ASSIGNMENT,     /* 赋值语句 */
    AST_IF,             /* if语句 */
    AST_FOR,            /* for循环 */
    AST_WHILE,          /* while循环 */
    AST_DO_WHILE,       /* do-while循环 */
    AST_BLOCK,          /* 语句块 */
    AST_IMPORT,         /* import语句 */
    
    /* NAQL 节点 */
    AST_HOOK_CREATE,    /* HOOK TABLE users; */
    AST_HOOK_SWITCH,    /* HOOK users; */
    AST_HOOK_DELETE,    /* HOOK DEL users; */
    AST_HOOK_CLEAR,     /* HOOK CLEAR users; */
    
    AST_FIELD_ADD,      /* FIELD ADD id i4 PKEY; */
    AST_FIELD_DEL,      /* FIELD DEL 0; */
    AST_FIELD_SWAP,     /* FIELD SWAP 0 1; */
    
    AST_TABLE_ADD,      /* ADD 1 'Alice' 25; */
    AST_TABLE_GET,      /* GET 0; */
    AST_TABLE_SET,      /* SET 0 1 'Bob'; */
    AST_TABLE_DEL,      /* DEL 0; */
    AST_TABLE_WHERE,    /* GET WHERE id == 1; */
    
    AST_KVALOT_SET,     /* SET key value; */
    AST_KVALOT_GET,     /* GET key; */
    AST_KVALOT_DEL,     /* DEL key; */
    AST_KVALOT_EXISTS,  /* EXISTS key; */
    
    AST_LIST_LPUSH,     /* LPUSH value; */
    AST_LIST_RPUSH,     /* RPUSH value; */
    AST_LIST_LPOP,      /* LPOP; */
    AST_LIST_RPOP,      /* RPOP; */
    AST_LIST_GET,       /* GET index; */
    
    AST_BITMAP_SET,     /* SET offset value; */
    AST_BITMAP_GET,     /* GET offset; */
    AST_BITMAP_COUNT,   /* COUNT; */
    AST_BITMAP_FLIP     /* FLIP offset; */
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
    int is_static;      /* 是否持久化（注册到 Mhuixs 注册表） */
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

/* NAQL HOOK 操作节点 */
typedef struct {
    char *obj_type;               /* TABLE/KVALOT/LIST/BITMAP/STREAM */
    char *obj_name;               /* 对象名 */
    char *operation;              /* CREATE/SWITCH/DELETE/CLEAR */
} ASTHookOp;

/* NAQL FIELD 操作节点 */
typedef struct {
    char *operation;              /* ADD/DEL/SWAP */
    char *field_name;             /* 字段名（ADD 时使用） */
    char *data_type;              /* 数据类型（ADD 时使用）i4/str/... */
    char *constraint;             /* 约束（ADD 时使用）PKEY/NOTNULL/... */
    int index1;                   /* 字段索引1（DEL/SWAP 时使用） */
    int index2;                   /* 字段索引2（SWAP 时使用） */
} ASTFieldOp;

/* NAQL TABLE 操作节点 */
typedef struct {
    char *operation;              /* ADD/GET/SET/DEL/WHERE */
    char **value_strings;         /* 值字符串数组（运行时解析） */
    int value_count;              /* 值数量 */
    char *condition;              /* WHERE 条件表达式 */
    int row_index;                /* 行索引（GET/SET/DEL 时使用） */
    int col_index;                /* 列索引（SET 时使用） */
} ASTTableOp;

/* NAQL KVALOT 操作节点 */
typedef struct {
    char *operation;              /* SET/GET/DEL/EXISTS */
    char *key;                    /* 键名 */
    char *value_string;           /* 值字符串（运行时解析） */
} ASTKvalotOp;

/* NAQL LIST 操作节点 */
typedef struct {
    char *operation;              /* LPUSH/RPUSH/LPOP/RPOP/GET */
    char *value_string;           /* 值字符串（运行时解析） */
    int index;                    /* 索引（GET 时使用） */
} ASTListOp;

/* NAQL BITMAP 操作节点 */
typedef struct {
    char *operation;              /* SET/GET/COUNT/FLIP */
    int offset;                   /* 偏移量 */
    int value;                    /* 值（SET 时使用，0 或 1） */
} ASTBitmapOp;

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
        
        /* NAQL 节点 */
        ASTHookOp hook_op;
        ASTFieldOp field_op;
        ASTTableOp table_op;
        ASTKvalotOp kvalot_op;
        ASTListOp list_op;
        ASTBitmapOp bitmap_op;
    } data;
} ASTNode;

/**
 * 创建表达式节点
 */
ASTNode* ast_create_expression(const char *expr);

/**
 * 创建赋值节点
 * @param is_static 是否持久化（1=注册到Mhuixs注册表，0=临时变量）
 */
ASTNode* ast_create_assignment(const char *var, const char *expr, int is_static);

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
 * 创建 HOOK 操作节点
 */
ASTNode* ast_create_hook(const char *operation, const char *obj_type, const char *obj_name);

/**
 * 创建 FIELD ADD 节点
 */
ASTNode* ast_create_field_add(const char *field_name, const char *data_type, const char *constraint);

/**
 * 创建 FIELD DEL 节点
 */
ASTNode* ast_create_field_del(int index);

/**
 * 创建 FIELD SWAP 节点
 */
ASTNode* ast_create_field_swap(int index1, int index2);

/**
 * 创建 TABLE ADD 节点
 */
ASTNode* ast_create_table_add(char **value_strings, int value_count);

/**
 * 创建 TABLE GET 节点
 */
ASTNode* ast_create_table_get(int row_index);

/**
 * 创建 TABLE SET 节点
 */
ASTNode* ast_create_table_set(int row_index, int col_index, const char *value_string);

/**
 * 创建 TABLE DEL 节点
 */
ASTNode* ast_create_table_del(int row_index);

/**
 * 创建 TABLE WHERE 节点
 */
ASTNode* ast_create_table_where(const char *condition);

/**
 * 创建 KVALOT SET 节点
 */
ASTNode* ast_create_kvalot_set(const char *key, const char *value_string);

/**
 * 创建 KVALOT GET 节点
 */
ASTNode* ast_create_kvalot_get(const char *key);

/**
 * 创建 KVALOT DEL 节点
 */
ASTNode* ast_create_kvalot_del(const char *key);

/**
 * 创建 KVALOT EXISTS 节点
 */
ASTNode* ast_create_kvalot_exists(const char *key);

/**
 * 创建 LIST PUSH 节点
 */
ASTNode* ast_create_list_push(const char *operation, const char *value_string);

/**
 * 创建 LIST POP 节点
 */
ASTNode* ast_create_list_pop(const char *operation);

/**
 * 创建 LIST GET 节点
 */
ASTNode* ast_create_list_get(int index);

/**
 * 创建 BITMAP SET 节点
 */
ASTNode* ast_create_bitmap_set(int offset, int value);

/**
 * 创建 BITMAP GET 节点
 */
ASTNode* ast_create_bitmap_get(int offset);

/**
 * 创建 BITMAP COUNT 节点
 */
ASTNode* ast_create_bitmap_count(void);

/**
 * 创建 BITMAP FLIP 节点
 */
ASTNode* ast_create_bitmap_flip(int offset);

/**
 * 销毁AST节点
 */
void ast_destroy(ASTNode *node);

/**
 * 执行AST节点
 */
int ast_execute(ASTNode *node, void *ctx, void *func_registry, void *pkg_manager, int precision, char *result_str, size_t max_len);

#endif /* AST_H */
