#include "list.h"

// Block 内部辅助函数
static void locate(const LIST* lst, size_t pos, Block** blk, size_t* offset);
static void center_block(Block* blk);
static void split_block(LIST* lst, Block* blk);
static void merge_block(LIST* lst, Block* blk);
static size_t block_left_space(const Block* blk);
static size_t block_right_space(const Block* blk);
static int block_is_centered(const Block* blk);

// Block 函数实现
static size_t block_left_space(const Block* blk) {
    return blk->start;
}

static size_t block_right_space(const Block* blk) {
    return UINTDEQUE_BLOCK_SIZE - (blk->start + blk->size);
}

static int block_is_centered(const Block* blk) {
    return blk->start >= UINTDEQUE_BLOCK_SIZE / 4 && 
           (blk->start + blk->size) <= UINTDEQUE_BLOCK_SIZE * 3 / 4;
}

// LIST 内部辅助函数实现
static void locate(const LIST* lst, size_t pos, Block** blk, size_t* offset) {
    *blk = lst->head_block;
    while (*blk && pos >= (*blk)->size) {
        pos -= (*blk)->size;
        *blk = (*blk)->next;
    }
    *offset = pos;
}

static void center_block(Block* blk) {
    size_t new_start = (UINTDEQUE_BLOCK_SIZE - blk->size) / 2;
    if (block_is_centered(blk) || new_start + blk->size > UINTDEQUE_BLOCK_SIZE) return;
    memmove(&blk->data[new_start], &blk->data[blk->start], blk->size * sizeof(pointer));
    blk->start = new_start;
}

static void split_block(LIST* lst, Block* blk) {
    if (blk->size < UINTDEQUE_BLOCK_SIZE) return;
    size_t mid = blk->size / 2;
    Block* new_blk = (Block*)calloc(1, sizeof(Block));
    if (!new_blk) return;
    new_blk->size = blk->size - mid;
    new_blk->start = (UINTDEQUE_BLOCK_SIZE - new_blk->size) / 2;
    if (new_blk->start + new_blk->size > UINTDEQUE_BLOCK_SIZE) new_blk->start = 0;
    memcpy(&new_blk->data[new_blk->start], &blk->data[blk->start + mid], new_blk->size * sizeof(pointer));
    new_blk->prev = blk;
    new_blk->next = blk->next;
    if (blk->next) blk->next->prev = new_blk;
    blk->next = new_blk;
    if (lst->tail_block == blk) lst->tail_block = new_blk;
    blk->size = mid;
    center_block(blk);
    center_block(new_blk);
}

static void merge_block(LIST* lst, Block* blk) {
    if (!blk->next) return;
    Block* nxt = blk->next;
    if (blk->size + nxt->size > UINTDEQUE_BLOCK_SIZE) return;
    memmove(&blk->data[0], &blk->data[blk->start], blk->size * sizeof(pointer));
    memcpy(&blk->data[blk->size], &nxt->data[nxt->start], nxt->size * sizeof(pointer));
    blk->start = 0;
    blk->size += nxt->size;
    blk->next = nxt->next;
    if (nxt->next) nxt->next->prev = blk;
    if (lst->tail_block == nxt) lst->tail_block = blk;
    center_block(blk);
    free(nxt);
}

// LIST 公共函数实现
LIST* list_create(void) {
    LIST* lst = (LIST*)calloc(1, sizeof(LIST));
    if (!lst) return NULL;
    lst->head_block = NULL;
    lst->tail_block = NULL;
    lst->num = 0;
    return lst;
}

LIST* list_copy(const LIST* other) {
    if (!other) return NULL;
    LIST* lst = list_create();
    if (!lst) return NULL;
    
    Block* cur = other->head_block;
    while (cur) {
        Block* blk = (Block*)calloc(1, sizeof(Block));
        if (!blk) {
            list_destroy(lst);
            return NULL;
        }
        blk->size = cur->size;
        blk->start = cur->start;
        memcpy(&blk->data[0], &cur->data[0], sizeof(pointer) * UINTDEQUE_BLOCK_SIZE);
        blk->prev = lst->tail_block;
        blk->next = NULL;
        if (lst->tail_block) lst->tail_block->next = blk;
        else lst->head_block = blk;
        lst->tail_block = blk;
        cur = cur->next;
    }
    lst->num = other->num;
    return lst;
}

