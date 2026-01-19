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
 */
ASTNode* parser_parse_assignment(Parser *parser, const char *var_name);

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

#endif /* PARSER_H */
