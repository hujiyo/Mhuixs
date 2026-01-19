#include "bignum.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>  /* for SIZE_MAX */

#include "lib/list.h"

/* 内部辅助函数声明 */
static int bignum_add_internal(const BHS *a, const BHS *b, BHS *result);
static int bignum_sub_internal(const BHS *a, const BHS *b, BHS *result);
static int bignum_mul_internal(const BHS *a, const BHS *b, BHS *result);
static int bignum_div_internal(const BHS *a, const BHS *b, BHS *result, int precision);
static int bignum_pow_internal(const BHS *base, const BHS *exponent, BHS *result, int precision);
static int bignum_mod_internal(const BHS *a, const BHS *b, BHS *result);

/* 创建并初始化一个新的 BHS */
BHS* bignum_create(void) {
    BHS *num = (BHS *)malloc(sizeof(BHS));
    if (num == NULL) return NULL;
    
    memset(num->data.small_data, 0, BIGNUM_SMALL_SIZE);
    num->length = 1;
    num->capacity = BIGNUM_SMALL_SIZE;
    num->type = BIGNUM_TYPE_NUMBER;
    num->is_large = 0;
    num->type_data.num.decimal_pos = 0;
    num->type_data.num.is_negative = 0;
    
    return num;
}

/* 销毁 BHS 并释放所有内存 */
void bignum_destroy(BHS *num) {
    if (num == NULL) return;
    
    /* 根据类型释放内部数据 */
    if (num->type == BIGNUM_TYPE_LIST) {
        /* 释放 LIST 类型的数据 */
        if (num->data.list != NULL) {
            free_list(num->data.list);
            num->data.list = NULL;
        }
    } else {
        /* 释放其他类型的数据（数字、字符串、位图） */
        if (num->is_large && num->data.large_data != NULL) {
            free(num->data.large_data);
            num->data.large_data = NULL;
        }
    }
    
    /* 然后释放结构体本身 */
    free(num);
}

/* 初始化大数为0（旧版本 API） */
void bignum_init(BHS *num) {
    memset(num->data.small_data, 0, BIGNUM_SMALL_SIZE);
    num->length = 1;
    num->capacity = BIGNUM_SMALL_SIZE;
    num->type = BIGNUM_TYPE_NUMBER;
    num->is_large = 0;
    num->type_data.num.decimal_pos = 0;
    num->type_data.num.is_negative = 0;
}

/* 清理大数（释放动态内存，旧版本 API） */
void bignum_free(BHS *num) {
    if (num == NULL) return;
    
    /* 根据类型释放内存 */
    if (num->type == BIGNUM_TYPE_LIST) {
        /* 列表类型：释放LIST结构 */
        if (num->data.list != NULL) {
            free_list(num->data.list);
            num->data.list = NULL;
        }
    } else {
        /* 其他类型：释放数据内存 */
        if (num->is_large && num->data.large_data != NULL) {
            free(num->data.large_data);
            num->data.large_data = NULL;
        }
    }
    
    num->is_large = 0;
    num->length = 0;
    num->capacity = 0;
    num->type = BIGNUM_TYPE_NULL;
}

/* 确保有足够容量，必要时扩展 */
static int bignum_ensure_capacity(BHS *num, int required_capacity) {
    if (num == NULL) return BIGNUM_ERROR;
    
    /* 检查数字类型的限制 - 临时计算允许使用更大空间 */
    if (num->type == BIGNUM_TYPE_NUMBER && required_capacity > BIGNUM_MAX_DIGITS * 2) {
        return BIGNUM_ERROR;  /* 临时计算空间最多允许 2 倍限制 */
    }
    
    /* 字符串类型无限制，数字类型有限制 */
    if (required_capacity <= num->capacity) {
        return BIGNUM_SUCCESS;  /* 容量足够 */
    }
    
    /* 需要扩展 */
    int new_capacity = required_capacity;
    
    /* 如果当前是小数据模式 */
    if (!num->is_large) {
        if (required_capacity <= BIGNUM_SMALL_SIZE) {
            return BIGNUM_SUCCESS;  /* 仍在小数据范围内 */
        }
        
        /* 需要切换到大数据模式 */
        char *new_data = (char *)malloc(new_capacity);
        if (new_data == NULL) return BIGNUM_ERROR;
        
        /* 复制旧数据 - 对于 bitmap 类型，需要转换位数到字节数 */
        size_t copy_size = num->length;
        if (num->type == BIGNUM_TYPE_BITMAP) {
            copy_size = (num->length + 7) / 8;
        }
        memcpy(new_data, num->data.small_data, copy_size);
        memset(new_data + copy_size, 0, new_capacity - copy_size);
        
        /* 切换到大数据模式 */
        num->data.large_data = new_data;
        num->is_large = 1;
        num->capacity = new_capacity;
    } else {
        /* 已经是大数据模式，重新分配 */
        char *new_data = (char *)realloc(num->data.large_data, new_capacity);
        if (new_data == NULL) return BIGNUM_ERROR;
        
        /* 清零新分配的部分 */
        if (new_capacity > num->capacity) {
            memset(new_data + num->capacity, 0, new_capacity - num->capacity);
        }
        
        num->data.large_data = new_data;
        num->capacity = new_capacity;
    }
    
    return BIGNUM_SUCCESS;
}

/* 复制BigNum */
int bignum_copy(const BHS *src, BHS *dst) {
    if (src == NULL || dst == NULL) return BIGNUM_ERROR;
    
    /* 检查源数据长度是否在最终结果的合法范围内 */
    if (src->type == BIGNUM_TYPE_NUMBER && src->length > BIGNUM_MAX_DIGITS) {
        return BIGNUM_ERROR;  /* 最终结果超出限制 */
    }
    
    /* 先清理目标 */
    bignum_free(dst);
    
    /* 复制基本字段 */
    dst->length = src->length;
    dst->type = src->type;
    dst->type_data = src->type_data;  /* 复制整个类型数据联合体 */
    
    /* 根据类型复制数据 */
    if (src->type == BIGNUM_TYPE_LIST) {
        /* 列表类型：复制LIST指针 */
        if (src->data.list != NULL) {
            LIST *new_list = list_copy(src->data.list);
            if (new_list == NULL) {
                return BIGNUM_ERROR;
            }
            dst->data.list = new_list;
        } else {
            dst->data.list = NULL;
        }
        dst->is_large = 0;  /* 列表类型不使用large_data */
        dst->capacity = 0;
    } else {
        /* 其他类型：复制数据内容 */
        size_t copy_size = src->length;
        /* 对于 bitmap 类型，length 是位数，需要转换为字节数 */
        if (src->type == BIGNUM_TYPE_BITMAP) {
            copy_size = (src->length + 7) / 8;
        }
        
        if (src->is_large) {
            /* 源是大数据 */
            if (bignum_ensure_capacity(dst, src->capacity) != BIGNUM_SUCCESS) {
                return BIGNUM_ERROR;
            }
            memcpy(BIGNUM_DIGITS(dst), BIGNUM_DIGITS(src), copy_size);
        } else {
            /* 源是小数据 */
            dst->is_large = 0;
            dst->capacity = BIGNUM_SMALL_SIZE;
            memcpy(dst->data.small_data, src->data.small_data, copy_size);
        }
    }
    
    return BIGNUM_SUCCESS;
}

