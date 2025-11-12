#include "lexer.h"
#include <string.h>
#include <ctype.h>

/* 初始化词法分析器 */
void lexer_init(Lexer *lexer, const char *input) {
    lexer->input = input;
    lexer->pos = 0;
    lexer->length = strlen(input);
    lexer->current_type = TOK_ERROR;
    lexer->current_value[0] = '\0';
}

/* 跳过空白字符 */
void lexer_skip_whitespace(Lexer *lexer) {
    while (lexer->pos < lexer->length && isspace((unsigned char)lexer->input[lexer->pos])) {
        lexer->pos++;
    }
}

/* 获取下一个词法单元 */
TokenType lexer_next(Lexer *lexer) {
    lexer_skip_whitespace(lexer);
    
    if (lexer->pos >= lexer->length) {
        lexer->current_type = TOK_END;
        return TOK_END;
    }
    
    const char *current = &lexer->input[lexer->pos];
    
    /* 检查 UTF-8 多字节字符（布尔运算符） */
    if (strncmp(current, "→", 3) == 0) {
        lexer->pos += 3;
        lexer->current_type = TOK_IMPL;
        return TOK_IMPL;
    } else if (strncmp(current, "↔", 3) == 0) {
        lexer->pos += 3;
        lexer->current_type = TOK_IFF;
        return TOK_IFF;
    } else if (strncmp(current, "⊽", 3) == 0) {
        lexer->pos += 3;
        lexer->current_type = TOK_XOR;
        return TOK_XOR;
    }
    
    char c = lexer->input[lexer->pos];
    
    /* 特殊处理：B开头可能是位图字面量 */
    if (c == 'B' && lexer->pos + 1 < lexer->length) {
        char next_char = lexer->input[lexer->pos + 1];
        if (next_char == '0' || next_char == '1') {
            /* 这是一个位图字面量 */
            lexer->pos++;  /* 跳过 B */
            int val_pos = 0;
            
            while (lexer->pos < lexer->length && val_pos < BIGNUM_MAX_DIGITS - 1) {
                c = lexer->input[lexer->pos];
                if (c == '0' || c == '1') {
                    lexer->current_value[val_pos++] = c;
                    lexer->pos++;
                } else {
                    break;
                }
            }
            
            lexer->current_value[val_pos] = '\0';
            lexer->current_type = TOK_BITMAP;
            return TOK_BITMAP;
        }
    }
    
    /* 标识符和关键字（字母、下划线或 UTF-8 字符开头） */
    unsigned char uc = (unsigned char)c;
    if (isalpha(c) || c == '_' || uc >= 0x80) {
        int val_pos = 0;
        
        while (lexer->pos < lexer->length && val_pos < BIGNUM_MAX_DIGITS - 1) {
            c = lexer->input[lexer->pos];
            uc = (unsigned char)c;
            
            /* 允许字母、数字、下划线或 UTF-8 多字节字符 */
            if (isalnum(c) || c == '_' || uc >= 0x80) {
                lexer->current_value[val_pos++] = c;
                lexer->pos++;
            } else {
                break;
            }
        }
        
        lexer->current_value[val_pos] = '\0';
        
        /* 检查是否为关键字或保留运算符 */
        if (strcmp(lexer->current_value, "let") == 0) {
            lexer->current_type = TOK_LET;
            return TOK_LET;
        }
        
        if (strcmp(lexer->current_value, "import") == 0) {
            lexer->current_type = TOK_IMPORT;
            return TOK_IMPORT;
        }
        
        /* 控制流关键字 */
        if (strcmp(lexer->current_value, "if") == 0) {
            lexer->current_type = TOK_IF;
            return TOK_IF;
        }
        
        if (strcmp(lexer->current_value, "else") == 0) {
            lexer->current_type = TOK_ELSE;
            return TOK_ELSE;
        }
        
        if (strcmp(lexer->current_value, "for") == 0) {
            lexer->current_type = TOK_FOR;
            return TOK_FOR;
        }
        
        if (strcmp(lexer->current_value, "while") == 0) {
            lexer->current_type = TOK_WHILE;
            return TOK_WHILE;
        }
        
        if (strcmp(lexer->current_value, "do") == 0) {
            lexer->current_type = TOK_DO;
            return TOK_DO;
        }
        
        if (strcmp(lexer->current_value, "end") == 0) {
            lexer->current_type = TOK_END_STMT;
            return TOK_END_STMT;
        }
        
        if (strcmp(lexer->current_value, "in") == 0) {
            lexer->current_type = TOK_IN;
            return TOK_IN;
        }
        
        if (strcmp(lexer->current_value, "range") == 0) {
            lexer->current_type = TOK_RANGE;
            return TOK_RANGE;
        }
        
        /* 检查单字符保留运算符 */
        if (strcmp(lexer->current_value, "v") == 0) {
            lexer->current_type = TOK_OR;
            return TOK_OR;
        }
        
        /* 否则为标识符 */
        lexer->current_type = TOK_IDENTIFIER;
        return TOK_IDENTIFIER;
    }
    
    /* 字符串字面量（双引号） */
    if (c == '"') {
        int val_pos = 0;
        lexer->pos++;  /* 跳过开始的引号 */
        
        while (lexer->pos < lexer->length) {
            c = lexer->input[lexer->pos];
            if (c == '"') {
                lexer->pos++;  /* 跳过结束的引号 */
                break;
            } else if (c == '\\' && lexer->pos + 1 < lexer->length) {
                /* 处理转义字符 */
                lexer->pos++;
                char next = lexer->input[lexer->pos];
                if (val_pos < BIGNUM_MAX_DIGITS - 1) {
                    switch (next) {
                        case 'n': lexer->current_value[val_pos++] = '\n'; break;
                        case 't': lexer->current_value[val_pos++] = '\t'; break;
                        case '\\': lexer->current_value[val_pos++] = '\\'; break;
                        case '"': lexer->current_value[val_pos++] = '"'; break;
                        default: lexer->current_value[val_pos++] = next; break;
                    }
                }
                lexer->pos++;
            } else {
                if (val_pos < BIGNUM_MAX_DIGITS - 1) {
                    lexer->current_value[val_pos++] = c;
                }
                lexer->pos++;
            }
        }
        
        lexer->current_value[val_pos] = '\0';
        lexer->current_type = TOK_STRING;
        return TOK_STRING;
    }
    
    /* 数字（包括小数点开头） */
    if (isdigit(c) || c == '.') {
        int val_pos = 0;
        int has_dot = 0;
        
        while (lexer->pos < lexer->length) {
            c = lexer->input[lexer->pos];
            if (isdigit(c)) {
                if (val_pos < BIGNUM_MAX_DIGITS - 1) {
                    lexer->current_value[val_pos++] = c;
                }
                lexer->pos++;
            } else if (c == '.' && !has_dot) {
                if (val_pos < BIGNUM_MAX_DIGITS - 1) {
                    lexer->current_value[val_pos++] = c;
                }
                has_dot = 1;
                lexer->pos++;
            } else {
                break;
            }
        }
        
        lexer->current_value[val_pos] = '\0';
        lexer->current_type = TOK_NUMBER;
        return TOK_NUMBER;
    }
    
    /* 多字符运算符检查 */
    if (lexer->pos + 1 < lexer->length) {
        char next = lexer->input[lexer->pos + 1];
        
        /* ** 幂运算 */
        if (c == '*' && next == '*') {
            lexer->pos += 2;
            lexer->current_type = TOK_POWER;
            return TOK_POWER;
        }
        /* == 等于 */
        if (c == '=' && next == '=') {
            lexer->pos += 2;
            lexer->current_type = TOK_EQ;
            return TOK_EQ;
        }
        /* != 不等于 */
        if (c == '!' && next == '=') {
            lexer->pos += 2;
            lexer->current_type = TOK_NE;
            return TOK_NE;
        }
        /* >= 大于等于 */
        if (c == '>' && next == '=') {
            lexer->pos += 2;
            lexer->current_type = TOK_GE;
            return TOK_GE;
        }
        /* <= 小于等于 */
        if (c == '<' && next == '=') {
            lexer->pos += 2;
            lexer->current_type = TOK_LE;
            return TOK_LE;
        }
        /* << 左移 */
        if (c == '<' && next == '<') {
            lexer->pos += 2;
            lexer->current_type = TOK_BITSHL;
            return TOK_BITSHL;
        }
        /* >> 右移 */
        if (c == '>' && next == '>') {
            lexer->pos += 2;
            lexer->current_type = TOK_BITSHR;
            return TOK_BITSHR;
        }
    }
    
    /* 单字符运算符 */
    lexer->pos++;
    switch (c) {
        case '+': lexer->current_type = TOK_PLUS; return TOK_PLUS;
        case '-': lexer->current_type = TOK_MINUS; return TOK_MINUS;
        case '*': lexer->current_type = TOK_MULTIPLY; return TOK_MULTIPLY;
        case '/': lexer->current_type = TOK_DIVIDE; return TOK_DIVIDE;
        case '%': lexer->current_type = TOK_MOD; return TOK_MOD;
        case '^': lexer->current_type = TOK_AND; return TOK_AND;  /* 在context中会根据操作数类型区分布尔AND或位异或 */
        case '!': lexer->current_type = TOK_NOT; return TOK_NOT;
        case '>': lexer->current_type = TOK_GT; return TOK_GT;
        case '<': lexer->current_type = TOK_LT; return TOK_LT;
        case '=': lexer->current_type = TOK_ASSIGN; return TOK_ASSIGN;
        case '(': lexer->current_type = TOK_LPAREN; return TOK_LPAREN;
        case ')': lexer->current_type = TOK_RPAREN; return TOK_RPAREN;
        case ',': lexer->current_type = TOK_COMMA; return TOK_COMMA;
        case ':': lexer->current_type = TOK_COLON; return TOK_COLON;
        case '&': lexer->current_type = TOK_BITAND; return TOK_BITAND;
        case '|': lexer->current_type = TOK_BITOR; return TOK_BITOR;
        case '~': lexer->current_type = TOK_BITNOT; return TOK_BITNOT;
    }
    
    lexer->current_type = TOK_ERROR;
    return TOK_ERROR;
}

