#ifndef MEMAP_H
#define MEMAP_H
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define merr -1

#define BITMAP_LAYER_WIDTH 320 // 每个高层位代表多少低层位，可修改

typedef uint32_t OFFSET;//偏移量
#define default_block_size 32//默认页大小
#define default_block_num 128//默认页数量

#define add_block_num_base 128//每次扩容的页数的基数

#define NULL_OFFSET 0xffffffff

struct MEMAP{
    uint8_t* strpool;
    MEMAP(uint32_t block_size,uint32_t block_num);
    ~MEMAP();
    OFFSET smalloc(uint32_t len);
    void sfree(OFFSET offset,uint32_t len);
    int iserr(OFFSET offset);
    int iserr();
    uint8_t* addr(OFFSET offset) {
        uint32_t block_num = *(uint32_t*)(strpool + 8);
        uint32_t layer_sizes[10], layer_offsets[10], total_bitmap_bytes = 0;
        get_layer_count(block_num, layer_sizes, layer_offsets, &total_bitmap_bytes);
        return strpool + 12 + total_bitmap_bytes + offset;
    }
    int get_layer_count(uint32_t block_num, uint32_t* layer_sizes, uint32_t* layer_offsets, uint32_t* total_bitmap_bytes);
    int find_free_blocks(uint8_t* strpool, uint32_t* layer_sizes, uint32_t* layer_offsets, int layer, int need_block_num, uint32_t block_size, uint32_t* found_start);
    void set_bits(uint8_t* bitmap, uint32_t start, uint32_t count);
    void clear_bits(uint8_t* bitmap, uint32_t start, uint32_t count);
    void update_upper_layers(uint8_t* strpool, uint32_t* layer_sizes, uint32_t* layer_offsets, int layer_count, uint32_t block_idx, int alloc);
};

/*************************************************************************
**  函数实现部分
**************************************************************************/

MEMAP::MEMAP(uint32_t block_size,uint32_t block_num){
    if(block_size%32!=0 || block_num%32!=0){
        block_size=default_block_size;  
        block_num=default_block_num;
    }
    uint32_t layer_sizes[10], layer_offsets[10], total_bitmap_bytes = 0;
    int layer_count = get_layer_count(block_num, layer_sizes, layer_offsets, &total_bitmap_bytes);
    uint32_t strpool_len = 12 + total_bitmap_bytes + block_size * block_num;
    strpool = (uint8_t*)calloc(1, strpool_len);
    if (strpool == NULL) return;
    *(uint32_t*)strpool = strpool_len;
    *(uint32_t*)(strpool + 4) = block_size;
    *(uint32_t*)(strpool + 8) = block_num;
}

MEMAP::~MEMAP(){
    free(strpool);
}

