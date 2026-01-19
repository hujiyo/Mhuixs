#ifndef BITMAP_NEW_H
#define BITMAP_NEW_H
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

#include "../bignum.h"  /* 提供 BHS 类型定义 */ 


#define bitmap_debug

/*
BITMAP 使用 BHS 结构
BHS 结构用于表示 BITMAP 类型：
- type = BIGNUM_TYPE_BITMAP (2)
- length = 位图的位数（以位为单位）
- capacity = 分配的字节数
- data.large_data 或 data.small_data 直接存储位图数据
  
数据存储方式：
- 直接存储位图数据，不需要额外的头部信息
- 使用 BHS.length 表示位数
- 使用 BHS.capacity 表示分配的字节数
- 小于等于 BIGNUM_SMALL_SIZE (32) 字节时使用 small_data
- 大于 BIGNUM_SMALL_SIZE 时使用 large_data
*/

// 类型检查函数
int check_if_bitmap(const BHS* bm);

// 构造和析构函数
BHS* bitmap_create(void);
BHS* bitmap_create_with_size(uint64_t size);
BHS* bitmap_create_from_string(const char* s);
BHS* bitmap_create_copy(const BHS* other);
BHS* bitmap_create_from_data(char* s, uint64_t len, uint8_t zerochar);
void free_bitmap(BHS* bm);

// 赋值操作
int bitmap_assign_string(BHS* bm, char* s);
int bitmap_assign_bitmap(BHS* bm, const BHS* other);
int bitmap_append(BHS* bm, BHS* other);

// 访问和修改
int bitmap_get(const BHS* bm, uint64_t offset);
int bitmap_set(BHS* bm, uint64_t offset, uint8_t value);
int bitmap_set_range(BHS* bm, uint64_t offset, uint64_t len, uint8_t value);
int bitmap_set_from_stream(BHS* bm, uint64_t offset, uint64_t len, const char* data_stream, char zero_value);

// 查询操作
uint64_t bitmap_size(const BHS* bm);
uint64_t bitmap_count(const BHS* bm, uint64_t st_offset, uint64_t ed_offset);
int64_t bitmap_find(const BHS* bm, uint8_t value, uint64_t start, uint64_t end);

// 状态和调试
int bitmap_iserr(BHS* bm);
void bitmap_print(const BHS* bm);

// 位运算操作（Logex支持）
BHS* bitmap_bitand(const BHS* a, const BHS* b);
BHS* bitmap_bitor(const BHS* a, const BHS* b);
BHS* bitmap_bitxor(const BHS* a, const BHS* b);
BHS* bitmap_bitnot(const BHS* a);
BHS* bitmap_bitshl(const BHS* a, uint64_t shift);
BHS* bitmap_bitshr(const BHS* a, uint64_t shift);

#endif

