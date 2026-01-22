#include "kvalh.h"

#define merr -1

/* ========================================
 * 内部辅助函数
 * ======================================== */

/**
 * 计算哈希桶数量对应的位数
 */
static uint32_t bucket_count_to_bits(uint32_t count) {
    switch (count) {
        case HASH_TONG_1024:      return 10;
        case HASH_TONG_4096:      return 12;
        case HASH_TONG_16384:     return 14;
        case HASH_TONG_65536:     return 16;
        case HASH_TONG_262144:    return 18;
        case HASH_TONG_1048576:   return 20;
        case HASH_TONG_4194304:   return 22;
        case HASH_TONG_16777216:  return 24;
        default:                  return 16; // 默认 65536
    }
}

/**
 * MurmurHash 算法
 */
static uint32_t murmur_hash(const uint8_t* data, size_t len, uint32_t bits) {
    if (!data || bits > 32) return 0;
    
    uint32_t seed = 0x9747b28c;
    const int nblocks = len / 4;
    
    for (int i = 0; i < nblocks; i++) {
        uint32_t k1 = ((uint32_t)data[i*4]     ) |
                     ((uint32_t)data[i*4 + 1] <<  8) |
                     ((uint32_t)data[i*4 + 2] << 16) |
                     ((uint32_t)data[i*4 + 3] << 24);
        k1 *= 0xcc9e2d51;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= 0x1b873593;
        seed ^= k1;
        seed = (seed << 13) | (seed >> 19);
        seed = seed * 5 + 0xe6546b64;
    }
    
    const uint8_t* tail = data + nblocks * 4;
    uint32_t k1 = 0;
    switch(len & 3) {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] <<  8;
        case 1: k1 ^= tail[0];
                k1 *= 0xcc9e2d51;
                k1 = (k1 << 15) | (k1 >> 17);
                k1 *= 0x1b873593;
                seed ^= k1;
    }
    
    seed ^= len;
    seed ^= seed >> 16;
    seed *= 0x85ebca6b;
    seed ^= seed >> 13;
    seed *= 0xc2b2ae35;
    seed ^= seed >> 16;
    
    return seed & ((1 << bits) - 1);
}

/**
 * 计算 mstring 的哈希值
 */
static uint32_t hash_mstring(mstring key, uint32_t bits) {
    if (!key) return 0;
    size_t len = mstrlen(key);
    const uint8_t* data = (const uint8_t*)mstr_cstr(key);
    return murmur_hash(data, len, bits);
}

/**
 * 在桶中查找键
 * @return 键在 keypool 中的索引，未找到返回 UINT32_MAX
 */
static uint32_t find_key_in_bucket(const KVALOT* kv, const HASH_BUCKET* bucket, mstring key) {
    if (!kv || !bucket || !key) return UINT32_MAX;
    
    for (uint32_t i = 0; i < bucket->num_keys; i++) {
        uint32_t key_idx = bucket->key_indices[i];
        if (mstr_equals(kv->keypool[key_idx].key, key)) {
            return key_idx;
        }
    }
    return UINT32_MAX;
}

/**
 * 确保桶容量足够
 */
static int ensure_bucket_capacity(HASH_BUCKET* bucket, uint32_t needed) {
    if (!bucket) return merr;
    
    if (bucket->num_keys == 0) {
        bucket->key_indices = (uint32_t*)malloc(sizeof(uint32_t) * 4);
        if (!bucket->key_indices) return merr;
        bucket->capacity = 4;
        return 0;
    }
    
    if (bucket->num_keys >= bucket->capacity) {
        uint32_t new_cap = bucket->capacity * 2;
        uint32_t* new_indices = (uint32_t*)realloc(bucket->key_indices, 
                                                    sizeof(uint32_t) * new_cap);
        if (!new_indices) return merr;
        bucket->key_indices = new_indices;
        bucket->capacity = new_cap;
    }
    
    return 0;
}

/**
 * 确保 keypool 容量足够
 */
