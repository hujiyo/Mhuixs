#include "bitmap.h"
/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
#define merr -1

// 内部辅助函数：扩展bitmap大小
static int bitmap_rexpand(BITMAP* bm, uint64_t size) {
    if (!bm || !bm->bitmap) return merr;
    
    uint64_t old_byte_num = (*(uint64_t*)bm->bitmap + 7)/8 + sizeof(uint64_t);
    uint64_t new_byte_num = (size + 7)/8 + sizeof(uint64_t);
    
    if (old_byte_num >= new_byte_num) {
        // 缩小内存realloc一般不会失败，更新尺寸字段
        bm->bitmap = (uint8_t*)realloc(bm->bitmap, new_byte_num);
        *(uint64_t*)bm->bitmap = size;
        return 0;
    }
    return merr;
}

// 创建空bitmap
BITMAP* bitmap_create(void) {
    BITMAP* bm = (BITMAP*)malloc(sizeof(BITMAP));
    if (!bm) return NULL;
    bm->bitmap = (uint8_t*)calloc(8, 1);
    if (!bm->bitmap) {
        #ifdef bitmap_debug
        perror("BITMAP memory init failed!");
        #endif
        free(bm);
        return NULL;
    }
    return bm;
}

// 创建指定大小的bitmap
BITMAP* bitmap_create_with_size(uint64_t size) {
    BITMAP* bm = (BITMAP*)malloc(sizeof(BITMAP));
    if (!bm) return NULL;
    
    bm->bitmap = (uint8_t*)calloc((size+7)/8 + sizeof(uint64_t), 1);
    if (!bm->bitmap) {
        bm->bitmap = (uint8_t*)calloc(8, 1);
        #ifdef bitmap_debug
        perror("BITMAP memory init failed!");
        #endif
        free(bm);
        return NULL;
    }
    *(uint64_t*)bm->bitmap = size;
    return bm;
}

// 从字符串创建bitmap
BITMAP* bitmap_create_from_string(const char* s) {
    if (!s) return NULL;
    
    BITMAP* bm = (BITMAP*)malloc(sizeof(BITMAP));
    if (!bm) return NULL;
    
    uint64_t len = strlen(s);
    bm->bitmap = (uint8_t*)malloc(8 + len/8 + 1);
    if (!bm->bitmap) {
        #ifdef bitmap_debug
        perror("BITMAP memory init failed!");
        #endif
        free(bm);
        return NULL;
    }
    
    *(uint64_t*)bm->bitmap = len;
    for (uint64_t i = 0; i < len; i++) {
        if (s[i] == '0') bitmap_set(bm, i, 0);
        else bitmap_set(bm, i, 1);
    }
    return bm;
}

// 拷贝构造
BITMAP* bitmap_create_copy(const BITMAP* other) {
    if (!other || !other->bitmap) return NULL;
    
    BITMAP* bm = (BITMAP*)malloc(sizeof(BITMAP));
    if (!bm) return NULL;
    
    uint64_t size = *(uint64_t*)other->bitmap;
    bm->bitmap = (uint8_t*)calloc((size+7)/8 + sizeof(uint64_t), 1);
    if (!bm->bitmap) {
        bm->bitmap = (uint8_t*)calloc(8, 1);
        #ifdef bitmap_debug
        perror("BITMAP memory init failed!");
        #endif
        free(bm);
        return NULL;
    }
    memcpy(bm->bitmap, other->bitmap, (size+7)/8 + sizeof(uint64_t));
    return bm;
}

// 从数据流创建bitmap
BITMAP* bitmap_create_from_data(char* s, uint64_t len, uint8_t zerochar) {
    if (!s) return NULL;
    
    BITMAP* bm = (BITMAP*)malloc(sizeof(BITMAP));
    if (!bm) return NULL;
    
    bm->bitmap = (uint8_t*)calloc((len+7)/8 + sizeof(uint64_t), 1);
    if (!bm->bitmap) {
        #ifdef bitmap_debug
        perror("BITMAP memory init failed!");
        #endif
        free(bm);
        return NULL;
    }
    
    *(uint64_t*)bm->bitmap = len;
    uint8_t* first = bm->bitmap + sizeof(uint64_t);
    for (uint64_t i = 0; i < len; i++) {
        if (s[i] != zerochar) {
            first[i/8] |= 1 << (i%8);
        }
    }
    return bm;
}

