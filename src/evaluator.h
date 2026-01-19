#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "bignum.h"

/**
 * 统一表达式求值器
 * 
 * 支持布尔运算和数值运算的无缝融合：
 * - 数值 != 0 被视为 true (布尔值 1)
 * - 数值 == 0 被视为 false (布尔值 0)
 * - 布尔运算结果 (0/1) 可以参与数值计算
 * - 支持在同一表达式中混用布尔和数值运算符
 * 
 * 支持的运算符:
 * 
 * 数值运算:
 * - +, -, *, /  : 加减乘除
 * - **          : 幂运算（右结合，指数必须为非负整数）
 * - %           : 取模（操作数必须为整数）
 * 
 * 比较运算:
 * - ==    : 等于
 * - !=    : 不等于
 * - >     : 大于
 * - >=    : 大于等于
 * - <     : 小于
 * - <=    : 小于等于
 * 
 * 布尔运算:
 * - ^     : 合取 (AND)
 * - v     : 析取 (OR)
 * - !     : 否定 (NOT)
 * - →     : 蕴含 (IMPLIES)
 * - ↔     : 等价 (IFF)
 * - ⊽     : 异或 (XOR)
 * 
 * 优先级 (从高到低):
 * 1. 括号 ()
 * 2. 一元运算符 (!, 负号)
 * 3. 幂运算 (**)
 * 4. 乘法/除法/取模 (*, /, %)
 * 5. 加法/减法 (+, -)
 * 6. 比较运算 (==, !=, >, >=, <, <=)
 * 7. 逻辑运算 (^, v, →, ↔, ⊽)
 */

/* 返回值宏 */
#define EVAL_SUCCESS    0      /* 成功 */
#define EVAL_ERROR     -1      /* 语法错误 */
#define EVAL_DIV_ZERO  -2      /* 除零错误 */

/**
 * 求值表达式（支持布尔和数值混合，支持变量引用）
 * 
 * @param expr 表达式字符串
 * @param result 结果大数
 * @param ctx 变量上下文（可以为NULL）
 * @param precision 精度（-1 表示使用默认精度）
 * @return 0 成功, -1 错误, -2 除零错误
 */
int eval_expression(const char *expr, BHS *result, void *ctx, int precision);

/**
 * 求值并转换为字符串
 * 
 * @param expr 表达式字符串
 * @param result_str 结果字符串
 * @param max_len 最大长度
 * @param ctx 变量上下文（可以为NULL）
 * @param precision 精度（-1 表示使用默认精度）
 * @return 0 成功, -1 错误, -2 除零错误
 */
int eval_to_string(const char *expr, char *result_str, size_t max_len, void *ctx, int precision);

/**
 * 执行语句（支持赋值语句: let A = expr, 导入语句: import math）
 * 
 * @param stmt 语句字符串
 * @param result_str 结果字符串（用于显示）
 * @param max_len 最大长度
 * @param ctx 变量上下文（必须提供）
 * @param func_registry 函数注册表（必须提供）
 * @param pkg_manager 包管理器（必须提供）
 * @param precision 精度（-1 表示使用默认精度）
 * @return 0 成功, -1 错误, -2 除零错误, 1 表示是赋值语句, 2 表示是导入语句
 */
int eval_statement(const char *stmt, char *result_str, size_t max_len, void *ctx, void *func_registry, void *pkg_manager, int precision);

/**
 * 判断大数是否为"真"（非零）
 * 
 * @param num 大数
 * @return 1 为真, 0 为假
 */
int bignum_is_true(const BHS *num);

/**
 * 将大数转换为布尔值（0或1）
 * 
 * @param num 输入大数
 * @param result 输出布尔值（0或1）
 * @return 0 成功
 */
int bignum_to_bool(const BHS *num, BHS *result);

#endif /* EVALUATOR_H */

