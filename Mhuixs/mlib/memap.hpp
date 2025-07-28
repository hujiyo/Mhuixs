#ifndef MEMAP_H
#define MEMAP_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "tlsf.h"

#define merr -1
#define NULL_OFFSET 0xffffffff

typedef uint32_t OFFSET;

// 自动扩容时每次增加的块数基数
const uint32_t add_block_num = 64;

struct MEMAP {
    void* strpool; // 首池指针，兼容旧接口
    tlsf_t tlsf;
    uint32_t pool_bytes;

    uint32_t block_size;
    uint32_t block_num;

    // 记录所有池指针和大小
    std::vector<void*> pools;
    std::vector<uint32_t> pool_sizes;

    MEMAP(uint32_t block_size_, uint32_t block_num_) {
        block_size = block_size_;
        block_num = block_num_;
        
        // 确保初始池至少64字节（TLSF最小要求）
        pool_bytes = block_size * block_num;
        if (pool_bytes < 64) pool_bytes = 64;
        
        // 分配内存（使用标准malloc）
        strpool = malloc(pool_bytes + tlsf_size());
        if (!strpool) {
            tlsf = nullptr;
            return;
        }

        // 创建TLSF并添加首池
        tlsf = tlsf_create_with_pool(strpool, pool_bytes);
        if (!tlsf) {
            free(strpool);
            strpool = nullptr;
            return;
        }
        
        pools.push_back(strpool);
        pool_sizes.push_back(pool_bytes);
    }

    ~MEMAP() {
        if (tlsf) tlsf_destroy(tlsf);
        // 只手动释放首池，其他池由TLSF内部释放
        if (strpool) free(strpool);
    }

    OFFSET smalloc(uint32_t len) {
        if (!tlsf || len == 0) return NULL_OFFSET;
        void* ptr = tlsf_malloc(tlsf, len);
        if (!ptr) {
            uint32_t new_pool_bytes = std::max(len, pool_bytes * 2);
            if (new_pool_bytes < 64) new_pool_bytes = 64;
            void* new_pool = malloc(new_pool_bytes);
            if (!new_pool) return NULL_OFFSET;
            if (!tlsf_add_pool(tlsf, new_pool, new_pool_bytes)) {
                free(new_pool);
                return NULL_OFFSET;
            }
            pools.push_back(new_pool);
            pool_sizes.push_back(new_pool_bytes);
            ptr = tlsf_malloc(tlsf, len);
            if (!ptr) return NULL_OFFSET;
        }
        // 查找ptr属于哪个池，计算offset
        for (size_t i = 0; i < pools.size(); ++i) {
            uint8_t* base = (uint8_t*)pools[i];
            if ((uint8_t*)ptr >= base && (uint8_t*)ptr < base + pool_sizes[i]) {
                if (i == 0)
                    return (OFFSET)((uint8_t*)ptr - (uint8_t*)strpool);
                return (OFFSET)((i << 24) | ((uint8_t*)ptr - base));
            }
        }
        return NULL_OFFSET;
    }

    void sfree(OFFSET offset, uint32_t /*len*/) {
        if (!tlsf || offset == NULL_OFFSET) return;
        void* ptr = nullptr;
        if ((offset >> 24) == 0) {
            ptr = (uint8_t*)strpool + offset;
        } else {
            size_t pool_idx = offset >> 24;
            uint32_t off = offset & 0xFFFFFF;
            if (pool_idx < pools.size())
                ptr = (uint8_t*)pools[pool_idx] + off;
        }
        if (ptr)
            tlsf_free(tlsf, ptr);
    }

    int iserr(OFFSET offset) {
        if (!tlsf || offset == NULL_OFFSET) return merr;
        void* ptr = nullptr;
        if ((offset >> 24) == 0) {
            ptr = (uint8_t*)strpool + offset;
            if ((uint8_t*)ptr < (uint8_t*)strpool ||
                (uint8_t*)ptr >= (uint8_t*)strpool + pool_bytes)
                return merr;
        } else {
            size_t pool_idx = offset >> 24;
            uint32_t off = offset & 0xFFFFFF;
            if (pool_idx >= pools.size()) return merr;
            ptr = (uint8_t*)pools[pool_idx] + off;
            if ((uint8_t*)ptr < (uint8_t*)pools[pool_idx] ||
                (uint8_t*)ptr >= (uint8_t*)pools[pool_idx] + pool_sizes[pool_idx])
                return merr;
        }
        // TLSF无法直接判断块是否已分配，只能保证指针合法
        return 0;
    }

    int iserr() {
        if (!tlsf) return merr;
        if (tlsf_check(tlsf)) return merr;
        return 0;
    }

    uint8_t* addr(OFFSET offset) {
        if (!tlsf || offset == NULL_OFFSET) return nullptr;
        if ((offset >> 24) == 0) {
            return (uint8_t*)strpool + offset;
        } else {
            size_t pool_idx = offset >> 24;
            uint32_t off = offset & 0xFFFFFF;
            if (pool_idx >= pools.size()) return nullptr;
            return (uint8_t*)pools[pool_idx] + off;
        }
    }
};

#endif