/* 移除前导零 */
static void bignum_trim(BHS *num) {
    char *digits = BIGNUM_DIGITS(num);
    
    /* 去除高位的0 */
    while (num->length > 1 && digits[num->length - 1] == 0) {
        num->length--;
    }
    
    /* 去除小数部分低位的0 */
    if (num->type == BIGNUM_TYPE_NUMBER && num->type_data.num.decimal_pos > 0) {
        int trailing_zeros = 0;
        /* 从最低位开始数有多少个连续的0 */
        for (int i = 0; i < num->type_data.num.decimal_pos && i < num->length; i++) {
            if (digits[i] == 0) {
                trailing_zeros++;
            } else {
                break;
            }
        }
        
        /* 如果有尾部的0，需要移除它们 */
        if (trailing_zeros > 0) {
            /* 将数字向左移动（去除低位的0） */
            for (size_t i = 0; i < num->length - trailing_zeros; i++) {
                digits[i] = digits[i + trailing_zeros];
            }
            num->length -= trailing_zeros;
            num->type_data.num.decimal_pos -= trailing_zeros;
        }
    }
    
    /* 如果结果是0，设置为正数（仅数字类型） */
    if (num->type == BIGNUM_TYPE_NUMBER && num->length == 1 && digits[0] == 0) {
        num->type_data.num.is_negative = 0;
        num->type_data.num.decimal_pos = 0;
    }
}

/* 比较两个大数的绝对值 (返回: 1 if a>b, 0 if a==b, -1 if a<b) */
static int bignum_compare_abs(const BHS *a, const BHS *b) {
    /* 先比较整数部分长度 */
    int a_int_len = a->length - a->type_data.num.decimal_pos;
    int b_int_len = b->length - b->type_data.num.decimal_pos;
    
    if (a_int_len > b_int_len) return 1;
    if (a_int_len < b_int_len) return -1;
    
    char *a_digits = BIGNUM_DIGITS(a);
    char *b_digits = BIGNUM_DIGITS(b);
    
    /* 从高位到低位比较 */
    for (int i = a->length - 1; i >= 0; i--) {
        int b_idx = i - a->type_data.num.decimal_pos + b->type_data.num.decimal_pos;
        int b_digit = (b_idx >= 0 && b_idx < b->length) ? b_digits[b_idx] : 0;
        int a_digit = a_digits[i];
        
        if (a_digit > b_digit) return 1;
        if (a_digit < b_digit) return -1;
    }
    
    /* 检查b是否有更多的小数位 */
    for (int i = a->type_data.num.decimal_pos; i < b->type_data.num.decimal_pos; i++) {
        if (i < b->length && b_digits[i] != 0) return -1;
    }
    
    return 0;
}

/* 大数绝对值加法（忽略符号） */
static int bignum_add_abs(const BHS *a, const BHS *b, BHS *result) {
    int max_decimal = (a->type_data.num.decimal_pos > b->type_data.num.decimal_pos) ? a->type_data.num.decimal_pos : b->type_data.num.decimal_pos;
    int max_len = ((a->length - a->type_data.num.decimal_pos) > (b->length - b->type_data.num.decimal_pos)) ? 
                   (a->length - a->type_data.num.decimal_pos) : (b->length - b->type_data.num.decimal_pos);
    max_len += max_decimal;
    
    bignum_free(result);
    bignum_init(result);
    result->type = a->type;  /* 继承类型 */
    
    /* 确保容量足够（需要max_len + 1用于进位） */
    if (bignum_ensure_capacity(result, max_len + 2) != BIGNUM_SUCCESS) {
        return BIGNUM_ERROR;
    }
    
    result->type_data.num.decimal_pos = max_decimal;
    
    char *a_digits = BIGNUM_DIGITS(a);
    char *b_digits = BIGNUM_DIGITS(b);
    char *result_digits = BIGNUM_DIGITS(result);
    
    int carry = 0;
    int result_len = 0;
    
    for (int i = 0; i < max_len + 1; i++) {
        int a_idx = i - max_decimal + a->type_data.num.decimal_pos;
        int b_idx = i - max_decimal + b->type_data.num.decimal_pos;
        
        int a_digit = (a_idx >= 0 && a_idx < a->length) ? a_digits[a_idx] : 0;
        int b_digit = (b_idx >= 0 && b_idx < b->length) ? b_digits[b_idx] : 0;
        
        int sum = a_digit + b_digit + carry;
        result_digits[i] = sum % 10;
        carry = sum / 10;
        result_len = i + 1;
        
        if (i >= max_len && carry == 0) break;
    }
    
    result->length = result_len;
    bignum_trim(result);
    return BIGNUM_SUCCESS;
}

/* 大数绝对值减法（a - b，假设 a >= b） */
static int bignum_sub_abs(const BHS *a, const BHS *b, BHS *result) {
    int max_decimal = (a->type_data.num.decimal_pos > b->type_data.num.decimal_pos) ? a->type_data.num.decimal_pos : b->type_data.num.decimal_pos;
    
    bignum_free(result);
    bignum_init(result);
    result->type = a->type;  /* 继承类型 */
    result->type_data.num.decimal_pos = max_decimal;
    
    int max_len = (a->length > max_decimal) ? a->length : max_decimal;
    if (bignum_ensure_capacity(result, max_len + 1) != BIGNUM_SUCCESS) {
        return BIGNUM_ERROR;
    }
    
    char *a_digits = BIGNUM_DIGITS(a);
    char *b_digits = BIGNUM_DIGITS(b);
    char *result_digits = BIGNUM_DIGITS(result);
    
    int borrow = 0;
    int result_len = 0;
    
    for (int i = 0; i < a->length || i < max_decimal; i++) {
        int a_idx = i - max_decimal + a->type_data.num.decimal_pos;
        int b_idx = i - max_decimal + b->type_data.num.decimal_pos;
        
        int a_digit = (a_idx >= 0 && a_idx < a->length) ? a_digits[a_idx] : 0;
        int b_digit = (b_idx >= 0 && b_idx < b->length) ? b_digits[b_idx] : 0;
        
        int diff = a_digit - b_digit - borrow;
        if (diff < 0) {
            diff += 10;
            borrow = 1;
        } else {
            borrow = 0;
        }
        
        result_digits[i] = diff;
        result_len = i + 1;
    }
    
    result->length = result_len;
    bignum_trim(result);
    return BIGNUM_SUCCESS;
}


/* API 实现 */