// 销毁bitmap
void free_bitmap(BITMAP* bm) {
    if (bm) {
        if (bm->bitmap) free(bm->bitmap);
        free(bm);
    }
}

// 从字符串赋值
int bitmap_assign_string(BITMAP* bm, char* s) {
    if (!bm || !s) return merr;
    
    uint64_t len = strlen(s);
    uint8_t* temp = bm->bitmap;
    bm->bitmap = (uint8_t*)calloc((len+7)/8 + sizeof(uint64_t), 1);
    if (!bm->bitmap) {
        #ifdef bitmap_debug
        perror("BITMAP memory init failed!");
        #endif
        bm->bitmap = temp;
        return -1;
    }
    
    *(uint64_t*)bm->bitmap = len;
    for (uint64_t i = 0; i < len; i++) {
        if (s[i] - '0') {
            bm->bitmap[sizeof(uint64_t) + i/8] |= 1 << (i%8);
        } else {
            bm->bitmap[sizeof(uint64_t) + i/8] &= ~(1 << (i%8));
        }
    }
    free(temp);
    return 0;
}

// 从另一个bitmap赋值
int bitmap_assign_bitmap(BITMAP* bm, const BITMAP* other) {
    if (!bm || !other || !other->bitmap) return merr;
    if (bm == other) return 0; // 自赋值保护
    
    uint64_t size = *(uint64_t*)other->bitmap;
    uint64_t byte_num = (size + 7) / 8 + sizeof(uint64_t);
    
    uint8_t* new_bitmap = (uint8_t*)malloc(byte_num);
    if (!new_bitmap) {
        #ifdef bitmap_debug
        perror("BITMAP memory allocation failed!");
        #endif
        return merr;
    }
    
    memcpy(new_bitmap, other->bitmap, byte_num);
    if (bm->bitmap) free(bm->bitmap);
    bm->bitmap = new_bitmap;
    return 0;
}

// 追加另一个bitmap
int bitmap_append(BITMAP* bm, BITMAP* other) {
    if (!bm || !other || !other->bitmap) {
        #ifdef bitmap_debug
        perror("BITMAP failed! other bitmap is NULL!");
        #endif
        return -1;
    }
    
    uint64_t size = *(uint64_t*)bm->bitmap + *(uint64_t*)other->bitmap;
    uint8_t* temp = (uint8_t*)malloc((size+7)/8 + sizeof(uint64_t));
    if (!temp) {
        #ifdef bitmap_debug
        perror("BITMAP memory init failed!");
        #endif
        return -1;
    }
    bm->bitmap = temp;
    
    *(uint64_t*)bm->bitmap = size;
    uint64_t temp_size = *(uint64_t*)temp;
    uint64_t other_size = *(uint64_t*)other->bitmap;
    uint64_t dest_buffer_size = (size+7)/8;
    uint64_t temp_buffer_size = (temp_size+7)/8;
    uint64_t other_buffer_size = (other_size+7)/8;
    
    bitcpy(bm->bitmap+sizeof(uint64_t), 0, temp+sizeof(uint64_t), 0, temp_size, dest_buffer_size, temp_buffer_size);
    bitcpy(bm->bitmap+sizeof(uint64_t), temp_size, other->bitmap+sizeof(uint64_t), 0, other_size, dest_buffer_size, other_buffer_size);
    free(temp);
    return 0;
}

// 获取某个位的值
int bitmap_get(const BITMAP* bm, uint64_t offset) {
    if (!bm || !bm->bitmap) return merr;
    if (offset >= *(uint64_t*)bm->bitmap) return merr;
    
    uint8_t* first = bm->bitmap + sizeof(uint64_t);
    return (first[offset/8] >> (offset%8)) & 1;
}

