/**
 * Logex 内置函数实现
 * 
 * 将 list_package 和 type_package 的函数内置化
 * 这些是 Mhuixs 核心功能，无需 import
 */

#include "builtin.h"
#include "lib/list.h"
#include "lib/bitmap.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ========== LIST 操作函数 ========== */

/* list() - 创建空列表 */
static int builtin_list(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)args;
    (void)precision;
    
    if (arg_count != 0) return -1;
    
    BigNum *list_num = bignum_create_list();
    if (!list_num) return -1;
    
    int ret = bignum_copy(list_num, result);
    bignum_destroy(list_num);
    return ret;
}

/* lpush(list, value) - 左侧插入 */
static int builtin_lpush(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)precision;
    
    if (arg_count != 2) return -1;
    if (!bignum_is_list(&args[0])) return -1;
    
    /* 复制列表 */
    if (bignum_copy(&args[0], result) != 0) return -1;
    
    LIST *list = bignum_get_list(result);
    if (!list) return -1;
    
    /* 创建元素副本 */
    BigNum *element = bignum_create();
    if (!element) return -1;
    
    if (bignum_copy(&args[1], element) != 0) {
        bignum_destroy(element);
        return -1;
    }
    
    /* 插入 */
    if (list_lpush(list, (Obj)element) != 0) {
        bignum_destroy(element);
        return -1;
    }
    
    return 0;
}

/* rpush(list, value) - 右侧插入 */
static int builtin_rpush(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)precision;
    
    if (arg_count != 2) return -1;
    if (!bignum_is_list(&args[0])) return -1;
    
    if (bignum_copy(&args[0], result) != 0) return -1;
    
    LIST *list = bignum_get_list(result);
    if (!list) return -1;
    
    BigNum *element = bignum_create();
    if (!element) return -1;
    
    if (bignum_copy(&args[1], element) != 0) {
        bignum_destroy(element);
        return -1;
    }
    
    if (list_rpush(list, (Obj)element) != 0) {
        bignum_destroy(element);
        return -1;
    }
    
    return 0;
}

/* lpop(list) - 左侧弹出 */
static int builtin_lpop(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)precision;
    
    if (arg_count != 1) return -1;
    if (!bignum_is_list(&args[0])) return -1;
    
    LIST *list = bignum_get_list((BigNum*)&args[0]);
    if (!list || list_size(list) == 0) return -1;
    
    Obj obj = list_lpop(list);
    if ((intptr_t)obj == -1) return -1;
    
    BigNum *value = (BigNum*)obj;
    int ret = bignum_copy(value, result);
    bignum_destroy(value);
    
    return ret;
}

/* rpop(list) - 右侧弹出 */
static int builtin_rpop(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)precision;
    
    if (arg_count != 1) return -1;
    if (!bignum_is_list(&args[0])) return -1;
    
    LIST *list = bignum_get_list((BigNum*)&args[0]);
    if (!list || list_size(list) == 0) return -1;
    
    Obj obj = list_rpop(list);
    if ((intptr_t)obj == -1) return -1;
    
    BigNum *value = (BigNum*)obj;
    int ret = bignum_copy(value, result);
    bignum_destroy(value);
    
    return ret;
}

/* lget(list, index) - 获取元素 */
static int builtin_lget(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)precision;
    
    if (arg_count != 2) return -1;
    if (!bignum_is_list(&args[0]) || !bignum_is_number(&args[1])) return -1;
    
    LIST *list = bignum_get_list((BigNum*)&args[0]);
    if (!list) return -1;
    
    /* 获取索引 */
    char idx_str[64];
    if (bignum_to_string(&args[1], idx_str, sizeof(idx_str), 0) != 0) return -1;
    size_t idx = (size_t)atoi(idx_str);
    
    Obj obj = list_get_index(list, idx);
    if ((intptr_t)obj == -1) return -1;
    
    BigNum *value = (BigNum*)obj;
    return bignum_copy(value, result);
}

