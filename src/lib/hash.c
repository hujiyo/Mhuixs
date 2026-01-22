#include "hash.h"
#include <stdio.h>
#include <assert.h>

/* ========================================
 * 内部辅助函数
 * ======================================== */

/* 向上取整到2的幂 */
static inline uint32_t next_power_of_2(uint32_t n) {
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}

/* MurmurHash3 算法（32位版本，优化版）*/
static inline uint32_t murmur_hash3(const uint8_t* data, size_t len) {
    uint32_t seed = 0x9747b28c;
    const int nblocks = len / 4;
    
    /* 处理4字节块 */
    for (int i = 0; i < nblocks; i++) {
        uint32_t k1;
        memcpy(&k1, data + i * 4, sizeof(k1));  /* 避免未对齐访问 */
        
        k1 *= 0xcc9e2d51;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= 0x1b873593;
        seed ^= k1;
        seed = (seed << 13) | (seed >> 19);
        seed = seed * 5 + 0xe6546b64;
    }
    
    /* 处理剩余字节 */
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
    
    /* 最终混合 */
    seed ^= len;
    seed ^= seed >> 16;
    seed *= 0x85ebca6b;
    seed ^= seed >> 13;
    seed *= 0xc2b2ae35;
    seed ^= seed >> 16;
    
    return seed;
}

/* 获取条目的键（处理内联/堆分配）*/
static inline const char* entry_get_key(const hash_entry_t* entry) {
    return entry->key_len <= HASH_INLINE_KEY_SIZE ? 
           entry->key.inline_key : entry->key.heap_key;
}

/* 设置条目的键 */
static inline int entry_set_key(hash_entry_t* entry, const char* key, uint16_t key_len) {
    if (key_len <= HASH_INLINE_KEY_SIZE) {
        /* 内联存储 */
        memcpy(entry->key.inline_key, key, key_len);
        entry->key.inline_key[key_len] = '\0';
    } else {
        /* 堆分配 */
        entry->key.heap_key = (char*)malloc(key_len + 1);
        if (!entry->key.heap_key) return -1;
        memcpy(entry->key.heap_key, key, key_len);
        entry->key.heap_key[key_len] = '\0';
    }
    entry->key_len = key_len;
    return 0;
}

/* 释放条目的键 */
static inline void entry_free_key(hash_entry_t* entry) {
    if (entry->key_len > HASH_INLINE_KEY_SIZE && entry->key.heap_key) {
        free(entry->key.heap_key);
        entry->key.heap_key = NULL;
    }
}

/* 比较键是否相等（快速路径优化）*/
static inline bool keys_equal(const hash_entry_t* entry, const char* key, 
                              uint16_t key_len, uint32_t hash) {
    /* 快速路径：先比较哈希值和长度 */
    if (entry->hash != hash || entry->key_len != key_len) {
        return false;
    }
    /* 慢速路径：比较实际内容 */
    return memcmp(entry_get_key(entry), key, key_len) == 0;
}

