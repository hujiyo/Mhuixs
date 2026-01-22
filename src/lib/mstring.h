#ifndef MSTRING_H
#define MSTRING_H

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/*
核心特性：
不使用程序层面的结构抽象，
压缩mstring不只是压缩字符串内容，
而是整个mstring的内存布局都能被压缩

内存布局 (8字节对齐):
[0-3] len:   uint32_t 字符串长度
[4-7] cap8:  uint32_t 实际分配容量-8
[8+]  data:  字符串内容
*/

typedef char* mstring;

#define MSTR_HDR_SIZE 8
#define MSTR_PREALLOC_THRESHOLD 1024  /* 超过此值不预分配 */
#define MSTR_PREALLOC_FACTOR 2        /* 预分配倍数 */

/* 获取长度指针 */
#define MSTR_LEN_PTR(s)  ((uint32_t*)(s))
/* 获取容量-8指针 */
#define MSTR_CAP8_PTR(s) ((uint32_t*)((s) + 4))
/* 获取实际容量 */
#define MSTR_CAP(s)      (*(MSTR_CAP8_PTR(s)) + MSTR_HDR_SIZE)

static inline mstring mstr(char* str){
    size_t len = strlen(str);
    if(len > UINT32_MAX) return NULL;
    
    /* 小字符串预分配，大字符串精确分配 */
    size_t alloc_size;
    if(len < MSTR_PREALLOC_THRESHOLD){
        alloc_size = MSTR_HDR_SIZE + len * MSTR_PREALLOC_FACTOR;
        if(alloc_size < MSTR_HDR_SIZE + 16) alloc_size = MSTR_HDR_SIZE + 16;
    } else {
        alloc_size = MSTR_HDR_SIZE + len;
    }
    
    mstring result = (mstring)malloc(alloc_size);
    if(result == NULL){
        return NULL;
    }
    *MSTR_LEN_PTR(result) = (uint32_t)len;
    *MSTR_CAP8_PTR(result) = (uint32_t)(alloc_size - MSTR_HDR_SIZE);
    memcpy(result + MSTR_HDR_SIZE, str, len);
    return result;
}

static inline size_t mstrlen(mstring str){
    if(str == NULL){
        return SIZE_MAX;
    }
    return *MSTR_LEN_PTR(str);
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
    size_t total_size = MSTR_CAP(str);
    mstring result = (mstring)malloc(total_size);
    if(!result){
        return NULL;
    }
    memcpy(result, str, MSTR_HDR_SIZE + mstrlen(str));
    *MSTR_CAP8_PTR(result) = (uint32_t)(total_size - MSTR_HDR_SIZE);
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
    if(total_len > UINT32_MAX) return NULL;
    
    /* 小字符串预分配，大字符串精确分配 */
    size_t alloc_size;
    if(total_len < MSTR_PREALLOC_THRESHOLD){
        alloc_size = MSTR_HDR_SIZE + total_len * MSTR_PREALLOC_FACTOR;
        if(alloc_size < MSTR_HDR_SIZE + 16) alloc_size = MSTR_HDR_SIZE + 16;
    } else {
        alloc_size = MSTR_HDR_SIZE + total_len;
    }
    
    mstring result = (mstring)malloc(alloc_size);
    if(!result){
        return NULL;
    }
    
    *MSTR_LEN_PTR(result) = (uint32_t)total_len;
    *MSTR_CAP8_PTR(result) = (uint32_t)(alloc_size - MSTR_HDR_SIZE);
    memcpy(result + MSTR_HDR_SIZE, a + MSTR_HDR_SIZE, len_a);
    memcpy(result + MSTR_HDR_SIZE + len_a, b + MSTR_HDR_SIZE, len_b);
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
    return memcmp(a + MSTR_HDR_SIZE, b + MSTR_HDR_SIZE, len_a) == 0;
}

/* 获取 C 字符串指针（无需拷贝，不带 \0） */
static inline const char* mstr_cstr(mstring str){
    return str ? (str + MSTR_HDR_SIZE) : NULL;
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
    memcpy(cstr, str + MSTR_HDR_SIZE, len);
    cstr[len] = '\0';
    return cstr;
}

/* 从二进制数据创建字符串 */
static inline mstring mstr_from_bytes(const uint8_t* data, size_t len){
    if(!data){
        return NULL;
    }
    if(len > UINT32_MAX) return NULL;
    
    /* 小字符串预分配，大字符串精确分配 */
    size_t alloc_size;
    if(len < MSTR_PREALLOC_THRESHOLD){
        alloc_size = MSTR_HDR_SIZE + len * MSTR_PREALLOC_FACTOR;
        if(alloc_size < MSTR_HDR_SIZE + 16) alloc_size = MSTR_HDR_SIZE + 16;
    } else {
        alloc_size = MSTR_HDR_SIZE + len;
    }
    
    mstring result = (mstring)malloc(alloc_size);
    if(!result){
        return NULL;
    }
    *MSTR_LEN_PTR(result) = (uint32_t)len;
    *MSTR_CAP8_PTR(result) = (uint32_t)(alloc_size - MSTR_HDR_SIZE);
    memcpy(result + MSTR_HDR_SIZE, data, len);
    return result;
}

/* ========================================
 * BHS 互操作函数（需要 bignum.h）
 * ======================================== */

#ifdef BIGNUM_H  /* 只有在包含了 bignum.h 后才定义这些函数 */

/* 从 BHS (字符串类型) 创建 mstring */
static inline mstring mstr_from_bhs(const BHS* bhs){
    if(!bhs || bhs->type != BIGNUM_TYPE_STRING){
        return NULL;
    }
    const char* str_data = bhs->is_large ? bhs->data.large_data : bhs->data.small_data;
    size_t len = bhs->length;
    return mstr_from_bytes((const uint8_t*)str_data, len);
}

/* 比较 mstring 和 BHS (字符串类型) 是否相等 */
static inline int mstr_equals_bhs(mstring a, const BHS* b){
    if(!a || !b || b->type != BIGNUM_TYPE_STRING){
        return 0;
    }
    size_t len_a = mstrlen(a);
    if(len_a != b->length){
        return 0;
    }
    const char* b_data = b->is_large ? b->data.large_data : b->data.small_data;
    return memcmp(a + MSTR_HDR_SIZE, b_data, len_a) == 0;
}

/* 创建 BHS (字符串类型) 从 mstring */
static inline BHS* bhs_from_mstr(mstring str){
    if(!str){
        return NULL;
    }
    size_t len = mstrlen(str);
    const char* data = mstr_cstr(str);
    
    BHS* bhs = (BHS*)malloc(sizeof(BHS));
    if(!bhs) return NULL;
    
    memset(bhs, 0, sizeof(BHS));
    bhs->type = BIGNUM_TYPE_STRING;
    bhs->length = len;
    
    if(len <= BIGNUM_SMALL_SIZE){
        bhs->is_large = 0;
        bhs->capacity = BIGNUM_SMALL_SIZE;
        memcpy(bhs->data.small_data, data, len);
    } else {
        bhs->is_large = 1;
        bhs->capacity = len;
        bhs->data.large_data = (char*)malloc(len);
        if(!bhs->data.large_data){
            free(bhs);
            return NULL;
        }
        memcpy(bhs->data.large_data, data, len);
    }
    
    return bhs;
}

#endif /* BIGNUM_H */

#endif