#ifndef UINTDEQUE_HPP
#define UINTDEQUE_HPP

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define merr -1
#define BLOCK_SIZE 2048 // 每块最大元素数
#define MIN_BLOCK_SIZE 521 // 块合并的最小阈值

class UintDeque {
    struct Block {
        uint32_t data[BLOCK_SIZE];
        uint32_t size; // 当前块内元素数
        uint32_t start; // 块内数据起始下标（循环队列思想）
        Block* prev;
        Block* next;
        Block() : size(0), start(BLOCK_SIZE / 2), prev(NULL), next(NULL) {}
        // 返回块内第i个元素的真实下标
        uint32_t idx(uint32_t i) const { return (start + i) % BLOCK_SIZE; }
        // 返回块内可插入的最左/最右位置
        uint32_t left_space() const { return start; }
        uint32_t right_space() const { return BLOCK_SIZE - ((start + size) % BLOCK_SIZE); }
        // 判断是否“居中”
        bool is_centered() const { return start > BLOCK_SIZE / 4 && (start + size) < BLOCK_SIZE * 3 / 4; }
    };
    Block* head;
    Block* tail;
    uint32_t num; // 总元素数
    uint32_t* movebuf; // 专用搬移缓冲区

    // 定位到第pos个元素所在的块和块内偏移
    void locate(uint32_t pos, Block*& blk, uint32_t& offset) {
        blk = head;
        uint32_t idx = pos;
        while (blk && idx >= blk->size) {
            idx -= blk->size;
            blk = blk->next;
        }
        offset = idx;
    }

    // 居中搬移，使块内数据向中间靠拢
    void center_block(Block* blk) {
        if (blk->is_centered()) return;
        uint32_t new_start = (BLOCK_SIZE - blk->size) / 2;
        if (new_start == blk->start) return;
        uint32_t tmp[BLOCK_SIZE];
        for (uint32_t i = 0; i < blk->size; ++i)
            tmp[i] = blk->data[blk->idx(i)];
        for (uint32_t i = 0; i < blk->size; ++i)
            blk->data[new_start + i] = tmp[i];
        blk->start = new_start;
    }

    // 分裂块，保证新旧块都居中
    void split_block(Block* blk) {
        if (blk->size < BLOCK_SIZE) return;
        uint32_t mid = blk->size / 2;
        Block* new_blk = (Block*)calloc(1,sizeof(Block));
        new_blk->size = blk->size - mid;
        new_blk->start = (BLOCK_SIZE - new_blk->size) / 2;
        new_blk->prev = blk;
        new_blk->next = blk->next;
        if (blk->next) blk->next->prev = new_blk;
        blk->next = new_blk;
        if (tail == blk) tail = new_blk;
        // 拷贝后半部分到新块（考虑循环队列）
        for (uint32_t i = 0; i < new_blk->size; ++i)
            new_blk->data[new_blk->start + i] = blk->data[blk->idx(mid + i)];
        blk->size = mid;
        center_block(blk);
        center_block(new_blk);
    }

    // 合并块，合并后居中
    void merge_block(Block* blk) {
        if (!blk->next) return;
        Block* nxt = blk->next;
        if (blk->size + nxt->size > BLOCK_SIZE) return;
        // 合并数据（考虑循环队列）
        for (uint32_t i = 0; i < nxt->size; ++i)
            blk->data[blk->idx(blk->size + i)] = nxt->data[nxt->idx(i)];
        blk->size += nxt->size;
        blk->next = nxt->next;
        if (nxt->next) nxt->next->prev = blk;
        if (tail == nxt) tail = blk;
        center_block(blk);
        free(nxt);
    }
public:
    UintDeque() : head(NULL), tail(NULL), num(0) {
        movebuf = (uint32_t*)malloc(BLOCK_SIZE * sizeof(uint32_t));
    }
    ~UintDeque() {
        clear();
        free(movebuf);
    }

    void clear() {
        Block* cur = head;
        while (cur) {
            Block* nxt = cur->next;
            free(cur);
            cur = nxt;
        }
        head = tail = NULL;
        num = 0;
    }

    uint32_t size() { return num; }

    int lpush(uint32_t value) {
        if (!head || head->size == BLOCK_SIZE) {
            Block* blk = (Block*)calloc(1,sizeof(Block));
            blk->size = 0;
            blk->start = BLOCK_SIZE / 2;
            blk->prev = NULL;
            blk->next = head;
            if (head) head->prev = blk;
            head = blk;
            if (!tail) tail = blk;
        }
        if (head->left_space() == 0) {
            // 只有在元素数小于3/4块容量时才居中，否则直接分裂
            if (head->size < BLOCK_SIZE * 3 / 4) {
                center_block(head);
            } else {
                split_block(head);
            }
            // 再次判断空间
            if (head->left_space() == 0) return merr;
        }
        head->start--;
        head->data[head->start] = value; // 修正：写入 value
        head->size++;
        num++;
        return 0;
    }

