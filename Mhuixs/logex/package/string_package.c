/*
 * String Package for Logex
 * 提供字符串和数字之间的类型转换函数
 */

#include <stdlib.h>
#include "../function.h"
#include "../bignum.h"

/* num() 函数：将字符串转换为数字
 * 参数：1个（字符串类型的 BigNum）
 * 返回：转换成功返回数字，失败报错
 */
static int string_num(const BigNum *args, int arg_count, BigNum *result, int precision) {
    (void)precision;  /* 未使用 */
    
    if (arg_count != 1) {
        return BIGNUM_ERROR;  /* 参数数量错误 */
    }
    
    const BigNum *input = &args[0];
    
    /* 如果已经是数字类型，直接返回 */
    if (bignum_is_number(input)) {
        return bignum_copy(input, result);
    }
    
    /* 尝试将字符串转换为数字 */
    if (bignum_string_to_number(input, result) == BIGNUM_SUCCESS) {
        return BIGNUM_SUCCESS;
    }
    
    /* 转换失败 */
    return BIGNUM_ERROR;
}

/* str() 函数：将数字转换为字符串
 * 参数：1个（数字类型的 BigNum）
 * 返回：转换成功返回字符串，失败报错
 */
static int string_str(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) {
        return BIGNUM_ERROR;  /* 参数数量错误 */
    }
    
    const BigNum *input = &args[0];
    
    /* 如果已经是字符串类型，直接返回 */
    if (bignum_is_string(input)) {
        return bignum_copy(input, result);
    }
    
    /* 将数字转换为字符串 */
    if (bignum_number_to_string_type(input, result, precision) == BIGNUM_SUCCESS) {
        return BIGNUM_SUCCESS;
    }
    
    /* 转换失败 */
    return BIGNUM_ERROR;
}

/* 包注册函数 - 由包管理器调用 */
int package_register(FunctionRegistry *registry) {
    if (registry == NULL) return -1;
    
    int count = 0;
    
    /* 注册 num() 函数 */
    if (function_register(registry, "num", string_num, 1, 1, 
                         "Convert string to number") == 0) {
        count++;
    }
    
    /* 注册 str() 函数 */
    if (function_register(registry, "str", string_str, 1, 1, 
                         "Convert number to string") == 0) {
        count++;
    }
    
    return count;
}

