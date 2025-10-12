#ifndef BITMAP_H
#define BITMAP_H
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

#include "bitcpy.h"
#include "obj.h"

#define bitmap_debug

/*
BITMAP 结构
0~7             bit数
8~(bit/8+15)    数据区大小
8字节对齐
*/

typedef struct {
    uint8_t* bitmap;
} BITMAP;

// 构造和析构函数
BITMAP* bitmap_create(void);
BITMAP* bitmap_create_with_size(uint64_t size);
BITMAP* bitmap_create_from_string(const char* s);
BITMAP* bitmap_create_copy(const BITMAP* other);
BITMAP* bitmap_create_from_data(char* s, uint64_t len, uint8_t zerochar);
void free_bitmap(BITMAP* bm);

// 赋值操作
int bitmap_assign_string(BITMAP* bm, char* s);
int bitmap_assign_bitmap(BITMAP* bm, const BITMAP* other);
int bitmap_append(BITMAP* bm, BITMAP* other);

// 访问和修改
int bitmap_get(const BITMAP* bm, uint64_t offset);
int bitmap_set(BITMAP* bm, uint64_t offset, uint8_t value);
int bitmap_set_range(BITMAP* bm, uint64_t offset, uint64_t len, uint8_t value);
int bitmap_set_from_stream(BITMAP* bm, uint64_t offset, uint64_t len, const char* data_stream, char zero_value);

// 查询操作
uint64_t bitmap_size(const BITMAP* bm);
uint64_t bitmap_count(const BITMAP* bm, uint64_t st_offset, uint64_t ed_offset);
int64_t bitmap_find(const BITMAP* bm, uint8_t value, uint64_t start, uint64_t end);

// 状态和调试
int bitmap_iserr(BITMAP* bm);
void bitmap_print(const BITMAP* bm);

#endif

