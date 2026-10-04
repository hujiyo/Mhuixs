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
#include "bignum.h"  /* 提供 BHS 类型定义 */

/* 内部使用 Obj 作为 void* 别名，减少代码改动 */
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

/*
 * ============================ 所有权规则 ============================
 *
 * LIST **拥有**它存放的每一个元素（Obj，即堆分配的 BHS*）。
 *
 *   存入即交出所有权：list_lpush / list_rpush / list_insert / list_set_index
 *       调用方把 Obj 交出去之后，不要再对它调用 bignum_destroy。
 *
 *   取出即拿回所有权：list_lpop / list_rpop / list_rm_index
 *       返回值归调用方所有，用完必须 bignum_destroy。
 *
 *   只看不拿（借用）：list_get_index
 *       返回的是内部指针，**不要释放、不要修改**；
 *       它只在对应元素被移除或 list 被清空之前有效。
 *
 *   复制：list_copy 是**深拷贝**，新表与原表完全独立。
 *
 * 元素不可为 NULL —— 需要"空值"请用 BIGNUM_TYPE_NULL 类型的 BHS。
 * 这条不变式是持久化格式依赖的前提（LIST 序列化不带空值标志位）。
 * ===================================================================
 */

// LIST 函数（对外接口使用 BHS*）
LIST* list_create(void);
LIST* list_copy(const LIST* other);
void free_list(LIST* lst);
void list_clear(LIST* lst);
size_t list_size(const LIST* lst);
int list_lpush(LIST* lst, Obj value);
int list_rpush(LIST* lst, Obj value);
Obj list_lpop(LIST* lst);
Obj list_rpop(LIST* lst);
int list_insert(LIST* lst, size_t pos, Obj value);
Obj list_rm_index(LIST* lst, size_t pos);       /* 返回被移除的元素，归调用方 */
Obj list_get_index(const LIST* lst, size_t pos); /* 借用，勿释放 */
int list_set_index(LIST* lst, size_t pos, Obj value);
int list_swap(LIST* lst, size_t idx1, size_t idx2);

#endif

