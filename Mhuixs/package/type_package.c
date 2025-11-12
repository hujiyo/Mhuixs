/*
 * Type Package for Logex
 * 提供字符串、数字和位图之间的类型转换函数
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../function.h"
#include "../bignum.h"
#include "../lib/list.h"

/* num() 函数：将字符串或位图转换为数字
 * 参数：1个（字符串或位图类型的 BigNum）
 * 返回：转换成功返回数字，失败报错
 */
static int type_num(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)precision;  /* 未使用 */
    
    if (arg_count != 1) {
        return BIGNUM_ERROR;  /* 参数数量错误 */
    }
    
    const BigNum *input = &args[0];
    
    /* 如果已经是数字类型，直接返回 */
    if (bignum_is_number(input)) {
        return bignum_copy(input, result);
    }
    
    /* 如果是位图类型，转换为数字 */
    if (bignum_is_bitmap(input)) {
        BigNum *temp = bignum_bitmap_to_number(input);
        if (temp != NULL) {
            int ret = bignum_copy(temp, result);
            bignum_destroy(temp);
            return ret;
        }
        return BIGNUM_ERROR;
    }
    
    /* 尝试将字符串转换为数字 */
    BigNum *temp = bignum_string_to_number(input);
    if (temp != NULL) {
        int ret = bignum_copy(temp, result);
        bignum_destroy(temp);
        return ret;
    }
    
    /* 转换失败 */
    return BIGNUM_ERROR;
}

/* str() 函数：将数字或bitmap转换为字符串
 * 参数：1个（数字或bitmap类型的 BigNum）
 * 返回：转换成功返回字符串，失败报错
 */
static int type_str(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) {
        return BIGNUM_ERROR;  /* 参数数量错误 */
    }
    
    const BigNum *input = &args[0];
    
    /* 如果已经是字符串类型，直接返回 */
    if (bignum_is_string(input)) {
        return bignum_copy(input, result);
    }
    
    /* 如果是bitmap类型，先转换为数字再转为字符串 */
    if (bignum_is_bitmap(input)) {
        BigNum *num_temp = bignum_bitmap_to_number(input);
        if (num_temp == NULL) return BIGNUM_ERROR;
        
        BigNum *str_temp = bignum_number_to_string_type(num_temp, precision);
        bignum_destroy(num_temp);
        
        if (str_temp != NULL) {
            int ret = bignum_copy(str_temp, result);
            bignum_destroy(str_temp);
            return ret;
        }
        return BIGNUM_ERROR;
    }
    
    /* 如果是数字类型，直接转换为字符串 */
    if (bignum_is_number(input)) {
        BigNum *temp = bignum_number_to_string_type(input, precision);
        if (temp != NULL) {
            int ret = bignum_copy(temp, result);
            bignum_destroy(temp);
            return ret;
        }
    }
    
    /* 转换失败 */
    return BIGNUM_ERROR;
}

/* bmp() 函数：将字符串或数字转换为位图
 * 参数：1个（字符串或数字类型的 BigNum）
 * 返回：转换成功返回位图，失败报错
 */
static int type_bmp(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)precision;  /* 未使用 */
    
    if (arg_count != 1) {
        return BIGNUM_ERROR;  /* 参数数量错误 */
    }
    
    const BigNum *input = &args[0];
    
    /* 如果已经是位图类型，直接返回 */
    if (bignum_is_bitmap(input)) {
        return bignum_copy(input, result);
    }
    
    /* 如果是数字类型，转换为位图 */
    if (bignum_is_number(input)) {
        BigNum *temp = bignum_number_to_bitmap(input);
        if (temp != NULL) {
            int ret = bignum_copy(temp, result);
            bignum_destroy(temp);
            return ret;
        }
        return BIGNUM_ERROR;
    }
    
    /* 如果是字符串类型，尝试将其作为二进制字符串解析 */
    if (bignum_is_string(input)) {
        /* 提取字符串内容 */
        char *str_data = BIGNUM_DIGITS(input);
        char *temp_str = (char *)malloc(input->length + 1);
        if (temp_str == NULL) return BIGNUM_ERROR;
        
        memcpy(temp_str, str_data, input->length);
        temp_str[input->length] = '\0';
        
        /* 尝试解析为二进制字符串 */
        BigNum *temp = bignum_from_binary_string(temp_str);
        free(temp_str);
        
        if (temp != NULL) {
            int ret = bignum_copy(temp, result);
            bignum_destroy(temp);
            return ret;
        }
        return BIGNUM_ERROR;
    }
    
    /* 转换失败 */
    return BIGNUM_ERROR;
}

/* lst() 函数：创建空列表或将其他类型转换为列表
 * 参数：0个或1个
 * 返回：列表类型
 */
static int type_lst(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)precision;  /* 未使用 */
    
    if (arg_count == 0) {
        /* 创建空列表 */
        BigNum *list_num = bignum_create_list();
        if (list_num == NULL) {
            return BIGNUM_ERROR;
        }
        
        int ret = bignum_copy(list_num, result);
        bignum_destroy(list_num);
        return ret;
    } else if (arg_count == 1) {
        const BigNum *input = &args[0];
        
        /* 如果已经是列表类型，直接返回 */
        if (bignum_is_list(input)) {
            return bignum_copy(input, result);
        }
        
        /* 创建新列表并添加元素 */
        BigNum *list_num = bignum_create_list();
        if (list_num == NULL) {
            return BIGNUM_ERROR;
        }
        
        LIST *list = bignum_get_list(list_num);
        if (list == NULL) {
            bignum_destroy(list_num);
            return BIGNUM_ERROR;
        }
        
        /* 创建元素的副本 */
        BigNum *element_copy = bignum_create();
        if (element_copy == NULL) {
            bignum_destroy(list_num);
            return BIGNUM_ERROR;
        }
        
        int ret = bignum_copy(input, element_copy);
        if (ret != BIGNUM_SUCCESS) {
            bignum_destroy(element_copy);
            bignum_destroy(list_num);
            return BIGNUM_ERROR;
        }
        
        /* 添加到列表 */
        if (list_rpush(list, (Obj)element_copy) != 0) {
            bignum_destroy(element_copy);
            bignum_destroy(list_num);
            return BIGNUM_ERROR;
        }
        
        /* 更新长度 */
        list_num->length = list_size(list);
        
        ret = bignum_copy(list_num, result);
        bignum_destroy(list_num);
        return ret;
    }
    
    /* 参数数量错误 */
    return BIGNUM_ERROR;
}

/* 包注册函数 - 由包管理器调用 */
int package_register(FunctionRegistry *registry) {
    if (registry == NULL) return -1;
    
    int count = 0;
    
    /* 注册 num() 函数 */
    if (function_register(registry, "num", type_num, 1, 1, 
                         "Convert string/bitmap to number") == 0) {
        count++;
    }
    
    /* 注册 str() 函数 */
    if (function_register(registry, "str", type_str, 1, 1, 
                         "Convert number to string") == 0) {
        count++;
    }
    
    /* 注册 bmp() 函数 */
    if (function_register(registry, "bmp", type_bmp, 1, 1, 
                         "Convert string/number to bitmap") == 0) {
        count++;
    }
    
    /* 注册 lst() 函数 */
    if (function_register(registry, "lst", type_lst, 0, 1, 
                         "Create list or convert to list") == 0) {
        count++;
    }
    
    return count;
}

