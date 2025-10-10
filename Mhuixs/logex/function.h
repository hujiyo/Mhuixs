#ifndef FUNCTION_H
#define FUNCTION_H

#include "bignum.h"

/**
 * 函数注册和调用机制
 * 
 * 支持将C语言实现的函数注册到解释器中，
 * 使得用户可以在表达式中直接调用这些函数。
 */

#define MAX_FUNCTIONS 256      /* 最大函数数量 */
#define MAX_FUNC_NAME_LEN 64   /* 函数名最大长度 */
#define MAX_FUNC_ARGS 10       /* 函数最大参数数量 */

/* 函数指针类型定义 */
typedef int (*NativeFunction)(const BigNum *args, int arg_count, BigNum *result, int precision);

/* 函数元信息 */
typedef struct {
    char name[MAX_FUNC_NAME_LEN];    /* 函数名 */
    NativeFunction func;              /* C函数指针 */
    int min_args;                     /* 最小参数数量 */
    int max_args;                     /* 最大参数数量（-1表示无限制） */
    char description[256];            /* 函数描述 */
    int is_defined;                   /* 是否已定义 */
} FunctionInfo;

/* 函数注册表 */
typedef struct {
    FunctionInfo functions[MAX_FUNCTIONS];
    int count;
} FunctionRegistry;

/**
 * 初始化函数注册表
 */
void function_registry_init(FunctionRegistry *registry);

/**
 * 注册一个函数
 * 
 * @param registry 函数注册表
 * @param name 函数名
 * @param func C函数指针
 * @param min_args 最小参数数量
 * @param max_args 最大参数数量（-1表示无限制）
 * @param description 函数描述
 * @return 0 成功, -1 失败
 */
int function_register(FunctionRegistry *registry, 
                     const char *name, 
                     NativeFunction func,
                     int min_args,
                     int max_args,
                     const char *description);

/**
 * 查找函数
 * 
 * @param registry 函数注册表
 * @param name 函数名
 * @return 函数信息指针，找不到返回NULL
 */
FunctionInfo* function_lookup(FunctionRegistry *registry, const char *name);

/**
 * 调用函数
 * 
 * @param info 函数信息
 * @param args 参数数组
 * @param arg_count 参数数量
 * @param result 结果
 * @param precision 精度
 * @return 0 成功, -1 失败
 */
int function_call(FunctionInfo *info, 
                 const BigNum *args, 
                 int arg_count, 
                 BigNum *result, 
                 int precision);

/**
 * 列出所有已注册的函数
 * 
 * @param registry 函数注册表
 * @param output 输出缓冲区
 * @param max_len 缓冲区大小
 */
void function_list(FunctionRegistry *registry, char *output, size_t max_len);

#endif /* FUNCTION_H */