/* Robin Hood 插入辅助函数 */
static void insert_entry(hash_table_t* ht, const char* key, uint16_t key_len,
                        uint32_t hash, void* value, uint8_t psl) {
    uint32_t mask = ht->capacity - 1;
    uint32_t index = hash & mask;
    
    hash_entry_t temp_entry;
    temp_entry.state = HASH_OCCUPIED;
    temp_entry.psl = psl;
    temp_entry.key_len = key_len;
    temp_entry.hash = hash;
    temp_entry.value = value;
    
    /* 临时存储键 */
    char temp_key[256];
    if (key_len < sizeof(temp_key)) {
        memcpy(temp_key, key, key_len);
        temp_key[key_len] = '\0';
    }
    
    while (true) {
        hash_entry_t* entry = &ht->entries[index];
        
        /* 找到空槽位或墓碑 */
        if (entry->state != HASH_OCCUPIED) {
            *entry = temp_entry;
            if (key_len < sizeof(temp_key)) {
                entry_set_key(entry, temp_key, key_len);
            } else {
                entry_set_key(entry, key, key_len);
            }
            if (entry->psl > ht->stats.max_psl) {
                ht->stats.max_psl = entry->psl;
            }
            return;
        }
        
        /* Robin Hood：如果当前条目的 PSL 小于我们的，交换 */
        if (entry->psl < temp_entry.psl) {
            /* 保存当前条目的键 */
            char swap_key[256];
            const char* entry_key = entry_get_key(entry);
            if (entry->key_len < sizeof(swap_key)) {
                memcpy(swap_key, entry_key, entry->key_len);
                swap_key[entry->key_len] = '\0';
            }
            
            /* 交换 */
            hash_entry_t swap_entry = *entry;
            *entry = temp_entry;
            
            /* 设置新键 */
            if (key_len < sizeof(temp_key)) {
                entry_set_key(entry, temp_key, key_len);
            } else {
                entry_set_key(entry, key, key_len);
            }
            
            /* 继续插入被交换的条目 */
            temp_entry = swap_entry;
            if (swap_entry.key_len < sizeof(swap_key)) {
                memcpy(temp_key, swap_key, swap_entry.key_len);
                temp_key[swap_entry.key_len] = '\0';
            }
        }
        
        /* 移动到下一个槽位 */
        index = (index + 1) & mask;
        temp_entry.psl++;
        ht->stats.num_probes++;
    }
}

/* 扩容/缩容 */
static int hash_resize(hash_table_t* ht, uint32_t new_capacity) {
    if (new_capacity < HASH_INITIAL_CAPACITY) {
        new_capacity = HASH_INITIAL_CAPACITY;
    }
    new_capacity = next_power_of_2(new_capacity);
    
    /* 分配新数组 */
    hash_entry_t* new_entries = (hash_entry_t*)calloc(new_capacity, sizeof(hash_entry_t));
    if (!new_entries) return -1;
    
    /* 保存旧数据 */
    hash_entry_t* old_entries = ht->entries;
    uint32_t old_capacity = ht->capacity;
    
    /* 更新哈希表 */
    ht->entries = new_entries;
    ht->capacity = new_capacity;
    ht->size = 0;
    ht->tombstones = 0;
    ht->stats.max_psl = 0;
    
    /* 重新插入所有条目 */
    for (uint32_t i = 0; i < old_capacity; i++) {
        hash_entry_t* old_entry = &old_entries[i];
        if (old_entry->state == HASH_OCCUPIED) {
            const char* key = entry_get_key(old_entry);
            insert_entry(ht, key, old_entry->key_len, old_entry->hash, 
                        old_entry->value, 0);
            ht->size++;
            entry_free_key(old_entry);
        }
    }
    
    free(old_entries);
    ht->stats.num_resizes++;
    return 0;
}

/* ========================================
 * 公共 API 实现
 * ======================================== */

hash_table_t* hash_create(uint32_t initial_capacity) {
    if (initial_capacity < HASH_INITIAL_CAPACITY) {
        initial_capacity = HASH_INITIAL_CAPACITY;
    }
    initial_capacity = next_power_of_2(initial_capacity);
    
    hash_table_t* ht = (hash_table_t*)malloc(sizeof(hash_table_t));
    if (!ht) return NULL;
    
    ht->entries = (hash_entry_t*)calloc(initial_capacity, sizeof(hash_entry_t));
    if (!ht->entries) {
        free(ht);
        return NULL;
    }
    
    ht->capacity = initial_capacity;
    ht->size = 0;
    ht->tombstones = 0;
    ht->load_factor = HASH_MAX_LOAD_FACTOR;
    memset(&ht->stats, 0, sizeof(hash_stats_t));
    
    return ht;
}

