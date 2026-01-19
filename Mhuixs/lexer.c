#include "lexer.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* 初始化词法分析器 */
void lexer_init(Lexer *lexer, const char *input, const char *filename, LogexError *error) {
    if (!lexer) return;
    
    lexer->input = input;
    lexer->filename = filename;
    lexer->pos = 0;
    lexer->length = input ? strlen(input) : 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->error = error;
    
    /* 初始化当前token */
    lexer->current_token.type = TOK_ERROR;
    lexer->current_token.value[0] = '\0';
    lexer->current_token.line = 1;
    lexer->current_token.column = 1;
    lexer->current_token.length = 0;
}

void lexer_save_state(const Lexer *lexer, LexerState *state) {
    if (!lexer || !state) return;
    state->pos = lexer->pos;
    state->line = lexer->line;
    state->column = lexer->column;
    state->token = lexer->current_token;
}

void lexer_restore(Lexer *lexer, const LexerState *state) {
    if (!lexer || !state) return;
    lexer->pos = state->pos;
    lexer->line = state->line;
    lexer->column = state->column;
    lexer->current_token = state->token;
}

/* 跳过空白字符（但保留换行符） */
void lexer_skip_whitespace(Lexer *lexer) {
    while (lexer->pos < lexer->length) {
        char c = lexer->input[lexer->pos];
        if (c == ' ' || c == '\t' || c == '\r') {
            lexer->pos++;
            lexer->column++;
        } else {
            break;  /* 遇到换行符或其他字符时停止 */
        }
    }
}

/* 更新位置信息（处理换行） */
static void update_position(Lexer *lexer, char c) {
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    lexer->pos++;
}

/* 设置词法错误 */
static void set_lexer_error(Lexer *lexer, const char *msg) {
    if (lexer->error) {
        error_set(lexer->error, ERR_SYNTAX, msg, lexer->input, 
                  lexer->line, lexer->column, 1);
        if (lexer->filename) {
            strncpy(lexer->error->filename, lexer->filename, sizeof(lexer->error->filename) - 1);
            lexer->error->filename[sizeof(lexer->error->filename) - 1] = '\0';
        }
    }
}

/* 检查关键字 */
static TokenType check_keyword(const char *value) {
    if (strcmp(value, "let") == 0) return TOK_LET;
    if (strcmp(value, "import") == 0) return TOK_IMPORT;
    if (strcmp(value, "if") == 0) return TOK_IF;
    if (strcmp(value, "else") == 0) return TOK_ELSE;
    if (strcmp(value, "for") == 0) return TOK_FOR;
    if (strcmp(value, "while") == 0) return TOK_WHILE;
    if (strcmp(value, "do") == 0) return TOK_DO;
    if (strcmp(value, "end") == 0) return TOK_END_STMT;
    if (strcmp(value, "in") == 0) return TOK_IN;
    if (strcmp(value, "range") == 0) return TOK_RANGE;
    return TOK_IDENTIFIER;
}

/* 读取标识符或关键字 */
static TokenType read_identifier(Lexer *lexer) {
    int start_pos = lexer->pos;
    int start_col = lexer->column;
    int val_pos = 0;
    
    while (lexer->pos < lexer->length && val_pos < BIGNUM_MAX_DIGITS - 1) {
        char c = lexer->input[lexer->pos];
        unsigned char uc = (unsigned char)c;
        
        /* 允许字母、数字、下划线或 UTF-8 多字节字符 */
        if (isalnum(c) || c == '_' || uc >= 0x80) {
            lexer->current_token.value[val_pos++] = c;
            update_position(lexer, c);
        } else {
            break;
        }
    }
    
    lexer->current_token.value[val_pos] = '\0';
    lexer->current_token.line = lexer->line;
    lexer->current_token.column = start_col;
    lexer->current_token.length = lexer->pos - start_pos;
    
    /* 检查是否为关键字 */
    return check_keyword(lexer->current_token.value);
}