static int ensure_keypool_capacity(KVALOT* kv) {
    if (!kv) return merr;
    
    if (kv->num_keys >= kv->keypool_capacity) {
        uint32_t new_cap = kv->keypool_capacity == 0 ? 16 : kv->keypool_capacity * 2;
        KVPAIR* new_pool = (KVPAIR*)realloc(kv->keypool, sizeof(KVPAIR) * new_cap);
        if (!new_pool) return merr;
        kv->keypool = new_pool;
        kv->keypool_capacity = new_cap;
    }
    
    return 0;
}

/**
 * 扩容哈希表
 */
static int resize_hash_table(KVALOT* kv) {
    if (!kv) return merr;
    
    // 确定新的桶数量
    uint32_t new_num_buckets = 0;
    if (kv->num_buckets < HASH_TONG_4096)       new_num_buckets = HASH_TONG_4096;
    else if (kv->num_buckets < HASH_TONG_16384)     new_num_buckets = HASH_TONG_16384;
    else if (kv->num_buckets < HASH_TONG_65536)     new_num_buckets = HASH_TONG_65536;
    else if (kv->num_buckets < HASH_TONG_262144)    new_num_buckets = HASH_TONG_262144;
    else if (kv->num_buckets < HASH_TONG_1048576)   new_num_buckets = HASH_TONG_1048576;
    else if (kv->num_buckets < HASH_TONG_4194304)   new_num_buckets = HASH_TONG_4194304;
    else if (kv->num_buckets < HASH_TONG_16777216)  new_num_buckets = HASH_TONG_16777216;
    else return merr; // 已达最大容量
    
    // 创建新哈希表
    HASH_BUCKET* new_table = (HASH_BUCKET*)calloc(new_num_buckets, sizeof(HASH_BUCKET));
    if (!new_table) return merr;
    
    // 重新分配所有键
    uint32_t new_bits = bucket_count_to_bits(new_num_buckets);
    for (uint32_t i = 0; i < kv->num_keys; i++) {
        KVPAIR* pair = &kv->keypool[i];
        if (!pair->key) continue; // 跳过已删除的键
        
        // 重新计算哈希
        uint32_t new_hash = hash_mstring(pair->key, new_bits);
        HASH_BUCKET* bucket = &new_table[new_hash];
        
        // 确保桶容量
        if (ensure_bucket_capacity(bucket, bucket->num_keys + 1) != 0) {
            // 清理并返回错误
            for (uint32_t j = 0; j < new_num_buckets; j++) {
                if (new_table[j].key_indices) free(new_table[j].key_indices);
            }
            free(new_table);
            return merr;
        }
        
        // 添加到桶中
        bucket->key_indices[bucket->num_keys++] = i;
        pair->hash_index = new_hash;
    }
    
    // 释放旧哈希表
    for (uint32_t i = 0; i < kv->num_buckets; i++) {
        if (kv->hash_table[i].key_indices) {
            free(kv->hash_table[i].key_indices);
        }
    }
    free(kv->hash_table);
    
    // 更新哈希表
    kv->hash_table = new_table;
    kv->num_buckets = new_num_buckets;
    
    return 0;
}

/* ========================================
 * KVALOT 基本操作
 * ======================================== */

KVALOT* kvalot_create(Obj name) {
    if (!name || name->type != BIGNUM_TYPE_STRING) return NULL;
    
    KVALOT* kv = (KVALOT*)calloc(1, sizeof(KVALOT));
    if (!kv) return NULL;
    
    // 初始化哈希表
    kv->num_buckets = HASH_TONG_1024;
    kv->hash_table = (HASH_BUCKET*)calloc(kv->num_buckets, sizeof(HASH_BUCKET));
    if (!kv->hash_table) {
        free(kv);
        return NULL;
    }
    
    // 深拷贝名称
    kv->name = bignum_create();
    if (!kv->name) {
        free(kv->hash_table);
        free(kv);
        return NULL;
    }
    if (bignum_copy(name, kv->name) != 0) {
        bignum_destroy(kv->name);
        free(kv->hash_table);
        free(kv);
        return NULL;
    }
    
    kv->keypool = NULL;
    kv->num_keys = 0;
    kv->keypool_capacity = 0;
    
    return kv;
}

