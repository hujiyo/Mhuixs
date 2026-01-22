#include "error.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* 初始化错误结构 */
void error_init(LogexError *err) {
    if (!err) return;
    
    err->type = ERR_NONE;
    err->message[0] = '\0';
    err->filename[0] = '\0';
    err->source = NULL;
    err->line = 0;
    err->column = 0;
    err->length = 0;
}

/* 设置错误信息 */
void error_set(LogexError *err, ErrorType type, const char *msg, 
               const char *source, int line, int column, int length) {
    if (!err) return;
    
    err->type = type;
    
    /* 复制错误消息 */
    if (msg) {
        strncpy(err->message, msg, sizeof(err->message) - 1);
        err->message[sizeof(err->message) - 1] = '\0';
    } else {
        err->message[0] = '\0';
    }
    
    /* 设置源代码（不拥有所有权） */
    err->source = source;
    err->line = line;
    err->column = column;
    err->length = length > 0 ? length : 1;
}

/* 设置简单错误（无位置信息） */
void error_set_simple(LogexError *err, ErrorType type, const char *msg) {
    error_set(err, type, msg, NULL, 0, 0, 0);
}

/* 获取错误类型名称 */
const char* error_type_name(ErrorType type) {
    switch (type) {
        case ERR_NONE:     return "Success";
        case ERR_SYNTAX:   return "SyntaxError";
        case ERR_TYPE:     return "TypeError";
        case ERR_NAME:     return "NameError";
        case ERR_VALUE:    return "ValueError";
        case ERR_RUNTIME:  return "RuntimeError";
        case ERR_IMPORT:   return "ImportError";
        case ERR_MEMORY:   return "MemoryError";
        case ERR_DIV_ZERO: return "ZeroDivisionError";
        case ERR_INDEX:    return "IndexError";
        case ERR_ATTR:     return "AttributeError";
        default:           return "Error";
    }
}

/* 获取源代码中指定行的内容 */
static int get_source_line(const char *source, int target_line, char *line_buf, size_t max_len) {
    if (!source || !line_buf || max_len == 0) return -1;
    
    int current_line = 1;
    const char *line_start = source;
    const char *p = source;
    
    /* 找到目标行 */
    while (*p != '\0') {
        if (*p == '\n') {
            if (current_line == target_line) {
                /* 找到目标行，复制内容 */
                size_t len = p - line_start;
                if (len >= max_len) len = max_len - 1;
                memcpy(line_buf, line_start, len);
                line_buf[len] = '\0';
                return 0;
            }
            current_line++;
            line_start = p + 1;
        }
        p++;
    }
    
    /* 最后一行（没有换行符） */
    if (current_line == target_line) {
        size_t len = p - line_start;
        if (len >= max_len) len = max_len - 1;
        memcpy(line_buf, line_start, len);
        line_buf[len] = '\0';
        return 0;
    }
    
    return -1;
}

/* 格式化错误消息 */
int error_format(const LogexError *err, char *buffer, size_t max_len) {
    if (!err || !buffer || max_len == 0) return -1;
    
    int pos = 0;
    
    /* 无错误 */
    if (err->type == ERR_NONE) {
        buffer[0] = '\0';
        return 0;
    }
    
    /* 错误类型和消息 */
    pos += snprintf(buffer + pos, max_len - pos, "%s: %s\n", 
                    error_type_name(err->type), err->message);
    
    /* 如果有位置信息，显示详细上下文 */
    if (err->source && err->line > 0 && pos < (int)max_len) {
        /* 文件名和行号 */
        const char *filename = err->filename[0] ? err->filename : "<stdin>";
        pos += snprintf(buffer + pos, max_len - pos, "  File \"%s\", line %d\n", 
                        filename, err->line);
        
        /* 获取错误所在行的内容 */
        char line_buf[512];
        if (get_source_line(err->source, err->line, line_buf, sizeof(line_buf)) == 0 && pos < (int)max_len) {
            /* 显示源代码行（去除前导空格但保留缩进） */
            int leading_spaces = 0;
            while (line_buf[leading_spaces] == ' ' || line_buf[leading_spaces] == '\t') {
                leading_spaces++;
            }
            
            pos += snprintf(buffer + pos, max_len - pos, "    %s\n", line_buf + leading_spaces);
            
            /* 显示错误指示符 */
            if (err->column > 0 && pos < (int)max_len) {
                pos += snprintf(buffer + pos, max_len - pos, "    ");
                
                /* 添加空格直到错误列 */
                int adjusted_col = err->column - leading_spaces;
                if (adjusted_col < 1) adjusted_col = 1;
                
                for (int i = 1; i < adjusted_col && pos < (int)max_len - 1; i++) {
                    buffer[pos++] = ' ';
                }
                
                /* 添加 ^ 指示符 */
                for (int i = 0; i < err->length && pos < (int)max_len - 2; i++) {
                    buffer[pos++] = '^';
                }
                
                buffer[pos++] = '\n';
                buffer[pos] = '\0';
            }
        }
    }
    
    return pos;
}

/* 打印错误信息 */
void error_print(const LogexError *err) {
    if (!err || err->type == ERR_NONE) return;
    
    char buffer[1024];
    error_format(err, buffer, sizeof(buffer));
    fprintf(stderr, "%s", buffer);
}

/* 清除错误信息 */
void error_clear(LogexError *err) {
    error_init(err);
}

/* 检查是否有错误 */
int error_has_error(const LogexError *err) {
    return err && err->type != ERR_NONE;
}
