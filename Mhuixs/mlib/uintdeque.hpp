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
        Block() : size(0), start(UINTDEQUE_BLOCK_SIZE / 2), prev(NULL), next(NULL) {}        
        uint32_t idx(uint32_t i) const { return start + i; }// 返回块内第i个元素的真实下标        
        uint32_t left_space() const { return start; }// 返回块内可插入的最左/最右位置
        uint32_t right_space() const { return UINTDEQUE_BLOCK_SIZE - (start + size); }
        // 判断是否“居中”
        bool is_centered() const { 
            return start > UINTDEQUE_BLOCK_SIZE / 4 && (start + size) < UINTDEQUE_BLOCK_SIZE * 3 / 4; 
        }
        // 边界断言
        void check_valid() const {
            assert(start <= UINTDEQUE_BLOCK_SIZE);
            assert(size <= UINTDEQUE_BLOCK_SIZE);
            assert(start + size <= UINTDEQUE_BLOCK_SIZE);
        }
    };
    Block* head_block;
    Block* tail_block;
    uint32_t num; // 总元素数

    // 定位到pos所在的块和偏移量
    void locate(uint32_t pos, Block* &blk, uint32_t &offset) {
        blk = head_block;
        while (blk && pos >= blk->size) {
            pos -= blk->size;
            blk = blk->next;
        }
        offset = pos;
    }

    // 块居中
    void center_block(Block* blk) {
        blk->check_valid();// 检查块的合法性
        uint32_t new_start = (UINTDEQUE_BLOCK_SIZE - blk->size) / 2;
        if (blk->is_centered() || new_start + blk->size > UINTDEQUE_BLOCK_SIZE) return;
        memmove(&blk->data[new_start], &blk->data[blk->start], blk->size * sizeof(uint32_t));
        blk->start = new_start;
    }

    void split_block(Block* blk) {
        blk->check_valid();
        if (blk->size < UINTDEQUE_BLOCK_SIZE) return;
        uint32_t mid = blk->size / 2;// 计算中间位置
        Block* new_blk = (Block*)calloc(1, sizeof(Block));
        new_blk->size = blk->size - mid;
        new_blk->start = (UINTDEQUE_BLOCK_SIZE - new_blk->size) / 2;
        if (new_blk->start + new_blk->size > UINTDEQUE_BLOCK_SIZE) new_blk->start = 0;
        memcpy(&new_blk->data[new_blk->start], &blk->data[blk->start + mid], new_blk->size * sizeof(uint32_t));
        new_blk->prev = blk;
        new_blk->next = blk->next;
        if (blk->next) blk->next->prev = new_blk;
        blk->next = new_blk;
        if (tail_block == blk) tail_block = new_blk;
        blk->size = mid;
        center_block(blk);
        center_block(new_blk);
        blk->check_valid();
        new_blk->check_valid();
    }

    void merge_block(Block* blk) {
        blk->check_valid();
        if (!blk->next) return;
        Block* nxt = blk->next;
        nxt->check_valid();
        if (blk->size + nxt->size > UINTDEQUE_BLOCK_SIZE) return;
        // 先把所有数据搬到start=0
        memmove(&blk->data[0], &blk->data[blk->start], blk->size * sizeof(uint32_t));
        memcpy(&blk->data[blk->size], &nxt->data[nxt->start], nxt->size * sizeof(uint32_t));
        blk->start = 0;
        blk->size += nxt->size;
        blk->next = nxt->next;
        if (nxt->next) nxt->next->prev = blk;
        if (tail_block == nxt) tail_block = blk;
        center_block(blk);
        free(nxt);
        blk->check_valid();
    }
    /*
    void merge_block(Block* blk) {
        blk->check_valid();
        if (!blk->next) return;
        Block* nxt = blk->next;
        nxt->check_valid();
        if (blk->size + nxt->size > UINTDEQUE_BLOCK_SIZE) return;
        memcpy(&blk->data[blk->start + blk->size], &nxt->data[nxt->start], nxt->size * sizeof(uint32_t));
        blk->size += nxt->size;
        blk->next = nxt->next;
        if (nxt->next) nxt->next->prev = blk;
        if (tail_block == nxt) tail_block = blk;
        center_block(blk);
        free(nxt);
        blk->check_valid();
    }
    */
