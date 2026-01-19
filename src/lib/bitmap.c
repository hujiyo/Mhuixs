#include "bitmap.h"
/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
#define merr -1

// ============================================================================
// 类型检查函数
// ============================================================================

/**
 * 检查 BHS 是否为 BITMAP 类型
 * @param bm BHS 指针
 * @return 1 表示是 BITMAP 类型, 0 表示不是
 */
int check_if_bitmap(const BHS* bm) {
    if (!bm) return 0;
    return (bm->type == BIGNUM_TYPE_BITMAP);
}

// ============================================================================
// 内部辅助函数
// ============================================================================

/**
 * 获取 bitmap 数据指针
 * 直接返回数据区指针，不需要跳过 size_t
 */
static inline uint8_t* get_bitmap_data(const BHS* bm) {
    if (!bm) return NULL;
    return (uint8_t*)(bm->is_large ? bm->data.large_data : bm->data.small_data);
}

/**
 * 扩展 bitmap 大小（支持扩大和缩小）
 * @param bm bitmap 指针
 * @param new_bit_num 新的位数
 * @return 0 成功, -1 失败
 */
static int bitmap_rexpand(BHS* bm, uint64_t new_bit_num) {
    if (!check_if_bitmap(bm)) return merr;
    
    uint64_t old_bit_num = bm->length;
    uint64_t old_byte_num = (old_bit_num + 7) / 8;
    uint64_t new_byte_num = (new_bit_num + 7) / 8;
    
    // 如果字节数相同，只需更新位数
    if (old_byte_num == new_byte_num) {
        bm->length = new_bit_num;
        return 0;
    }
    
    // 处理 small_data 到 large_data 的转换
    if (!bm->is_large && new_byte_num > BIGNUM_SMALL_SIZE) {
        char* new_data = (char*)malloc(new_byte_num);
        if (!new_data) return merr;
        
        // 复制旧数据
        memcpy(new_data, bm->data.small_data, old_byte_num);
        // 清零新增部分
        if (new_byte_num > old_byte_num) {
            memset(new_data + old_byte_num, 0, new_byte_num - old_byte_num);
        }
        
        bm->data.large_data = new_data;
        bm->is_large = 1;
        bm->capacity = new_byte_num;
        bm->length = new_bit_num;
        return 0;
    }
    
    // 处理 large_data 到 small_data 的转换
    if (bm->is_large && new_byte_num <= BIGNUM_SMALL_SIZE) {
        char temp[BIGNUM_SMALL_SIZE];
        memcpy(temp, bm->data.large_data, new_byte_num);
        
        free(bm->data.large_data);
        memcpy(bm->data.small_data, temp, new_byte_num);
        if (new_byte_num < BIGNUM_SMALL_SIZE) {
            memset(bm->data.small_data + new_byte_num, 0, BIGNUM_SMALL_SIZE - new_byte_num);
        }
        
        bm->is_large = 0;
        bm->capacity = BIGNUM_SMALL_SIZE;
        bm->length = new_bit_num;
        return 0;
    }
    
    // 处理 large_data 的扩大或缩小
    if (bm->is_large) {
        char* new_data = (char*)realloc(bm->data.large_data, new_byte_num);
        if (!new_data) return merr;
        
        // 如果是扩大，清零新增部分
        if (new_byte_num > old_byte_num) {
            memset(new_data + old_byte_num, 0, new_byte_num - old_byte_num);
        }
        
        bm->data.large_data = new_data;
        bm->capacity = new_byte_num;
        bm->length = new_bit_num;
        return 0;
    }
    
    // 处理 small_data 的扩大或缩小（仍在 small_data 范围内）
    if (!bm->is_large && new_byte_num <= BIGNUM_SMALL_SIZE) {
        // 如果是扩大，清零新增部分
        if (new_byte_num > old_byte_num) {
            memset(bm->data.small_data + old_byte_num, 0, new_byte_num - old_byte_num);
        }
        bm->length = new_bit_num;
        return 0;
    }
    
    return merr;
}

