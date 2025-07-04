#ifndef UINTDEQUE_HPP
#define UINTDEQUE_HPP

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
#include <assert.h>

#define merr -1
#define UINTDEQUE_BLOCK_SIZE 4096 // 每块最大元素数
#define MIN_BLOCK_SIZE 512 // 块合并的最小阈值

class UintDeque {
    struct Block {
        uint32_t data[UINTDEQUE_BLOCK_SIZE];
        uint32_t size;  // 当前块内元素数
        uint32_t start; // 块内数据起始下标（data[start]为第一个元素）
        Block *prev,*next;
        Block();
        uint32_t idx(uint32_t i) const;
        uint32_t left_space() const;
        uint32_t right_space() const;
        bool is_centered() const;
        void check_valid() const;
    };
    Block* head_block;
    Block* tail_block;
    uint32_t num; // 总元素数

    void locate(uint32_t pos, Block* &blk, uint32_t &offset);
    void center_block(Block* blk);
    void split_block(Block* blk);
    void merge_block(Block* blk);
public:
    UintDeque();
    UintDeque(const UintDeque& other);
    UintDeque& operator=(const UintDeque& other);
    ~UintDeque();

    void clear();
    uint32_t size();
    int lpush(uint32_t value);
    int rpush(uint32_t value);
    int64_t lpop();
    int64_t rpop();
    int insert(uint32_t pos, uint32_t value);
    int rm_index(uint32_t pos);
    int64_t get_index(uint32_t pos);
    int set_index(uint32_t pos, uint32_t value);
};

#endif