public:
    UintDeque() : head_block(NULL), tail_block(NULL), num(0) {}
    ~UintDeque() {
        clear();
    }

    void clear() {
        Block* cur = head_block;
        while (cur) {
            Block* nxt = cur->next;
            free(cur);
            cur = nxt;
        }
        head_block = tail_block = NULL;
        num = 0;
    }

    uint32_t size() { return num; }

    int lpush(uint32_t value) {
        if (!head_block || head_block->left_space() == 0) {
            Block* blk = (Block*)calloc(1, sizeof(Block));
            blk->size = 0;
            blk->start = UINTDEQUE_BLOCK_SIZE / 2;
            blk->prev = NULL;
            blk->next = head_block;
            if (head_block) head_block->prev = blk;
            head_block = blk;
            if (!tail_block) tail_block = blk;
        }
        if (head_block->left_space() == 0) center_block(head_block);
        if (head_block->left_space() == 0) {
            split_block(head_block);
            if (head_block->left_space() == 0) return merr;
        }
        if (head_block->start == 0) {
            center_block(head_block);
            if (head_block->start == 0) {
                split_block(head_block);
                if (head_block->start == 0) return merr;
            }
        }
        head_block->start--;
        head_block->data[head_block->start] = value;
        head_block->size++;
        num++;
        head_block->check_valid();
        return 0;
    }

    int rpush(uint32_t value) {
        if (!tail_block || tail_block->right_space() == 0) {
            Block* blk = (Block*)calloc(1, sizeof(Block));
            blk->size = 0;
            blk->start = UINTDEQUE_BLOCK_SIZE / 2;
            blk->next = NULL;
            blk->prev = tail_block;
            if (tail_block) tail_block->next = blk;
            tail_block = blk;
            if (!head_block) head_block = blk;
        }
        if (tail_block->right_space() == 0) center_block(tail_block);
        if (tail_block->right_space() == 0) {
            split_block(tail_block);
            if (tail_block->right_space() == 0) return merr;
        }
        if (tail_block->start + tail_block->size >= UINTDEQUE_BLOCK_SIZE) {
            center_block(tail_block);
            if (tail_block->start + tail_block->size >= UINTDEQUE_BLOCK_SIZE) {
                split_block(tail_block);
                if (tail_block->start + tail_block->size >= UINTDEQUE_BLOCK_SIZE) return merr;
            }
        }
        tail_block->data[tail_block->start + tail_block->size] = value;
        tail_block->size++;
        num++;
        tail_block->check_valid();
        return 0;
    }

    int64_t lpop() {
        if (!num) return merr;
        uint32_t ret = head_block->data[head_block->start];
        head_block->start++;
        head_block->size--;
        num--;
        head_block->check_valid();
        if (head_block->size == 0) {
            Block* old = head_block;
            head_block = head_block->next;
            if (head_block) head_block->prev = NULL;
            else tail_block = NULL;
            free(old);
        } else if (head_block->size < MIN_BLOCK_SIZE && head_block->next) {
            merge_block(head_block);
        }
        return ret;
    }

    int64_t rpop() {
        if (!num) return merr;
        uint32_t ret = tail_block->data[tail_block->start + tail_block->size - 1];
        tail_block->size--;
        num--;
        tail_block->check_valid();
        if (tail_block->size == 0) {
            Block* old = tail_block;
            tail_block = tail_block->prev;
            if (tail_block) tail_block->next = NULL;
            else head_block = NULL;
            free(old);
        } else if (tail_block->size < MIN_BLOCK_SIZE && tail_block->prev) {
            merge_block(tail_block->prev);
        }
        return ret;
    }

    int insert(uint32_t pos, uint32_t value) {
        if (pos > num) return merr;
        if (pos == 0) return lpush(value);
        if (pos == num) return rpush(value);
        Block* blk; uint32_t offset;
        locate(pos, blk, offset);
        if (!blk) return merr;
        // 插入前只要空间不足就分裂，分裂后重新定位
        if (blk->size == UINTDEQUE_BLOCK_SIZE || blk->left_space() == 0 || blk->right_space() == 0) {
            center_block(blk);
            if (blk->size == UINTDEQUE_BLOCK_SIZE || blk->left_space() == 0 || blk->right_space() == 0) {
                split_block(blk);
                locate(pos, blk, offset);
            }
        }
        // 判断左/右空间，决定搬移方向
        if (blk->left_space() > blk->right_space()) {
            blk->start--;
            for (uint32_t i = 0; i < offset; ++i)
                blk->data[blk->start + i] = blk->data[blk->start + i + 1];
            blk->data[blk->start + offset] = value;
        } else {
            for (uint32_t i = blk->size; i > offset; --i)
                blk->data[blk->start + i] = blk->data[blk->start + i - 1];
            blk->data[blk->start + offset] = value;
        }
        blk->size++;
        blk->check_valid();
        num++;
        // 插入后如有必要再分裂（但此时不会再插入到满块）
        return 0;
    }

    int rm_index(uint32_t pos) {
        if (pos >= num) return merr;
        Block* blk; uint32_t offset;
        locate(pos, blk, offset);
        if (!blk) return merr;
        if (offset < blk->size / 2) {
            for (uint32_t i = offset; i > 0; --i)
                blk->data[blk->start + i] = blk->data[blk->start + i - 1];
            blk->start++;
        } else {
            for (uint32_t i = offset; i < blk->size - 1; ++i)
                blk->data[blk->start + i] = blk->data[blk->start + i + 1];
        }
        blk->size--;
        num--;
        blk->check_valid();
        if (blk->size == 0) {
            if (blk->prev) blk->prev->next = blk->next;
            if (blk->next) blk->next->prev = blk->prev;
            if (head_block == blk) head_block = blk->next;
            if (tail_block == blk) tail_block = blk->prev;
            free(blk);
        } else if (blk->size < MIN_BLOCK_SIZE && blk->next) {
            merge_block(blk);
        }
        return 0;
    }

    int64_t get_index(uint32_t pos) {
        if (pos >= num) return merr;
        Block* blk; uint32_t offset;
        locate(pos, blk, offset);
        if (!blk) return merr;
        blk->check_valid();
        return blk->data[blk->start + offset];
    }

    int set_index(uint32_t pos, uint32_t value) {
        if (pos >= num) return merr;
        Block* blk; uint32_t offset;
        locate(pos, blk, offset);
        if (!blk) return merr;
        blk->check_valid();
        blk->data[blk->start + offset] = value;
        return 0;
    }
};

#endif

