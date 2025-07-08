#include "memap.hpp"

MEMAP::MEMAP(uint32_t block_size_, uint32_t block_num_) {
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

MEMAP::~MEMAP() {
    if (tlsf) tlsf_destroy(tlsf);
    // 只手动释放首池，其他池由TLSF内部释放
    if (strpool) free(strpool);
}

MEMAP::MEMAP(const MEMAP& other) {
    block_size = other.block_size;
    block_num = other.block_num;
    pool_bytes = other.pool_bytes;
    tlsf = nullptr;
    strpool = nullptr;
    pools.clear();
    pool_sizes.clear();
    // 拷贝每个池
    for (size_t i = 0; i < other.pools.size(); ++i) {
        void* new_pool = malloc(other.pool_sizes[i]);
        if (!new_pool) continue; // 分配失败跳过
        memcpy(new_pool, other.pools[i], other.pool_sizes[i]);
        pools.push_back(new_pool);
        pool_sizes.push_back(other.pool_sizes[i]);
    }
    // 重新初始化TLSF
    if (!pools.empty()) {
        strpool = pools[0];
        tlsf = tlsf_create_with_pool(strpool, pool_sizes[0]);
        for (size_t i = 1; i < pools.size(); ++i) {
            tlsf_add_pool(tlsf, pools[i], pool_sizes[i]);
        }
    }
}

MEMAP& MEMAP::operator=(const MEMAP& other) {
    if (this == &other) return *this;
    // 释放自身资源
    if (tlsf) tlsf_destroy(tlsf);
    for (size_t i = 0; i < pools.size(); ++i) {
        free(pools[i]);
    }
    pools.clear();
    pool_sizes.clear();
    strpool = nullptr;
    tlsf = nullptr;
    // 拷贝参数
    block_size = other.block_size;
    block_num = other.block_num;
    pool_bytes = other.pool_bytes;
    // 拷贝每个池
    for (size_t i = 0; i < other.pools.size(); ++i) {
        void* new_pool = malloc(other.pool_sizes[i]);
        if (!new_pool) continue;
        memcpy(new_pool, other.pools[i], other.pool_sizes[i]);
        pools.push_back(new_pool);
        pool_sizes.push_back(other.pool_sizes[i]);
    }
    // 重新初始化TLSF
    if (!pools.empty()) {
        strpool = pools[0];
        tlsf = tlsf_create_with_pool(strpool, pool_sizes[0]);
        for (size_t i = 1; i < pools.size(); ++i) {
            tlsf_add_pool(tlsf, pools[i], pool_sizes[i]);
        }
    }
    return *this;
}

OFFSET MEMAP::smalloc(uint32_t len) {
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

void MEMAP::sfree(OFFSET offset, uint32_t /*len*/) {
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

int MEMAP::iserr(OFFSET offset) {
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

int MEMAP::iserr() {
    if (!tlsf) return merr;
    if (tlsf_check(tlsf)) return merr;
    return 0;
}

uint8_t* MEMAP::addr(OFFSET offset) {
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

// 获取所有池预分配内存总和（单位KB，向上取整）
uint32_t MEMAP::used_kb() const {
    uint64_t total = 0;
    for (size_t i = 0; i < pool_sizes.size(); ++i) {
        total += pool_sizes[i];
    }
    return (uint32_t)((total + 1023) / 1024);
}