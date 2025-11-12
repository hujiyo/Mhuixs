#ifndef CONTEXT_H
#define CONTEXT_H

#include "bignum.h"

/**
 * 变量上下文管理模块
 * 
 * 提供变量的存储、查询和更新功能
 * 支持数学脚本语言的变量机制
 */

/* 变量结构 */
typedef struct {
    char *name;                    /* 变量名（动态分配） */
    BigNum *value;                 /* 变量值指针（堆分配） */
    int is_defined;                /* 是否已定义 */
} Variable;

/* 变量上下文 */
typedef struct {
    Variable *vars;                /* 变量数组（动态分配） */
    int count;                     /* 当前变量数量 */
    int capacity;                  /* 当前容量 */
} Context;

/**
 * 初始化上下文
 * 
 * @param ctx 上下文指针
 */
void context_init(Context *ctx);

/**
 * 设置变量值（如果变量不存在则创建）
 * 
 * @param ctx 上下文指针
 * @param name 变量名
 * @param value 变量值
 * @return 0 成功, -1 失败（变量名非法或空间不足）
 */
int context_set(Context *ctx, const char *name, const BigNum *value);

/**
 * 获取变量值
 * 
 * @param ctx 上下文指针
 * @param name 变量名
 * @param value 输出的变量值
 * @return 0 成功, -1 失败（变量未定义）
 */
int context_get(const Context *ctx, const char *name, BigNum *value);

/**
 * 检查变量是否存在
 * 
 * @param ctx 上下文指针
 * @param name 变量名
 * @return 1 存在, 0 不存在
 */
int context_has(const Context *ctx, const char *name);

/**
 * 列出所有变量（用于调试和显示）
 * 
 * @param ctx 上下文指针
 * @param buffer 输出缓冲区
 * @param max_len 缓冲区最大长度
 */
void context_list(const Context *ctx, char *buffer, size_t max_len);

/**
 * 清空所有变量
 * 
 * @param ctx 上下文指针
 */
void context_clear(Context *ctx);

#endif /* CONTEXT_H */