/* 读取数字 */
static TokenType read_number(Lexer *lexer) {
    int start_pos = lexer->pos;
    int start_col = lexer->column;
    int val_pos = 0;
    int has_dot = 0;
    
    while (lexer->pos < lexer->length && val_pos < BIGNUM_MAX_DIGITS - 1) {
        char c = lexer->input[lexer->pos];
        if (isdigit(c)) {
            lexer->current_token.value[val_pos++] = c;
            update_position(lexer, c);
        } else if (c == '.' && !has_dot) {
            has_dot = 1;
            lexer->current_token.value[val_pos++] = c;
            update_position(lexer, c);
        } else {
            break;
        }
    }
    
    lexer->current_token.value[val_pos] = '\0';
    lexer->current_token.line = lexer->line;
    lexer->current_token.column = start_col;
    lexer->current_token.length = lexer->pos - start_pos;
    
    return TOK_NUMBER;
}

/* 读取字符串 */
static TokenType read_string(Lexer *lexer) {
    int start_pos = lexer->pos;
    int start_col = lexer->column;
    int val_pos = 0;
    
    update_position(lexer, '"');  /* 跳过开始的引号 */
    
    while (lexer->pos < lexer->length && val_pos < BIGNUM_MAX_DIGITS - 1) {
        char c = lexer->input[lexer->pos];
        if (c == '"') {
            update_position(lexer, c);  /* 跳过结束的引号 */
            break;
        } else if (c == '\\' && lexer->pos + 1 < lexer->length) {
            /* 处理转义字符 */
            update_position(lexer, c);  /* 跳过反斜杠 */
            char next = lexer->input[lexer->pos];
            if (val_pos < BIGNUM_MAX_DIGITS - 1) {
                switch (next) {
                    case 'n': lexer->current_token.value[val_pos++] = '\n'; break;
                    case 't': lexer->current_token.value[val_pos++] = '\t'; break;
                    case '\\': lexer->current_token.value[val_pos++] = '\\'; break;
                    case '"': lexer->current_token.value[val_pos++] = '"'; break;
                    default: 
                        lexer->current_token.value[val_pos++] = '\\';
                        lexer->current_token.value[val_pos++] = next;
                        break;
                }
            }
            update_position(lexer, next);
        } else {
            lexer->current_token.value[val_pos++] = c;
            update_position(lexer, c);
        }
    }
    
    lexer->current_token.value[val_pos] = '\0';
    lexer->current_token.line = lexer->line;
    lexer->current_token.column = start_col;
    lexer->current_token.length = lexer->pos - start_pos;
    
    return TOK_STRING;
}

/* 读取位图字面量 */
static TokenType read_bitmap(Lexer *lexer) {
    int start_pos = lexer->pos;
    int start_col = lexer->column;
    int val_pos = 0;
    
    update_position(lexer, 'B');  /* 跳过 B */
    
    while (lexer->pos < lexer->length && val_pos < BIGNUM_MAX_DIGITS - 1) {
        char c = lexer->input[lexer->pos];
        if (c == '0' || c == '1') {
            lexer->current_token.value[val_pos++] = c;
            update_position(lexer, c);
        } else {
            break;
        }
    }
    
    lexer->current_token.value[val_pos] = '\0';
    lexer->current_token.line = lexer->line;
    lexer->current_token.column = start_col;
    lexer->current_token.length = lexer->pos - start_pos;
    
    return TOK_BITMAP;
}

/* 创建单字符token */
static TokenType make_single_char_token(Lexer *lexer, TokenType type) {
    lexer->current_token.type = type;
    lexer->current_token.value[0] = lexer->input[lexer->pos];
    lexer->current_token.value[1] = '\0';
    lexer->current_token.line = lexer->line;
    lexer->current_token.column = lexer->column;
    lexer->current_token.length = 1;
    
    update_position(lexer, lexer->input[lexer->pos]);
    return type;
}

/* 创建双字符token */
static TokenType make_double_char_token(Lexer *lexer, TokenType type, const char *value) {
    lexer->current_token.type = type;
    strncpy(lexer->current_token.value, value, sizeof(lexer->current_token.value) - 1);
    lexer->current_token.value[sizeof(lexer->current_token.value) - 1] = '\0';
    lexer->current_token.line = lexer->line;
    lexer->current_token.column = lexer->column;
    lexer->current_token.length = 2;
    
    update_position(lexer, lexer->input[lexer->pos]);
    update_position(lexer, lexer->input[lexer->pos]);
    return type;
}