// ============================================================================
// 构造和析构函数
// ============================================================================

/**
 * 创建空 bitmap
 */
BHS* bitmap_create(void) {
    BHS* bm = (BHS*)malloc(sizeof(BHS));
    if (!bm) return NULL;
    
    memset(bm, 0, sizeof(BHS));
    bm->type = BIGNUM_TYPE_BITMAP;
    bm->length = 0;         // 0位
    bm->capacity = BIGNUM_SMALL_SIZE;  // 使用 small_data
    bm->is_large = 0;
    
    return bm;
}

/**
 * 创建指定大小的 bitmap
 */
BHS* bitmap_create_with_size(uint64_t bit_num) {
    /* 检查 bit_num 是否超过 SIZE_MAX - 1 */
    if (bit_num > SIZE_MAX - 1) return NULL;
    
    BHS* bm = (BHS*)malloc(sizeof(BHS));
    if (!bm) return NULL;
    
    memset(bm, 0, sizeof(BHS));
    bm->type = BIGNUM_TYPE_BITMAP;
    bm->length = (size_t)bit_num;   // 位数
    
    uint64_t byte_num = (bit_num + 7) / 8;
    
    /* 检查 byte_num 是否合理（避免分配过大内存导致系统崩溃） */
    if (byte_num > SIZE_MAX / 2) return NULL;  /* 限制为系统最大内存的一半 */
    
    // 判断使用 small_data 还是 large_data
    if (byte_num <= BIGNUM_SMALL_SIZE) {
        bm->is_large = 0;
        bm->capacity = BIGNUM_SMALL_SIZE;
        memset(bm->data.small_data, 0, BIGNUM_SMALL_SIZE);
    } else {
        bm->is_large = 1;
        bm->capacity = byte_num;
        bm->data.large_data = (char*)calloc(byte_num, 1);
        if (!bm->data.large_data) {
            #ifdef bitmap_debug
            perror("BITMAP memory init failed!");
            #endif
            free(bm);
            return NULL;
        }
    }
    
    return bm;
}

/**
 * 从字符串创建 bitmap（字符串格式："010101"）
 */
BHS* bitmap_create_from_string(const char* s) {
    if (!s) return NULL;
    
    uint64_t len = strlen(s);
    BHS* bm = bitmap_create_with_size(len);
    if (!bm) return NULL;
    
    // 设置位
    for (uint64_t i = 0; i < len; i++) {
        if (s[i] != '0') {
            bitmap_set(bm, i, 1);
        }
    }
    
    return bm;
}

/**
 * 拷贝构造
 */
BHS* bitmap_create_copy(const BHS* other) {
    if (!check_if_bitmap(other)) return NULL;
    
    BHS* bm = (BHS*)malloc(sizeof(BHS));
    if (!bm) return NULL;
    
    // 复制基本信息
    memcpy(bm, other, sizeof(BHS));
    
    // 处理数据复制
    if (other->is_large) {
        uint64_t byte_num = other->capacity;
        bm->data.large_data = (char*)malloc(byte_num);
        if (!bm->data.large_data) {
            #ifdef bitmap_debug
            perror("BITMAP memory init failed!");
            #endif
            free(bm);
            return NULL;
        }
        memcpy(bm->data.large_data, other->data.large_data, byte_num);
    }
    // small_data 已经在 memcpy 时复制了
    
    return bm;
}

/**
 * 从数据流创建 bitmap
 */
BHS* bitmap_create_from_data(char* s, uint64_t len, uint8_t zerochar) {
    if (!s) return NULL;
    
    BHS* bm = bitmap_create_with_size(len);
    if (!bm) return NULL;
    
    uint8_t* data = get_bitmap_data(bm);
    for (uint64_t i = 0; i < len; i++) {
        if (s[i] != zerochar) {
            data[i / 8] |= 1 << (i % 8);
        }
    }
    
    return bm;
}

