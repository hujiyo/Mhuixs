#ifndef LEXER_H
#define LEXER_H

#include "bignum.h"
#include "error.h"

/**
 * 增强版词法分析器
 * 
 * 特性：
 * - 精确的位置追踪（行号、列号）
 * - 统一的错误处理
 * - 更清晰的接口设计
 * - 支持源文件名
 */

/* 词法单元类型（与原版兼容） */
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
    TOK_AND,         /* ^ 与 */
    TOK_OR,          /* | 或 */
    TOK_NOT,         /* ! 非 */
    TOK_IMPL,        /* → 蕴含 */
    TOK_IFF,         /* ↔ 当且仅当 */
    TOK_XOR,         /* ⊽ 异或 */
    
    /* 位运算符 */
    TOK_BITAND,      /* & 按位与 */
    TOK_BITOR,       /* | 按位或 */
    TOK_BITXOR,      /* ^ 按位异或 */
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

/* 增强的词法单元 */
typedef struct {
    TokenType type;                      /* 词法单元类型 */
    char value[BIGNUM_MAX_DIGITS];       /* 词法单元值 */
    int line;                            /* 行号（1-based） */
    int column;                          /* 列号（1-based） */
    int length;                          /* 词法单元长度 */
} Token;

/* 增强的词法分析器 */
typedef struct {
    const char *input;                   /* 输入字符串 */
    const char *filename;                /* 文件名（可选） */
    int pos;                             /* 当前位置 */
    int length;                          /* 输入长度 */
    int line;                            /* 当前行号（1-based） */
    int column;                          /* 当前列号（1-based） */
    Token current_token;                 /* 当前词法单元 */
    LogexError *error;                   /* 错误处理器 */
} Lexer;

/**
 * 初始化词法分析器
 * 
 * @param lexer 词法分析器
 * @param input 输入字符串
 * @param filename 文件名（可选，可为 NULL）
 * @param error 错误处理器
 */
void lexer_init(Lexer *lexer, const char *input, const char *filename, LogexError *error);

/**
 * 获取下一个词法单元
 * 
 * @param lexer 词法分析器
 * @return 词法单元类型，TOK_ERROR 表示出错
 */
TokenType lexer_next(Lexer *lexer);

/**
 * 查看下一个词法单元（不消费）
 * 
 * @param lexer 词法分析器
 * @return 词法单元类型
 */
TokenType lexer_peek(Lexer *lexer);

/**
 * 获取当前词法单元
 * 
 * @param lexer 词法分析器
 * @return 当前词法单元指针
 */
const Token* lexer_current(const Lexer *lexer);

/**
 * 检查是否到达输入末尾
 * 
 * @param lexer 词法分析器
 * @return 1 表示到达末尾，0 表示未到达
 */
int lexer_is_end(const Lexer *lexer);

/**
 * 获取词法单元类型的字符串表示
 * 
 * @param type 词法单元类型
 * @return 类型名称字符串
 */
const char* token_type_name(TokenType type);

/* 词法分析器状态快照（用于回退） */
typedef struct {
    int pos;
    int line;
    int column;
    Token token;
} LexerState;

/**
 * 保存当前词法分析器状态
 */
void lexer_save_state(const Lexer *lexer, LexerState *state);

/**
 * 恢复之前保存的词法分析器状态
 */
void lexer_restore(Lexer *lexer, const LexerState *state);

/* 内联辅助函数，便于访问当前Token信息 */
static inline TokenType lexer_token_type(const Lexer *lexer) {
    return lexer ? lexer->current_token.type : TOK_ERROR;
}

static inline const char* lexer_token_value(const Lexer *lexer) {
    return lexer ? lexer->current_token.value : NULL;
}

static inline int lexer_token_line(const Lexer *lexer) {
    return lexer ? lexer->current_token.line : 0;
}

static inline int lexer_token_column(const Lexer *lexer) {
    return lexer ? lexer->current_token.column : 0;
}

static inline int lexer_token_length(const Lexer *lexer) {
    return lexer ? lexer->current_token.length : 0;
}

/* 兼容旧接口的辅助函数 */
static inline TokenType lexer_current_type(const Lexer *lexer) {
    return lexer_token_type(lexer);
}

static inline const char* lexer_current_value(const Lexer *lexer) {
    return lexer_token_value(lexer);
}

static inline int lexer_current_line(const Lexer *lexer) {
    return lexer_token_line(lexer);
}

static inline int lexer_current_column(const Lexer *lexer) {
    return lexer_token_column(lexer);
}

static inline int lexer_current_length(const Lexer *lexer) {
    return lexer_token_length(lexer);
}

/**
 * 跳过空白字符（但保留换行符）
 * 
 * @param lexer 词法分析器
 */
void lexer_skip_whitespace(Lexer *lexer);

#endif /* LEXER_H */
