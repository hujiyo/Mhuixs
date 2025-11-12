#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 辅助函数：设置错误 */
static void parser_set_error(Parser *parser, const char *msg) {
    parser->has_error = 1;
    strncpy(parser->error_msg, msg, sizeof(parser->error_msg) - 1);
    parser->error_msg[sizeof(parser->error_msg) - 1] = '\0';
}

/* 辅助函数：期望特定token */
static int parser_expect(Parser *parser, TokenType expected) {
    if (parser->lexer->current_type != expected) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Expected token type %d, got %d", expected, parser->lexer->current_type);
        parser_set_error(parser, msg);
        return 0;
    }
    lexer_next(parser->lexer);
    return 1;
}

/* 辅助函数：复制字符串 */
static char* parser_strdup(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char *copy = malloc(len);
    if (copy) {
        memcpy(copy, str, len);
    }
    return copy;
}

/* 初始化解析器 */
void parser_init(Parser *parser, Lexer *lexer) {
    parser->lexer = lexer;
    parser->has_error = 0;
    parser->error_msg[0] = '\0';
}

/* 解析表达式字符串（简化版本，收集到特定结束符为止） */
char* parser_parse_expression_string(Parser *parser) {
    char *expr = malloc(1024);
    if (!expr) {
        parser_set_error(parser, "Memory allocation failed");
        return NULL;
    }
    
    expr[0] = '\0';
    int pos = 0;
    int paren_count = 0;
    
    while (parser->lexer->current_type != TOK_END && 
           parser->lexer->current_type != TOK_ERROR &&
           parser->lexer->current_type != TOK_COLON &&
           parser->lexer->current_type != TOK_END_STMT &&
           parser->lexer->current_type != TOK_ELSE &&
           parser->lexer->current_type != TOK_COMMA &&
           (parser->lexer->current_type != TOK_RPAREN || paren_count > 0)) {
        
        /* 处理括号平衡 */
        if (parser->lexer->current_type == TOK_LPAREN) {
            paren_count++;
        } else if (parser->lexer->current_type == TOK_RPAREN) {
            paren_count--;
        }
        
        /* 添加token到表达式字符串 */
        const char *token_str = NULL;
        char temp_str[64];
        
        switch (parser->lexer->current_type) {
            case TOK_NUMBER:
            case TOK_STRING:
            case TOK_BITMAP:
            case TOK_IDENTIFIER:
                token_str = parser->lexer->current_value;
                break;
            case TOK_PLUS: token_str = " + "; break;
            case TOK_MINUS: token_str = " - "; break;
            case TOK_MULTIPLY: token_str = " * "; break;
            case TOK_DIVIDE: token_str = " / "; break;
            case TOK_POWER: token_str = " ** "; break;
            case TOK_MOD: token_str = " % "; break;
            case TOK_EQ: token_str = " == "; break;
            case TOK_NE: token_str = " != "; break;
            case TOK_GT: token_str = " > "; break;
            case TOK_GE: token_str = " >= "; break;
            case TOK_LT: token_str = " < "; break;
            case TOK_LE: token_str = " <= "; break;
            case TOK_AND: token_str = " ^ "; break;
            case TOK_OR: token_str = " v "; break;
            case TOK_NOT: token_str = "!"; break;
            case TOK_IMPL: token_str = " → "; break;
            case TOK_IFF: token_str = " ↔ "; break;
            case TOK_XOR: token_str = " ⊽ "; break;
            case TOK_BITAND: token_str = " & "; break;
            case TOK_BITOR: token_str = " | "; break;
            case TOK_BITNOT: token_str = "~"; break;
            case TOK_BITSHL: token_str = " << "; break;
            case TOK_BITSHR: token_str = " >> "; break;
            case TOK_LPAREN: token_str = "("; break;
            case TOK_RPAREN: token_str = ")"; break;
            case TOK_COMMA: token_str = ", "; break;
            default:
                snprintf(temp_str, sizeof(temp_str), "<%d>", parser->lexer->current_type);
                token_str = temp_str;
                break;
        }
        
        /* 检查缓冲区空间 */
        int token_len = strlen(token_str);
        if (pos + token_len >= 1023) {
            parser_set_error(parser, "Expression too long");
            free(expr);
            return NULL;
        }
        
        strcpy(expr + pos, token_str);
        pos += token_len;
        
        lexer_next(parser->lexer);
    }
    
    expr[pos] = '\0';
    return expr;
}

