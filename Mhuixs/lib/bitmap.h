#ifndef BITMAP_C_H
#define BITMAP_C_H
/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "../lib/bitcpy.h"
#include "../share/merr.h"

#define bitmap_debug

#ifdef __cplusplus
extern "C" {
#endif

/*
BITMAP 结构
0~7             bit数
8~(bit/8+15)    数据区大小
8字节对齐
*/

typedef struct {
    uint8_t* bitmap;
    int state; // 对象状态，通过改变状态来表示异常状态
} bitmap_t;

// 构造和析构函数
bitmap_t* bitmap_create(void);
bitmap_t* bitmap_create_with_size(uint64_t size);
bitmap_t* bitmap_create_from_string(const char* s);
bitmap_t* bitmap_create_copy(const bitmap_t* other);
bitmap_t* bitmap_create_from_data(char* s, uint64_t len, uint8_t zerochar);
void bitmap_destroy(bitmap_t* bm);

// 赋值操作
int bitmap_assign_string(bitmap_t* bm, char* s);
int bitmap_assign_bitmap(bitmap_t* bm, const bitmap_t* other);
int bitmap_append(bitmap_t* bm, bitmap_t* other);

// 访问和修改
int bitmap_get(const bitmap_t* bm, uint64_t offset);
int bitmap_set(bitmap_t* bm, uint64_t offset, uint8_t value);
int bitmap_set_range(bitmap_t* bm, uint64_t offset, uint64_t len, uint8_t value);
int bitmap_set_from_stream(bitmap_t* bm, uint64_t offset, uint64_t len, const char* data_stream, char zero_value);

// 查询操作
uint64_t bitmap_size(const bitmap_t* bm);
uint64_t bitmap_count(const bitmap_t* bm, uint64_t st_offset, uint64_t ed_offset);
int64_t bitmap_find(const bitmap_t* bm, uint8_t value, uint64_t start, uint64_t end);

// 状态和调试
int bitmap_iserr(bitmap_t* bm);
void bitmap_print(const bitmap_t* bm);

#ifdef __cplusplus
}
#endif

#endif