/* 旧版本 API - 保留用于兼容 */
int bignum_from_string_legacy(const char *str, BHS *num) {
    if (str == NULL || num == NULL) return BIGNUM_ERROR;
    
    bignum_init(num);
    
    int len = strlen(str);
    if (len == 0) return BIGNUM_ERROR;
    
    int pos = 0;
    int is_negative = 0;
    
    /* 处理符号 */
    if (str[pos] == '-') {
        is_negative = 1;
        pos++;
    } else if (str[pos] == '+') {
        pos++;
    }
    
    /* 查找小数点 */
    int decimal_point = -1;
    for (int i = pos; i < len; i++) {
        if (str[i] == '.') {
            decimal_point = i;
            break;
        }
    }
    
    /* 计算需要的位数 */
    int digit_count = 0;
    int decimal_digits = 0;
    
    if (decimal_point >= 0) {
        decimal_digits = len - decimal_point - 1;
        digit_count = len - pos - 1;  /* 减去符号和小数点 */
    } else {
        digit_count = len - pos;
    }
    
    if (digit_count == 0) return BIGNUM_ERROR;
    if (digit_count > BIGNUM_MAX_DIGITS) return BIGNUM_ERROR;
    
    /* 确保容量足够 */
    if (bignum_ensure_capacity(num, digit_count + 1) != BIGNUM_SUCCESS) {
        return BIGNUM_ERROR;
    }
    
    char *digits = BIGNUM_DIGITS(num);
    int idx = 0;
    
    /* 先处理小数部分（从后往前） */
    if (decimal_point >= 0) {
        for (int i = len - 1; i > decimal_point; i--) {
            if (!isdigit(str[i])) return BIGNUM_ERROR;
            digits[idx++] = str[i] - '0';
        }
    }
    
    /* 处理整数部分（从后往前） */
    int end_pos = (decimal_point >= 0) ? decimal_point : len;
    for (int i = end_pos - 1; i >= pos; i--) {
        if (!isdigit(str[i])) return BIGNUM_ERROR;
        digits[idx++] = str[i] - '0';
    }
    
    num->length = idx;
    num->type_data.num.decimal_pos = decimal_digits;
    num->type_data.num.is_negative = is_negative;
    num->type = BIGNUM_TYPE_NUMBER;
    
    bignum_trim(num);
    return BIGNUM_SUCCESS;
}

/* 新版本 API - 返回堆分配的 BHS */
BHS* bignum_from_string(const char *str) {
    if (str == NULL) return NULL;
    
    BHS *num = bignum_create();
    if (num == NULL) return NULL;
    
    if (bignum_from_string_legacy(str, num) != BIGNUM_SUCCESS) {
        bignum_destroy(num);
        return NULL;
    }
    
    return num;
}

/* 从原始字符串创建字符串类型的 BHS - 旧版本 */
int bignum_from_raw_string_legacy(const char *str, BHS *num) {
    if (str == NULL || num == NULL) return BIGNUM_ERROR;
    
    bignum_init(num);
    
    int len = strlen(str);
    
    /* 字符串类型无长度限制，动态扩展 */
    if (bignum_ensure_capacity(num, len + 1) != BIGNUM_SUCCESS) {
        return BIGNUM_ERROR;
    }
    
    char *digits = BIGNUM_DIGITS(num);
    
    /* 正序存储字符串 */
    memcpy(digits, str, len);
    num->length = len;
    num->type_data.str.reserved1 = 0;
    num->type_data.str.reserved2 = 0;
    num->type = BIGNUM_TYPE_STRING;  /* 设置为字符串类型 */
    
    return BIGNUM_SUCCESS;
}

/* 新版本 API - 返回堆分配的 BHS */
BHS* bignum_from_raw_string(const char *str) {
    if (str == NULL) return NULL;
    
    BHS *num = bignum_create();
    if (num == NULL) return NULL;
    
    if (bignum_from_raw_string_legacy(str, num) != BIGNUM_SUCCESS) {
        bignum_destroy(num);
        return NULL;
    }
    
    return num;
}

int bignum_to_string(const BHS *num, char *str, size_t max_len, int precision) {
    if (num == NULL || str == NULL || max_len == 0) return BIGNUM_ERROR;
    
    char *digits = BIGNUM_DIGITS(num);
    
    /* 如果是字符串类型，直接输出字符串内容（带引号） */
    if (num->type == BIGNUM_TYPE_STRING) {
        if (num->length + 3 > (int)max_len) return BIGNUM_ERROR;  /* +3 for quotes and null */
        str[0] = '"';
        memcpy(str + 1, digits, num->length);
        str[num->length + 1] = '"';
        str[num->length + 2] = '\0';
        return BIGNUM_SUCCESS;
    }
    
    /* 如果是位图类型，输出为 B 开头的二进制字符串 */
    if (num->type == BIGNUM_TYPE_BITMAP) {
        if (num->length + 2 > max_len) return BIGNUM_ERROR;  /* +2 for 'B' and null */
        str[0] = 'B';
        unsigned char *bitmap_data = (unsigned char *)digits;
        for (size_t i = 0; i < num->length; i++) {
            str[i + 1] = ((bitmap_data[i / 8] >> (i % 8)) & 1) ? '1' : '0';
        }
        str[num->length + 1] = '\0';
        return BIGNUM_SUCCESS;
    }
    
    /* 如果是列表类型，输出为 [size] 格式 */
    if (num->type == BIGNUM_TYPE_LIST) {
        if (num->data.list == NULL) {
            if (max_len < 6) return BIGNUM_ERROR;  /* "[]" + null */
            strcpy(str, "[]");
            return BIGNUM_SUCCESS;
        }
        
        size_t size = list_size(num->data.list);
        int written = snprintf(str, max_len, "[%zu]", size);
        if (written >= (int)max_len) return BIGNUM_ERROR;
        return BIGNUM_SUCCESS;
    }
    
    if (precision < 0) precision = BIGNUM_DEFAULT_PRECISION;
    
    int pos = 0;
    
    /* 添加符号 */
    if (num->type_data.num.is_negative && !(num->length == 1 && digits[0] == 0)) {
        if (pos >= (int)max_len - 1) return BIGNUM_ERROR;
        str[pos++] = '-';
    }
    
    /* 计算整数部分长度 */
    int int_len = num->length - num->type_data.num.decimal_pos;
    if (int_len <= 0) int_len = 1;
    
    /* 输出整数部分 */
    int started = 0;
    for (int i = num->length - 1; i >= num->type_data.num.decimal_pos; i--) {
        if (digits[i] != 0 || started || i == num->type_data.num.decimal_pos) {
            if (pos >= (int)max_len - 1) return BIGNUM_ERROR;
            str[pos++] = digits[i] + '0';
            started = 1;
        }
    }
    
    if (!started) {
        if (pos >= (int)max_len - 1) return BIGNUM_ERROR;
        str[pos++] = '0';
    }
    
    /* 输出小数部分 */
    if (num->type_data.num.decimal_pos > 0 || precision > 0) {
        if (pos >= (int)max_len - 1) return BIGNUM_ERROR;
        str[pos++] = '.';
        
        int decimal_output = (precision < num->type_data.num.decimal_pos) ? precision : num->type_data.num.decimal_pos;
        for (int i = num->type_data.num.decimal_pos - 1; i >= num->type_data.num.decimal_pos - decimal_output; i--) {
            if (pos >= (int)max_len - 1) return BIGNUM_ERROR;
            if (i >= 0) {
                str[pos++] = digits[i] + '0';
            } else {
                str[pos++] = '0';
            }
        }
        
        /* 如果需要更多精度，补0 */
        for (int i = num->type_data.num.decimal_pos; i < precision; i++) {
            if (pos >= (int)max_len - 1) return BIGNUM_ERROR;
            str[pos++] = '0';
        }
        
        /* 移除尾随的零 */
        while (pos > 0 && str[pos - 1] == '0') {
            pos--;
        }
        if (pos > 0 && str[pos - 1] == '.') {
            pos--;
        }
    }
    
    str[pos] = '\0';
    return BIGNUM_SUCCESS;
}