void list_destroy(LIST* lst) {
    if (!lst) return;
    list_clear(lst);
    free(lst);
}

void list_clear(LIST* lst) {
    if (!lst) return;
    Block* cur = lst->head_block;
    while (cur) {
        Block* nxt = cur->next;
        free(cur);
        cur = nxt;
    }
    lst->head_block = NULL;
    lst->tail_block = NULL;
    lst->num = 0;
}

size_t list_size(const LIST* lst) {
    return lst ? lst->num : 0;
}

int list_lpush(LIST* lst, pointer value) {
    if (!lst) return merr;
    // 如果没有头块或头块左边没有空间，创建新块
    if (!lst->head_block || block_left_space(lst->head_block) == 0) {
        Block* blk = (Block*)calloc(1, sizeof(Block));
        if (!blk) return merr;
        blk->size = 0;
        blk->start = UINTDEQUE_BLOCK_SIZE / 2;
        blk->prev = NULL;
        blk->next = lst->head_block;
        if (lst->head_block) lst->head_block->prev = blk;
        lst->head_block = blk;
        if (!lst->tail_block) lst->tail_block = blk;
    }
    // 如果左边还是没空间，尝试居中或分裂
    if (block_left_space(lst->head_block) == 0) {
        center_block(lst->head_block);
        if (block_left_space(lst->head_block) == 0) {
            split_block(lst, lst->head_block);
            if (block_left_space(lst->head_block) == 0) return merr;
        }
    }
    // 插入元素
    lst->head_block->start--;
    lst->head_block->data[lst->head_block->start] = value;
    lst->head_block->size++;
    lst->num++;
    return 0;
}

int list_rpush(LIST* lst, pointer value) {
    if (!lst) return merr;
    // 如果没有尾块或尾块右边没有空间，创建新块
    if (!lst->tail_block || block_right_space(lst->tail_block) == 0) {
        Block* blk = (Block*)calloc(1, sizeof(Block));
        if (!blk) return merr;
        blk->size = 0;
        blk->start = UINTDEQUE_BLOCK_SIZE / 2;
        blk->next = NULL;
        blk->prev = lst->tail_block;
        if (lst->tail_block) lst->tail_block->next = blk;
        lst->tail_block = blk;
        if (!lst->head_block) lst->head_block = blk;
    }
    // 如果右边还是没空间，尝试居中或分裂
    if (block_right_space(lst->tail_block) == 0) {
        center_block(lst->tail_block);
        if (block_right_space(lst->tail_block) == 0) {
            split_block(lst, lst->tail_block);
            if (block_right_space(lst->tail_block) == 0) return merr;
        }
    }
    // 插入元素
    lst->tail_block->data[lst->tail_block->start + lst->tail_block->size] = value;
    lst->tail_block->size++;
    lst->num++;
    return 0;
}

pointer list_lpop(LIST* lst) {
    if (!lst || !lst->num) return (pointer)(intptr_t)merr;
    pointer ret = lst->head_block->data[lst->head_block->start];
    lst->head_block->start++;
    lst->head_block->size--;
    lst->num--;
    if (lst->head_block->size == 0) {
        Block* old = lst->head_block;
        lst->head_block = lst->head_block->next;
        if (lst->head_block) lst->head_block->prev = NULL;
        else lst->tail_block = NULL;
        free(old);
    } else if (lst->head_block->size < MIN_BLOCK_SIZE && lst->head_block->next) {
        merge_block(lst, lst->head_block);
    }
    return ret;
}

pointer list_rpop(LIST* lst) {
    if (!lst || !lst->num) return (pointer)(intptr_t)merr;
    pointer ret = lst->tail_block->data[lst->tail_block->start + lst->tail_block->size - 1];
    lst->tail_block->size--;
    lst->num--;
    if (lst->tail_block->size == 0) {
        Block* old = lst->tail_block;
        lst->tail_block = lst->tail_block->prev;
        if (lst->tail_block) lst->tail_block->next = NULL;
        else lst->head_block = NULL;
        free(old);
    } else if (lst->tail_block->size < MIN_BLOCK_SIZE && lst->tail_block->prev) {
        merge_block(lst, lst->tail_block->prev);
    }
    return ret;
}