/* llen(list) - 列表长度 */
static int builtin_llen(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)precision;
    
    if (arg_count != 1) return -1;
    if (!bignum_is_list(&args[0])) return -1;
    
    LIST *list = bignum_get_list((BigNum*)&args[0]);
    if (!list) return -1;
    
    size_t size = list_size(list);
    char size_str[64];
    snprintf(size_str, sizeof(size_str), "%zu", size);
    
    return bignum_from_string_legacy(size_str, result);
}

/* ========== TYPE 转换函数 ========== */

/* num(value) - 转换为数字 */
static int builtin_num(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)precision;
    
    if (arg_count != 1) return -1;
    
    /* 如果已经是数字，直接返回 */
    if (bignum_is_number(&args[0])) {
        return bignum_copy(&args[0], result);
    }
    
    /* 如果是位图，转换为数字 */
    if (bignum_is_bitmap(&args[0])) {
        BigNum *temp = bignum_bitmap_to_number(&args[0]);
        if (!temp) return -1;
        int ret = bignum_copy(temp, result);
        bignum_destroy(temp);
        return ret;
    }
    
    /* 尝试将字符串转换为数字 */
    BigNum *temp = bignum_string_to_number(&args[0]);
    if (!temp) return -1;
    int ret = bignum_copy(temp, result);
    bignum_destroy(temp);
    return ret;
}

/* str(value) - 转换为字符串 */
static int builtin_str(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) return -1;
    
    /* 如果已经是字符串，直接返回 */
    if (bignum_is_string(&args[0])) {
        return bignum_copy(&args[0], result);
    }
    
    /* 如果是位图，先转数字再转字符串 */
    if (bignum_is_bitmap(&args[0])) {
        BigNum *num_temp = bignum_bitmap_to_number(&args[0]);
        if (!num_temp) return -1;
        
        BigNum *str_temp = bignum_number_to_string_type(num_temp, precision);
        bignum_destroy(num_temp);
        
        if (!str_temp) return -1;
        int ret = bignum_copy(str_temp, result);
        bignum_destroy(str_temp);
        return ret;
    }
    
    /* 如果是数字，转换为字符串 */
    if (bignum_is_number(&args[0])) {
        BigNum *temp = bignum_number_to_string_type(&args[0], precision);
        if (!temp) return -1;
        int ret = bignum_copy(temp, result);
        bignum_destroy(temp);
        return ret;
    }
    
    return -1;
}

/* bmp(value) - 转换为位图 */
static int builtin_bmp(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)precision;
    
    if (arg_count != 1) return -1;
    
    /* 如果已经是位图，直接返回 */
    if (bignum_is_bitmap(&args[0])) {
        return bignum_copy(&args[0], result);
    }
    
    /* 如果是数字，转换为位图 */
    if (bignum_is_number(&args[0])) {
        BigNum *temp = bignum_number_to_bitmap(&args[0]);
        if (!temp) return -1;
        int ret = bignum_copy(temp, result);
        bignum_destroy(temp);
        return ret;
    }
    
    /* 如果是字符串，先转数字再转位图 */
    BigNum *num_temp = bignum_string_to_number(&args[0]);
    if (!num_temp) return -1;
    
    BigNum *bmp_temp = bignum_number_to_bitmap(num_temp);
    bignum_destroy(num_temp);
    
    if (!bmp_temp) return -1;
    int ret = bignum_copy(bmp_temp, result);
    bignum_destroy(bmp_temp);
    return ret;
}

/* ========== BITMAP 操作函数 ========== */