// 设置单个位
int bitmap_set(BITMAP* bm, uint64_t offset, uint8_t value) {
    if (!bm || !bm->bitmap) return merr;
    
    if (offset >= *(uint64_t*)bm->bitmap && bitmap_rexpand(bm, offset+1) == merr) {
        return merr;
    }
    
    uint8_t* first = bm->bitmap + sizeof(uint64_t);
    if (value) {
        first[offset/8] |= 1 << (offset%8);
    } else {
        first[offset/8] &= ~(1 << (offset%8));
    }
    return 0;
}

// 设置一段位
int bitmap_set_range(BITMAP* bm, uint64_t offset, uint64_t len, uint8_t value) {
    if (!bm || !bm->bitmap) return merr;
    if (!len) return 0;
    
    if (offset + len > *(uint64_t*)bm->bitmap && bitmap_rexpand(bm, offset + len) == merr) {
        return merr;
    }
    
    uint8_t* data = bm->bitmap + sizeof(uint64_t);
    uint64_t s_byte = offset / 8;
    uint64_t e_bit = offset + len - 1;
    uint64_t e_byte = e_bit / 8;
    uint64_t s_bit = offset % 8;
    uint64_t e_bit_in_byte = e_bit % 8;
    
    if (s_byte == e_byte) {
        // 同一字节处理
        uint8_t mask = ((1 << (e_bit_in_byte + 1)) - 1) & ~((1 << s_bit) - 1);
        data[s_byte] = value ? (data[s_byte] | mask) : (data[s_byte] & ~mask);
        return 0;
    }
    
    // 处理头部：起始字节的s_bit到末尾
    uint8_t mask_head = (0xFF << s_bit) & 0xFF;
    data[s_byte] = value ? (data[s_byte] | mask_head) : (data[s_byte] & ~mask_head);
    
    // 处理中间完整字节
    uint64_t mid_bytes = e_byte - s_byte - 1;
    if (mid_bytes > 0) {
        memset(data + s_byte + 1, value ? 0xFF : 0, mid_bytes);
    }
    
    // 处理尾部：结束字节的0到e_bit_in_byte
    uint8_t mask_tail = (1 << (e_bit_in_byte + 1)) - 1;
    data[e_byte] = value ? (data[e_byte] | mask_tail) : (data[e_byte] & ~mask_tail);
    
    return 0;
}

// 从数据流设置位
int bitmap_set_from_stream(BITMAP* bm, uint64_t offset, uint64_t len, const char* data_stream, char zero_value) {
    if (!bm || !bm->bitmap || !data_stream || !len) return merr;
    
    if (offset + len > *(uint64_t*)bm->bitmap && bitmap_rexpand(bm, offset + len) == merr) {
        return merr;
    }
    
    uint8_t* first = bm->bitmap + sizeof(uint64_t);
    for (uint64_t i = 0; i < len; i++, offset++) {
        if (data_stream[i] == zero_value) {
            first[offset/8] &= ~(1 << (offset%8));
        } else {
            first[offset/8] |= 1 << (offset%8);
        }
    }
    return 0;
}

// 获取bitmap大小
uint64_t bitmap_size(const BITMAP* bm) {
    if (!bm || !bm->bitmap) return 0;
    return *(uint64_t*)bm->bitmap;
}

// 检查错误状态
int bitmap_iserr(BITMAP* bm) {
    if (!bm || !bm->bitmap) return merr;
    
    bm->bitmap = (uint8_t*)realloc(bm->bitmap, (*(uint64_t*)bm->bitmap + 7)/8 + sizeof(uint64_t));
    if (!bm->bitmap) return merr;
    
    return 0;
}

// 统计指定范围内1的个数
uint64_t bitmap_count(const BITMAP* bm, uint64_t st_offset, uint64_t ed_offset) {
    if (!bm || !bm->bitmap) return merr;
    if (st_offset > ed_offset || ed_offset >= *(uint64_t*)bm->bitmap || st_offset >= *(uint64_t*)bm->bitmap) {
        return merr;
    }
    
    uint8_t* first = bm->bitmap + sizeof(uint64_t);
    uint64_t sum = 0;
    for (; st_offset <= ed_offset; st_offset++) {
        if (first[st_offset/8] & (1 << (st_offset%8))) sum++;
    }
    return sum;
}

