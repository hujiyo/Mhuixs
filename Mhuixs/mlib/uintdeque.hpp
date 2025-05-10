#ifndef UINTDEQUE_HPP
#define UINTDEQUE_HPP

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
        Block* prev;
        Block* next;
        Block() : size(0), start(UINTDEQUE_BLOCK_SIZE / 2), prev(NULL), next(NULL) {}
        // 返回块内第i个元素的真实下标
        uint32_t idx(uint32_t i) const { return start + i; }
        // 返回块内可插入的最左/最右位置
        uint32_t left_space() const { return start; }
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
    Block* head;
    Block* tail;
    uint32_t num; // 总元素数
    uint32_t* movebuf; // 专用搬移缓冲区

    void locate(uint32_t pos, Block*& blk, uint32_t& offset) {
        blk = head;
        uint32_t idx = pos;
        while (blk && idx >= blk->size) {
            idx -= blk->size;
            blk = blk->next;
        }
        offset = idx;
    }

    void center_block(Block* blk) {
        blk->check_valid();
        if (blk->is_centered()) return;
        uint32_t new_start = (UINTDEQUE_BLOCK_SIZE - blk->size) / 2;
        if (new_start + blk->size > UINTDEQUE_BLOCK_SIZE) new_start = 0;
        if (new_start == blk->start) return;
        memmove(&blk->data[new_start], &blk->data[blk->start], blk->size * sizeof(uint32_t));
        blk->start = new_start;
        blk->check_valid();
    }

    void split_block(Block* blk) {
        blk->check_valid();
        if (blk->size < UINTDEQUE_BLOCK_SIZE) return;
        uint32_t mid = blk->size / 2;
        Block* new_blk = (Block*)calloc(1, sizeof(Block));
        new_blk->size = blk->size - mid;
        new_blk->start = (UINTDEQUE_BLOCK_SIZE - new_blk->size) / 2;
        if (new_blk->start + new_blk->size > UINTDEQUE_BLOCK_SIZE) new_blk->start = 0;
        memcpy(&new_blk->data[new_blk->start], &blk->data[blk->start + mid], new_blk->size * sizeof(uint32_t));
        new_blk->prev = blk;
        new_blk->next = blk->next;
        if (blk->next) blk->next->prev = new_blk;
        blk->next = new_blk;
        if (tail == blk) tail = new_blk;
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
        memcpy(&blk->data[blk->start + blk->size], &nxt->data[nxt->start], nxt->size * sizeof(uint32_t));
        blk->size += nxt->size;
        blk->next = nxt->next;
        if (nxt->next) nxt->next->prev = blk;
        if (tail == nxt) tail = blk;
        center_block(blk);
        free(nxt);
        blk->check_valid();
    }
public:
    UintDeque() : head(NULL), tail(NULL), num(0) {
        movebuf = (uint32_t*)malloc(UINTDEQUE_BLOCK_SIZE * sizeof(uint32_t));
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
        if (!head || head->left_space() == 0) {
            Block* blk = (Block*)calloc(1, sizeof(Block));
            blk->size = 0;
            blk->start = UINTDEQUE_BLOCK_SIZE / 2;
            blk->prev = NULL;
            blk->next = head;
            if (head) head->prev = blk;
            head = blk;
            if (!tail) tail = blk;
        }
        if (head->left_space() == 0) center_block(head);
        if (head->left_space() == 0) {
            split_block(head);
            if (head->left_space() == 0) return merr;
        }
        if (head->start == 0) {
            center_block(head);
            if (head->start == 0) {
                split_block(head);
                if (head->start == 0) return merr;
            }
        }
        head->start--;
        head->data[head->start] = value;
        head->size++;
        num++;
        head->check_valid();
        return 0;
    }

    int rpush(uint32_t value) {
        if (!tail || tail->right_space() == 0) {
            Block* blk = (Block*)calloc(1, sizeof(Block));
            blk->size = 0;
            blk->start = UINTDEQUE_BLOCK_SIZE / 2;
            blk->next = NULL;
            blk->prev = tail;
            if (tail) tail->next = blk;
            tail = blk;
            if (!head) head = blk;
        }
        if (tail->right_space() == 0) center_block(tail);
        if (tail->right_space() == 0) {
            split_block(tail);
            if (tail->right_space() == 0) return merr;
        }
        if (tail->start + tail->size >= UINTDEQUE_BLOCK_SIZE) {
            center_block(tail);
            if (tail->start + tail->size >= UINTDEQUE_BLOCK_SIZE) {
                split_block(tail);
                if (tail->start + tail->size >= UINTDEQUE_BLOCK_SIZE) return merr;
            }
        }
        tail->data[tail->start + tail->size] = value;
        tail->size++;
        num++;
        tail->check_valid();
        return 0;
    }

    int64_t lpop() {
        if (!num) return merr;
        uint32_t ret = head->data[head->start];
        head->start++;
        head->size--;
        num--;
        head->check_valid();
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
        uint32_t ret = tail->data[tail->start + tail->size - 1];
        tail->size--;
        num--;
        tail->check_valid();
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