OFFSET MEMAP::smalloc(uint32_t len){
    if(len==0 || strpool==NULL) return NULL_OFFSET;
    
    uint32_t block_size = *(uint32_t*)(strpool + 4);
    uint32_t block_num = *(uint32_t*)(strpool + 8);

    uint32_t layer_sizes[10], layer_offsets[10], total_bitmap_bytes = 0;
    int layer_count = get_layer_count(block_num, layer_sizes, layer_offsets, &total_bitmap_bytes);

    uint32_t need_block_num = (len + block_size - 1) / block_size;
    uint32_t found_start = 0;
    if (find_free_blocks(strpool, layer_sizes, layer_offsets, layer_count - 1, need_block_num, block_size, &found_start)) {
        set_bits(strpool + layer_offsets[0], found_start, need_block_num);
        update_upper_layers(strpool, layer_sizes, layer_offsets, layer_count, found_start, 1);
        return found_start * block_size;
    }

    // 扩容处理
    uint32_t add_block_num = ((need_block_num + add_block_num_base - 1) / add_block_num_base) * add_block_num_base;
    uint32_t new_block_num = block_num + add_block_num;

    uint32_t new_layer_sizes[10], new_layer_offsets[10], new_total_bitmap_bytes = 0;
    int new_layer_count = get_layer_count(new_block_num, new_layer_sizes, new_layer_offsets, &new_total_bitmap_bytes);
    uint32_t new_strpool_len = 12 + new_total_bitmap_bytes + new_block_num * block_size;
    uint8_t* new_strpool = (uint8_t*)calloc(1, new_strpool_len);
    if (new_strpool == NULL) return NULL_OFFSET;

    memcpy(new_strpool, strpool, 12);
    for (int l = 0; l < layer_count && l < new_layer_count; l++) {
        uint32_t old_bytes = (layer_sizes[l] + 7) / 8;
        memcpy(new_strpool + new_layer_offsets[l], strpool + layer_offsets[l], old_bytes);
    }
    memcpy(new_strpool + 12 + new_total_bitmap_bytes, strpool + 12 + total_bitmap_bytes, block_num * block_size);

    // 新增块的低层位图已初始化为0（calloc），需要同步高层位图
    // 先清理新增块的低层位图（其实calloc已做），再同步所有高层位图
    for (uint32_t i = 0; i < new_block_num; ++i) {
        // 只需同步高层位图
        update_upper_layers(new_strpool, new_layer_sizes, new_layer_offsets, new_layer_count, i, 
            ((new_strpool + new_layer_offsets[0])[i >> 3] & (1 << (i & 7))) ? 1 : 0);
    }

    free(strpool);
    strpool = new_strpool;
    *(uint32_t*)strpool = new_strpool_len;
    *(uint32_t*)(strpool + 4) = block_size;
    *(uint32_t*)(strpool + 8) = new_block_num;

    if (find_free_blocks(strpool, new_layer_sizes, new_layer_offsets, new_layer_count - 1, need_block_num, block_size, &found_start)) {
        set_bits(strpool + new_layer_offsets[0], found_start, need_block_num);
        update_upper_layers(strpool, new_layer_sizes, new_layer_offsets, new_layer_count, found_start, 1);
        return found_start * block_size;
    }
    return NULL_OFFSET;
}

void MEMAP::sfree(OFFSET offset,uint32_t len) {
    if( !len || !strpool ) return;
    uint32_t block_size=*(uint32_t*)(strpool+4);
    uint32_t block_num=*(uint32_t*)(strpool+8);
    uint32_t layer_sizes[10], layer_offsets[10], total_bitmap_bytes = 0;
    int layer_count = get_layer_count(block_num, layer_sizes, layer_offsets, &total_bitmap_bytes);
    uint32_t need_block_num=( len + block_size - 1) / block_size;
    if(offset%block_size!=0) return;
    uint32_t i = offset/block_size;
    clear_bits(strpool + layer_offsets[0], i, need_block_num);
    update_upper_layers(strpool, layer_sizes, layer_offsets, layer_count, i, 0);
}

int MEMAP::iserr(){
    if (!strpool) return merr;
    uint32_t block_num = *(uint32_t*)(strpool + 8);
    uint32_t layer_sizes[10], layer_offsets[10], total_bitmap_bytes = 0;
    int layer_count = get_layer_count(block_num, layer_sizes, layer_offsets, &total_bitmap_bytes);
    uint32_t stored_len = *(uint32_t*)strpool;
    uint32_t block_size = *(uint32_t*)(strpool + 4);
    if (block_size%32 != 0 || block_num%32 != 0 || stored_len < 12) return merr;
    if (stored_len != 12 + total_bitmap_bytes + block_size * block_num) return merr;
    return 0;
}

int MEMAP::iserr(OFFSET offset){
    uint32_t block_size = *(uint32_t*)(strpool + 4);
    uint32_t block_num = *(uint32_t*)(strpool + 8);
    uint32_t layer_sizes[10], layer_offsets[10], total_bitmap_bytes = 0;
    get_layer_count(block_num, layer_sizes, layer_offsets, &total_bitmap_bytes);
    if (offset == NULL_OFFSET || offset % block_size != 0 || offset / block_size >= block_num) return merr;
    uint32_t idx = offset / block_size;
    if ((strpool + layer_offsets[0])[idx >> 3] & (1 << (idx & 7))) return 0;
    return 1;
}