    int rpush(uint32_t value) {
        if (!tail || tail->size == BLOCK_SIZE) {
            Block* blk = (Block*)calloc(1,sizeof(Block));
            blk->size = 0;
            blk->start = BLOCK_SIZE / 2;
            blk->next = NULL;
            blk->prev = tail;
            if (tail) tail->next = blk;
            tail = blk;
            if (!head) head = blk;
        }
        if (tail->right_space() == 0) {
            if (tail->size < BLOCK_SIZE * 3 / 4) {
                center_block(tail);
            } else {
                split_block(tail);
            }
            if (tail->right_space() == 0) return merr;
        }
        tail->data[tail->idx(tail->size)] = value; // 修正：考虑循环队列
        tail->size++;
        num++;
        return 0;
    }

    int64_t lpop() {
        if (!num) return merr;
        uint32_t ret = head->data[head->idx(0)]; // 修正：考虑循环队列
        head->start++;
        head->size--;
        num--;
        if (head->size == 0) {
            Block* old = head;
            head = head->next;
            if (head) head->prev = NULL;
            else tail = NULL;
            free(old);
        } else if (head->size < MIN_BLOCK_SIZE && head->next) {
            merge_block(head);
        }
        return ret;
    }

    int64_t rpop() {
        if (!num) return merr;
        uint32_t ret = tail->data[tail->idx(tail->size - 1)]; // 修正：考虑循环队列
        tail->size--;
        num--;
        if (tail->size == 0) {
            Block* old = tail;
            tail = tail->prev;
            if (tail) tail->next = NULL;
            else head = NULL;
            free(old);
        } else if (tail->size < MIN_BLOCK_SIZE && tail->prev) {
            merge_block(tail->prev);
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
        // 判断左/右空间，决定搬移方向
        if (blk->left_space() > blk->right_space()) {
            // 向左搬移
            if (blk->left_space() == 0) center_block(blk);
            blk->start--;
            // 利用movebuf进行安全搬移
            if (offset > 0) {
                memcpy(movebuf, &blk->data[blk->start + 1], offset * sizeof(uint32_t));
                memcpy(&blk->data[blk->start], movebuf, offset * sizeof(uint32_t));
            }
            blk->data[blk->start + offset] = value;
        } else {
            // 向右搬移
            if (blk->right_space() == 0) center_block(blk);
            if (blk->size > offset) {
                memcpy(movebuf, &blk->data[blk->start + offset], (blk->size - offset) * sizeof(uint32_t));
                memcpy(&blk->data[blk->start + offset + 1], movebuf, (blk->size - offset) * sizeof(uint32_t));
            }
            blk->data[blk->start + offset] = value;
        }
        blk->size++;
        num++;
        split_block(blk);
        return 0;
    }

    int rm_index(uint32_t pos) {
        if (pos >= num) return merr;
        Block* blk; uint32_t offset;
        locate(pos, blk, offset);
        if (!blk) return merr;
        if (offset < blk->size / 2) {
            if (offset > 0) {
                memcpy(movebuf, &blk->data[blk->start], offset * sizeof(uint32_t));
                memcpy(&blk->data[blk->start + 1], movebuf, offset * sizeof(uint32_t));
            }
            blk->start++;
        } else {
            if (blk->size - offset - 1 > 0) {
                memcpy(movebuf, &blk->data[blk->start + offset + 1], (blk->size - offset - 1) * sizeof(uint32_t));
                memcpy(&blk->data[blk->start + offset], movebuf, (blk->size - offset - 1) * sizeof(uint32_t));
            }
        }
        blk->size--;
        num--;
        if (blk->size == 0) {
            if (blk->prev) blk->prev->next = blk->next;
            if (blk->next) blk->next->prev = blk->prev;
            if (head == blk) head = blk->next;
            if (tail == blk) tail = blk->prev;
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
        return blk->data[blk->idx(offset)];
    }

    int set_index(uint32_t pos, uint32_t value) {
        if (pos >= num) return merr;
        Block* blk; uint32_t offset;
        locate(pos, blk, offset);
        if (!blk) return merr;
        blk->data[blk->idx(offset)] = value;
        return 0;
    }
};

#endif