/* 解析if语句 */
ASTNode* parser_parse_if(Parser *parser) {
    /* 已经消费了 'if' token */
    
    /* 解析条件表达式 */
    char *condition = parser_parse_expression_string(parser);
    if (!condition || parser->has_error) {
        free(condition);
        return NULL;
    }
    
    /* 期望冒号 */
    if (!parser_expect(parser, TOK_COLON)) {
        free(condition);
        return NULL;
    }
    
    /* 解析then分支 */
    ASTNode *then_block = parser_parse_block(parser);
    if (!then_block || parser->has_error) {
        free(condition);
        ast_destroy(then_block);
        return NULL;
    }
    
    /* 检查是否有else分支 */
    ASTNode *else_block = NULL;
    if (parser->lexer->current_type == TOK_ELSE) {
        lexer_next(parser->lexer);  /* 消费 'else' */
        
        if (!parser_expect(parser, TOK_COLON)) {
            free(condition);
            ast_destroy(then_block);
            return NULL;
        }
        
        else_block = parser_parse_block(parser);
        if (!else_block || parser->has_error) {
            free(condition);
            ast_destroy(then_block);
            ast_destroy(else_block);
            return NULL;
        }
    }
    
    /* 期望end */
    if (!parser_expect(parser, TOK_END_STMT)) {
        free(condition);
        ast_destroy(then_block);
        ast_destroy(else_block);
        return NULL;
    }
    
    ASTNode *node = ast_create_if(condition, then_block, else_block);
    free(condition);
    return node;
}

/* 解析for语句 */
ASTNode* parser_parse_for(Parser *parser) {
    /* 已经消费了 'for' token */
    
    /* 期望变量名 */
    if (parser->lexer->current_type != TOK_IDENTIFIER) {
        parser_set_error(parser, "Expected variable name in for loop");
        return NULL;
    }
    
    char *var_name = parser_strdup(parser->lexer->current_value);
    lexer_next(parser->lexer);
    
    /* 期望 'in' */
    if (!parser_expect(parser, TOK_IN)) {
        free(var_name);
        return NULL;
    }
    
    /* 期望 'range' */
    if (!parser_expect(parser, TOK_RANGE)) {
        free(var_name);
        return NULL;
    }
    
    /* 期望左括号 */
    if (!parser_expect(parser, TOK_LPAREN)) {
        free(var_name);
        return NULL;
    }
    
    /* 解析起始值 */
    char *start_expr = parser_parse_expression_string(parser);
    if (!start_expr || parser->has_error) {
        free(var_name);
        free(start_expr);
        return NULL;
    }
    
    /* 期望逗号 */
    if (!parser_expect(parser, TOK_COMMA)) {
        free(var_name);
        free(start_expr);
        return NULL;
    }
    
    /* 解析结束值 */
    char *end_expr = parser_parse_expression_string(parser);
    if (!end_expr || parser->has_error) {
        free(var_name);
        free(start_expr);
        free(end_expr);
        return NULL;
    }
    
    /* 检查是否有步长 */
    char *step_expr = NULL;
    if (parser->lexer->current_type == TOK_COMMA) {
        lexer_next(parser->lexer);  /* 消费逗号 */
        step_expr = parser_parse_expression_string(parser);
        if (!step_expr || parser->has_error) {
            free(var_name);
            free(start_expr);
            free(end_expr);
            free(step_expr);
            return NULL;
        }
    }
    
    /* 期望右括号 */
    if (!parser_expect(parser, TOK_RPAREN)) {
        free(var_name);
        free(start_expr);
        free(end_expr);
        free(step_expr);
        return NULL;
    }
    
    /* 期望冒号 */
    if (!parser_expect(parser, TOK_COLON)) {
        free(var_name);
        free(start_expr);
        free(end_expr);
        free(step_expr);
        return NULL;
    }
    
    /* 解析循环体 */
    ASTNode *body = parser_parse_block(parser);
    if (!body || parser->has_error) {
        free(var_name);
        free(start_expr);
        free(end_expr);
        free(step_expr);
        ast_destroy(body);
        return NULL;
    }
    
    /* 期望end */
    if (!parser_expect(parser, TOK_END_STMT)) {
        free(var_name);
        free(start_expr);
        free(end_expr);
        free(step_expr);
        ast_destroy(body);
        return NULL;
    }
    
    ASTNode *node = ast_create_for(var_name, start_expr, end_expr, step_expr, body);
    free(var_name);
    free(start_expr);
    free(end_expr);
    free(step_expr);
    return node;
}

/* 解析while语句 */
ASTNode* parser_parse_while(Parser *parser) {
    /* 已经消费了 'while' token */
    
    /* 解析条件表达式 */
    char *condition = parser_parse_expression_string(parser);
    if (!condition || parser->has_error) {
        free(condition);
        return NULL;
    }
    
    /* 期望冒号 */
    if (!parser_expect(parser, TOK_COLON)) {
        free(condition);
        return NULL;
    }
    
    /* 解析循环体 */
    ASTNode *body = parser_parse_block(parser);
    if (!body || parser->has_error) {
        free(condition);
        ast_destroy(body);
        return NULL;
    }
    
    /* 期望end */
    if (!parser_expect(parser, TOK_END_STMT)) {
        free(condition);
        ast_destroy(body);
        return NULL;
    }
    
    ASTNode *node = ast_create_while(condition, body);
    free(condition);
    return node;
}

