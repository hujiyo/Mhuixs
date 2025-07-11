#ifndef MEMAP_HPP
#define MEMAP_HPP

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "tlsf.h"

#define merr -1
#define NULL_OFFSET 0xffffffff

typedef uint32_t OFFSET;

// 自动扩容时每次增加的块数基数
#define add_block_num 1024

struct MEMAP {
    void* strpool; // 首池指针，兼容旧接口
    tlsf_t tlsf;
    uint32_t pool_bytes;

    uint32_t block_size;
    uint32_t block_num;

    // 记录所有池指针和大小
    vector<void*> pools;
    vector<uint32_t> pool_sizes;

    MEMAP(uint32_t block_size_, uint32_t block_num_);
    MEMAP(const MEMAP& other);
    ~MEMAP();
    MEMAP& operator=(const MEMAP& other);
    OFFSET smalloc(uint32_t len);
    void sfree(OFFSET offset, uint32_t /*len*/);
    int iserr(OFFSET offset);
    int iserr();
    uint8_t* addr(OFFSET offset);
    // 获取所有池预分配内存总和（单位KB，向上取整）
    uint32_t used_kb() const;
};

#endif