KVALOT* kvalot_copy(const KVALOT* other) {
    if (!other) return NULL;
    
    KVALOT* kv = (KVALOT*)calloc(1, sizeof(KVALOT));
    if (!kv) return NULL;
    
    // 深拷贝名称
    kv->name = bignum_create();
    if (!kv->name) {
        free(kv);
        return NULL;
    }
    if (bignum_copy(other->name, kv->name) != 0) {
        bignum_destroy(kv->name);
        free(kv);
        return NULL;
    }
    
    // 复制哈希表结构
    kv->num_buckets = other->num_buckets;
    kv->hash_table = (HASH_BUCKET*)calloc(kv->num_buckets, sizeof(HASH_BUCKET));
    if (!kv->hash_table) {
        bignum_destroy(kv->name);
        free(kv);
        return NULL;
    }
    
    // 复制 keypool
    kv->num_keys = other->num_keys;
    kv->keypool_capacity = other->keypool_capacity;
    if (kv->keypool_capacity > 0) {
        kv->keypool = (KVPAIR*)malloc(sizeof(KVPAIR) * kv->keypool_capacity);
        if (!kv->keypool) {
            free(kv->hash_table);
            bignum_destroy(kv->name);
            free(kv);
            return NULL;
        }
        
        for (uint32_t i = 0; i < kv->num_keys; i++) {
            // 深拷贝键名
            kv->keypool[i].key = mstr_copy(other->keypool[i].key);
            if (!kv->keypool[i].key) {
                // 清理已分配的资源
                for (uint32_t j = 0; j < i; j++) {
                    mstr_free(kv->keypool[j].key);
                    bignum_destroy(kv->keypool[j].value);
                }
                free(kv->keypool);
                free(kv->hash_table);
                bignum_destroy(kv->name);
                free(kv);
                return NULL;
            }
            
            // 深拷贝值
            kv->keypool[i].value = bignum_create();
            if (!kv->keypool[i].value) {
                mstr_free(kv->keypool[i].key);
                // 清理已分配的资源
                for (uint32_t j = 0; j < i; j++) {
                    mstr_free(kv->keypool[j].key);
                    bignum_destroy(kv->keypool[j].value);
                }
                free(kv->keypool);
                free(kv->hash_table);
                bignum_destroy(kv->name);
                free(kv);
                return NULL;
            }
            if (bignum_copy(other->keypool[i].value, kv->keypool[i].value) != 0) {
                bignum_destroy(kv->keypool[i].value);
                mstr_free(kv->keypool[i].key);
                // 清理已分配的资源
                for (uint32_t j = 0; j < i; j++) {
                    mstr_free(kv->keypool[j].key);
                    bignum_destroy(kv->keypool[j].value);
                }
                free(kv->keypool);
                free(kv->hash_table);
                bignum_destroy(kv->name);
                free(kv);
                return NULL;
            }
            
            kv->keypool[i].hash_index = other->keypool[i].hash_index;
        }
    }
    
    // 复制哈希桶
    for (uint32_t i = 0; i < kv->num_buckets; i++) {
        const HASH_BUCKET* src_bucket = &other->hash_table[i];
        HASH_BUCKET* dst_bucket = &kv->hash_table[i];
        
        dst_bucket->num_keys = src_bucket->num_keys;
        dst_bucket->capacity = src_bucket->capacity;
        
        if (src_bucket->num_keys > 0) {
            dst_bucket->key_indices = (uint32_t*)malloc(sizeof(uint32_t) * dst_bucket->capacity);
            if (!dst_bucket->key_indices) {
                kvalot_destroy(kv);
                return NULL;
            }
            memcpy(dst_bucket->key_indices, src_bucket->key_indices, 
                   sizeof(uint32_t) * src_bucket->num_keys);
        }
    }
    
    return kv;
}