/**
 * 销毁 bitmap
 */
void free_bitmap(BHS* bm) {
    if (!bm) return;
    
    if (check_if_bitmap(bm) && bm->is_large && bm->data.large_data) {
        free(bm->data.large_data);
    }
    free(bm);
}

// ============================================================================
// 赋值操作
// ============================================================================

/**
 * 从字符串赋值
 */
int bitmap_assign_string(BHS* bm, char* s) {
    if (!check_if_bitmap(bm) || !s) return merr;
    
    uint64_t len = strlen(s);
    uint64_t byte_num = (len + 7) / 8;
    
    // 释放旧数据
    if (bm->is_large && bm->data.large_data) {
        free(bm->data.large_data);
        bm->data.large_data = NULL;
    }
    
    // 分配新数据
    if (byte_num <= BIGNUM_SMALL_SIZE) {
        bm->is_large = 0;
        bm->capacity = BIGNUM_SMALL_SIZE;
        memset(bm->data.small_data, 0, BIGNUM_SMALL_SIZE);
    } else {
        bm->is_large = 1;
        bm->capacity = byte_num;
        bm->data.large_data = (char*)calloc(byte_num, 1);
        if (!bm->data.large_data) {
            #ifdef bitmap_debug
            perror("BITMAP memory init failed!");
            #endif
            return merr;
        }
    }
    
    bm->length = len;
    
    // 设置位
    uint8_t* data = get_bitmap_data(bm);
    for (uint64_t i = 0; i < len; i++) {
        if (s[i] != '0') {
            data[i / 8] |= 1 << (i % 8);
        }
    }
    
    return 0;
}

/**
 * 从另一个 bitmap 赋值
 */
int bitmap_assign_bitmap(BHS* bm, const BHS* other) {
    if (!check_if_bitmap(bm) || !check_if_bitmap(other)) return merr;
    if (bm == other) return 0; // 自赋值保护
    
    uint64_t bit_num = other->length;
    uint64_t byte_num = (bit_num + 7) / 8;
    
    // 释放旧数据
    if (bm->is_large && bm->data.large_data) {
        free(bm->data.large_data);
        bm->data.large_data = NULL;
    }
    
    // 分配新数据
    if (byte_num <= BIGNUM_SMALL_SIZE) {
        bm->is_large = 0;
        bm->capacity = BIGNUM_SMALL_SIZE;
        memcpy(bm->data.small_data, other->data.small_data, BIGNUM_SMALL_SIZE);
    } else {
        bm->is_large = 1;
        bm->capacity = byte_num;
        bm->data.large_data = (char*)malloc(byte_num);
        if (!bm->data.large_data) {
            #ifdef bitmap_debug
            perror("BITMAP memory allocation failed!");
            #endif
            return merr;
        }
        memcpy(bm->data.large_data, other->data.large_data, byte_num);
    }
    
    bm->type = BIGNUM_TYPE_BITMAP;
    bm->length = bit_num;
    
    return 0;
}

/**
 * 追加另一个 bitmap
 */