/* 解析do-while语句 */
ASTNode* parser_parse_do_while(Parser *parser) {
    /* 已经消费了 'do' token */
    
    /* 期望冒号 */
    if (!parser_expect(parser, TOK_COLON)) {
        return NULL;
    }
    
    /* 解析循环体 */
    ASTNode *body = parser_parse_block(parser);
    if (!body || parser->has_error) {
        ast_destroy(body);
        return NULL;
    }
    
    /* 期望while */
    if (!parser_expect(parser, TOK_WHILE)) {
        ast_destroy(body);
        return NULL;
    }
    
    /* 解析条件表达式 */
    char *condition = parser_parse_expression_string(parser);
    if (!condition || parser->has_error) {
        ast_destroy(body);
        free(condition);
        return NULL;
    }
    
    ASTNode *node = ast_create_do_while(body, condition);
    free(condition);
    return node;
}

/* 解析赋值语句 */
ASTNode* parser_parse_assignment(Parser *parser, const char *var_name) {
    /* 已经消费了变量名和等号 */
    
    /* 解析表达式 */
    char *expr = parser_parse_expression_string(parser);
    if (!expr || parser->has_error) {
        free(expr);
        return NULL;
    }
    
    ASTNode *node = ast_create_assignment(var_name, expr);
    free(expr);
    return node;
}

/* 解析import语句 */
ASTNode* parser_parse_import(Parser *parser) {
    /* 已经消费了 'import' token */
    
    /* 期望包名 */
    if (parser->lexer->current_type != TOK_IDENTIFIER) {
        parser_set_error(parser, "Expected package name after import");
        return NULL;
    }
    
    char *package_name = parser_strdup(parser->lexer->current_value);
    lexer_next(parser->lexer);
    
    ASTNode *node = ast_create_import(package_name);
    free(package_name);
    return node;
}

/* 解析语句块 */
ASTNode* parser_parse_block(Parser *parser) {
    ASTNode *block = ast_create_block();
    if (!block) {
        parser_set_error(parser, "Failed to create block");
        return NULL;
    }
    
    while (parser->lexer->current_type != TOK_END_STMT && 
           parser->lexer->current_type != TOK_ELSE &&
           parser->lexer->current_type != TOK_WHILE &&  /* for do-while */
           parser->lexer->current_type != TOK_END && 
           parser->lexer->current_type != TOK_ERROR) {
        
        ASTNode *stmt = parser_parse_statement(parser);
        if (!stmt || parser->has_error) {
            ast_destroy(block);
            ast_destroy(stmt);
            return NULL;
        }
        
        if (ast_block_add_statement(block, stmt) != 0) {
            parser_set_error(parser, "Failed to add statement to block");
            ast_destroy(block);
            ast_destroy(stmt);
            return NULL;
        }
    }
    
    return block;
}

/* 解析语句 */
ASTNode* parser_parse_statement(Parser *parser) {
    switch (parser->lexer->current_type) {
        case TOK_IF:
            lexer_next(parser->lexer);
            return parser_parse_if(parser);
            
        case TOK_FOR:
            lexer_next(parser->lexer);
            return parser_parse_for(parser);
            
        case TOK_WHILE:
            lexer_next(parser->lexer);
            return parser_parse_while(parser);
            
        case TOK_DO:
            lexer_next(parser->lexer);
            return parser_parse_do_while(parser);
            
        case TOK_IMPORT:
            lexer_next(parser->lexer);
            return parser_parse_import(parser);
            
        case TOK_LET: {
            lexer_next(parser->lexer);  /* 消费 'let' */
            
            if (parser->lexer->current_type != TOK_IDENTIFIER) {
                parser_set_error(parser, "Expected variable name after let");
                return NULL;
            }
            
            char *var_name = parser_strdup(parser->lexer->current_value);
            lexer_next(parser->lexer);
            
            if (!parser_expect(parser, TOK_ASSIGN)) {
                free(var_name);
                return NULL;
            }
            
            ASTNode *node = parser_parse_assignment(parser, var_name);
            free(var_name);
            return node;
        }
        
        case TOK_IDENTIFIER: {
            char *var_name = parser_strdup(parser->lexer->current_value);
            lexer_next(parser->lexer);
            
            if (parser->lexer->current_type == TOK_ASSIGN) {
                lexer_next(parser->lexer);  /* 消费 '=' */
                ASTNode *node = parser_parse_assignment(parser, var_name);
                free(var_name);
                return node;
            } else {
                /* 这是一个表达式，需要回退 */
                /* 构造表达式字符串 */
                char *expr = malloc(1024);
                if (!expr) {
                    parser_set_error(parser, "Memory allocation failed");
                    free(var_name);
                    return NULL;
                }
                
                strcpy(expr, var_name);
                free(var_name);
                
                /* 继续解析表达式的其余部分 */
                char *rest = parser_parse_expression_string(parser);
                if (rest) {
                    strcat(expr, rest);
                    free(rest);
                }
                
                ASTNode *node = ast_create_expression(expr);
                free(expr);
                return node;
            }
        }
        
        default: {
            /* 解析为表达式 */
            char *expr = parser_parse_expression_string(parser);
            if (!expr || parser->has_error) {
                free(expr);
                return NULL;
            }
            
            ASTNode *node = ast_create_expression(expr);
            free(expr);
            return node;
        }
    }
}

/* 检查是否有错误 */
int parser_has_error(Parser *parser) {
    return parser->has_error;
}

/* 获取错误消息 */
const char* parser_get_error(Parser *parser) {
    return parser->error_msg;
}
