#include "uintdeque.hpp"

// Block实现
UintDeque::Block::Block() : size(0), start(UINTDEQUE_BLOCK_SIZE / 2), prev(NULL), next(NULL) {}
uint32_t UintDeque::Block::idx(uint32_t i) const { return start + i; }
uint32_t UintDeque::Block::left_space() const { return start; }
uint32_t UintDeque::Block::right_space() const { return UINTDEQUE_BLOCK_SIZE - (start + size); }
bool UintDeque::Block::is_centered() const {
    return start > UINTDEQUE_BLOCK_SIZE / 4 && (start + size) < UINTDEQUE_BLOCK_SIZE * 3 / 4;
}
void UintDeque::Block::check_valid() const {
    assert(start <= UINTDEQUE_BLOCK_SIZE);
    assert(size <= UINTDEQUE_BLOCK_SIZE);
    assert(start + size <= UINTDEQUE_BLOCK_SIZE);
}

// UintDeque实现
void UintDeque::locate(uint32_t pos, Block* &blk, uint32_t &offset) {
    blk = head_block;
    while (blk && pos >= blk->size) {
        pos -= blk->size;
        blk = blk->next;
    }
    offset = pos;
}

void UintDeque::center_block(Block* blk) {
    blk->check_valid();
    uint32_t new_start = (UINTDEQUE_BLOCK_SIZE - blk->size) / 2;
    if (blk->is_centered() || new_start + blk->size > UINTDEQUE_BLOCK_SIZE) return;
    memmove(&blk->data[new_start], &blk->data[blk->start], blk->size * sizeof(uint64_t));
    blk->start = new_start;
}

void UintDeque::split_block(Block* blk) {
    blk->check_valid();
    if (blk->size < UINTDEQUE_BLOCK_SIZE) return;
    uint32_t mid = blk->size / 2;
    Block* new_blk = (Block*)calloc(1, sizeof(Block));
    new_blk->size = blk->size - mid;
    new_blk->start = (UINTDEQUE_BLOCK_SIZE - new_blk->size) / 2;
    if (new_blk->start + new_blk->size > UINTDEQUE_BLOCK_SIZE) new_blk->start = 0;
    memcpy(&new_blk->data[new_blk->start], &blk->data[blk->start + mid], new_blk->size * sizeof(uint64_t));
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

void UintDeque::merge_block(Block* blk) {
    blk->check_valid();
    if (!blk->next) return;
    Block* nxt = blk->next;
    nxt->check_valid();
    if (blk->size + nxt->size > UINTDEQUE_BLOCK_SIZE) return;
    memmove(&blk->data[0], &blk->data[blk->start], blk->size * sizeof(uint64_t));
    memcpy(&blk->data[blk->size], &nxt->data[nxt->start], nxt->size * sizeof(uint64_t));
    blk->start = 0;
    blk->size += nxt->size;
    blk->next = nxt->next;
    if (nxt->next) nxt->next->prev = blk;
    if (tail_block == nxt) tail_block = blk;
    center_block(blk);
    free(nxt);
    blk->check_valid();
}

UintDeque::UintDeque() : head_block(NULL), tail_block(NULL), num(0) {}

UintDeque::UintDeque(const UintDeque& other) : head_block(NULL), tail_block(NULL), num(0) {
    Block* cur = other.head_block;
    while (cur) {
        Block* blk = (Block*)calloc(1, sizeof(Block));
        blk->size = cur->size;
        blk->start = cur->start;
        memcpy(&blk->data[0], &cur->data[0], sizeof(uint64_t) * UINTDEQUE_BLOCK_SIZE);
        blk->prev = tail_block;
        blk->next = NULL;
        if (tail_block) tail_block->next = blk;
        else head_block = blk;
        tail_block = blk;
        cur = cur->next;
    }
    num = other.num;
}

UintDeque& UintDeque::operator=(const UintDeque& other) {
    if (this == &other) return *this;
    clear();
    Block* cur = other.head_block;
    while (cur) {
        Block* blk = (Block*)calloc(1, sizeof(Block));
        blk->size = cur->size;
        blk->start = cur->start;
        memcpy(&blk->data[0], &cur->data[0], sizeof(uint64_t) * UINTDEQUE_BLOCK_SIZE);
        blk->prev = tail_block;
        blk->next = NULL;
        if (tail_block) tail_block->next = blk;
        else head_block = blk;
        tail_block = blk;
        cur = cur->next;
    }
    num = other.num;
    return *this;
}

UintDeque::~UintDeque() {
    clear();
}

void UintDeque::clear() {
    Block* cur = head_block;
    while (cur) {
        Block* nxt = cur->next;
        free(cur);
        cur = nxt;
    }
    head_block = tail_block = NULL;
    num = 0;
}

uint32_t UintDeque::size() { return num; }

int UintDeque::lpush(uint64_t value) {
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

int UintDeque::rpush(uint64_t value) {
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

int64_t UintDeque::lpop() {
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

int64_t UintDeque::rpop() {
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

int UintDeque::insert(uint32_t pos, uint64_t value) {
    if (pos > num) return merr;
    if (pos == 0) return lpush(value);
    if (pos == num) return rpush(value);
    Block* blk; uint32_t offset;
    locate(pos, blk, offset);
    if (!blk) return merr;
    if (blk->size == UINTDEQUE_BLOCK_SIZE || blk->left_space() == 0 || blk->right_space() == 0) {
        center_block(blk);
        if (blk->size == UINTDEQUE_BLOCK_SIZE || blk->left_space() == 0 || blk->right_space() == 0) {
            split_block(blk);
            locate(pos, blk, offset);
        }
    }
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
    return 0;
}

int UintDeque::rm_index(uint32_t pos) {
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

int64_t UintDeque::get_index(uint32_t pos) {
    if (pos >= num) return merr;
    Block* blk; uint32_t offset;
    locate(pos, blk, offset);
    if (!blk) return merr;
    blk->check_valid();
    return blk->data[blk->start + offset];
}

int UintDeque::set_index(uint32_t pos, uint64_t value) {
    if (pos >= num) return merr;
    Block* blk; uint32_t offset;
    locate(pos, blk, offset);
    if (!blk) return merr;
    blk->check_valid();
    blk->data[blk->start + offset] = value;
    return 0;
}

int UintDeque::swap(uint32_t idx1, uint32_t idx2) {
    if (idx1 >= num || idx2 >= num) return merr;
    if (idx1 == idx2) return 0;
    Block* blk1; uint32_t offset1;
    Block* blk2; uint32_t offset2;
    locate(idx1, blk1, offset1);
    locate(idx2, blk2, offset2);
    if (!blk1 || !blk2) return merr;
    uint64_t temp = blk1->data[blk1->start + offset1];
    blk1->data[blk1->start + offset1] = blk2->data[blk2->start + offset2];
    blk2->data[blk2->start + offset2] = temp;
    return 0;
}