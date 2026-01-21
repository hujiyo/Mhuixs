#include "parser.h"
#include "lexer.h"
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
    if (lexer_current_type(parser->lexer) != expected) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Expected token type %d, got %d", expected, lexer_current_type(parser->lexer));
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
    
    while (lexer_current_type(parser->lexer) != TOK_END && 
           lexer_current_type(parser->lexer) != TOK_ERROR &&
           lexer_current_type(parser->lexer) != TOK_COLON &&
           lexer_current_type(parser->lexer) != TOK_END_STMT &&
           lexer_current_type(parser->lexer) != TOK_ELSE &&
           lexer_current_type(parser->lexer) != TOK_COMMA &&
           lexer_current_type(parser->lexer) != TOK_NEWLINE &&
           (lexer_current_type(parser->lexer) != TOK_RPAREN || paren_count > 0)) {
        
        /* 处理括号平衡 */
        if (lexer_current_type(parser->lexer) == TOK_LPAREN) {
            paren_count++;
        } else if (lexer_current_type(parser->lexer) == TOK_RPAREN) {
            paren_count--;
        }
        
        /* 添加token到表达式字符串 */
        const char *token_str = NULL;
        char temp_str[64];
        
        switch (lexer_current_type(parser->lexer)) {
            case TOK_NUMBER:
            case TOK_STRING:
            case TOK_BITMAP:
            case TOK_IDENTIFIER:
                token_str = lexer_current_value(parser->lexer);
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
                snprintf(temp_str, sizeof(temp_str), "<%d>", lexer_current_type(parser->lexer));
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
    if (lexer_current_type(parser->lexer) == TOK_ELSE) {
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
    if (lexer_current_type(parser->lexer) != TOK_IDENTIFIER) {
        parser_set_error(parser, "Expected variable name in for loop");
        return NULL;
    }
    
    char *var_name = parser_strdup(lexer_current_value(parser->lexer));
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
    if (lexer_current_type(parser->lexer) == TOK_COMMA) {
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
ASTNode* parser_parse_assignment(Parser *parser, const char *var_name, int is_static) {
    /* 已经消费了变量名和等号 */
    
    /* 解析表达式 */
    char *expr = parser_parse_expression_string(parser);
    if (!expr || parser->has_error) {
        free(expr);
        return NULL;
    }
    
    ASTNode *node = ast_create_assignment(var_name, expr, is_static);
    free(expr);
    return node;
}

/* 解析import语句 */
ASTNode* parser_parse_import(Parser *parser) {
    /* 已经消费了 'import' token */
    
    /* 期望包名 */
    if (lexer_current_type(parser->lexer) != TOK_IDENTIFIER) {
        parser_set_error(parser, "Expected package name after import");
        return NULL;
    }
    
    char *package_name = parser_strdup(lexer_current_value(parser->lexer));
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
    
    while (lexer_current_type(parser->lexer) != TOK_END_STMT && 
           lexer_current_type(parser->lexer) != TOK_ELSE &&
           lexer_current_type(parser->lexer) != TOK_WHILE &&  /* for do-while */
           lexer_current_type(parser->lexer) != TOK_END && 
           lexer_current_type(parser->lexer) != TOK_ERROR) {
        
        /* 跳过换行符 */
        if (lexer_current_type(parser->lexer) == TOK_NEWLINE) {
            lexer_next(parser->lexer);
            continue;
        }
        
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
    switch (lexer_current_type(parser->lexer)) {
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
        
        /* NAQL 语句 */
        case TOK_HOOK:
            return parser_parse_hook(parser);
            
        case TOK_FIELD:
            return parser_parse_field(parser);
            
        case TOK_ADD:
        case TOK_GET:
        case TOK_SET:
        case TOK_DEL:
            /* 需要根据当前上下文判断是 TABLE/KVALOT/LIST/BITMAP 操作 */
            /* 这里简化处理，默认为 TABLE 操作 */
            return parser_parse_table_op(parser);
            
        case TOK_LPUSH:
        case TOK_RPUSH:
        case TOK_LPOP:
        case TOK_RPOP:
            return parser_parse_list_op(parser);
            
        case TOK_EXISTS:
            return parser_parse_kvalot_op(parser);
            
        case TOK_COUNT:
        case TOK_FLIP:
            return parser_parse_bitmap_op(parser);
            
        case TOK_STATIC: {
            /* static let var = expr; 持久化变量 */
            lexer_next(parser->lexer);  /* 消费 'static' */
            
            if (lexer_current_type(parser->lexer) != TOK_LET) {
                parser_set_error(parser, "Expected 'let' after 'static'");
                return NULL;
            }
            lexer_next(parser->lexer);  /* 消费 'let' */
            
            if (lexer_current_type(parser->lexer) != TOK_IDENTIFIER) {
                parser_set_error(parser, "Expected variable name after static let");
                return NULL;
            }
            
            char *var_name = parser_strdup(lexer_current_value(parser->lexer));
            lexer_next(parser->lexer);
            
            if (!parser_expect(parser, TOK_ASSIGN)) {
                free(var_name);
                return NULL;
            }
            
            ASTNode *node = parser_parse_assignment(parser, var_name, 1);  /* is_static = 1 */
            free(var_name);
            return node;
        }
        
        case TOK_LET: {
            /* let var = expr; 临时变量（栈上） */
            lexer_next(parser->lexer);  /* 消费 'let' */
            
            if (lexer_current_type(parser->lexer) != TOK_IDENTIFIER) {
                parser_set_error(parser, "Expected variable name after let");
                return NULL;
            }
            
            char *var_name = parser_strdup(lexer_current_value(parser->lexer));
            lexer_next(parser->lexer);
            
            if (!parser_expect(parser, TOK_ASSIGN)) {
                free(var_name);
                return NULL;
            }
            
            ASTNode *node = parser_parse_assignment(parser, var_name, 0);  /* is_static = 0 */
            free(var_name);
            return node;
        }
        
        case TOK_IDENTIFIER: {
            char *var_name = parser_strdup(lexer_current_value(parser->lexer));
            lexer_next(parser->lexer);
            
            if (lexer_current_type(parser->lexer) == TOK_ASSIGN) {
                lexer_next(parser->lexer);  /* 消费 '=' */
                ASTNode *node = parser_parse_assignment(parser, var_name, 0);  /* 默认非持久化 */
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

/* ==================== NAQL 解析函数 ==================== */

/* 解析 HOOK 语句 */
ASTNode* parser_parse_hook(Parser *parser) {
    /* HOOK TABLE users; */
    /* HOOK users; */
    /* HOOK DEL users; */
    
    if (!parser_expect(parser, TOK_HOOK)) {
        return NULL;
    }
    
    TokenType current = lexer_current_type(parser->lexer);
    char *operation = NULL;
    char *obj_type = NULL;
    char *obj_name = NULL;
    
    /* 检查是否是对象类型 */
    if (current == TOK_TABLE || current == TOK_KVALOT || 
        current == TOK_LIST || current == TOK_BITMAP || current == TOK_STREAM) {
        operation = parser_strdup("CREATE");
        obj_type = parser_strdup(lexer_current_value(parser->lexer));
        lexer_next(parser->lexer);
        
        if (lexer_current_type(parser->lexer) != TOK_IDENTIFIER) {
            parser_set_error(parser, "Expected object name after object type");
            free(operation);
            free(obj_type);
            return NULL;
        }
        obj_name = parser_strdup(lexer_current_value(parser->lexer));
        lexer_next(parser->lexer);
    }
    /* 检查是否是 DEL 操作 */
    else if (current == TOK_DEL) {
        operation = parser_strdup("DELETE");
        lexer_next(parser->lexer);
        
        if (lexer_current_type(parser->lexer) != TOK_IDENTIFIER) {
            parser_set_error(parser, "Expected object name after DEL");
            free(operation);
            return NULL;
        }
        obj_name = parser_strdup(lexer_current_value(parser->lexer));
        lexer_next(parser->lexer);
    }
    /* 检查是否是 CLEAR 操作 */
    else if (current == TOK_CLEAR) {
        operation = parser_strdup("CLEAR");
        lexer_next(parser->lexer);
        
        if (lexer_current_type(parser->lexer) != TOK_IDENTIFIER) {
            parser_set_error(parser, "Expected object name after CLEAR");
            free(operation);
            return NULL;
        }
        obj_name = parser_strdup(lexer_current_value(parser->lexer));
        lexer_next(parser->lexer);
    }
    /* 否则是 SWITCH 操作 */
    else if (current == TOK_IDENTIFIER) {
        operation = parser_strdup("SWITCH");
        obj_name = parser_strdup(lexer_current_value(parser->lexer));
        lexer_next(parser->lexer);
    }
    else {
        parser_set_error(parser, "Invalid HOOK syntax");
        return NULL;
    }
    
    /* 期望分号 */
    if (!parser_expect(parser, TOK_SEMICOLON)) {
        free(operation);
        free(obj_type);
        free(obj_name);
        return NULL;
    }
    
    ASTNode *node = ast_create_hook(operation, obj_type, obj_name);
    free(operation);
    free(obj_type);
    free(obj_name);
    
    return node;
}

/* 解析 FIELD 语句 */
ASTNode* parser_parse_field(Parser *parser) {
    /* FIELD ADD id i4 PKEY; */
    /* FIELD DEL 0; */
    /* FIELD SWAP 0 1; */
    
    if (!parser_expect(parser, TOK_FIELD)) {
        return NULL;
    }
    
    TokenType op_type = lexer_current_type(parser->lexer);
    
    if (op_type == TOK_ADD) {
        lexer_next(parser->lexer);
        
        /* 字段名 */
        if (lexer_current_type(parser->lexer) != TOK_IDENTIFIER) {
            parser_set_error(parser, "Expected field name");
            return NULL;
        }
        char *field_name = parser_strdup(lexer_current_value(parser->lexer));
        lexer_next(parser->lexer);
        
        /* 数据类型 */
        char *data_type = parser_strdup(lexer_current_value(parser->lexer));
        lexer_next(parser->lexer);
        
        /* 约束（可选） */
        char *constraint = NULL;
        if (lexer_current_type(parser->lexer) != TOK_SEMICOLON) {
            constraint = parser_strdup(lexer_current_value(parser->lexer));
            lexer_next(parser->lexer);
        }
        
        if (!parser_expect(parser, TOK_SEMICOLON)) {
            free(field_name);
            free(data_type);
            free(constraint);
            return NULL;
        }
        
        ASTNode *node = ast_create_field_add(field_name, data_type, constraint);
        free(field_name);
        free(data_type);
        free(constraint);
        return node;
    }
    else if (op_type == TOK_DEL) {
        lexer_next(parser->lexer);
        
        if (lexer_current_type(parser->lexer) != TOK_NUMBER) {
            parser_set_error(parser, "Expected field index");
            return NULL;
        }
        int index = atoi(lexer_current_value(parser->lexer));
        lexer_next(parser->lexer);
        
        if (!parser_expect(parser, TOK_SEMICOLON)) {
            return NULL;
        }
        
        return ast_create_field_del(index);
    }
    else if (op_type == TOK_SWAP) {
        lexer_next(parser->lexer);
        
        if (lexer_current_type(parser->lexer) != TOK_NUMBER) {
            parser_set_error(parser, "Expected first field index");
            return NULL;
        }
        int index1 = atoi(lexer_current_value(parser->lexer));
        lexer_next(parser->lexer);
        
        if (lexer_current_type(parser->lexer) != TOK_NUMBER) {
            parser_set_error(parser, "Expected second field index");
            return NULL;
        }
        int index2 = atoi(lexer_current_value(parser->lexer));
        lexer_next(parser->lexer);
        
        if (!parser_expect(parser, TOK_SEMICOLON)) {
            return NULL;
        }
        
        return ast_create_field_swap(index1, index2);
    }
    else {
        parser_set_error(parser, "Invalid FIELD operation");
        return NULL;
    }
}


/* 解析 TABLE 操作 */
ASTNode* parser_parse_table_op(Parser *parser) {
    /* ADD 1 'Alice' 25; */
    /* GET 0; */
    /* SET 0 1 'Bob'; */
    /* DEL 0; */
    
    TokenType op_type = lexer_current_type(parser->lexer);
    
    if (op_type == TOK_ADD) {
        lexer_next(parser->lexer);
        
        /* 收集所有值字符串 */
        char **value_strings = malloc(sizeof(char*) * 32);
        int count = 0;
        
        while (lexer_current_type(parser->lexer) != TOK_SEMICOLON && 
               lexer_current_type(parser->lexer) != TOK_END) {
            value_strings[count] = parser_strdup(lexer_current_value(parser->lexer));
            if (!value_strings[count]) {
                for (int i = 0; i < count; i++) {
                    free(value_strings[i]);
                }
                free(value_strings);
                return NULL;
            }
            lexer_next(parser->lexer);
            count++;
        }
        
        if (!parser_expect(parser, TOK_SEMICOLON)) {
            for (int i = 0; i < count; i++) {
                free(value_strings[i]);
            }
            free(value_strings);
            return NULL;
        }
        
        return ast_create_table_add(value_strings, count);
    }
    else if (op_type == TOK_GET) {
        lexer_next(parser->lexer);
        
        if (lexer_current_type(parser->lexer) == TOK_WHERE) {
            lexer_next(parser->lexer);
            char *condition = parser_parse_expression_string(parser);
            if (!parser_expect(parser, TOK_SEMICOLON)) {
                free(condition);
                return NULL;
            }
            ASTNode *node = ast_create_table_where(condition);
            free(condition);
            return node;
        }
        else if (lexer_current_type(parser->lexer) == TOK_NUMBER) {
            int index = atoi(lexer_current_value(parser->lexer));
            lexer_next(parser->lexer);
            if (!parser_expect(parser, TOK_SEMICOLON)) {
                return NULL;
            }
            return ast_create_table_get(index);
        }
        else {
            parser_set_error(parser, "Expected index or WHERE after GET");
            return NULL;
        }
    }
    else if (op_type == TOK_SET) {
        lexer_next(parser->lexer);
        
        if (lexer_current_type(parser->lexer) != TOK_NUMBER) {
            parser_set_error(parser, "Expected row index");
            return NULL;
        }
        int row_index = atoi(lexer_current_value(parser->lexer));
        lexer_next(parser->lexer);
        
        if (lexer_current_type(parser->lexer) != TOK_NUMBER) {
            parser_set_error(parser, "Expected column index");
            return NULL;
        }
        int col_index = atoi(lexer_current_value(parser->lexer));
        lexer_next(parser->lexer);
        
        char *value_string = parser_strdup(lexer_current_value(parser->lexer));
        if (!value_string) {
            return NULL;
        }
        lexer_next(parser->lexer);
        
        if (!parser_expect(parser, TOK_SEMICOLON)) {
            free(value_string);
            return NULL;
        }
        
        return ast_create_table_set(row_index, col_index, value_string);
    }
    else if (op_type == TOK_DEL) {
        lexer_next(parser->lexer);
        
        if (lexer_current_type(parser->lexer) != TOK_NUMBER) {
            parser_set_error(parser, "Expected row index");
            return NULL;
        }
        int index = atoi(lexer_current_value(parser->lexer));
        lexer_next(parser->lexer);
        
        if (!parser_expect(parser, TOK_SEMICOLON)) {
            return NULL;
        }
        
        return ast_create_table_del(index);
    }
    else {
        parser_set_error(parser, "Invalid TABLE operation");
        return NULL;
    }
}

/* 解析 KVALOT 操作 */
ASTNode* parser_parse_kvalot_op(Parser *parser) {
    /* SET key value; */
    /* GET key; */
    /* DEL key; */
    /* EXISTS key; */
    
    TokenType op_type = lexer_current_type(parser->lexer);
    lexer_next(parser->lexer);
    
    if (lexer_current_type(parser->lexer) != TOK_IDENTIFIER && 
        lexer_current_type(parser->lexer) != TOK_STRING) {
        parser_set_error(parser, "Expected key");
        return NULL;
    }
    
    char *key = parser_strdup(lexer_current_value(parser->lexer));
    lexer_next(parser->lexer);
    
    if (op_type == TOK_SET) {
        char *value_string = parser_strdup(lexer_current_value(parser->lexer));
        if (!value_string) {
            free(key);
            return NULL;
        }
        lexer_next(parser->lexer);
        
        if (!parser_expect(parser, TOK_SEMICOLON)) {
            free(key);
            free(value_string);
            return NULL;
        }
        
        ASTNode *node = ast_create_kvalot_set(key, value_string);
        free(key);
        free(value_string);
        return node;
    }
    else if (op_type == TOK_GET) {
        if (!parser_expect(parser, TOK_SEMICOLON)) {
            free(key);
            return NULL;
        }
        ASTNode *node = ast_create_kvalot_get(key);
        free(key);
        return node;
    }
    else if (op_type == TOK_DEL) {
        if (!parser_expect(parser, TOK_SEMICOLON)) {
            free(key);
            return NULL;
        }
        ASTNode *node = ast_create_kvalot_del(key);
        free(key);
        return node;
    }
    else if (op_type == TOK_EXISTS) {
        if (!parser_expect(parser, TOK_SEMICOLON)) {
            free(key);
            return NULL;
        }
        ASTNode *node = ast_create_kvalot_exists(key);
        free(key);
        return node;
    }
    else {
        free(key);
        parser_set_error(parser, "Invalid KVALOT operation");
        return NULL;
    }
}

/* 解析 LIST 操作 */
ASTNode* parser_parse_list_op(Parser *parser) {
    /* LPUSH value; */
    /* RPUSH value; */
    /* LPOP; */
    /* RPOP; */
    /* GET index; */
    
    TokenType op_type = lexer_current_type(parser->lexer);
    
    if (op_type == TOK_LPUSH || op_type == TOK_RPUSH) {
        char *operation = (op_type == TOK_LPUSH) ? "LPUSH" : "RPUSH";
        lexer_next(parser->lexer);
        
        char *value_string = parser_strdup(lexer_current_value(parser->lexer));
        if (!value_string) {
            return NULL;
        }
        lexer_next(parser->lexer);
        
        if (!parser_expect(parser, TOK_SEMICOLON)) {
            free(value_string);
            return NULL;
        }
        
        return ast_create_list_push(operation, value_string);
    }
    else if (op_type == TOK_LPOP || op_type == TOK_RPOP) {
        char *operation = (op_type == TOK_LPOP) ? "LPOP" : "RPOP";
        lexer_next(parser->lexer);
        
        if (!parser_expect(parser, TOK_SEMICOLON)) {
            return NULL;
        }
        
        return ast_create_list_pop(operation);
    }
    else if (op_type == TOK_GET) {
        lexer_next(parser->lexer);
        
        if (lexer_current_type(parser->lexer) != TOK_NUMBER) {
            parser_set_error(parser, "Expected index");
            return NULL;
        }
        int index = atoi(lexer_current_value(parser->lexer));
        lexer_next(parser->lexer);
        
        if (!parser_expect(parser, TOK_SEMICOLON)) {
            return NULL;
        }
        
        return ast_create_list_get(index);
    }
    else {
        parser_set_error(parser, "Invalid LIST operation");
        return NULL;
    }
}

/* 解析 BITMAP 操作 */
ASTNode* parser_parse_bitmap_op(Parser *parser) {
    /* SET offset value; */
    /* GET offset; */
    /* COUNT; */
    /* FLIP offset; */
    
    TokenType op_type = lexer_current_type(parser->lexer);
    
    if (op_type == TOK_SET) {
        lexer_next(parser->lexer);
        
        if (lexer_current_type(parser->lexer) != TOK_NUMBER) {
            parser_set_error(parser, "Expected offset");
            return NULL;
        }
        int offset = atoi(lexer_current_value(parser->lexer));
        lexer_next(parser->lexer);
        
        if (lexer_current_type(parser->lexer) != TOK_NUMBER) {
            parser_set_error(parser, "Expected value (0 or 1)");
            return NULL;
        }
        int value = atoi(lexer_current_value(parser->lexer));
        lexer_next(parser->lexer);
        
        if (!parser_expect(parser, TOK_SEMICOLON)) {
            return NULL;
        }
        
        return ast_create_bitmap_set(offset, value);
    }
    else if (op_type == TOK_GET) {
        lexer_next(parser->lexer);
        
        if (lexer_current_type(parser->lexer) != TOK_NUMBER) {
            parser_set_error(parser, "Expected offset");
            return NULL;
        }
        int offset = atoi(lexer_current_value(parser->lexer));
        lexer_next(parser->lexer);
        
        if (!parser_expect(parser, TOK_SEMICOLON)) {
            return NULL;
        }
        
        return ast_create_bitmap_get(offset);
    }
    else if (op_type == TOK_COUNT) {
        lexer_next(parser->lexer);
        
        if (!parser_expect(parser, TOK_SEMICOLON)) {
            return NULL;
        }
        
        return ast_create_bitmap_count();
    }
    else if (op_type == TOK_FLIP) {
        lexer_next(parser->lexer);
        
        if (lexer_current_type(parser->lexer) != TOK_NUMBER) {
            parser_set_error(parser, "Expected offset");
            return NULL;
        }
        int offset = atoi(lexer_current_value(parser->lexer));
        lexer_next(parser->lexer);
        
        if (!parser_expect(parser, TOK_SEMICOLON)) {
            return NULL;
        }
        
        return ast_create_bitmap_flip(offset);
    }
    else {
        parser_set_error(parser, "Invalid BITMAP operation");
        return NULL;
    }
}