int MEMAP::get_layer_count(uint32_t block_num, uint32_t* layer_sizes, uint32_t* layer_offsets, uint32_t* total_bitmap_bytes) {
    int layer = 0;
    uint32_t n = block_num;
    uint32_t offset = 12;
    while (layer < 10) {
        layer_sizes[layer] = n;
        layer_offsets[layer] = offset;
        uint32_t bytes = (n + 7) / 8;
        offset += bytes;
        n = (n + BITMAP_LAYER_WIDTH - 1) / BITMAP_LAYER_WIDTH;
        layer++;
        if (n <= 1) break;
    }
    *total_bitmap_bytes = offset - 12;
    return layer;
}

int MEMAP::find_free_blocks(uint8_t* strpool, uint32_t* layer_sizes, uint32_t* layer_offsets, int layer, int need_block_num, uint32_t block_size, uint32_t* found_start) {
    if (layer == 0) {
        uint8_t* bitmap = strpool + layer_offsets[0];
        uint32_t size = layer_sizes[0];
        uint32_t k = 0, start = 0;
        for (uint32_t i = 0; i < size; i++) {
            if (!(bitmap[i >> 3] & (1 << (i & 7)))) k++;
            else k = 0;
            if (k == (uint32_t)need_block_num) {
                *found_start = i - need_block_num + 1;
                return 1;
            }
        }
        return 0;
    } else {
        uint8_t* bitmap = strpool + layer_offsets[layer];
        uint32_t size = layer_sizes[layer];
        for (uint32_t i = 0; i < size; i++) {
            if (!(bitmap[i >> 3] & (1 << (i & 7)))) {
                // 递归到下层查找
                uint32_t low_start = i * BITMAP_LAYER_WIDTH;
                uint32_t low_end = low_start + BITMAP_LAYER_WIDTH;
                if (low_end > layer_sizes[layer - 1]) low_end = layer_sizes[layer - 1];
                // 只在该高层位对应的下层区块递归查找
                int found = find_free_blocks(strpool, layer_sizes, layer_offsets, layer - 1, need_block_num, block_size, found_start);
                if (found && *found_start >= low_start && *found_start + need_block_num <= low_end) {
                    return 1;
                }
            }
        }
        return 0;
    }
}

void MEMAP::set_bits(uint8_t* bitmap, uint32_t start, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        bitmap[(start + i) >> 3] |= (1 << ((start + i) & 7));
    }
}

void MEMAP::clear_bits(uint8_t* bitmap, uint32_t start, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        bitmap[(start + i) >> 3] &= ~(1 << ((start + i) & 7));
    }
}

void MEMAP::update_upper_layers(uint8_t* strpool, uint32_t* layer_sizes, uint32_t* layer_offsets, int layer_count, uint32_t block_idx, int alloc) {
    for (int l = 0; l < layer_count - 1; l++) {
        uint32_t idx = block_idx / BITMAP_LAYER_WIDTH;
        uint8_t* lower = strpool + layer_offsets[l];
        uint8_t* upper = strpool + layer_offsets[l + 1];
        uint32_t start = idx * BITMAP_LAYER_WIDTH;
        uint32_t end = start + BITMAP_LAYER_WIDTH;
        if (end > layer_sizes[l]) end = layer_sizes[l];
        int all_set = 1, all_clear = 1;
        for (uint32_t i = start; i < end; i++) {
            if (i >= layer_sizes[l]) break;
            if ((lower[i >> 3] & (1 << (i & 7)))) all_clear = 0;
            else all_set = 0;
        }
        if (all_set) upper[idx >> 3] |= (1 << (idx & 7));
        else upper[idx >> 3] &= ~(1 << (idx & 7));
        block_idx = idx;
    }
}

#endif