int bitmap_append(BHS* bm, BHS* other) {
    if (!check_if_bitmap(bm) || !check_if_bitmap(other)) {
        #ifdef bitmap_debug
        perror("BITMAP failed! other bitmap is NULL or invalid type!");
        #endif
        return merr;
    }
    
    uint64_t bm_size = bm->length;
    uint64_t other_size = other->length;
    uint64_t new_size = bm_size + other_size;
    uint64_t new_byte_num = (new_size + 7) / 8;
    
    // 创建临时数据存储旧数据
    char* temp_data;
    int temp_is_large = bm->is_large;
    uint64_t old_byte_num = (bm_size + 7) / 8;
    
    if (bm->is_large) {
        temp_data = bm->data.large_data;
    } else {
        temp_data = (char*)malloc(BIGNUM_SMALL_SIZE);
        if (!temp_data) return merr;
        memcpy(temp_data, bm->data.small_data, BIGNUM_SMALL_SIZE);
    }
    
    // 分配新数据
    if (new_byte_num <= BIGNUM_SMALL_SIZE) {
        bm->is_large = 0;
        bm->capacity = BIGNUM_SMALL_SIZE;
        memset(bm->data.small_data, 0, BIGNUM_SMALL_SIZE);
    } else {
        bm->is_large = 1;
        bm->capacity = new_byte_num;
        bm->data.large_data = (char*)calloc(new_byte_num, 1);
        if (!bm->data.large_data) {
            #ifdef bitmap_debug
            perror("BITMAP memory init failed!");
            #endif
            if (!temp_is_large) free(temp_data);
            return merr;
        }
    }
    
    bm->length = new_size;
    
    // 复制数据
    uint64_t dest_buffer_size = (new_size + 7) / 8;
    uint64_t bm_buffer_size = (bm_size + 7) / 8;
    uint64_t other_buffer_size = (other_size + 7) / 8;
    
    uint8_t* new_data = get_bitmap_data(bm);
    uint8_t* other_data = get_bitmap_data(other);
    
    bitcpy(new_data, 0, 
           (uint8_t*)temp_data, 0, 
           bm_size, dest_buffer_size, bm_buffer_size);
    
    bitcpy(new_data, bm_size, 
           other_data, 0, 
           other_size, dest_buffer_size, other_buffer_size);
    
    // 清理临时数据
    if (temp_is_large) {
        free(temp_data);
    } else {
        free(temp_data);
    }
    
    return 0;
}

// ============================================================================
// 访问和修改
// ============================================================================

/**
 * 获取某个位的值
 */
int bitmap_get(const BHS* bm, uint64_t offset) {
    if (!check_if_bitmap(bm)) return merr;
    if (offset >= bm->length) return merr;
    
    uint8_t* data = get_bitmap_data(bm);
    return (data[offset / 8] >> (offset % 8)) & 1;
}

/**
 * 设置单个位
 */
int bitmap_set(BHS* bm, uint64_t offset, uint8_t value) {
    if (!check_if_bitmap(bm)) return merr;
    
    if (offset >= bm->length && bitmap_rexpand(bm, offset + 1) == merr) {
        return merr;
    }
    
    uint8_t* data = get_bitmap_data(bm);
    if (value) {
        data[offset / 8] |= 1 << (offset % 8);
    } else {
        data[offset / 8] &= ~(1 << (offset % 8));
    }
    return 0;
}

/**
 * 设置一段位
 */
