/*
 * List Package for Logex
 * 提供列表操作函数，基于 lib/list.h 实现
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../function.h"
#include "../bignum.h"
#include "../lib/list.h"

/* list() 函数：创建空列表
 * 参数：0个
 * 返回：新的空列表
 */
static int list_create_func(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)args;      /* 未使用 */
    (void)precision; /* 未使用 */
    
    if (arg_count != 0) {
        return BIGNUM_ERROR;  /* 参数数量错误 */
    }
    
    BigNum *list_num = bignum_create_list();
    if (list_num == NULL) {
        return BIGNUM_ERROR;
    }
    
    int ret = bignum_copy(list_num, result);
    bignum_destroy(list_num);
    return ret;
}

/* lpush() 函数：在列表左端添加元素
 * 参数：2个（列表，元素）
 * 返回：修改后的列表
 */
static int list_lpush_func(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)precision; /* 未使用 */
    
    if (arg_count != 2) {
        return BIGNUM_ERROR;  /* 参数数量错误 */
    }
    
    const BigNum *list_arg = &args[0];
    const BigNum *element = &args[1];
    
    if (!bignum_is_list(list_arg)) {
        return BIGNUM_ERROR;  /* 第一个参数必须是列表 */
    }
    
    /* 复制列表 */
    int ret = bignum_copy(list_arg, result);
    if (ret != BIGNUM_SUCCESS) {
        return ret;
    }
    
    LIST *list = bignum_get_list(result);
    if (list == NULL) {
        return BIGNUM_ERROR;
    }
    
    /* 创建元素的副本作为 Obj */
    BigNum *element_copy = bignum_create();
    if (element_copy == NULL) {
        return BIGNUM_ERROR;
    }
    
    ret = bignum_copy(element, element_copy);
    if (ret != BIGNUM_SUCCESS) {
        bignum_destroy(element_copy);
        return BIGNUM_ERROR;
    }
    
    /* 添加到列表左端 */
    if (list_lpush(list, (Obj)element_copy) != 0) {
        bignum_destroy(element_copy);
        return BIGNUM_ERROR;
    }
    
    /* 更新长度 */
    result->length = list_size(list);
    
    return BIGNUM_SUCCESS;
}

/* rpush() 函数：在列表右端添加元素
 * 参数：2个（列表，元素）
 * 返回：修改后的列表
 */
static int list_rpush_func(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)precision; /* 未使用 */
    
    if (arg_count != 2) {
        return BIGNUM_ERROR;  /* 参数数量错误 */
    }
    
    const BigNum *list_arg = &args[0];
    const BigNum *element = &args[1];
    
    if (!bignum_is_list(list_arg)) {
        return BIGNUM_ERROR;  /* 第一个参数必须是列表 */
    }
    
    /* 复制列表 */
    int ret = bignum_copy(list_arg, result);
    if (ret != BIGNUM_SUCCESS) {
        return ret;
    }
    
    LIST *list = bignum_get_list(result);
    if (list == NULL) {
        return BIGNUM_ERROR;
    }
    
    /* 创建元素的副本作为 Obj */
    BigNum *element_copy = bignum_create();
    if (element_copy == NULL) {
        return BIGNUM_ERROR;
    }
    
    ret = bignum_copy(element, element_copy);
    if (ret != BIGNUM_SUCCESS) {
        bignum_destroy(element_copy);
        return BIGNUM_ERROR;
    }
    
    /* 添加到列表右端 */
    if (list_rpush(list, (Obj)element_copy) != 0) {
        bignum_destroy(element_copy);
        return BIGNUM_ERROR;
    }
    
    /* 更新长度 */
    result->length = list_size(list);
    
    return BIGNUM_SUCCESS;
}

/* lpop() 函数：从列表左端弹出元素
 * 参数：1个（列表）
 * 返回：弹出的元素
 */
static int list_lpop_func(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)precision; /* 未使用 */
    
    if (arg_count != 1) {
        return BIGNUM_ERROR;  /* 参数数量错误 */
    }
    
    const BigNum *list_arg = &args[0];
    
    if (!bignum_is_list(list_arg)) {
        return BIGNUM_ERROR;  /* 参数必须是列表 */
    }
    
    LIST *list = bignum_get_list(list_arg);
    if (list == NULL || list_size(list) == 0) {
        return BIGNUM_ERROR;  /* 空列表 */
    }
    
    /* 弹出元素 */
    Obj obj = list_lpop(list);
    if (obj == NULL) {
        return BIGNUM_ERROR;
    }
    
    BigNum *element = (BigNum *)obj;
    int ret = bignum_copy(element, result);
    bignum_destroy(element);  /* 释放弹出的元素 */
    
    return ret;
}

