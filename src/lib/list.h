#ifndef LIST_H
#define LIST_H

/*
#版权所有 (c) HUJI 2025
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.4
Email:hj18914255909@outlook.com
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../bignum.h"  /* 提供 BHS 类型定义 */

/* 内部使用 Obj 作为 void* 别名，减少代码改动 */
typedef void* Obj;

#define UINTDEQUE_BLOCK_SIZE 4096 // 每块最大元素数
#define MIN_BLOCK_SIZE 512 // 块合并的最小阈值


typedef struct Block {
    Obj data[UINTDEQUE_BLOCK_SIZE];
    struct Block *prev, *next;
    uint32_t size;  // 当前块内元素数
    uint32_t start; // 块内数据起始下标（data[start]为第一个元素）
} Block;

typedef struct LIST {
    Block* head_block;
    Block* tail_block;
    size_t num; // 总元素数
} LIST;

// LIST 函数（对外接口使用 BHS*）
LIST* list_create(void);
LIST* list_copy(const LIST* other);
void free_list(LIST* lst);
void list_clear(LIST* lst);
size_t list_size(const LIST* lst);
int list_lpush(LIST* lst, BHS* value);
int list_rpush(LIST* lst, BHS* value);
BHS* list_lpop(LIST* lst);
BHS* list_rpop(LIST* lst);
int list_insert(LIST* lst, size_t pos, BHS* value);
int list_rm_index(LIST* lst, size_t pos);
BHS* list_get_index(const LIST* lst, size_t pos);
int list_set_index(LIST* lst, size_t pos, BHS* value);
int list_swap(LIST* lst, size_t idx1, size_t idx2);

#endif

