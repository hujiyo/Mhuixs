#ifndef MSTRING_H
#define MSTRING_H

#include <string.h>
#include <stdlib.h>
#include <stdint.h>


typedef char* mstring;

static inline mstring mstr(char* str){
    mstring result = (mstring)malloc(strlen(str)+sizeof(size_t));
    if(result == NULL){
        return NULL;
    }
    *(size_t*)result = strlen(str);
    memcpy(result+sizeof(size_t), str, strlen(str));
    return result;
}

static inline size_t mstrlen(mstring str){
    if(str == NULL){
        return SIZE_MAX;
    }
    return *(size_t*)str;
}

/* 释放字符串 */
static inline void mstr_free(mstring str){
    if(str){
        free(str);
    }
}

/* 复制字符串 */
static inline mstring mstr_copy(mstring str){
    if(!str){
        return NULL;
    }
    size_t total_size = sizeof(size_t) + mstrlen(str);
    mstring result = (mstring)malloc(total_size);
    if(!result){
        return NULL;
    }
    memcpy(result, str, total_size);
    return result;
}

/* 拼接字符串 */
static inline mstring mstr_concat(mstring a, mstring b){
    if(!a || !b){
        return NULL;
    }
    size_t len_a = mstrlen(a);
    size_t len_b = mstrlen(b);
    size_t total_len = len_a + len_b;
    
    mstring result = (mstring)malloc(sizeof(size_t) + total_len);
    if(!result){
        return NULL;
    }
    
    *(size_t*)result = total_len;
    memcpy(result + sizeof(size_t), a + sizeof(size_t), len_a);
    memcpy(result + sizeof(size_t) + len_a, b + sizeof(size_t), len_b);
    return result;
}

/* 比较字符串 */
static inline int mstr_equals(mstring a, mstring b){
    if(!a || !b){
        return 0;
    }
    size_t len_a = mstrlen(a);
    if(len_a != mstrlen(b)){
        return 0;
    }
    return memcmp(a + sizeof(size_t), b + sizeof(size_t), len_a) == 0;
}

/* 获取 C 字符串指针（无需拷贝，不带 \0） */
static inline const char* mstr_cstr(mstring str){
    return str ? (str + sizeof(size_t)) : NULL;
}

/* 转换为 C 字符串（需要拷贝，带 \0，调用者需要 free） */
static inline char* mstr_to_cstr(mstring str){
    if(!str){
        return NULL;
    }
    size_t len = mstrlen(str);
    char* cstr = (char*)malloc(len + 1);
    if(!cstr){
        return NULL;
    }
    memcpy(cstr, str + sizeof(size_t), len);
    cstr[len] = '\0';
    return cstr;
}

/* 从二进制数据创建字符串 */
static inline mstring mstr_from_bytes(const uint8_t* data, size_t len){
    if(!data){
        return NULL;
    }
    mstring result = (mstring)malloc(sizeof(size_t) + len);
    if(!result){
        return NULL;
    }
    *(size_t*)result = len;
    memcpy(result + sizeof(size_t), data, len);
    return result;
}

#endif