/* rpop() 函数：从列表右端弹出元素
 * 参数：1个（列表）
 * 返回：弹出的元素
 */
static int list_rpop_func(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)precision; /* 未使用 */
    
    if (arg_count != 1) {
        return BIGNUM_ERROR;  /* 参数数量错误 */
    }
    
    const BigNum *list_arg = &args[0];
    
    if (!bignum_is_list(list_arg)) {
        return BIGNUM_ERROR;  /* 参数必须是列表 */
    }
    
    LIST *list = bignum_get_list(list_arg);
    if (list == NULL || list_size(list) == 0) {
        return BIGNUM_ERROR;  /* 空列表 */
    }
    
    /* 弹出元素 */
    Obj obj = list_rpop(list);
    if (obj == NULL) {
        return BIGNUM_ERROR;
    }
    
    BigNum *element = (BigNum *)obj;
    int ret = bignum_copy(element, result);
    bignum_destroy(element);  /* 释放弹出的元素 */
    
    return ret;
}

/* get() 函数：获取列表指定位置的元素
 * 参数：2个（列表，索引）
 * 返回：指定位置的元素
 */
static int list_get_func(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)precision; /* 未使用 */
    
    if (arg_count != 2) {
        return BIGNUM_ERROR;  /* 参数数量错误 */
    }
    
    const BigNum *list_arg = &args[0];
    const BigNum *index_arg = &args[1];
    
    if (!bignum_is_list(list_arg) || !bignum_is_number(index_arg)) {
        return BIGNUM_ERROR;  /* 参数类型错误 */
    }
    
    LIST *list = bignum_get_list(list_arg);
    if (list == NULL) {
        return BIGNUM_ERROR;
    }
    
    /* 将索引转换为 size_t */
    double index_double = bignum_to_double(index_arg);
    if (index_double < 0 || index_double >= list_size(list)) {
        return BIGNUM_ERROR;  /* 索引越界 */
    }
    
    size_t index = (size_t)index_double;
    
    /* 获取元素 */
    Obj obj = list_get_index(list, index);
    if (obj == NULL) {
        return BIGNUM_ERROR;
    }
    
    BigNum *element = (BigNum *)obj;
    return bignum_copy(element, result);
}

/* size() 函数：获取列表大小
 * 参数：1个（列表）
 * 返回：列表大小（数字）
 */
static int list_size_func(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)precision; /* 未使用 */
    
    if (arg_count != 1) {
        return BIGNUM_ERROR;  /* 参数数量错误 */
    }
    
    const BigNum *list_arg = &args[0];
    
    if (!bignum_is_list(list_arg)) {
        return BIGNUM_ERROR;  /* 参数必须是列表 */
    }
    
    LIST *list = bignum_get_list(list_arg);
    if (list == NULL) {
        return BIGNUM_ERROR;
    }
    
    size_t size = list_size(list);
    
    /* 将 size_t 转换为 BigNum */
    char size_str[32];
    snprintf(size_str, sizeof(size_str), "%zu", size);
    
    BigNum *size_num = bignum_from_string(size_str);
    if (size_num == NULL) {
        return BIGNUM_ERROR;
    }
    
    int ret = bignum_copy(size_num, result);
    bignum_destroy(size_num);
    
    return ret;
}

/* 包注册函数 - 由包管理器调用 */
int package_register(FunctionRegistry *registry) {
    if (registry == NULL) return -1;
    
    int count = 0;
    
    /* 注册 list() 函数 */
    if (function_register(registry, "list", list_create_func, 0, 0, 
                         "Create empty list") == 0) {
        count++;
    }
    
    /* 注册 lpush() 函数 */
    if (function_register(registry, "lpush", list_lpush_func, 2, 2, 
                         "Push element to left of list") == 0) {
        count++;
    }
    
    /* 注册 rpush() 函数 */
    if (function_register(registry, "rpush", list_rpush_func, 2, 2, 
                         "Push element to right of list") == 0) {
        count++;
    }
    
    /* 注册 lpop() 函数 */
    if (function_register(registry, "lpop", list_lpop_func, 1, 1, 
                         "Pop element from left of list") == 0) {
        count++;
    }
    
    /* 注册 rpop() 函数 */
    if (function_register(registry, "rpop", list_rpop_func, 1, 1, 
                         "Pop element from right of list") == 0) {
        count++;
    }
    
    /* 注册 lget() 函数 */
    if (function_register(registry, "lget", list_get_func, 2, 2, 
                         "Get element at index from list") == 0) {
        count++;
    }
    
    /* 注册 lsize() 函数 */
    if (function_register(registry, "lsize", list_size_func, 1, 1, 
                         "Get size of list") == 0) {
        count++;
    }
    
    return count;
}
