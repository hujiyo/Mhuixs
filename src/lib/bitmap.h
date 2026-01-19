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

#include "../share/obj.h" 


#define bitmap_debug

/*
BITMAP 使用 BigNum 结构
BigNum 结构用于表示 BITMAP 类型：
- type = BIGNUM_TYPE_BITMAP (2)
- length = 位图的位数（以位为单位）
- capacity = 分配的字节数
- data.large_data 或 data.small_data 直接存储位图数据
  
数据存储方式：
- 直接存储位图数据，不需要额外的头部信息
- 使用 BigNum.length 表示位数
- 使用 BigNum.capacity 表示分配的字节数
- 小于等于 BIGNUM_SMALL_SIZE (32) 字节时使用 small_data
- 大于 BIGNUM_SMALL_SIZE 时使用 large_data
*/

// 类型检查函数
int check_if_bitmap(const BigNum* bm);

// 构造和析构函数
BigNum* bitmap_create(void);
BigNum* bitmap_create_with_size(uint64_t size);
BigNum* bitmap_create_from_string(const char* s);
BigNum* bitmap_create_copy(const BigNum* other);
BigNum* bitmap_create_from_data(char* s, uint64_t len, uint8_t zerochar);
void free_bitmap(BigNum* bm);

// 赋值操作
int bitmap_assign_string(BigNum* bm, char* s);
int bitmap_assign_bitmap(BigNum* bm, const BigNum* other);
int bitmap_append(BigNum* bm, BigNum* other);

// 访问和修改
int bitmap_get(const BigNum* bm, uint64_t offset);
int bitmap_set(BigNum* bm, uint64_t offset, uint8_t value);
int bitmap_set_range(BigNum* bm, uint64_t offset, uint64_t len, uint8_t value);
int bitmap_set_from_stream(BigNum* bm, uint64_t offset, uint64_t len, const char* data_stream, char zero_value);

// 查询操作
uint64_t bitmap_size(const BigNum* bm);
uint64_t bitmap_count(const BigNum* bm, uint64_t st_offset, uint64_t ed_offset);
int64_t bitmap_find(const BigNum* bm, uint8_t value, uint64_t start, uint64_t end);

// 状态和调试
int bitmap_iserr(BigNum* bm);
void bitmap_print(const BigNum* bm);

// 位运算操作（Logex支持）
BigNum* bitmap_bitand(const BigNum* a, const BigNum* b);
BigNum* bitmap_bitor(const BigNum* a, const BigNum* b);
BigNum* bitmap_bitxor(const BigNum* a, const BigNum* b);
BigNum* bitmap_bitnot(const BigNum* a);
BigNum* bitmap_bitshl(const BigNum* a, uint64_t shift);
BigNum* bitmap_bitshr(const BigNum* a, uint64_t shift);

#endif

