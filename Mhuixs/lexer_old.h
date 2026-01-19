#ifndef LEXER_H
#define LEXER_H

#include "bignum.h"

/**
 * 统一词法分析器
 * 
 * 负责将表达式字符串分解为词法单元（tokens）
 * 支持数值、运算符、括号等所有类型的符号
 */

/* 词法单元类型（统一的枚举） */
typedef enum {
    /* 数字和布尔值 */
    TOK_NUMBER,      /* 数字（包括小数） */
    TOK_STRING,      /* 字符串字面量 */
    TOK_BITMAP,      /* 位图字面量（B开头） */
    
    /* 标识符和关键字 */
    TOK_IDENTIFIER,  /* 标识符（变量名） */
    TOK_LET,         /* let 关键字 */
    TOK_IMPORT,      /* import 关键字 */
    TOK_ASSIGN,      /* = 赋值运算符 */
    
    /* 控制流关键字 */
    TOK_IF,          /* if 关键字 */
    TOK_ELSE,        /* else 关键字 */
    TOK_FOR,         /* for 关键字 */
    TOK_WHILE,       /* while 关键字 */
    TOK_DO,          /* do 关键字 */
    TOK_END_STMT,    /* end 关键字（语句结束） */
    TOK_IN,          /* in 关键字 */
    TOK_RANGE,       /* range 关键字 */
    TOK_COLON,       /* : 冒号 */
    
    /* 算术运算符 */
    TOK_PLUS,        /* + */
    TOK_MINUS,       /* - */
    TOK_MULTIPLY,    /* * */
    TOK_DIVIDE,      /* / */
    TOK_POWER,       /* ** */
    TOK_MOD,         /* % */
    
    /* 比较运算符 */
    TOK_EQ,          /* == 等于 */
    TOK_NE,          /* != 不等于 */
    TOK_GT,          /* > 大于 */
    TOK_GE,          /* >= 大于等于 */
    TOK_LT,          /* < 小于 */
    TOK_LE,          /* <= 小于等于 */
    
    /* 布尔运算符 */
    TOK_AND,         /* ^ 合取 (在位图上下文中为异或) */
    TOK_OR,          /* v 析取 */
    TOK_NOT,         /* ! 否定 */
    TOK_IMPL,        /* → 蕴含 */
    TOK_IFF,         /* ↔ 等价 */
    TOK_XOR,         /* ⊽ 异或 */
    
    /* 位运算符 */
    TOK_BITAND,      /* & 按位与 */
    TOK_BITOR,       /* | 按位或 */
    TOK_BITNOT,      /* ~ 按位非 */
    TOK_BITSHL,      /* << 左移 */
    TOK_BITSHR,      /* >> 右移 */
    
    /* 括号和分隔符 */
    TOK_LPAREN,      /* ( */
    TOK_RPAREN,      /* ) */
    TOK_COMMA,       /* , */
    
    /* 控制符号 */
    TOK_NEWLINE,     /* 换行符（语句分隔符） */
    TOK_END,         /* 结束 */
    TOK_ERROR        /* 错误 */
} TokenType;

/* 词法分析器状态 */
typedef struct {
    const char *input;                    /* 输入字符串 */
    int pos;                              /* 当前位置 */
    int length;                           /* 输入长度 */
    TokenType current_type;               /* 当前词法单元类型 */
    char current_value[BIGNUM_MAX_DIGITS]; /* 当前词法单元值（用于数字） */
} Lexer;

/**
 * 初始化词法分析器
 * 
 * @param lexer 词法分析器
 * @param input 输入字符串
 */
void lexer_init(Lexer *lexer, const char *input);

/**
 * 获取下一个词法单元
 * 
 * @param lexer 词法分析器
 * @return 词法单元类型
 */
TokenType lexer_next(Lexer *lexer);

/**
 * 跳过空白字符
 * 
 * @param lexer 词法分析器
 */
void lexer_skip_whitespace(Lexer *lexer);

#endif /* LEXER_H */

