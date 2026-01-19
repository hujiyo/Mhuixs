#ifndef BUILTIN_H
#define BUILTIN_H

#include "bignum.h"

/**
 * Logex 内置函数
 * 
 * 这些是 Mhuixs 核心功能，不需要 import，直接可用
 * 包括：LIST 操作、TYPE 转换、BITMAP 操作等
 */

/* 内置函数指针类型 */
typedef int (*BuiltinFunction)(const BigNum *args, int arg_count, BigNum *result, int precision);

/* 内置函数信息 */
typedef struct {
    const char *name;
    BuiltinFunction func;
    int min_args;
    int max_args;  /* -1 表示可变参数 */
} BuiltinFunctionInfo;

/**
 * 查找内置函数
 * @param name 函数名
 * @return 内置函数信息，找不到返回 NULL
 */
const BuiltinFunctionInfo* builtin_lookup(const char *name);

/**
 * 调用内置函数
 * @param info 内置函数信息
 * @param args 参数数组
 * @param arg_count 参数数量
 * @param result 结果
 * @param precision 精度
 * @return 0 成功, -1 失败
 */
int builtin_call(const BuiltinFunctionInfo *info, 
                 const BigNum *args, 
                 int arg_count, 
                 BigNum *result, 
                 int precision);

#endif /* BUILTIN_H */