static int bignum_add_internal(const BHS *a, const BHS *b, BHS *result) {
    if (a == NULL || b == NULL || result == NULL) return BIGNUM_ERROR;
    
    /* 类型检查 */
    if (a->type != BIGNUM_TYPE_NUMBER || b->type != BIGNUM_TYPE_NUMBER) {
        return BIGNUM_ERROR;
    }
    
    /* 同号相加 */
    if (a->type_data.num.is_negative == b->type_data.num.is_negative) {
        if (bignum_add_abs(a, b, result) != BIGNUM_SUCCESS) return BIGNUM_ERROR;
        result->type_data.num.is_negative = a->type_data.num.is_negative;
        return BIGNUM_SUCCESS;
    }
    
    /* 异号相减 */
    int cmp = bignum_compare_abs(a, b);
    if (cmp >= 0) {
        if (bignum_sub_abs(a, b, result) != BIGNUM_SUCCESS) return BIGNUM_ERROR;
        result->type_data.num.is_negative = a->type_data.num.is_negative;
    } else {
        if (bignum_sub_abs(b, a, result) != BIGNUM_SUCCESS) return BIGNUM_ERROR;
        result->type_data.num.is_negative = b->type_data.num.is_negative;
    }
    
    return BIGNUM_SUCCESS;
}

static int bignum_sub_internal(const BHS *a, const BHS *b, BHS *result) {
    if (a == NULL || b == NULL || result == NULL) return BIGNUM_ERROR;
    
    /* 类型检查 */
    if (a->type != BIGNUM_TYPE_NUMBER || b->type != BIGNUM_TYPE_NUMBER) {
        return BIGNUM_ERROR;
    }
    
    /* 异号相加 */
    if (a->type_data.num.is_negative != b->type_data.num.is_negative) {
        if (bignum_add_abs(a, b, result) != BIGNUM_SUCCESS) return BIGNUM_ERROR;
        result->type_data.num.is_negative = a->type_data.num.is_negative;
        return BIGNUM_SUCCESS;
    }
    
    /* 同号相减 */
    int cmp = bignum_compare_abs(a, b);
    if (cmp >= 0) {
        if (bignum_sub_abs(a, b, result) != BIGNUM_SUCCESS) return BIGNUM_ERROR;
        result->type_data.num.is_negative = a->type_data.num.is_negative;
    } else {
        if (bignum_sub_abs(b, a, result) != BIGNUM_SUCCESS) return BIGNUM_ERROR;
        result->type_data.num.is_negative = !a->type_data.num.is_negative;
    }
    
    return BIGNUM_SUCCESS;
}

static int bignum_mul_internal(const BHS *a, const BHS *b, BHS *result) {
    if (a == NULL || b == NULL || result == NULL) return BIGNUM_ERROR;
    
    /* 类型检查 */
    if (a->type != BIGNUM_TYPE_NUMBER || b->type != BIGNUM_TYPE_NUMBER) {
        return BIGNUM_ERROR;
    }
    
    BHS temp;
    bignum_init(&temp);
    temp.type = BIGNUM_TYPE_NUMBER;
    
    int total_decimal = a->type_data.num.decimal_pos + b->type_data.num.decimal_pos;
    int max_result_len = a->length + b->length;  /* 两个n位数相乘最多2n位，不需要+1 */
    
    if (bignum_ensure_capacity(&temp, max_result_len) != BIGNUM_SUCCESS) {
        bignum_free(&temp);
        return BIGNUM_ERROR;
    }
    
    char *a_digits = BIGNUM_DIGITS(a);
    char *b_digits = BIGNUM_DIGITS(b);
    char *temp_digits = BIGNUM_DIGITS(&temp);
    
    /* 逐位相乘 */
    for (int i = 0; i < a->length; i++) {
        int carry = 0;
        for (int j = 0; j < b->length || carry > 0; j++) {
            int b_digit = (j < b->length) ? b_digits[j] : 0;
            int current = temp_digits[i + j] + a_digits[i] * b_digit + carry;
            temp_digits[i + j] = current % 10;
            carry = current / 10;
            if (i + j + 1 > temp.length) temp.length = i + j + 1;
        }
    }
    
    temp.type_data.num.decimal_pos = total_decimal;
    temp.type_data.num.is_negative = (a->type_data.num.is_negative != b->type_data.num.is_negative);
    
    bignum_trim(&temp);
    
    /* 立即截断小数位数到默认精度，防止小数位数爆炸 */
    if (temp.type_data.num.decimal_pos > BIGNUM_DEFAULT_PRECISION) {
        int excess = temp.type_data.num.decimal_pos - BIGNUM_DEFAULT_PRECISION;
        char *digits = BIGNUM_DIGITS(&temp);
        if (excess < temp.length) {
            memmove(digits, digits + excess, temp.length - excess);
            temp.length -= excess;
        } else {
            digits[0] = 0;
            temp.length = 1;
        }
        temp.type_data.num.decimal_pos = BIGNUM_DEFAULT_PRECISION;
        bignum_trim(&temp);
    }
    
    /* 检查总长度是否超过最大限制，优先舍弃小数 */
    if (temp.length > BIGNUM_MAX_DIGITS) {
        int to_remove = temp.length - BIGNUM_MAX_DIGITS;
        /* 最多只能删除小数部分 */
        if (to_remove > temp.type_data.num.decimal_pos) {
            to_remove = temp.type_data.num.decimal_pos;
        }
        if (to_remove > 0) {
            char *digits = BIGNUM_DIGITS(&temp);
            memmove(digits, digits + to_remove, temp.length - to_remove);
            temp.length -= to_remove;
            temp.type_data.num.decimal_pos -= to_remove;
            bignum_trim(&temp);
        }
        /* 如果还是超限，说明整数部分太大，返回错误 */
        if (temp.length > BIGNUM_MAX_DIGITS) {
            bignum_free(&temp);
            return BIGNUM_ERROR;
        }
    }
    
    /* 复制结果 */
    if (bignum_copy(&temp, result) != BIGNUM_SUCCESS) {
        bignum_free(&temp);
        return BIGNUM_ERROR;
    }
    
    bignum_free(&temp);
    return BIGNUM_SUCCESS;
}