void kvalot_destroy(KVALOT* kv) {
    if (!kv) return;
    
    // 释放哈希桶
    if (kv->hash_table) {
        for (uint32_t i = 0; i < kv->num_buckets; i++) {
            if (kv->hash_table[i].key_indices) {
                free(kv->hash_table[i].key_indices);
            }
        }
        free(kv->hash_table);
    }
    
    // 释放 keypool
    if (kv->keypool) {
        for (uint32_t i = 0; i < kv->num_keys; i++) {
            if (kv->keypool[i].key) {
                mstr_free(kv->keypool[i].key);
            }
            if (kv->keypool[i].value) {
                bignum_destroy(kv->keypool[i].value);
            }
        }
        free(kv->keypool);
    }
    
    // 释放名称
    if (kv->name) {
        bignum_destroy(kv->name);
    }
    
    free(kv);
}

void kvalot_clear(KVALOT* kv) {
    if (!kv) return;
    
    // 清空哈希桶
    for (uint32_t i = 0; i < kv->num_buckets; i++) {
        if (kv->hash_table[i].key_indices) {
            free(kv->hash_table[i].key_indices);
            kv->hash_table[i].key_indices = NULL;
        }
        kv->hash_table[i].num_keys = 0;
        kv->hash_table[i].capacity = 0;
    }
    
    // 清空 keypool
    if (kv->keypool) {
        for (uint32_t i = 0; i < kv->num_keys; i++) {
            if (kv->keypool[i].key) {
                mstr_free(kv->keypool[i].key);
            }
        }
        free(kv->keypool);
        kv->keypool = NULL;
    }
    
    kv->num_keys = 0;
    kv->keypool_capacity = 0;
}

/* ========================================
 * 键值对操作
 * ======================================== */

int kvalot_add(KVALOT* kv, Obj key, Obj value) {
    if (!kv || !key || key->type != BIGNUM_TYPE_STRING || !value) return merr;
    
    // 转换 key 为 mstring
    mstring key_str = mstr_from_bhs(key);
    if (!key_str) return merr;
    
    // 检查是否需要扩容
    if (kv->num_keys + 1 >= kv->num_buckets * HASH_LOAD_FACTOR) {
        if (resize_hash_table(kv) != 0) {
            mstr_free(key_str);
            return merr;
        }
    }
    
    // 计算哈希
    uint32_t bits = bucket_count_to_bits(kv->num_buckets);
    uint32_t hash = hash_mstring(key_str, bits);
    HASH_BUCKET* bucket = &kv->hash_table[hash];
    
    // 检查键是否已存在
    uint32_t existing = find_key_in_bucket(kv, bucket, key_str);
    if (existing != UINT32_MAX) {
        mstr_free(key_str);
        return merr; // 键已存在
    }
    
    // 确保 keypool 容量
    if (ensure_keypool_capacity(kv) != 0) {
        mstr_free(key_str);
        return merr;
    }
    
    // 确保桶容量
    if (ensure_bucket_capacity(bucket, bucket->num_keys + 1) != 0) {
        mstr_free(key_str);
        return merr;
    }
    
    // 添加到 keypool
    uint32_t key_idx = kv->num_keys;
    kv->keypool[key_idx].key = key_str;
    kv->keypool[key_idx].value = value;
    kv->keypool[key_idx].hash_index = hash;
    kv->num_keys++;
    
    // 添加到桶
    bucket->key_indices[bucket->num_keys++] = key_idx;
    
    return 0;
}

Obj kvalot_find(const KVALOT* kv, Obj key) {
    if (!kv || !key || key->type != BIGNUM_TYPE_STRING) return NULL;
    
    // 转换 key 为 mstring
    mstring key_str = mstr_from_bhs(key);
    if (!key_str) return NULL;
    
    // 计算哈希
    uint32_t bits = bucket_count_to_bits(kv->num_buckets);
    uint32_t hash = hash_mstring(key_str, bits);
    const HASH_BUCKET* bucket = &kv->hash_table[hash];
    
    // 查找键
    uint32_t key_idx = find_key_in_bucket(kv, bucket, key_str);
    mstr_free(key_str);
    
    if (key_idx == UINT32_MAX) return NULL;
    return kv->keypool[key_idx].value;
}