int bitmap_set_range(BHS* bm, uint64_t offset, uint64_t len, uint8_t value) {
    if (!check_if_bitmap(bm)) return merr;
    if (!len) return 0;
    
    if (offset + len > bm->length && bitmap_rexpand(bm, offset + len) == merr) {
        return merr;
    }
    
    uint8_t* data = get_bitmap_data(bm);
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

/**
 * 从数据流设置位
 */
int bitmap_set_from_stream(BHS* bm, uint64_t offset, uint64_t len, 
                          const char* data_stream, char zero_value) {
    if (!check_if_bitmap(bm) || !data_stream || !len) return merr;
    
    if (offset + len > bm->length && bitmap_rexpand(bm, offset + len) == merr) {
        return merr;
    }
    
    uint8_t* data = get_bitmap_data(bm);
    for (uint64_t i = 0; i < len; i++, offset++) {
        if (data_stream[i] == zero_value) {
            data[offset / 8] &= ~(1 << (offset % 8));
        } else {
            data[offset / 8] |= 1 << (offset % 8);
        }
    }
    return 0;
}

// ============================================================================
// 查询操作
// ============================================================================

/**
 * 获取 bitmap 大小
 */
uint64_t bitmap_size(const BHS* bm) {
    if (!check_if_bitmap(bm)) return 0;
    return bm->length;
}

/**
 * 统计指定范围内1的个数
 */
uint64_t bitmap_count(const BHS* bm, uint64_t st_offset, uint64_t ed_offset) {
    if (!check_if_bitmap(bm)) return merr;
    if (st_offset > ed_offset || ed_offset >= bm->length || st_offset >= bm->length) {
        return merr;
    }
    
    uint8_t* data = get_bitmap_data(bm);
    uint64_t sum = 0;
    for (; st_offset <= ed_offset; st_offset++) {
        if (data[st_offset / 8] & (1 << (st_offset % 8))) sum++;
    }
    return sum;
}

/**
 * 查找指定值的位
 */
int64_t bitmap_find(const BHS* bm, uint8_t value, uint64_t start, uint64_t end) {
    if (!check_if_bitmap(bm)) return merr;
    if (start > end || end >= bm->length) {
        return merr;
    }
    
    uint8_t* data = get_bitmap_data(bm);
    const uint64_t* p64 = (const uint64_t*)data;
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

// ============================================================================
// 状态和调试
// ============================================================================

/**
 * 检查错误状态并尝试修正
 */
int bitmap_iserr(BHS* bm) {
    if (!check_if_bitmap(bm)) return merr;
    
    uint64_t byte_num = (bm->length + 7) / 8;
    
    // 检查容量是否正确
    if (bm->is_large) {
        if (!bm->data.large_data) return merr;
        if (bm->capacity < byte_num) {
            // 尝试修正容量
            char* new_data = (char*)realloc(bm->data.large_data, byte_num);
            if (!new_data) return merr;
            bm->data.large_data = new_data;
            bm->capacity = byte_num;
        }
    } else {
        if (byte_num > BIGNUM_SMALL_SIZE) return merr;
    }
    
    return 0;
}

/**
 * 打印位图
 */
void bitmap_print(const BHS* bm) {
    if (!check_if_bitmap(bm)) {
        #ifdef bitmap_debug
        printf("BITMAP is in invalid state!\n");
        #endif
        return;
    }
    
    uint8_t* data = get_bitmap_data(bm);
    for (uint64_t i = 0; i < bm->length; i++) {
        printf("%d", (data[i / 8] >> (i % 8)) & 1);
    }
    printf("\n");
}

// ============================================================================
// 位运算操作（Logex支持）
// ============================================================================

/**
 * 按位与
 */
BHS* bitmap_bitand(const BHS* a, const BHS* b) {
    if (!check_if_bitmap(a) || !check_if_bitmap(b)) return NULL;
    
    /* 结果长度取较小值 */
    uint64_t result_len = (a->length < b->length) ? a->length : b->length;
    
    BHS* result = bitmap_create_with_size(result_len);
    if (!result) return NULL;
    
    uint8_t* a_data = get_bitmap_data(a);
    uint8_t* b_data = get_bitmap_data(b);
    uint8_t* result_data = get_bitmap_data(result);
    
    uint64_t byte_num = (result_len + 7) / 8;
    for (uint64_t i = 0; i < byte_num; i++) {
        result_data[i] = a_data[i] & b_data[i];
    }
    
    return result;
}

/**
 * 按位或
 */
BHS* bitmap_bitor(const BHS* a, const BHS* b) {
    if (!check_if_bitmap(a) || !check_if_bitmap(b)) return NULL;
    
    /* 结果长度取较大值 */
    uint64_t result_len = (a->length > b->length) ? a->length : b->length;
    
    BHS* result = bitmap_create_with_size(result_len);
    if (!result) return NULL;
    
    uint8_t* a_data = get_bitmap_data(a);
    uint8_t* b_data = get_bitmap_data(b);
    uint8_t* result_data = get_bitmap_data(result);
    
    uint64_t a_bytes = (a->length + 7) / 8;
    uint64_t b_bytes = (b->length + 7) / 8;
    uint64_t result_bytes = (result_len + 7) / 8;
    
    for (uint64_t i = 0; i < result_bytes; i++) {
        uint8_t a_byte = (i < a_bytes) ? a_data[i] : 0;
        uint8_t b_byte = (i < b_bytes) ? b_data[i] : 0;
        result_data[i] = a_byte | b_byte;
    }
    
    return result;
}

/**
 * 按位异或
 */
BHS* bitmap_bitxor(const BHS* a, const BHS* b) {
    if (!check_if_bitmap(a) || !check_if_bitmap(b)) return NULL;
    
    /* 结果长度取较大值 */
    uint64_t result_len = (a->length > b->length) ? a->length : b->length;
    
    BHS* result = bitmap_create_with_size(result_len);
    if (!result) return NULL;
    
    uint8_t* a_data = get_bitmap_data(a);
    uint8_t* b_data = get_bitmap_data(b);
    uint8_t* result_data = get_bitmap_data(result);
    
    uint64_t a_bytes = (a->length + 7) / 8;
    uint64_t b_bytes = (b->length + 7) / 8;
    uint64_t result_bytes = (result_len + 7) / 8;
    
    for (uint64_t i = 0; i < result_bytes; i++) {
        uint8_t a_byte = (i < a_bytes) ? a_data[i] : 0;
        uint8_t b_byte = (i < b_bytes) ? b_data[i] : 0;
        result_data[i] = a_byte ^ b_byte;
    }
    
    return result;
}

/**
 * 按位非
 */
BHS* bitmap_bitnot(const BHS* a) {
    if (!check_if_bitmap(a)) return NULL;
    
    BHS* result = bitmap_create_copy(a);
    if (!result) return NULL;
    
    uint8_t* result_data = get_bitmap_data(result);
    uint64_t byte_num = (a->length + 7) / 8;
    
    for (uint64_t i = 0; i < byte_num; i++) {
        result_data[i] = ~result_data[i];
    }
    
    /* 清除超出length的位 */
    if (a->length % 8 != 0) {
        uint8_t mask = (1 << (a->length % 8)) - 1;
        result_data[byte_num - 1] &= mask;
    }
    
    return result;
}

/**
 * 左移
 */
BHS* bitmap_bitshl(const BHS* a, uint64_t shift) {
    if (!check_if_bitmap(a)) return NULL;
    if (shift == 0) return bitmap_create_copy(a);
    
    /* 检查结果长度是否会溢出 SIZE_MAX - 1 */
    if (shift > SIZE_MAX - 1 - a->length) return NULL;
    
    uint64_t result_len = a->length + shift;
    BHS* result = bitmap_create_with_size(result_len);
    if (!result) return NULL;
    
    uint8_t* a_data = get_bitmap_data(a);
    uint8_t* result_data = get_bitmap_data(result);
    
    /* 复制并左移 */
    for (uint64_t i = 0; i < a->length; i++) {
        if ((a_data[i / 8] >> (i % 8)) & 1) {
            uint64_t new_pos = i + shift;
            result_data[new_pos / 8] |= (1 << (new_pos % 8));
        }
    }
    
    return result;
}

/**
 * 右移
 */
BHS* bitmap_bitshr(const BHS* a, uint64_t shift) {
    if (!check_if_bitmap(a)) return NULL;
    if (shift >= a->length) return bitmap_create_from_string("0");
    if (shift == 0) return bitmap_create_copy(a);
    
    uint64_t result_len = a->length - shift;
    BHS* result = bitmap_create_with_size(result_len);
    if (!result) return NULL;
    
    uint8_t* a_data = get_bitmap_data(a);
    uint8_t* result_data = get_bitmap_data(result);
    
    /* 复制并右移 */
    for (uint64_t i = shift; i < a->length; i++) {
        if ((a_data[i / 8] >> (i % 8)) & 1) {
            uint64_t new_pos = i - shift;
            result_data[new_pos / 8] |= (1 << (new_pos % 8));
        }
    }
    
    return result;
}