static int bignum_div_internal(const BHS *a, const BHS *b, BHS *result, int precision) {
    if (a == NULL || b == NULL || result == NULL) return BIGNUM_ERROR;
    
    /* 类型检查 */
    if (a->type != BIGNUM_TYPE_NUMBER || b->type != BIGNUM_TYPE_NUMBER) {
        return BIGNUM_ERROR;
    }
    
    /* 检查除零 */
    char *b_digits = BIGNUM_DIGITS(b);
    if (b->length == 1 && b_digits[0] == 0) return BIGNUM_DIV_ZERO;
    
    if (precision < 0) precision = BIGNUM_DEFAULT_PRECISION;
    
    bignum_free(result);
    bignum_init(result);
    result->type = BIGNUM_TYPE_NUMBER;
    
    /* 创建被除数的副本 */
    BHS dividend, divisor;
    bignum_init(&dividend);
    bignum_init(&divisor);
    
    if (bignum_copy(a, &dividend) != BIGNUM_SUCCESS) {
        return BIGNUM_ERROR;
    }
    if (bignum_copy(b, &divisor) != BIGNUM_SUCCESS) {
        bignum_free(&dividend);
        return BIGNUM_ERROR;
    }
    
    /* 为了获得足够的精度，我们需要扩展被除数 */
    int extra_scale = precision + 10;
    
    /* 记录原始的小数位数 */
    int a_decimal = a->type_data.num.decimal_pos;
    int b_decimal = b->type_data.num.decimal_pos;
    
    /* 先将小数转换为整数：
     * 对于 2.5，digits=[5,2], decimal_pos=1，我们需要保持这个表示不变
     * 但将 decimal_pos 设为 0，这样它就代表整数 25
     * 
     * 除法公式：a/b = (a的整数值 * 10^a_decimal) / (b的整数值 * 10^b_decimal)
     *              = (a的整数值 / b的整数值) * 10^(a_decimal - b_decimal)
     * 
     * 为了获得小数精度，我们需要：
     * (a的整数值 * 10^extra_scale) / b的整数值，结果的小数位是 extra_scale - (a_decimal - b_decimal)
     */
    dividend.type_data.num.decimal_pos = 0;
    divisor.type_data.num.decimal_pos = 0;
    
    /* 移位被除数以获得精度 */
    for (int i = 0; i < extra_scale; i++) {
        int carry = 0;
        char *div_digits = BIGNUM_DIGITS(&dividend);
        
        if (bignum_ensure_capacity(&dividend, dividend.length + 2) != BIGNUM_SUCCESS) {
            bignum_free(&dividend);
            bignum_free(&divisor);
            return BIGNUM_ERROR;
        }
        div_digits = BIGNUM_DIGITS(&dividend);  /* 重新获取指针 */
        
        for (int j = 0; j < dividend.length || carry > 0; j++) {
            int current = (j < dividend.length ? div_digits[j] : 0) * 10 + carry;
            div_digits[j] = current % 10;
            carry = current / 10;
            if (j >= dividend.length) dividend.length = j + 1;
        }
    }
    
    /* 长除法 */
    BHS remainder;
    bignum_init(&remainder);
    
    int max_result_len = dividend.length + 10;
    char *temp_result = (char *)malloc(max_result_len);
    if (temp_result == NULL) {
        bignum_free(&dividend);
        bignum_free(&divisor);
        bignum_free(&remainder);
        return BIGNUM_ERROR;
    }
    int result_len = 0;
    
    char *div_digits = BIGNUM_DIGITS(&dividend);
    
    /* 从高位到低位处理 */
    for (int i = dividend.length - 1; i >= 0; i--) {
        int carry = div_digits[i];
        char *rem_digits = BIGNUM_DIGITS(&remainder);
        
        if (bignum_ensure_capacity(&remainder, remainder.length + 2) != BIGNUM_SUCCESS) {
            free(temp_result);
            bignum_free(&dividend);
            bignum_free(&divisor);
            bignum_free(&remainder);
            return BIGNUM_ERROR;
        }
        rem_digits = BIGNUM_DIGITS(&remainder);
        
        for (int j = 0; j < remainder.length || carry > 0; j++) {
            int current = (j < remainder.length ? rem_digits[j] : 0) * 10 + carry;
            rem_digits[j] = current % 10;
            carry = current / 10;
            if (j >= remainder.length) remainder.length = j + 1;
        }
        bignum_trim(&remainder);
        
        /* 计算商的当前位 */
        int quotient_digit = 0;
        while (bignum_compare_abs(&remainder, &divisor) >= 0) {
            BHS temp;
            bignum_init(&temp);
            bignum_sub_abs(&remainder, &divisor, &temp);
            bignum_free(&remainder);
            remainder = temp;
            quotient_digit++;
            if (quotient_digit >= 10) break;
        }
        
        temp_result[result_len++] = quotient_digit;
    }
    
    /* 确保结果有足够容量 */
    if (bignum_ensure_capacity(result, result_len + 1) != BIGNUM_SUCCESS) {
        free(temp_result);
        bignum_free(&dividend);
        bignum_free(&divisor);
        bignum_free(&remainder);
        return BIGNUM_ERROR;
    }
    
    char *result_digits = BIGNUM_DIGITS(result);
    
    /* 将临时结果逆序复制到result */
    for (int i = 0; i < result_len; i++) {
        result_digits[i] = temp_result[result_len - 1 - i];
    }
    
    result->length = result_len;
    /* 结果的小数位数 = extra_scale + a_decimal - b_decimal
     * 因为我们计算的是 (a_int * 10^extra_scale) / b_int
     * 其中 a_int 是 a 去掉小数点后的整数（相当于 a * 10^a_decimal）
     * b_int 是 b 去掉小数点后的整数（相当于 b * 10^b_decimal）
     * 所以结果 = (a * 10^a_decimal * 10^extra_scale) / (b * 10^b_decimal)
     *         = (a / b) * 10^(a_decimal + extra_scale - b_decimal)
     * 因此小数位数 = extra_scale + a_decimal - b_decimal
     */
    result->type_data.num.decimal_pos = extra_scale + a_decimal - b_decimal;
    result->type_data.num.is_negative = (a->type_data.num.is_negative != b->type_data.num.is_negative);
    
    bignum_trim(result);
    
    free(temp_result);
    bignum_free(&dividend);
    bignum_free(&divisor);
    bignum_free(&remainder);
    
    return BIGNUM_SUCCESS;
}