int list_insert(LIST* lst, size_t pos, pointer value) {
    if (!lst || pos > lst->num) return merr;
    if (pos == 0) return list_lpush(lst, value);
    if (pos == lst->num) return list_rpush(lst, value);
    Block* blk;
    size_t offset;
    locate(lst, pos, &blk, &offset);
    if (!blk) return merr;
    if (blk->size == UINTDEQUE_BLOCK_SIZE || block_left_space(blk) == 0 || block_right_space(blk) == 0) {
        center_block(blk);
        if (blk->size == UINTDEQUE_BLOCK_SIZE || block_left_space(blk) == 0 || block_right_space(blk) == 0) {
            split_block(lst, blk);
            locate(lst, pos, &blk, &offset);
            if (!blk) return merr;
        }
    }
    if (offset <= blk->size / 2 && block_left_space(blk) > 0) {
        // 插入位置在前半部分，且左边有空间，左移前半部分
        memmove(&blk->data[blk->start - 1], &blk->data[blk->start], offset * sizeof(pointer));
        blk->start--;
        blk->data[blk->start + offset] = value;
    } else if (block_right_space(blk) > 0) {
        // 插入位置在后半部分，或左边没空间，右移后半部分
        memmove(&blk->data[blk->start + offset + 1], &blk->data[blk->start + offset], (blk->size - offset) * sizeof(pointer));
        blk->data[blk->start + offset] = value;
    } else {
        return merr;  // 没有空间
    }
    blk->size++;
    lst->num++;
    return 0;
}

int list_rm_index(LIST* lst, size_t pos) {
    if (!lst || pos >= lst->num) return merr;
    Block* blk;
    size_t offset;
    locate(lst, pos, &blk, &offset);
    if (!blk) return merr;
    if (offset < blk->size / 2) {
        memmove(&blk->data[blk->start + 1], &blk->data[blk->start], offset * sizeof(pointer));
        blk->start++;
    } else {
        memmove(&blk->data[blk->start + offset], &blk->data[blk->start + offset + 1], (blk->size - offset - 1) * sizeof(pointer));
    }
    blk->size--;
    lst->num--;
    if (blk->size == 0) {
        if (blk->prev) blk->prev->next = blk->next;
        if (blk->next) blk->next->prev = blk->prev;
        if (lst->head_block == blk) lst->head_block = blk->next;
        if (lst->tail_block == blk) lst->tail_block = blk->prev;
        free(blk);
    } else if (blk->size < MIN_BLOCK_SIZE && blk->next) {
        merge_block(lst, blk);
    }
    return 0;
}

pointer list_get_index(const LIST* lst, size_t pos) {
    if (!lst || pos >= lst->num) return (pointer)(intptr_t)merr;
    Block* blk;
    size_t offset;
    locate(lst, pos, &blk, &offset);
    if (!blk) return (pointer)(intptr_t)merr;
    return blk->data[blk->start + offset];
}

int list_set_index(LIST* lst, size_t pos, pointer value) {
    if (!lst || pos >= lst->num) return merr;
    Block* blk;
    size_t offset;
    locate(lst, pos, &blk, &offset);
    if (!blk) return merr;
    blk->data[blk->start + offset] = value;
    return 0;
}

int list_swap(LIST* lst, size_t idx1, size_t idx2) {
    if (!lst || idx1 >= lst->num || idx2 >= lst->num) return merr;
    if (idx1 == idx2) return 0;
    Block* blk1;
    size_t offset1;
    Block* blk2;
    size_t offset2;
    locate(lst, idx1, &blk1, &offset1);
    locate(lst, idx2, &blk2, &offset2);
    if (!blk1 || !blk2) return merr;
    pointer temp = blk1->data[blk1->start + offset1];
    blk1->data[blk1->start + offset1] = blk2->data[blk2->start + offset2];
    blk2->data[blk2->start + offset2] = temp;
    return 0;
}

