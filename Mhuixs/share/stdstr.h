#ifndef STDSTR_H
#define STDSTR_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * str 字节流类型 - C 版本
 * 使用 len 记录字节流的长度，防止字符串泄露
 * 适用于 C 语言环境，提供基本的字符串操作功能
 */
typedef struct str {
    uint8_t *string;    // 字节流数据
    uint32_t len;       // 字节流长度
} str;

static inline void str_init(str *s) {
    if (s) {
        s->string = NULL;
        s->len = 0;
    }
}

static inline void str_free(str *s) {
    if (s) {
        free(s->string);
        s->string = NULL;
        s->len = 0;
    }
}

static inline void str_clear(str *s) {
    if (s) {
        free(s->string);
        s->string = NULL;
        s->len = 0;
    }
}

static inline int str_from_cstr(str *s, const char *cstr) {
    if (!s || !cstr) return -1;
    
    str_clear(s);
    s->len = strlen(cstr);
    
    if (s->len == 0) {
        s->string = NULL;
        return 0;
    }
    
    s->string = (uint8_t*)malloc(s->len);
    if (!s->string) {
        s->len = 0;
        return -1;
    }
    
    memcpy(s->string, cstr, s->len);
    return 0;
}

static inline int str_from_bytes(str *s, const uint8_t *data, uint32_t len) {
    if (!s || !data) return -1;
    
    str_clear(s);
    s->len = len;
    
    if (len == 0) {
        s->string = NULL;
        return 0;
    }
    
    s->string = (uint8_t*)malloc(len);
    if (!s->string) {
        s->len = 0;
        return -1;
    }
    
    memcpy(s->string, data, len);
    return 0;
}

static inline int str_copy(str *dest, const str *src) {
    if (!dest || !src) return -1;
    
    str_clear(dest);
    
    if (src->len == 0) {
        dest->string = NULL;
        dest->len = 0;
        return 0;
    }
    
    dest->string = (uint8_t*)malloc(src->len);
    if (!dest->string) {
        dest->len = 0;
        return -1;
    }
    
    memcpy(dest->string, src->string, src->len);
    dest->len = src->len;
    return 0;
}

static inline int swrite(str *s, uint32_t pos, const void *data, uint32_t len) {
    if (!s || !data || len == 0) return -1;
    
    // 当前实现：总是追加到字符串末尾（忽略 pos 参数）
    uint32_t new_len = s->len + len;
    uint8_t *new_string = (uint8_t*)realloc(s->string, new_len);
    
    if (!new_string) {
        return -1;
    }
    
    s->string = new_string;
    memcpy(s->string + s->len, data, len);
    s->len = new_len;
    return 0;
}

static inline char* str_to_cstr(const str *s) {
    if (!s) return NULL;
    
    char *cstr = (char*)malloc(s->len + 1);
    if (!cstr) return NULL;
    
    if (s->len > 0 && s->string) {
        memcpy(cstr, s->string, s->len);
    }
    cstr[s->len] = '\0';
    return cstr;
}

static inline int str_equals(const str *s1, const str *s2) {
    if (!s1 || !s2) return 0;
    if (s1->len != s2->len) return 0;
    if (s1->len == 0) return 1;
    if (!s1->string || !s2->string) return 0;
    return memcmp(s1->string, s2->string, s1->len) == 0;
}

static inline void str_print(const str *s) {
    if (s && s->string) {
        for (uint32_t i = 0; i < s->len; i++) {
            printf("%c", s->string[i]);
        }
    }
}

static inline int sappend(str *s, const void *data, uint32_t len) {
    return swrite(s, s->len, data, len);
}

static inline int str_append_string(str *s, const char *cstr) {
    if (!s || !cstr) return -1;
    return swrite(s, s->len, cstr, strlen(cstr));
}

// 添加一个确保字符串以空字符结尾的函数
static inline int str_ensure_null_terminated(str *s) {
    if (!s) return -1;
    
    // 检查最后一个字符是否已经是空字符
    if (s->len > 0 && s->string[s->len - 1] == '\0') {
        return 0; // 已经是空字符结尾
    }
    
    // 添加空字符
    uint8_t null_char = '\0';
    return swrite(s, s->len, &null_char, 1);
}

static inline void sprint(str s, str end_marker) {
    str_print(&s);
    if (end_marker.len > 0) {
        str_print(&end_marker);
    }
}

// 定义空字符串常量
static const str end = {NULL, 0};

#ifdef __cplusplus
}
#endif

#endif