static int bignum_pow_internal(const BHS *base, const BHS *exponent, BHS *result, int precision) {
    if (base == NULL || exponent == NULL || result == NULL) return BIGNUM_ERROR;
    
    /* 类型检查 */
    if (base->type != BIGNUM_TYPE_NUMBER || exponent->type != BIGNUM_TYPE_NUMBER) {
        return BIGNUM_ERROR;
    }
    
    /* 检查指数是否为负数 */
    if (exponent->type_data.num.is_negative) return BIGNUM_ERROR;
    
    char *exp_digits = BIGNUM_DIGITS(exponent);
    
    /* 检查指数是否有小数部分 */
    if (exponent->type_data.num.decimal_pos > 0) {
        for (int i = 0; i < exponent->type_data.num.decimal_pos; i++) {
            if (exp_digits[i] != 0) return BIGNUM_ERROR;
        }
    }
    
    if (precision < 0) precision = BIGNUM_DEFAULT_PRECISION;
    
    /* 将指数转换为整数 */
    unsigned long long exp_value = 0;
    for (int i = exponent->length - 1; i >= exponent->type_data.num.decimal_pos; i--) {
        exp_value = exp_value * 10 + exp_digits[i];
        if (exp_value > 100000) return BIGNUM_ERROR;  /* 限制指数大小，防止计算时间过长 */
    }
    
    /* 初始化结果为1（先清理可能存在的旧数据） */
    bignum_free(result);
    bignum_init(result);
    result->type = BIGNUM_TYPE_NUMBER;
    char *result_digits = BIGNUM_DIGITS(result);
    result_digits[0] = 1;
    result->length = 1;
    
    /* 特殊情况：0的幂 */
    if (exp_value == 0) {
        return BIGNUM_SUCCESS;
    }
    
    /* 使用快速幂算法 */
    BHS current_power, temp;
    bignum_init(&current_power);
    bignum_init(&temp);
    
    if (bignum_copy(base, &current_power) != BIGNUM_SUCCESS) {
        return BIGNUM_ERROR;
    }
    
    while (exp_value > 0) {
        if (exp_value & 1) {
            /* 指数为奇数，累乘到结果 */
            if (bignum_mul_internal(result, &current_power, &temp) != BIGNUM_SUCCESS) {
                bignum_free(&current_power);
                bignum_free(&temp);
                return BIGNUM_ERROR;
            }
            /* 交换 result 和 temp 的内容 */
            BHS swap = *result;
            *result = temp;
            temp = swap;
            /* 清空被交换出去的temp（它现在持有旧result），供下次使用 */
            bignum_free(&temp);
            bignum_init(&temp);
            /* 截断小数精度，防止小数位数无限增长 */
            if (result->type_data.num.decimal_pos > precision) {
                int excess = result->type_data.num.decimal_pos - precision;
                char *digits = BIGNUM_DIGITS(result);
                /* 移除最低的 excess 位小数 */
                if (excess < result->length) {
                    memmove(digits, digits + excess, result->length - excess);
                    result->length -= excess;
                } else {
                    digits[0] = 0;
                    result->length = 1;
                }
                result->type_data.num.decimal_pos = precision;
                bignum_trim(result);
            }
            /* 检查总长度是否超限 */
            if (result->length > BIGNUM_MAX_DIGITS) {
                /* 优先舍弃小数部分 */
                int to_remove = result->length - BIGNUM_MAX_DIGITS;
                if (to_remove >= result->type_data.num.decimal_pos) {
                    /* 需要舍弃的超过小数部分，也要舍弃整数 */
                    to_remove = result->type_data.num.decimal_pos;
                }
                if (to_remove > 0) {
                    char *digits = BIGNUM_DIGITS(result);
                    memmove(digits, digits + to_remove, result->length - to_remove);
                    result->length -= to_remove;
                    result->type_data.num.decimal_pos -= to_remove;
                    bignum_trim(result);
                }
            }
        }
        
        exp_value >>= 1;
        if (exp_value > 0) {
            /* 底数自乘 */
            if (bignum_mul_internal(&current_power, &current_power, &temp) != BIGNUM_SUCCESS) {
                bignum_free(&current_power);
                bignum_free(&temp);
                return BIGNUM_ERROR;
            }
            /* 交换 current_power 和 temp 的内容 */
            BHS swap = current_power;
            current_power = temp;
            temp = swap;
            /* 清空被交换出去的temp（它现在持有旧current_power），供下次使用 */
            bignum_free(&temp);
            bignum_init(&temp);
            /* 截断小数精度，防止小数位数无限增长 */
            if (current_power.type_data.num.decimal_pos > precision) {
                int excess = current_power.type_data.num.decimal_pos - precision;
                char *digits = BIGNUM_DIGITS(&current_power);
                /* 移除最低的 excess 位小数 */
                if (excess < current_power.length) {
                    memmove(digits, digits + excess, current_power.length - excess);
                    current_power.length -= excess;
                } else {
                    digits[0] = 0;
                    current_power.length = 1;
                }
                current_power.type_data.num.decimal_pos = precision;
                bignum_trim(&current_power);
            }
            /* 检查总长度是否超限 */
            if (current_power.length > BIGNUM_MAX_DIGITS) {
                /* 优先舍弃小数部分 */
                int to_remove = current_power.length - BIGNUM_MAX_DIGITS;
                if (to_remove >= current_power.type_data.num.decimal_pos) {
                    /* 需要舍弃的超过小数部分，也要舍弃整数 */
                    to_remove = current_power.type_data.num.decimal_pos;
                }
                if (to_remove > 0) {
                    char *digits = BIGNUM_DIGITS(&current_power);
                    memmove(digits, digits + to_remove, current_power.length - to_remove);
                    current_power.length -= to_remove;
                    current_power.type_data.num.decimal_pos -= to_remove;
                    bignum_trim(&current_power);
                }
            }
        }
    }
    
    bignum_trim(result);
    bignum_free(&current_power);
    bignum_free(&temp);
    return BIGNUM_SUCCESS;
}

static int bignum_mod_internal(const BHS *a, const BHS *b, BHS *result) {
    if (a == NULL || b == NULL || result == NULL) return BIGNUM_ERROR;
    
    /* 类型检查 */
    if (a->type != BIGNUM_TYPE_NUMBER || b->type != BIGNUM_TYPE_NUMBER) {
        return BIGNUM_ERROR;
    }
    
    char *b_digits = BIGNUM_DIGITS(b);
    
    /* 检查除零 */
    if (b->length == 1 && b_digits[0] == 0) return BIGNUM_DIV_ZERO;
    
    char *a_digits = BIGNUM_DIGITS(a);
    
    /* 检查除数和被除数是否有小数部分 */
    int has_decimal = 0;
    if (a->type_data.num.decimal_pos > 0) {
        for (int i = 0; i < a->type_data.num.decimal_pos; i++) {
            if (a_digits[i] != 0) {
                has_decimal = 1;
                break;
            }
        }
    }
    if (!has_decimal && b->type_data.num.decimal_pos > 0) {
        for (int i = 0; i < b->type_data.num.decimal_pos; i++) {
            if (b_digits[i] != 0) {
                has_decimal = 1;
                break;
            }
        }
    }
    
    /* 取模运算要求操作数为整数 */
    if (has_decimal) return BIGNUM_ERROR;
    
    /* 创建a和b的整数副本 */
    BHS dividend, divisor;
    bignum_init(&dividend);
    bignum_init(&divisor);
    
    if (bignum_copy(a, &dividend) != BIGNUM_SUCCESS) {
        return BIGNUM_ERROR;
    }
    if (bignum_copy(b, &divisor) != BIGNUM_SUCCESS) {
        bignum_free(&dividend);
        return BIGNUM_ERROR;
    }
    
    char *div_digits = BIGNUM_DIGITS(&dividend);
    char *divisor_digits = BIGNUM_DIGITS(&divisor);
    
    /* 去除小数点 */
    if (dividend.type_data.num.decimal_pos > 0) {
        for (int i = dividend.type_data.num.decimal_pos; i < dividend.length; i++) {
            div_digits[i - dividend.type_data.num.decimal_pos] = div_digits[i];
        }
        dividend.length -= dividend.type_data.num.decimal_pos;
        dividend.type_data.num.decimal_pos = 0;
    }
    
    if (divisor.type_data.num.decimal_pos > 0) {
        for (int i = divisor.type_data.num.decimal_pos; i < divisor.length; i++) {
            divisor_digits[i - divisor.type_data.num.decimal_pos] = divisor_digits[i];
        }
        divisor.length -= divisor.type_data.num.decimal_pos;
        divisor.type_data.num.decimal_pos = 0;
    }
    
    bignum_trim(&dividend);
    bignum_trim(&divisor);
    
    /* 保存符号 */
    int dividend_negative = dividend.type_data.num.is_negative;
    
    /* 取绝对值进行运算 */
    dividend.type_data.num.is_negative = 0;
    divisor.type_data.num.is_negative = 0;
    
    /* 使用长除法计算余数 */
    BHS remainder;
    bignum_init(&remainder);
    
    div_digits = BIGNUM_DIGITS(&dividend);
    
    /* 从高位到低位处理被除数 */
    for (int i = dividend.length - 1; i >= 0; i--) {
        int carry = div_digits[i];
        char *rem_digits = BIGNUM_DIGITS(&remainder);
        
        if (bignum_ensure_capacity(&remainder, remainder.length + 2) != BIGNUM_SUCCESS) {
            bignum_free(&dividend);
            bignum_free(&divisor);
            bignum_free(&remainder);
            return BIGNUM_ERROR;
        }
        rem_digits = BIGNUM_DIGITS(&remainder);
        
        for (int j = 0; j < remainder.length || carry > 0; j++) {
            int current = (j < remainder.length ? rem_digits[j] : 0) * 10 + carry;
            rem_digits[j] = current % 10;
            carry = current / 10;
            if (j >= remainder.length) remainder.length = j + 1;
        }
        bignum_trim(&remainder);
        
        /* 尽可能多地从余数中减去除数 */
        while (bignum_compare_abs(&remainder, &divisor) >= 0) {
            BHS temp;
            bignum_init(&temp);
            bignum_sub_abs(&remainder, &divisor, &temp);
            bignum_free(&remainder);
            remainder = temp;
        }
    }
    
    /* 复制结果 */
    if (bignum_copy(&remainder, result) != BIGNUM_SUCCESS) {
        bignum_free(&dividend);
        bignum_free(&divisor);
        bignum_free(&remainder);
        return BIGNUM_ERROR;
    }
    
    /* C风格取模：余数符号与被除数相同 */
    result->type_data.num.is_negative = dividend_negative;
    
    /* 如果结果为0，符号应该为正 */
    char *result_digits = BIGNUM_DIGITS(result);
    if (result->length == 1 && result_digits[0] == 0) {
        result->type_data.num.is_negative = 0;
    }
    
    bignum_trim(result);
    
    bignum_free(&dividend);
    bignum_free(&divisor);
    bignum_free(&remainder);
    
    return BIGNUM_SUCCESS;
}