int hash_put(hash_table_t* ht, const char* key, void* value) {
    if (!ht || !key) return -1;
    
    uint16_t key_len = strlen(key);
    uint32_t hash = murmur_hash3((const uint8_t*)key, key_len);
    uint32_t mask = ht->capacity - 1;
    uint32_t index = hash & mask;
    uint8_t psl = 0;
    
    ht->stats.num_inserts++;
    
    /* 查找是否已存在 */
    while (true) {
        hash_entry_t* entry = &ht->entries[index];
        
        if (entry->state == HASH_EMPTY) {
            break;  /* 未找到，需要插入 */
        }
        
        if (entry->state == HASH_OCCUPIED && 
            keys_equal(entry, key, key_len, hash)) {
            /* 找到了，更新值 */
            entry->value = value;
            return 0;
        }
        
        /* Robin Hood：如果 PSL 超过当前条目，说明不存在 */
        if (entry->state == HASH_OCCUPIED && psl > entry->psl) {
            break;
        }
        
        index = (index + 1) & mask;
        psl++;
        ht->stats.num_probes++;
        
        if (psl > 255) {  /* 防止无限循环 */
            return -1;
        }
    }
    
    /* 检查是否需要扩容 */
    float load = (float)(ht->size + ht->tombstones + 1) / ht->capacity;
    if (load > ht->load_factor) {
        if (hash_resize(ht, ht->capacity * HASH_GROWTH_FACTOR) != 0) {
            return -1;
        }
        /* 重新计算索引 */
        mask = ht->capacity - 1;
        psl = 0;
    }
    
    /* 插入新条目 */
    insert_entry(ht, key, key_len, hash, value, psl);
    ht->size++;
    
    return 0;
}

void* hash_get(const hash_table_t* ht, const char* key) {
    if (!ht || !key) return NULL;
    
    uint16_t key_len = strlen(key);
    uint32_t hash = murmur_hash3((const uint8_t*)key, key_len);
    uint32_t mask = ht->capacity - 1;
    uint32_t index = hash & mask;
    uint8_t psl = 0;
    
    ((hash_table_t*)ht)->stats.num_lookups++;  /* 统计（const_cast）*/
    
    while (true) {
        const hash_entry_t* entry = &ht->entries[index];
        
        if (entry->state == HASH_EMPTY) {
            return NULL;  /* 未找到 */
        }
        
        if (entry->state == HASH_OCCUPIED) {
            /* Robin Hood：如果 PSL 超过当前条目，说明不存在 */
            if (psl > entry->psl) {
                return NULL;
            }
            
            if (keys_equal(entry, key, key_len, hash)) {
                return entry->value;  /* 找到了 */
            }
        }
        
        index = (index + 1) & mask;
        psl++;
        ((hash_table_t*)ht)->stats.num_probes++;
        
        if (psl > 255) {  /* 防止无限循环 */
            return NULL;
        }
    }
}

void* hash_remove(hash_table_t* ht, const char* key) {
    if (!ht || !key) return NULL;
    
    uint16_t key_len = strlen(key);
    uint32_t hash = murmur_hash3((const uint8_t*)key, key_len);
    uint32_t mask = ht->capacity - 1;
    uint32_t index = hash & mask;
    uint8_t psl = 0;
    
    ht->stats.num_deletes++;
    
    while (true) {
        hash_entry_t* entry = &ht->entries[index];
        
        if (entry->state == HASH_EMPTY) {
            return NULL;  /* 未找到 */
        }
        
        if (entry->state == HASH_OCCUPIED) {
            if (psl > entry->psl) {
                return NULL;  /* 不存在 */
            }
            
            if (keys_equal(entry, key, key_len, hash)) {
                /* 找到了，删除 */
                void* value = entry->value;
                entry_free_key(entry);
                entry->state = HASH_DELETED;
                entry->value = NULL;
                ht->size--;
                ht->tombstones++;
                
                /* 如果墓碑太多，触发 rehash */
                if (ht->tombstones > ht->capacity / 4) {
                    hash_resize(ht, ht->capacity);
                }
                
                return value;
            }
        }
        
        index = (index + 1) & mask;
        psl++;
        ht->stats.num_probes++;
        
        if (psl > 255) {
            return NULL;
        }
    }
}