int kvalot_remove(KVALOT* kv, Obj key) {
    if (!kv || !key || key->type != BIGNUM_TYPE_STRING) return merr;
    
    // 转换 key 为 mstring
    mstring key_str = mstr_from_bhs(key);
    if (!key_str) return merr;
    
    // 计算哈希
    uint32_t bits = bucket_count_to_bits(kv->num_buckets);
    uint32_t hash = hash_mstring(key_str, bits);
    HASH_BUCKET* bucket = &kv->hash_table[hash];
    
    // 查找键在桶中的位置
    uint32_t bucket_pos = UINT32_MAX;
    uint32_t key_idx = UINT32_MAX;
    for (uint32_t i = 0; i < bucket->num_keys; i++) {
        uint32_t idx = bucket->key_indices[i];
        if (mstr_equals(kv->keypool[idx].key, key_str)) {
            bucket_pos = i;
            key_idx = idx;
            break;
        }
    }
    
    mstr_free(key_str);
    
    if (key_idx == UINT32_MAX) return merr; // 未找到
    
    // 释放键
    mstr_free(kv->keypool[key_idx].key);
    kv->keypool[key_idx].key = NULL;
    kv->keypool[key_idx].value = NULL;
    
    // 从桶中移除（将最后一个元素移到当前位置）
    bucket->key_indices[bucket_pos] = bucket->key_indices[bucket->num_keys - 1];
    bucket->num_keys--;
    
    // 如果删除的不是最后一个键，需要交换 keypool 中的位置
    if (key_idx != kv->num_keys - 1) {
        // 将最后一个键移到当前位置
        kv->keypool[key_idx] = kv->keypool[kv->num_keys - 1];
        
        // 更新哈希桶中的索引
        HASH_BUCKET* moved_bucket = &kv->hash_table[kv->keypool[key_idx].hash_index];
        for (uint32_t i = 0; i < moved_bucket->num_keys; i++) {
            if (moved_bucket->key_indices[i] == kv->num_keys - 1) {
                moved_bucket->key_indices[i] = key_idx;
                break;
            }
        }
    }
    
    kv->num_keys--;
    return 0;
}

int kvalot_exists(const KVALOT* kv, Obj key) {
    return kvalot_find(kv, key) != NULL ? 1 : 0;
}

/* ========================================
 * 查询操作
 * ======================================== */

uint32_t kvalot_size(const KVALOT* kv) {
    return kv ? kv->num_keys : 0;
}

Obj kvalot_get_name(const KVALOT* kv) {
    return kv ? kv->name : NULL;
}

float kvalot_get_load_factor(const KVALOT* kv) {
    if (!kv || kv->num_buckets == 0) return 0.0f;
    return (float)kv->num_keys / (float)kv->num_buckets;
}

/* ========================================
 * 调试和统计
 * ======================================== */

void kvalot_print_stats(const KVALOT* kv) {
    if (!kv) return;
    
    printf("\n=== KVALOT Statistics ===\n");
    if (kv->name && kv->name->type == BIGNUM_TYPE_STRING) {
        const char* name_str = kv->name->is_large ? 
            kv->name->data.large_data : kv->name->data.small_data;
        printf("Name: %.*s\n", (int)kv->name->length, name_str);
    } else {
        printf("Name: (invalid)\n");
    }
    printf("Total keys: %u\n", kv->num_keys);
    printf("Total buckets: %u\n", kv->num_buckets);
    printf("Load factor: %.3f (target: %.2f)\n", 
           kvalot_get_load_factor(kv), HASH_LOAD_FACTOR);
    
    // 桶分布统计
    uint32_t empty_buckets = 0;
    uint32_t max_bucket_size = 0;
    
    for (uint32_t i = 0; i < kv->num_buckets; i++) {
        uint32_t size = kv->hash_table[i].num_keys;
        if (size == 0) {
            empty_buckets++;
        } else if (size > max_bucket_size) {
            max_bucket_size = size;
        }
    }
    
    printf("Empty buckets: %u (%.1f%%)\n", 
           empty_buckets, 100.0f * empty_buckets / kv->num_buckets);
    printf("Max bucket size: %u\n", max_bucket_size);
    
    uint32_t used_buckets = kv->num_buckets - empty_buckets;
    if (used_buckets > 0) {
        printf("Average bucket size: %.2f\n", 
               (float)kv->num_keys / used_buckets);
    }
    
    printf("========================\n\n");
}