/* 类型判断和转换函数 */

int bignum_is_number(const BHS *num) {
    if (num == NULL) return 0;
    return num->type == BIGNUM_TYPE_NUMBER;
}

int bignum_is_string(const BHS *num) {
    if (num == NULL) return 0;
    return num->type == BIGNUM_TYPE_STRING;
}

int bignum_is_bitmap(const BHS *num) {
    if (num == NULL) return 0;
    return num->type == BIGNUM_TYPE_BITMAP;
}

int bignum_is_list(const BHS *num) {
    if (num == NULL) return 0;
    return num->type == BIGNUM_TYPE_LIST;
}

int bignum_string_to_number_legacy(const BHS *str_num, BHS *num_result) {
    if (str_num == NULL || num_result == NULL) return BIGNUM_ERROR;
    if (str_num->type != BIGNUM_TYPE_STRING) return BIGNUM_ERROR;
    
    /* 将字符串内容提取出来 */
    char *temp_str = (char *)malloc(str_num->length + 1);
    if (temp_str == NULL) return BIGNUM_ERROR;
    
    char *str_digits = BIGNUM_DIGITS(str_num);
    memcpy(temp_str, str_digits, str_num->length);
    temp_str[str_num->length] = '\0';
    
    /* 尝试解析为数字 */
    int ret = bignum_from_string_legacy(temp_str, num_result);
    free(temp_str);
    return ret;
}

int bignum_number_to_string_type_legacy(const BHS *num, BHS *str_result, int precision) {
    if (num == NULL || str_result == NULL) return BIGNUM_ERROR;
    if (num->type != BIGNUM_TYPE_NUMBER) return BIGNUM_ERROR;
    
    /* 将数字转换为字符串表示 */
    char temp_str[16384];  /* 足够大的缓冲区 */
    int ret = bignum_to_string(num, temp_str, sizeof(temp_str), precision);
    if (ret != BIGNUM_SUCCESS) return ret;
    
    /* 创建字符串类型的 BHS */
    return bignum_from_raw_string_legacy(temp_str, str_result);
}

/* ========== 新版本 API 实现（返回堆分配的 BHS 指针） ========== */

BHS* bignum_add(const BHS *a, const BHS *b) {
    if (a == NULL || b == NULL) return NULL;
    
    BHS *result = bignum_create();
    if (result == NULL) return NULL;
    
    if (bignum_add_internal(a, b, result) != BIGNUM_SUCCESS) {
        bignum_destroy(result);
        return NULL;
    }
    
    return result;
}

BHS* bignum_sub(const BHS *a, const BHS *b) {
    if (a == NULL || b == NULL) return NULL;
    
    BHS *result = bignum_create();
    if (result == NULL) return NULL;
    
    if (bignum_sub_internal(a, b, result) != BIGNUM_SUCCESS) {
        bignum_destroy(result);
        return NULL;
    }
    
    return result;
}

BHS* bignum_mul(const BHS *a, const BHS *b) {
    if (a == NULL || b == NULL) return NULL;
    
    BHS *result = bignum_create();
    if (result == NULL) return NULL;
    
    if (bignum_mul_internal(a, b, result) != BIGNUM_SUCCESS) {
        bignum_destroy(result);
        return NULL;
    }
    
    return result;
}

BHS* bignum_div(const BHS *a, const BHS *b, int precision) {
    if (a == NULL || b == NULL) return NULL;
    
    BHS *result = bignum_create();
    if (result == NULL) return NULL;
    
    int ret = bignum_div_internal(a, b, result, precision);
    if (ret != BIGNUM_SUCCESS) {
        bignum_destroy(result);
        return NULL;
    }
    
    return result;
}

BHS* bignum_pow(const BHS *base, const BHS *exponent, int precision) {
    if (base == NULL || exponent == NULL) return NULL;
    
    BHS *result = bignum_create();
    if (result == NULL) return NULL;
    
    if (bignum_pow_internal(base, exponent, result, precision) != BIGNUM_SUCCESS) {
        bignum_destroy(result);
        return NULL;
    }
    
    return result;
}

BHS* bignum_mod(const BHS *a, const BHS *b) {
    if (a == NULL || b == NULL) return NULL;
    
    BHS *result = bignum_create();
    if (result == NULL) return NULL;
    
    if (bignum_mod_internal(a, b, result) != BIGNUM_SUCCESS) {
        bignum_destroy(result);
        return NULL;
    }
    
    return result;
}

BHS* bignum_string_to_number(const BHS *str_num) {
    if (str_num == NULL) return NULL;
    
    BHS *result = bignum_create();
    if (result == NULL) return NULL;
    
    if (bignum_string_to_number_legacy(str_num, result) != BIGNUM_SUCCESS) {
        bignum_destroy(result);
        return NULL;
    }
    
    return result;
}

BHS* bignum_number_to_string_type(const BHS *num, int precision) {
    if (num == NULL) return NULL;
    
    BHS *result = bignum_create();
    if (result == NULL) return NULL;
    
    if (bignum_number_to_string_type_legacy(num, result, precision) != BIGNUM_SUCCESS) {
        bignum_destroy(result);
        return NULL;
    }
    
    return result;
}

/*
 * 表达式求值功能已移至 evaluator.c/lexer.c
 * bignum 模块现在专注于基础的大数运算
 */

/* ========== Bitmap 类型支持 ========== */
/* 注意：大部分bitmap函数使用lib/bitmap.h中的实现 */

