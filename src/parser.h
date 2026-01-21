#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"

/**
 * 语句解析器
 * 
 * 负责将token流解析为AST
 */

/* 解析器状态 */
typedef struct {
    Lexer *lexer;
    int has_error;
    char error_msg[256];
} Parser;

/**
 * 初始化解析器
 */
void parser_init(Parser *parser, Lexer *lexer);

/**
 * 解析语句
 */
ASTNode* parser_parse_statement(Parser *parser);

/**
 * 解析语句块
 */
ASTNode* parser_parse_block(Parser *parser);

/**
 * 解析if语句
 */
ASTNode* parser_parse_if(Parser *parser);

/**
 * 解析for语句
 */
ASTNode* parser_parse_for(Parser *parser);

/**
 * 解析while语句
 */
ASTNode* parser_parse_while(Parser *parser);

/**
 * 解析do-while语句
 */
ASTNode* parser_parse_do_while(Parser *parser);

/**
 * 解析赋值语句
 * @param is_static 是否持久化（1=注册到Mhuixs注册表，0=临时变量）
 */
ASTNode* parser_parse_assignment(Parser *parser, const char *var_name, int is_static);

/**
 * 解析import语句
 */
ASTNode* parser_parse_import(Parser *parser);

/**
 * 解析表达式（返回表达式字符串）
 */
char* parser_parse_expression_string(Parser *parser);

/**
 * 检查是否有错误
 */
int parser_has_error(Parser *parser);

/**
 * 获取错误消息
 */
const char* parser_get_error(Parser *parser);

/**
 * 解析 NAQL HOOK 语句
 * HOOK TABLE users;
 * HOOK users;
 * HOOK DEL users;
 */
ASTNode* parser_parse_hook(Parser *parser);

/**
 * 解析 NAQL FIELD 语句
 * FIELD ADD id i4 PKEY;
 * FIELD DEL 0;
 * FIELD SWAP 0 1;
 */
ASTNode* parser_parse_field(Parser *parser);

/**
 * 解析 NAQL TABLE 操作
 * ADD 1 'Alice' 25;
 * GET 0;
 * SET 0 1 'Bob';
 * DEL 0;
 */
ASTNode* parser_parse_table_op(Parser *parser);

/**
 * 解析 NAQL KVALOT 操作
 * SET key value;
 * GET key;
 * DEL key;
 * EXISTS key;
 */
ASTNode* parser_parse_kvalot_op(Parser *parser);

/**
 * 解析 NAQL LIST 操作
 * LPUSH value;
 * RPUSH value;
 * LPOP;
 * RPOP;
 */
ASTNode* parser_parse_list_op(Parser *parser);

/**
 * 解析 NAQL BITMAP 操作
 * SET offset value;
 * GET offset;
 * COUNT;
 * FLIP offset;
 */
ASTNode* parser_parse_bitmap_op(Parser *parser);

#endif /* PARSER_H */
