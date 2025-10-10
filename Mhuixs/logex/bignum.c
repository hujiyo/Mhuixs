#include "bignum.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

/* 内部辅助函数 */

/* 初始化大数为0 */
void bignum_init(BigNum *num) {
    memset(num->data.small_data, 0, BIGNUM_SMALL_SIZE);
    num->length = 1;
    num->capacity = BIGNUM_SMALL_SIZE;
    num->type = BIGNUM_TYPE_NUMBER;
    num->is_large = 0;
    num->type_data.num.decimal_pos = 0;
    num->type_data.num.is_negative = 0;
}

/* 清理大数（释放动态内存） */
void bignum_free(BigNum *num) {
    if (num == NULL) return;
    if (num->is_large && num->data.large_data != NULL) {
        free(num->data.large_data);
        num->data.large_data = NULL;
    }
    num->is_large = 0;
    num->length = 0;
    num->capacity = 0;
}

/* 确保有足够容量，必要时扩展 */
static int bignum_ensure_capacity(BigNum *num, int required_capacity) {
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
        
        /* 复制旧数据 */
        memcpy(new_data, num->data.small_data, num->length);
        memset(new_data + num->length, 0, new_capacity - num->length);
        
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
int bignum_copy(const BigNum *src, BigNum *dst) {
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
    
    /* 复制数据 */
    if (src->is_large) {
        /* 源是大数据 */
        if (bignum_ensure_capacity(dst, src->capacity) != BIGNUM_SUCCESS) {
            return BIGNUM_ERROR;
        }
        memcpy(BIGNUM_DIGITS(dst), BIGNUM_DIGITS(src), src->length);
    } else {
        /* 源是小数据 */
        dst->is_large = 0;
        dst->capacity = BIGNUM_SMALL_SIZE;
        memcpy(dst->data.small_data, src->data.small_data, src->length);
    }
    
    return BIGNUM_SUCCESS;
}

/* 移除前导零 */
static void bignum_trim(BigNum *num) {
    char *digits = BIGNUM_DIGITS(num);
    while (num->length > 1 && digits[num->length - 1] == 0) {
        num->length--;
    }
    /* 如果结果是0，设置为正数（仅数字类型） */
    if (num->type == BIGNUM_TYPE_NUMBER && num->length == 1 && digits[0] == 0) {
        num->type_data.num.is_negative = 0;
        num->type_data.num.decimal_pos = 0;
    }
}

/* 比较两个大数的绝对值 (返回: 1 if a>b, 0 if a==b, -1 if a<b) */
static int bignum_compare_abs(const BigNum *a, const BigNum *b) {
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
static int bignum_add_abs(const BigNum *a, const BigNum *b, BigNum *result) {
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
static int bignum_sub_abs(const BigNum *a, const BigNum *b, BigNum *result) {
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

int bignum_from_string(const char *str, BigNum *num) {
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

/* 从原始字符串创建字符串类型的 BigNum */
int bignum_from_raw_string(const char *str, BigNum *num) {
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

int bignum_to_string(const BigNum *num, char *str, size_t max_len, int precision) {
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

int bignum_add(const BigNum *a, const BigNum *b, BigNum *result) {
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

int bignum_sub(const BigNum *a, const BigNum *b, BigNum *result) {
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

int bignum_mul(const BigNum *a, const BigNum *b, BigNum *result) {
    if (a == NULL || b == NULL || result == NULL) return BIGNUM_ERROR;
    
    /* 类型检查 */
    if (a->type != BIGNUM_TYPE_NUMBER || b->type != BIGNUM_TYPE_NUMBER) {
        return BIGNUM_ERROR;
    }
    
    BigNum temp;
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

int bignum_div(const BigNum *a, const BigNum *b, BigNum *result, int precision) {
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
    BigNum dividend, divisor;
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
    BigNum remainder;
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
            BigNum temp;
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
    result->type_data.num.decimal_pos = extra_scale;
    result->type_data.num.is_negative = (a->type_data.num.is_negative != b->type_data.num.is_negative);
    
    bignum_trim(result);
    
    free(temp_result);
    bignum_free(&dividend);
    bignum_free(&divisor);
    bignum_free(&remainder);
    
    return BIGNUM_SUCCESS;
}

int bignum_pow(const BigNum *base, const BigNum *exponent, BigNum *result, int precision) {
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
    BigNum current_power, temp;
    bignum_init(&current_power);
    bignum_init(&temp);
    
    if (bignum_copy(base, &current_power) != BIGNUM_SUCCESS) {
        return BIGNUM_ERROR;
    }
    
    while (exp_value > 0) {
        if (exp_value & 1) {
            /* 指数为奇数，累乘到结果 */
            if (bignum_mul(result, &current_power, &temp) != BIGNUM_SUCCESS) {
                bignum_free(&current_power);
                bignum_free(&temp);
                return BIGNUM_ERROR;
            }
            /* 交换 result 和 temp 的内容 */
            BigNum swap = *result;
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
            if (bignum_mul(&current_power, &current_power, &temp) != BIGNUM_SUCCESS) {
                bignum_free(&current_power);
                bignum_free(&temp);
                return BIGNUM_ERROR;
            }
            /* 交换 current_power 和 temp 的内容 */
            BigNum swap = current_power;
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

int bignum_mod(const BigNum *a, const BigNum *b, BigNum *result) {
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
    BigNum dividend, divisor;
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
    BigNum remainder;
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
            BigNum temp;
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

int bignum_is_number(const BigNum *num) {
    if (num == NULL) return 0;
    return num->type == BIGNUM_TYPE_NUMBER;
}

int bignum_is_string(const BigNum *num) {
    if (num == NULL) return 0;
    return num->type == BIGNUM_TYPE_STRING;
}

int bignum_string_to_number(const BigNum *str_num, BigNum *num_result) {
    if (str_num == NULL || num_result == NULL) return BIGNUM_ERROR;
    if (str_num->type != BIGNUM_TYPE_STRING) return BIGNUM_ERROR;
    
    /* 将字符串内容提取出来 */
    char *temp_str = (char *)malloc(str_num->length + 1);
    if (temp_str == NULL) return BIGNUM_ERROR;
    
    char *str_digits = BIGNUM_DIGITS(str_num);
    memcpy(temp_str, str_digits, str_num->length);
    temp_str[str_num->length] = '\0';
    
    /* 尝试解析为数字 */
    int ret = bignum_from_string(temp_str, num_result);
    free(temp_str);
    return ret;
}

int bignum_number_to_string_type(const BigNum *num, BigNum *str_result, int precision) {
    if (num == NULL || str_result == NULL) return BIGNUM_ERROR;
    if (num->type != BIGNUM_TYPE_NUMBER) return BIGNUM_ERROR;
    
    /* 将数字转换为字符串表示 */
    char temp_str[16384];  /* 足够大的缓冲区 */
    int ret = bignum_to_string(num, temp_str, sizeof(temp_str), precision);
    if (ret != BIGNUM_SUCCESS) return ret;
    
    /* 创建字符串类型的 BigNum */
    return bignum_from_raw_string(temp_str, str_result);
}

/*
 * 表达式求值功能已移至 evaluator.c/lexer.c
 * bignum 模块现在专注于基础的大数运算
 */