// 打印位图
void bitmap_print(const BITMAP* bm) {
    if (!bm || !bm->bitmap) {
        #ifdef bitmap_debug
        printf("BITMAP is in invalid state!\n");
        #endif
        return;
    }
    
    uint8_t* first = bm->bitmap + sizeof(uint64_t);
    for (uint64_t i = 0; i < *(uint64_t*)bm->bitmap; i++) {
        printf("%d", first[i/8] & (1 << (i%8)) ? 1 : 0);
    }
    printf("\n");
}

// 查找指定值的位
int64_t bitmap_find(const BITMAP* bm, uint8_t value, uint64_t start, uint64_t end) {
    if (!bm || !bm->bitmap) return merr;
    if (start > end || end >= *(uint64_t*)bm->bitmap) {
        return merr;
    }
    
    const uint64_t* p64 = (const uint64_t*)(bm->bitmap + sizeof(uint64_t));
    uint64_t pos = start;
    
    // 1. 处理起始的未对齐部分和单块内的查找
    uint64_t first_word_idx = pos / 64;
    uint64_t end_word_idx = end / 64;
    
    uint64_t chunk = p64[first_word_idx];
    
    // 创建掩码，屏蔽掉 pos 之前的所有位
    uint64_t start_mask = ~0ULL << (pos % 64);
    
    // 如果搜索范围在同一个64位块内
    if (first_word_idx == end_word_idx) {
        // 创建掩码，屏蔽掉 end 之后的所有位
        uint64_t end_mask = ~0ULL >> (63 - (end % 64));
        uint64_t mask = start_mask & end_mask;
        chunk &= mask;
    } else {
        chunk &= start_mask;
    }
    
    if (value) { // 查找 1
        if (chunk != 0) {
            for (int i = pos % 64; i < 64; ++i) {
                if ((chunk >> i) & 1) {
                    if (first_word_idx * 64 + i <= end) {
                        return first_word_idx * 64 + i;
                    }
                }
            }
        }
    } else { // 查找 0
        uint64_t search_chunk = ~chunk & start_mask;
        if (first_word_idx == end_word_idx) {
            uint64_t end_mask = ~0ULL >> (63 - (end % 64));
            search_chunk &= end_mask;
        }
        if (search_chunk != 0) {
            for (int i = pos % 64; i < 64; ++i) {
                if ((search_chunk >> i) & 1) {
                    if (first_word_idx * 64 + i <= end) {
                        return first_word_idx * 64 + i;
                    }
                }
            }
        }
    }
    
    if (first_word_idx == end_word_idx) {
        return merr;
    }
    
    // 2. 处理中间的全量64位块
    for (uint64_t i = first_word_idx + 1; i < end_word_idx; ++i) {
        chunk = p64[i];
        if (value) { // 查找 1
            if (chunk != 0) {
                for (int j = 0; j < 64; ++j) {
                    if ((chunk >> j) & 1) return i * 64 + j;
                }
            }
        } else { // 查找 0
            if (chunk != ~0ULL) {
                for (int j = 0; j < 64; ++j) {
                    if (!((chunk >> j) & 1)) return i * 64 + j;
                }
            }
        }
    }
    
    // 3. 处理结尾的未对齐部分
    chunk = p64[end_word_idx];
    uint64_t end_mask = ~0ULL >> (63 - (end % 64));
    
    if (value) {
        chunk &= end_mask;
        if (chunk != 0) {
            for (int i = 0; i <= (end % 64); ++i) {
                if ((chunk >> i) & 1) return end_word_idx * 64 + i;
            }
        }
    } else {
        chunk = ~chunk & end_mask;
        if (chunk != 0) {
            for (int i = 0; i <= (end % 64); ++i) {
                if ((chunk >> i) & 1) return end_word_idx * 64 + i;
            }
        }
    }
    
    return merr; // 如果没有找到，则返回merr
}