/* bset(bitmap, offset, value) - 设置位 */
static int builtin_bset(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)precision;
    
    if (arg_count != 3) return -1;
    if (!check_if_bitmap((BigNum*)&args[0])) return -1;
    
    char offset_str[64], value_str[64];
    if (bignum_to_string(&args[1], offset_str, sizeof(offset_str), 0) != 0) return -1;
    if (bignum_to_string(&args[2], value_str, sizeof(value_str), 0) != 0) return -1;
    
    uint64_t offset = (uint64_t)atoi(offset_str);
    uint8_t value = (uint8_t)atoi(value_str);
    
    /* 复制位图 */
    if (bignum_copy(&args[0], result) != 0) return -1;
    
    if (bitmap_set(result, offset, value) != 0) return -1;
    
    return 0;
}

/* bget(bitmap, offset) - 获取位 */
static int builtin_bget(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)precision;
    
    if (arg_count != 2) return -1;
    if (!check_if_bitmap((BigNum*)&args[0])) return -1;
    
    char offset_str[64];
    if (bignum_to_string(&args[1], offset_str, sizeof(offset_str), 0) != 0) return -1;
    uint64_t offset = (uint64_t)atoi(offset_str);
    
    int bit = bitmap_get((BigNum*)&args[0], offset);
    if (bit < 0) return -1;
    
    char bit_str[8];
    snprintf(bit_str, sizeof(bit_str), "%d", bit);
    return bignum_from_string_legacy(bit_str, result);
}

/* bcount(bitmap, start, end) - 统计位数 */
static int builtin_bcount(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)precision;
    
    if (arg_count != 3) return -1;
    if (!check_if_bitmap((BigNum*)&args[0])) return -1;
    
    char st_str[64], ed_str[64];
    if (bignum_to_string(&args[1], st_str, sizeof(st_str), 0) != 0) return -1;
    if (bignum_to_string(&args[2], ed_str, sizeof(ed_str), 0) != 0) return -1;
    
    uint64_t st = (uint64_t)atoi(st_str);
    uint64_t ed = (uint64_t)atoi(ed_str);
    
    uint64_t count = bitmap_count((BigNum*)&args[0], st, ed);
    
    char count_str[64];
    snprintf(count_str, sizeof(count_str), "%llu", (unsigned long long)count);
    return bignum_from_string_legacy(count_str, result);
}

/* ========== 内置函数表 ========== */

static const BuiltinFunctionInfo builtin_functions[] = {
    /* LIST 操作 */
    {"list",   builtin_list,   0, 0},
    {"lpush",  builtin_lpush,  2, 2},
    {"rpush",  builtin_rpush,  2, 2},
    {"lpop",   builtin_lpop,   1, 1},
    {"rpop",   builtin_rpop,   1, 1},
    {"lget",   builtin_lget,   2, 2},
    {"llen",   builtin_llen,   1, 1},
    
    /* TYPE 转换 */
    {"num",    builtin_num,    1, 1},
    {"str",    builtin_str,    1, 1},
    {"bmp",    builtin_bmp,    1, 1},
    
    /* BITMAP 操作 */
    {"bset",   builtin_bset,   3, 3},
    {"bget",   builtin_bget,   2, 2},
    {"bcount", builtin_bcount, 3, 3},
    
    {NULL, NULL, 0, 0}  /* 结束标记 */
};

/* 查找内置函数 */
const BuiltinFunctionInfo* builtin_lookup(const char *name) {
    if (!name) return NULL;
    
    for (int i = 0; builtin_functions[i].name != NULL; i++) {
        if (strcmp(builtin_functions[i].name, name) == 0) {
            return &builtin_functions[i];
        }
    }
    
    return NULL;
}

/* 调用内置函数 */
int builtin_call(const BuiltinFunctionInfo *info, 
                 const BigNum *args, 
                 int arg_count, 
                 BigNum *result, 
                 int precision) {
    if (!info || !result) return -1;
    
    /* 允许 args 为 NULL（当 arg_count 为 0 时） */
    if (arg_count > 0 && !args) return -1;
    
    /* 检查参数数量 */
    if (arg_count < info->min_args) return -1;
    if (info->max_args >= 0 && arg_count > info->max_args) return -1;
    
    /* 调用内置函数 */
    return info->func(args, arg_count, result, precision);
}
