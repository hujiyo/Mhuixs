#ifndef ERROR_H
#define ERROR_H

#include <stddef.h>

/**
 * Logex 统一错误处理系统
 * 
 * 提供清晰的错误类型、错误上下文和错误格式化输出
 * 类似 Python 的错误显示风格
 */

/* 错误类型 */
typedef enum {
    ERR_NONE = 0,           /* 无错误 */
    ERR_SYNTAX,             /* 语法错误 */
    ERR_TYPE,               /* 类型错误 */
    ERR_NAME,               /* 名称错误（变量/函数未定义） */
    ERR_VALUE,              /* 值错误 */
    ERR_RUNTIME,            /* 运行时错误 */
    ERR_IMPORT,             /* 导入错误 */
    ERR_MEMORY,             /* 内存错误 */
    ERR_DIV_ZERO,           /* 除零错误 */
    ERR_INDEX,              /* 索引错误 */
    ERR_ATTR,               /* 属性错误 */
} ErrorType;

/* 错误信息结构 */
typedef struct {
    ErrorType type;          /* 错误类型 */
    char message[256];       /* 错误消息 */
    char filename[128];      /* 文件名 */
    const char *source;      /* 源代码（不拥有所有权） */
    int line;                /* 行号（1-based） */
    int column;              /* 列号（1-based） */
    int length;              /* 错误标记长度 */
} LogexError;

/**
 * 初始化错误结构
 * 
 * @param err 错误结构指针
 */
void error_init(LogexError *err);

/**
 * 设置错误信息
 * 
 * @param err 错误结构指针
 * @param type 错误类型
 * @param msg 错误消息
 * @param source 源代码
 * @param line 行号（1-based）
 * @param column 列号（1-based）
 * @param length 错误标记长度
 */
void error_set(LogexError *err, ErrorType type, const char *msg, 
               const char *source, int line, int column, int length);

/**
 * 设置简单错误（无位置信息）
 * 
 * @param err 错误结构指针
 * @param type 错误类型
 * @param msg 错误消息
 */
void error_set_simple(LogexError *err, ErrorType type, const char *msg);

/**
 * 格式化错误消息（类似 Python 风格）
 * 
 * @param err 错误结构指针
 * @param buffer 输出缓冲区
 * @param max_len 缓冲区最大长度
 * @return 格式化的字符串长度
 */
int error_format(const LogexError *err, char *buffer, size_t max_len);

/**
 * 打印错误信息到 stderr
 * 
 * @param err 错误结构指针
 */
void error_print(const LogexError *err);

/**
 * 清除错误信息
 * 
 * @param err 错误结构指针
 */
void error_clear(LogexError *err);

/**
 * 检查是否有错误
 * 
 * @param err 错误结构指针
 * @return 1 表示有错误，0 表示无错误
 */
int error_has_error(const LogexError *err);

/**
 * 获取错误类型名称
 * 
 * @param type 错误类型
 * @return 错误类型名称字符串
 */
const char* error_type_name(ErrorType type);

#endif /* ERROR_H */