/* 从整数转换为位图 */
BHS* bignum_number_to_bitmap(const BHS *num) {
    if (num == NULL || num->type != BIGNUM_TYPE_NUMBER) return NULL;
    
    /* 不支持小数和负数 */
    if (num->type_data.num.is_negative) return NULL;
    if (num->type_data.num.decimal_pos > 0) {
        char *digits = BIGNUM_DIGITS(num);
        for (int i = 0; i < num->type_data.num.decimal_pos; i++) {
            if (digits[i] != 0) return NULL;
        }
    }
    
    /* 将整数转换为二进制字符串 */
    char binary[BIGNUM_MAX_DIGITS * 4];  /* 每个十进制位最多4个二进制位 */
    int binary_len = 0;
    
    /* 创建数字的副本进行处理 */
    BHS temp;
    bignum_init(&temp);
    if (bignum_copy(num, &temp) != BIGNUM_SUCCESS) {
        return NULL;
    }
    
    char *temp_digits = BIGNUM_DIGITS(&temp);
    
    /* 特殊情况：0 */
    if (temp.length == 1 && temp_digits[0] == 0) {
        bignum_free(&temp);
        return bignum_from_binary_string("0");
    }
    
    /* 不断除以2，取余数 */
    while (!(temp.length == 1 && temp_digits[0] == 0)) {
        /* 除以2，余数是最低位 */
        int remainder = 0;
        for (int i = temp.length - 1; i >= temp.type_data.num.decimal_pos; i--) {
            int current = remainder * 10 + temp_digits[i];
            temp_digits[i] = current / 2;
            remainder = current % 2;
        }
        
        binary[binary_len++] = '0' + remainder;
        bignum_trim(&temp);
        temp_digits = BIGNUM_DIGITS(&temp);
    }
    
    bignum_free(&temp);
    
    /* 反转二进制字符串（因为我们是从低位到高位构建的）*/
    for (int i = 0; i < binary_len / 2; i++) {
        char t = binary[i];
        binary[i] = binary[binary_len - 1 - i];
        binary[binary_len - 1 - i] = t;
    }
    binary[binary_len] = '\0';
    
    return bignum_from_binary_string(binary);
}

/* 将位图转换为整数 */
BHS* bignum_bitmap_to_number(const BHS *bitmap) {
    if (bitmap == NULL || bitmap->type != BIGNUM_TYPE_BITMAP) return NULL;
    
    BHS *result = bignum_create();
    if (result == NULL) return NULL;
    
    char *result_digits = BIGNUM_DIGITS(result);
    result_digits[0] = 0;
    result->length = 1;
    result->type_data.num.decimal_pos = 0;
    result->type_data.num.is_negative = 0;
    
    unsigned char *bitmap_data = (unsigned char *)BIGNUM_DIGITS(bitmap);
    
    /* 从高位到低位处理，使用二进制转十进制算法 */
    for (int i = (int)bitmap->length - 1; i >= 0; i--) {
        /* result = result * 2 */
        int carry = 0;
        for (int j = 0; j < result->length; j++) {
            int current = result_digits[j] * 2 + carry;
            result_digits[j] = current % 10;
            carry = current / 10;
        }
        if (carry > 0) {
            if (bignum_ensure_capacity(result, result->length + 1) != BIGNUM_SUCCESS) {
                bignum_destroy(result);
                return NULL;
            }
            result_digits = BIGNUM_DIGITS(result);
            result_digits[result->length++] = carry;
        }
        
        /* 加上当前位 */
        if ((bitmap_data[i / 8] >> (i % 8)) & 1) {
            carry = 1;
            for (int j = 0; j < result->length && carry > 0; j++) {
                int current = result_digits[j] + carry;
                result_digits[j] = current % 10;
                carry = current / 10;
            }
            if (carry > 0) {
                if (bignum_ensure_capacity(result, result->length + 1) != BIGNUM_SUCCESS) {
                    bignum_destroy(result);
                    return NULL;
                }
                result_digits = BIGNUM_DIGITS(result);
                result_digits[result->length++] = carry;
            }
        }
    }
    
    bignum_trim(result);
    return result;
}

/* 位图左移（包装函数：将BigNum转换为uint64_t后调用bitmap_bitshl） */
BHS* bignum_bitshl(const BHS *a, const BHS *shift) {
    if (a == NULL || shift == NULL) return NULL;
    if (a->type != BIGNUM_TYPE_BITMAP || shift->type != BIGNUM_TYPE_NUMBER) return NULL;
    if (shift->type_data.num.is_negative) return NULL;
    
    /* 将BigNum shift转换为uint64_t，检测溢出 */
    char *shift_digits = BIGNUM_DIGITS(shift);
    uint64_t shift_amount = 0;
    for (int i = shift->length - 1; i >= shift->type_data.num.decimal_pos; i--) {
        uint64_t old_amount = shift_amount;
        shift_amount = shift_amount * 10 + shift_digits[i];
        /* 检测溢出：如果新值小于旧值，说明溢出了 */
        if (shift_amount < old_amount) return NULL;
    }
    
    /* 检查结果长度是否会超过 SIZE_MAX - 1 */
    if (shift_amount > SIZE_MAX - 1 - a->length) return NULL;
    
    return bitmap_bitshl(a, shift_amount);
}

/* 位图右移（包装函数：将BigNum转换为uint64_t后调用bitmap_bitshr） */
BHS* bignum_bitshr(const BHS *a, const BHS *shift) {
    if (a == NULL || shift == NULL) return NULL;
    if (a->type != BIGNUM_TYPE_BITMAP || shift->type != BIGNUM_TYPE_NUMBER) return NULL;
    if (shift->type_data.num.is_negative) return NULL;
    
    /* 将BigNum shift转换为uint64_t，检测溢出 */
    char *shift_digits = BIGNUM_DIGITS(shift);
    uint64_t shift_amount = 0;
    for (int i = shift->length - 1; i >= shift->type_data.num.decimal_pos; i--) {
        uint64_t old_amount = shift_amount;
        shift_amount = shift_amount * 10 + shift_digits[i];
        /* 检测溢出：如果新值小于旧值，说明溢出了 */
        if (shift_amount < old_amount) return NULL;
    }
    
    return bitmap_bitshr(a, shift_amount);
}

/* ========== LIST 类型相关函数实现 ========== */

BHS* bignum_create_list(void) {
    BHS *num = bignum_create();
    if (num == NULL) return NULL;
    
    LIST *list = list_create();
    if (list == NULL) {
        bignum_destroy(num);
        return NULL;
    }
    
    num->type = BIGNUM_TYPE_LIST;
    num->data.list = list;
    num->length = 0;  /* 初始长度为0 */
    
    return num;
}

BHS* bignum_from_list(struct LIST *list) {
    if (list == NULL) return NULL;
    
    BHS *num = bignum_create();
    if (num == NULL) return NULL;
    
    /* 复制列表 */
    LIST *new_list = list_copy(list);
    if (new_list == NULL) {
        bignum_destroy(num);
        return NULL;
    }
    
    num->type = BIGNUM_TYPE_LIST;
    num->data.list = new_list;
    num->length = list_size(new_list);
    
    return num;
}

struct LIST* bignum_get_list(const BHS *num) {
    if (num == NULL || num->type != BIGNUM_TYPE_LIST) return NULL;
    return num->data.list;
}

double bignum_to_double(const BHS *num) {
    if (num == NULL || num->type != BIGNUM_TYPE_NUMBER) return 0.0;
    
    char *digits = BIGNUM_DIGITS(num);
    double result = 0.0;
    double decimal_multiplier = 1.0;
    
    /* 处理整数部分 */
    for (int i = num->length - 1; i >= num->type_data.num.decimal_pos; i--) {
        result = result * 10.0 + digits[i];
    }
    
    /* 处理小数部分 */
    for (int i = num->type_data.num.decimal_pos - 1; i >= 0; i--) {
        decimal_multiplier /= 10.0;
        result += digits[i] * decimal_multiplier;
    }
    
    /* 处理符号 */
    if (num->type_data.num.is_negative) {
        result = -result;
    }
    
    return result;
}