int hash_contains(const hash_table_t* ht, const char* key) {
    return hash_get(ht, key) != NULL;
}

void hash_clear(hash_table_t* ht, void (*free_value)(void*)) {
    if (!ht) return;
    
    for (uint32_t i = 0; i < ht->capacity; i++) {
        hash_entry_t* entry = &ht->entries[i];
        if (entry->state == HASH_OCCUPIED) {
            entry_free_key(entry);
            if (free_value && entry->value) {
                free_value(entry->value);
            }
        }
        entry->state = HASH_EMPTY;
        entry->value = NULL;
    }
    
    ht->size = 0;
    ht->tombstones = 0;
}

void hash_destroy(hash_table_t* ht, void (*free_value)(void*)) {
    if (!ht) return;
    
    hash_clear(ht, free_value);
    free(ht->entries);
    free(ht);
}

uint32_t hash_size(const hash_table_t* ht) {
    return ht ? ht->size : 0;
}

uint32_t hash_capacity(const hash_table_t* ht) {
    return ht ? ht->capacity : 0;
}

float hash_load_factor(const hash_table_t* ht) {
    return ht && ht->capacity > 0 ? (float)ht->size / ht->capacity : 0.0f;
}

int hash_reserve(hash_table_t* ht, uint32_t capacity) {
    if (!ht) return -1;
    
    uint32_t target = (uint32_t)(capacity / ht->load_factor);
    if (target > ht->capacity) {
        return hash_resize(ht, target);
    }
    return 0;
}

int hash_shrink_to_fit(hash_table_t* ht) {
    if (!ht) return -1;
    
    uint32_t target = (uint32_t)(ht->size / HASH_MIN_LOAD_FACTOR);
    if (target < ht->capacity / 2) {
        return hash_resize(ht, target);
    }
    return 0;
}

void hash_print_stats(const hash_table_t* ht) {
    if (!ht) return;
    
    printf("\n=== Hash Table Statistics ===\n");
    printf("Size: %u / %u (%.1f%% full)\n", 
           ht->size, ht->capacity, 100.0f * hash_load_factor(ht));
    printf("Tombstones: %u\n", ht->tombstones);
    printf("Lookups: %u (avg probes: %.2f)\n", 
           ht->stats.num_lookups, 
           ht->stats.num_lookups > 0 ? 
           (float)ht->stats.num_probes / ht->stats.num_lookups : 0.0f);
    printf("Inserts: %u\n", ht->stats.num_inserts);
    printf("Deletes: %u\n", ht->stats.num_deletes);
    printf("Resizes: %u\n", ht->stats.num_resizes);
    printf("Max PSL: %u\n", ht->stats.max_psl);
    printf("============================\n\n");
}

void hash_reset_stats(hash_table_t* ht) {
    if (ht) {
        memset(&ht->stats, 0, sizeof(hash_stats_t));
    }
}

hash_iterator_t hash_iterator_init(const hash_table_t* ht) {
    hash_iterator_t it;
    it.ht = ht;
    it.index = 0;
    return it;
}

int hash_iterator_next(hash_iterator_t* it, const char** key_out, void** value_out) {
    if (!it || !it->ht) return 0;
    
    while (it->index < it->ht->capacity) {
        const hash_entry_t* entry = &it->ht->entries[it->index++];
        if (entry->state == HASH_OCCUPIED) {
            if (key_out) *key_out = entry_get_key(entry);
            if (value_out) *value_out = entry->value;
            return 1;
        }
    }
    
    return 0;
}

void int_to_key(int value, char* buffer, size_t size) {
    snprintf(buffer, size, "%d", value);
}
