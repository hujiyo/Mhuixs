#ifndef KVALH_H
#define KVALH_H

/*
#版权所有 (c) HUJI 2025
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.2
Email:hj18914255909@outlook.com
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bignum.h"
#include "mstring.h"

/* 哈希桶数量级别 */
#define HASH_TONG_1024       1024        // 2^10
#define HASH_TONG_4096       4096        // 2^12
#define HASH_TONG_16384      16384       // 2^14
#define HASH_TONG_65536      65536       // 2^16
#define HASH_TONG_262144     262144      // 2^18
#define HASH_TONG_1048576    1048576     // 2^20
#define HASH_TONG_4194304    4194304     // 2^22
#define HASH_TONG_16777216   16777216    // 2^24

/* 哈希表装载因子 */
#define HASH_LOAD_FACTOR 0.75

/* 哈希桶结构 */
typedef struct {
    uint32_t num_keys;      // 桶内的 key 数量
    uint32_t capacity;      // 桶内数组的容量
    uint32_t* key_indices;  // key 在 keypool 中的索引数组
} HASH_BUCKET;

/* 键值对结构 */
typedef struct {
    mstring key;            // 键名 (mstring)
    Obj value;              // 值 (BHS*)
    uint32_t hash_index;    // 在哈希表中的桶索引
} KVPAIR;

/* KVALOT 结构 */
typedef struct {
    HASH_BUCKET* hash_table;  // 哈希桶表
    uint32_t num_buckets;     // 哈希桶数量
    
    KVPAIR* keypool;          // 键值对池
    uint32_t num_keys;        // 键数量
    uint32_t keypool_capacity;// 键池容量
    
    Obj name;                 // KVALOT 名称 (BHS* 字符串类型)
} KVALOT;

/* ========================================
 * KVALOT 基本操作
 * ======================================== */

/**
 * 创建 KVALOT
 * @param name KVALOT 名称 (BHS* 字符串类型)
 * @return KVALOT 指针，失败返回 NULL
 */
KVALOT* kvalot_create(Obj name);

/**
 * 拷贝 KVALOT
 * @param other 源 KVALOT
 * @return 新的 KVALOT 指针，失败返回 NULL
 */
KVALOT* kvalot_copy(const KVALOT* other);

/**
 * 销毁 KVALOT
 */
void kvalot_destroy(KVALOT* kv);

/**
 * 清空 KVALOT（保留结构，删除所有键值对）
 */
void kvalot_clear(KVALOT* kv);

/* ========================================
 * 键值对操作
 * ======================================== */

/**
 * 添加键值对
 * @param key 键名 (BHS* 字符串类型)
 * @param value 值 (BHS*)
 * @return 0 成功, -1 失败
 */
int kvalot_add(KVALOT* kv, Obj key, Obj value);

/**
 * 查找键值对
 * @param key 键名 (BHS* 字符串类型)
 * @return 值 (BHS*)，未找到返回 NULL
 */
Obj kvalot_find(const KVALOT* kv, Obj key);

/**
 * 删除键值对
 * @param key 键名 (BHS* 字符串类型)
 * @return 0 成功, -1 失败
 */
int kvalot_remove(KVALOT* kv, Obj key);

/**
 * 检查键是否存在
 * @param key 键名 (BHS* 字符串类型)
 * @return 1 存在, 0 不存在
 */
int kvalot_exists(const KVALOT* kv, Obj key);

/* ========================================
 * 查询操作
 * ======================================== */

/**
 * 获取键数量
 * @return 键数量
 */
uint32_t kvalot_size(const KVALOT* kv);

/**
 * 获取 KVALOT 名称（只读）
 * @return 名称 (BHS* 字符串类型)，失败返回 NULL
 * @note 返回的是内部指针，不要修改或释放
 */
Obj kvalot_get_name(const KVALOT* kv);

/**
 * 获取装载因子
 * @param kv KVALOT 指针
 * @return 装载因子
 */
float kvalot_get_load_factor(const KVALOT* kv);

/* ========================================
 * 调试和统计
 * ======================================== */

void kvalot_print_stats(const KVALOT* kv);

#endif /* KVALH_H */