/* 获取下一个词法单元 */
TokenType lexer_next(Lexer *lexer) {
    if (!lexer) return TOK_ERROR;

    lexer_skip_whitespace(lexer);

    if (lexer->pos >= lexer->length) {
        lexer->current_token.type = TOK_END;
        lexer->current_token.value[0] = '\0';
        lexer->current_token.line = lexer->line;
        lexer->current_token.column = lexer->column;
        lexer->current_token.length = 0;
        return TOK_END;
    }
    
    char c = lexer->input[lexer->pos];
    
    /* 检查换行符 */
    if (c == '\n') {
        lexer->current_token.type = TOK_NEWLINE;
        strcpy(lexer->current_token.value, "\\n");
        lexer->current_token.line = lexer->line;
        lexer->current_token.column = lexer->column;
        lexer->current_token.length = 1;
        update_position(lexer, c);
        return TOK_NEWLINE;
    }
    
    /* 检查 UTF-8 多字节字符（布尔运算符） */
    const char *current = &lexer->input[lexer->pos];
    if (strncmp(current, "→", 3) == 0) {
        lexer->current_token.type = TOK_IMPL;
        strcpy(lexer->current_token.value, "→");
        lexer->current_token.line = lexer->line;
        lexer->current_token.column = lexer->column;
        lexer->current_token.length = 3;
        lexer->pos += 3;
        lexer->column++;
        return TOK_IMPL;
    } else if (strncmp(current, "↔", 3) == 0) {
        lexer->current_token.type = TOK_IFF;
        strcpy(lexer->current_token.value, "↔");
        lexer->current_token.line = lexer->line;
        lexer->current_token.column = lexer->column;
        lexer->current_token.length = 3;
        lexer->pos += 3;
        lexer->column++;
        return TOK_IFF;
    } else if (strncmp(current, "⊽", 3) == 0) {
        lexer->current_token.type = TOK_XOR;
        strcpy(lexer->current_token.value, "⊽");
        lexer->current_token.line = lexer->line;
        lexer->current_token.column = lexer->column;
        lexer->current_token.length = 3;
        lexer->pos += 3;
        lexer->column++;
        return TOK_XOR;
    }
    
    /* 特殊处理：B开头可能是位图字面量 */
    if (c == 'B' && lexer->pos + 1 < lexer->length) {
        char next_char = lexer->input[lexer->pos + 1];
        if (next_char == '0' || next_char == '1') {
            return read_bitmap(lexer);
        }
    }
    
    /* 标识符和关键字 */
    unsigned char uc = (unsigned char)c;
    if (isalpha(c) || c == '_' || uc >= 0x80) {
        return read_identifier(lexer);
    }
    
    /* 字符串字面量 */
    if (c == '"') {
        return read_string(lexer);
    }
    
    /* 数字 */
    if (isdigit(c)) {
        return read_number(lexer);
    }
    
    /* 双字符运算符 */
    if (lexer->pos + 1 < lexer->length) {
        char next = lexer->input[lexer->pos + 1];
        switch (c) {
            case '*':
                if (next == '*') return make_double_char_token(lexer, TOK_POWER, "**");
                break;
            case '=':
                if (next == '=') return make_double_char_token(lexer, TOK_EQ, "==");
                break;
            case '!':
                if (next == '=') return make_double_char_token(lexer, TOK_NE, "!=");
                break;
            case '>':
                if (next == '=') return make_double_char_token(lexer, TOK_GE, ">=");
                if (next == '>') return make_double_char_token(lexer, TOK_BITSHR, ">>");
                break;
            case '<':
                if (next == '=') return make_double_char_token(lexer, TOK_LE, "<=");
                if (next == '<') return make_double_char_token(lexer, TOK_BITSHL, "<<");
                break;
        }
    }
    
    /* 单字符运算符和符号 */
    switch (c) {
        case '+': return make_single_char_token(lexer, TOK_PLUS);
        case '-': return make_single_char_token(lexer, TOK_MINUS);
        case '*': return make_single_char_token(lexer, TOK_MULTIPLY);
        case '/': return make_single_char_token(lexer, TOK_DIVIDE);
        case '%': return make_single_char_token(lexer, TOK_MOD);
        case '=': return make_single_char_token(lexer, TOK_ASSIGN);
        case '>': return make_single_char_token(lexer, TOK_GT);
        case '<': return make_single_char_token(lexer, TOK_LT);
        case '^': return make_single_char_token(lexer, TOK_AND);
        case '|': return make_single_char_token(lexer, TOK_OR);
        case '!': return make_single_char_token(lexer, TOK_NOT);
        case '&': return make_single_char_token(lexer, TOK_BITAND);
        case '~': return make_single_char_token(lexer, TOK_BITNOT);
        case '(': return make_single_char_token(lexer, TOK_LPAREN);
        case ')': return make_single_char_token(lexer, TOK_RPAREN);
        case ',': return make_single_char_token(lexer, TOK_COMMA);
        case ':': return make_single_char_token(lexer, TOK_COLON);
        default:
            set_lexer_error(lexer, "unexpected character");
            return TOK_ERROR;
    }
}

