/**
 * 简化版内置函数测试 - 直接测试 builtin.c 的函数
 */

#include <stdio.h>
#include <string.h>
#include "builtin.h"
#include "bignum.h"

void test_list_functions() {
    printf("========== 测试 LIST 函数 ==========\n");
    
    BigNum result;
    bignum_init(&result);
    
    /* 测试 list() */
    printf("1. 测试 list()...\n");
    const BuiltinFunctionInfo *func = builtin_lookup("list");
    if (func) {
        printf("   [DEBUG] 找到 list 函数\n");
        int ret = builtin_call(func, NULL, 0, &result, 10);
        printf("   [DEBUG] builtin_call 返回: %d\n", ret);
        printf("   [DEBUG] result.type: %d\n", result.type);
        printf("   [DEBUG] bignum_is_list: %d\n", bignum_is_list(&result));
        if (ret == 0 && bignum_is_list(&result)) {
            printf("   ✅ list() 创建成功\n");
            
            /* 测试 rpush */
            printf("2. 测试 rpush()...\n");
            BigNum args[2];
            bignum_init(&args[0]);
            bignum_init(&args[1]);
            
            bignum_copy(&result, &args[0]);
            BigNum *num = bignum_from_string("100");
            bignum_copy(num, &args[1]);
            bignum_destroy(num);
            
            func = builtin_lookup("rpush");
            if (func) {
                BigNum result2;
                bignum_init(&result2);
                ret = builtin_call(func, args, 2, &result2, 10);
                if (ret == 0) {
                    printf("   ✅ rpush() 成功\n");
                    
                    /* 测试 llen */
                    printf("3. 测试 llen()...\n");
                    func = builtin_lookup("llen");
                    if (func) {
                        BigNum len_result;
                        bignum_init(&len_result);
                        ret = builtin_call(func, &result2, 1, &len_result, 10);
                        if (ret == 0) {
                            char len_str[64];
                            bignum_to_string(&len_result, len_str, sizeof(len_str), 0);
                            printf("   ✅ llen() = %s\n", len_str);
                        }
                        bignum_free(&len_result);
                    }
                    bignum_free(&result2);
                }
            }
            
            bignum_free(&args[0]);
            bignum_free(&args[1]);
        } else {
            printf("   ❌ list() 失败\n");
        }
    } else {
        printf("   ❌ 找不到 list 函数\n");
    }
    
    bignum_free(&result);
    printf("\n");
}

void test_type_functions() {
    printf("========== 测试 TYPE 转换函数 ==========\n");
    
    /* 测试 num() */
    printf("1. 测试 num(\"123.456\")...\n");
    const BuiltinFunctionInfo *func = builtin_lookup("num");
    if (func) {
        BigNum arg;
        bignum_init(&arg);
        BigNum *str_num = bignum_from_raw_string("123.456");
        bignum_copy(str_num, &arg);
        bignum_destroy(str_num);
        
        BigNum result;
        bignum_init(&result);
        int ret = builtin_call(func, &arg, 1, &result, 10);
        if (ret == 0) {
            char result_str[64];
            bignum_to_string(&result, result_str, sizeof(result_str), 10);
            printf("   ✅ num(\"123.456\") = %s\n", result_str);
        } else {
            printf("   ❌ num() 失败\n");
        }
        
        bignum_free(&arg);
        bignum_free(&result);
    }
    
    /* 测试 str() */
    printf("2. 测试 str(789)...\n");
    func = builtin_lookup("str");
    if (func) {
        BigNum arg;
        bignum_init(&arg);
        BigNum *num = bignum_from_string("789");
        bignum_copy(num, &arg);
        bignum_destroy(num);
        
        BigNum result;
        bignum_init(&result);
        int ret = builtin_call(func, &arg, 1, &result, 10);
        if (ret == 0 && bignum_is_string(&result)) {
            printf("   ✅ str(789) 成功，类型为字符串\n");
        } else {
            printf("   ❌ str() 失败\n");
        }
        
        bignum_free(&arg);
        bignum_free(&result);
    }
    
    printf("\n");
}

void test_bitmap_functions() {
    printf("========== 测试 BITMAP 函数 ==========\n");
    
    /* 测试 bmp() */
    printf("1. 测试 bmp(0)...\n");
    const BuiltinFunctionInfo *func = builtin_lookup("bmp");
    if (func) {
        BigNum arg;
        bignum_init(&arg);
        BigNum *zero = bignum_from_string("0");
        bignum_copy(zero, &arg);
        bignum_destroy(zero);
        
        BigNum result;
        bignum_init(&result);
        int ret = builtin_call(func, &arg, 1, &result, 10);
        if (ret == 0 && bignum_is_bitmap(&result)) {
            printf("   ✅ bmp(0) 创建成功\n");
            
            /* 测试 bset */
            printf("2. 测试 bset(bm, 10, 1)...\n");
            func = builtin_lookup("bset");
            if (func) {
                BigNum args[3];
                bignum_init(&args[0]);
                bignum_init(&args[1]);
                bignum_init(&args[2]);
                
                bignum_copy(&result, &args[0]);
                BigNum *offset = bignum_from_string("10");
                BigNum *value = bignum_from_string("1");
                bignum_copy(offset, &args[1]);
                bignum_copy(value, &args[2]);
                bignum_destroy(offset);
                bignum_destroy(value);
                
                BigNum result2;
                bignum_init(&result2);
                ret = builtin_call(func, args, 3, &result2, 10);
                if (ret == 0) {
                    printf("   ✅ bset() 成功\n");
                    
                    /* 测试 bget */
                    printf("3. 测试 bget(bm, 10)...\n");
                    func = builtin_lookup("bget");
                    if (func) {
                        BigNum get_args[2];
                        bignum_init(&get_args[0]);
                        bignum_init(&get_args[1]);
                        bignum_copy(&result2, &get_args[0]);
                        bignum_copy(&args[1], &get_args[1]);
                        
                        BigNum get_result;
                        bignum_init(&get_result);
                        ret = builtin_call(func, get_args, 2, &get_result, 10);
                        if (ret == 0) {
                            char bit_str[8];
                            bignum_to_string(&get_result, bit_str, sizeof(bit_str), 0);
                            printf("   ✅ bget(bm, 10) = %s\n", bit_str);
                        }
                        
                        bignum_free(&get_args[0]);
                        bignum_free(&get_args[1]);
                        bignum_free(&get_result);
                    }
                    
                    bignum_free(&result2);
                }
                
                bignum_free(&args[0]);
                bignum_free(&args[1]);
                bignum_free(&args[2]);
            }
            
            bignum_free(&result);
        } else {
            printf("   ❌ bmp() 失败\n");
        }
        
        bignum_free(&arg);
    }
    
    printf("\n");
}

int main() {
    printf("========================================\n");
    printf("Logex 内置函数直接测试\n");
    printf("========================================\n\n");
    
    test_list_functions();
    test_type_functions();
    test_bitmap_functions();
    
    printf("========================================\n");
    printf("测试完成\n");
    printf("========================================\n");
    
    return 0;
}