/* 查看下一个词法单元（不消费） */
TokenType lexer_peek(Lexer *lexer) {
    /* 保存当前状态 */
    int saved_pos = lexer->pos;
    int saved_line = lexer->line;
    int saved_column = lexer->column;
    Token saved_token = lexer->current_token;
    
    /* 获取下一个token */
    TokenType type = lexer_next(lexer);
    
    /* 恢复状态 */
    lexer->pos = saved_pos;
    lexer->line = saved_line;
    lexer->column = saved_column;
    lexer->current_token = saved_token;
    
    return type;
}

/* 获取当前词法单元 */
const Token* lexer_current(const Lexer *lexer) {
    return lexer ? &lexer->current_token : NULL;
}

/* 检查是否到达输入末尾 */
int lexer_is_end(const Lexer *lexer) {
    return lexer && lexer->current_token.type == TOK_END;
}

/* 获取词法单元类型的字符串表示 */
const char* token_type_name(TokenType type) {
    switch (type) {
        case TOK_NUMBER: return "NUMBER";
        case TOK_STRING: return "STRING";
        case TOK_BITMAP: return "BITMAP";
        case TOK_IDENTIFIER: return "IDENTIFIER";
        case TOK_LET: return "LET";
        case TOK_IMPORT: return "IMPORT";
        case TOK_ASSIGN: return "ASSIGN";
        case TOK_IF: return "IF";
        case TOK_ELSE: return "ELSE";
        case TOK_FOR: return "FOR";
        case TOK_WHILE: return "WHILE";
        case TOK_DO: return "DO";
        case TOK_END_STMT: return "END";
        case TOK_IN: return "IN";
        case TOK_RANGE: return "RANGE";
        case TOK_COLON: return "COLON";
        case TOK_PLUS: return "PLUS";
        case TOK_MINUS: return "MINUS";
        case TOK_MULTIPLY: return "MULTIPLY";
        case TOK_DIVIDE: return "DIVIDE";
        case TOK_POWER: return "POWER";
        case TOK_MOD: return "MOD";
        case TOK_EQ: return "EQ";
        case TOK_NE: return "NE";
        case TOK_GT: return "GT";
        case TOK_GE: return "GE";
        case TOK_LT: return "LT";
        case TOK_LE: return "LE";
        case TOK_AND: return "AND";
        case TOK_OR: return "OR";
        case TOK_NOT: return "NOT";
        case TOK_IMPL: return "IMPL";
        case TOK_IFF: return "IFF";
        case TOK_XOR: return "XOR";
        case TOK_BITAND: return "BITAND";
        case TOK_BITOR: return "BITOR";
        case TOK_BITXOR: return "BITXOR";
        case TOK_BITNOT: return "BITNOT";
        case TOK_BITSHL: return "BITSHL";
        case TOK_BITSHR: return "BITSHR";
        case TOK_LPAREN: return "LPAREN";
        case TOK_RPAREN: return "RPAREN";
        case TOK_COMMA: return "COMMA";
        case TOK_NEWLINE: return "NEWLINE";
        case TOK_END: return "END";
        case TOK_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}
