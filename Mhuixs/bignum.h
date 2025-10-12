#ifndef BIGNUM_H
#define BIGNUM_H

#include <stddef.h>

#include "bitmap.h"
#include "tblh.h"
#include "list.h"
#include "obj.h"

/**
 * 任意精度数值计算库
 * 
 * 支持的运算符:
 * - +: 加法
 * - -: 减法
 * - *: 乘法
 * - /: 除法
 * 
 * 支持小数运算，精度可通过宏定义配置
 */

/* 宏定义 - 精度设置 */
#define BIGNUM_DEFAULT_PRECISION 100  /* 默认小数精度（小数点后位数） */
#define BIGNUM_MAX_DIGITS 10000       /* 最大数字位数（仅对数字类型有效） */


/* 类型标记 */
#define BIGNUM_TYPE_NUMBER  0         /* 数字类型 */
#define BIGNUM_TYPE_STRING  1         /* 字符串类型 */


/* 获取digits指针的辅助宏 */
#define BIGNUM_DIGITS(num) ((num)->is_large ? (num)->data.large_data : (char *)(num)->data.small_data)

/* 返回值宏 */
#define BIGNUM_SUCCESS  0      /* 成功 */
#define BIGNUM_ERROR   -1      /* 错误 */
#define BIGNUM_DIV_ZERO -2     /* 除零错误 */

/**
 * 创建并初始化一个新的 BigNum（堆分配）
 * 
 * @return 新创建的 BigNum 指针，失败返回 NULL
 */
BigNum* bignum_create(void);

/**
 * 销毁 BigNum 并释放所有内存（包括结构体本身）
 * 
 * @param num 大数指针
 */
void bignum_destroy(BigNum *num);

/**
 * 初始化大数为0（用于栈分配的 BigNum，不推荐使用）
 * 
 * @param num 大数结构
 * @deprecated 请使用 bignum_create() 代替
 */
void bignum_init(BigNum *num);

/**
 * 清理大数内部数据（不释放结构体本身，不推荐使用）
 * 
 * @param num 大数结构
 * @deprecated 请使用 bignum_destroy() 代替
 */
void bignum_free(BigNum *num);

/**
 * 复制大数
 * 
 * @param src 源大数
 * @param dst 目标大数
 * @return 0 成功, -1 失败
 */
int bignum_copy(const BigNum *src, BigNum *dst);

/**
 * 从字符串创建大数（返回新分配的 BigNum）
 * 
 * @param str 数字字符串（支持小数和负数，如 "-123.456"）
 * @return 新创建的 BigNum 指针，失败返回 NULL
 */
BigNum* bignum_from_string(const char *str);

/**
 * 从原始字符串创建字符串类型的 BigNum（返回新分配的 BigNum）
 * 
 * @param str 任意字符串
 * @return 新创建的 BigNum 指针，失败返回 NULL
 */
BigNum* bignum_from_raw_string(const char *str);

/**
 * 从字符串创建大数（旧版本 API，已弃用）
 * 
 * @param str 数字字符串
 * @param num 输出的大数结构
 * @return 0 成功, -1 失败
 * @deprecated 请使用 bignum_from_string(str) 代替
 */
int bignum_from_string_legacy(const char *str, BigNum *num);

/**
 * 从原始字符串创建字符串类型的 BigNum（旧版本 API，已弃用）
 * 
 * @param str 任意字符串
 * @param num 输出的大数结构
 * @return 0 成功, -1 失败
 * @deprecated 请使用 bignum_from_raw_string(str) 代替
 */
int bignum_from_raw_string_legacy(const char *str, BigNum *num);

/**
 * 将大数格式化为 C 字符串（用于打印输出）
 * 
 * 注意：此函数仅用于格式化输出，不改变 BigNum 的类型！
 * - 如果 BigNum 是数字类型，格式化为 "123.456" 形式
 * - 如果 BigNum 是字符串类型，格式化为 "\"hello\"" 形式（带引号）
 * - 输出到用户提供的 C 字符串缓冲区
 * 
 * 如果需要进行类型转换（数字→字符串类型的 BigNum），请使用：
 *   bignum_number_to_string_type()
 * 
 * @param num 大数结构
 * @param str 输出字符串缓冲区（C 字符串 char*）
 * @param max_len 缓冲区最大长度
 * @param precision 小数精度（-1表示使用默认精度）
 * @return 0 成功, -1 失败
 */
int bignum_to_string(const BigNum *num, char *str, size_t max_len, int precision);

/**
 * 大数加法（返回新分配的 BigNum）
 * 
 * @param a 被加数
 * @param b 加数
 * @return 结果 BigNum 指针，失败返回 NULL
 */
BigNum* bignum_add(const BigNum *a, const BigNum *b);

/**
 * 大数减法（返回新分配的 BigNum）
 * 
 * @param a 被减数
 * @param b 减数
 * @return 结果 BigNum 指针，失败返回 NULL
 */
BigNum* bignum_sub(const BigNum *a, const BigNum *b);

/**
 * 大数乘法（返回新分配的 BigNum）
 * 
 * @param a 被乘数
 * @param b 乘数
 * @return 结果 BigNum 指针，失败返回 NULL
 */
BigNum* bignum_mul(const BigNum *a, const BigNum *b);

/**
 * 大数除法（返回新分配的 BigNum）
 * 
 * @param a 被除数
 * @param b 除数
 * @param precision 除法精度（小数位数，-1表示使用默认精度）
 * @return 结果 BigNum 指针，失败返回 NULL（除零返回 NULL）
 */
BigNum* bignum_div(const BigNum *a, const BigNum *b, int precision);

/**
 * 大数幂运算（返回新分配的 BigNum）
 * 
 * @param base 底数
 * @param exponent 指数（必须为非负整数）
 * @param precision 计算精度（小数位数，-1表示使用默认精度）
 * @return 结果 BigNum 指针，失败返回 NULL
 */
BigNum* bignum_pow(const BigNum *base, const BigNum *exponent, int precision);

/**
 * 大数取模运算（返回新分配的 BigNum）
 * 
 * @param a 被除数
 * @param b 除数（模数）
 * @return 结果 BigNum 指针，失败返回 NULL（除零返回 NULL）
 */
BigNum* bignum_mod(const BigNum *a, const BigNum *b);

/**
 * 判断 BigNum 是否为数字类型
 * 
 * @param num BigNum 结构
 * @return 1 是数字, 0 是字符串
 */
int bignum_is_number(const BigNum *num);

/**
 * 判断 BigNum 是否为字符串类型
 * 
 * @param num BigNum 结构
 * @return 1 是字符串, 0 是数字
 */
int bignum_is_string(const BigNum *num);

/**
 * 尝试将字符串类型的 BigNum 转换为数字类型（返回新分配的 BigNum）
 * 
 * @param str_num 字符串类型的 BigNum
 * @return 新的数字类型 BigNum 指针，失败返回 NULL
 */
BigNum* bignum_string_to_number(const BigNum *str_num);

/**
 * 将数字类型的 BigNum 转换为字符串类型的 BigNum（返回新分配的 BigNum）
 * 
 * 注意：这是真正的类型转换函数！
 * - 输入：BIGNUM_TYPE_NUMBER (type=0)
 * - 输出：BIGNUM_TYPE_STRING (type=1)
 * - 内部流程：先格式化成 C 字符串，再创建字符串类型的 BigNum
 * 
 * 用途：在 Logex 类型系统内进行数字→字符串的类型转换
 * 例如：package/string 中的 str() 函数就是调用这个函数实现的
 * 
 * 如果只是需要打印输出，请使用：bignum_to_string()
 * 
 * @param num 数字类型的 BigNum (必须是 BIGNUM_TYPE_NUMBER)
 * @param precision 精度（-1 表示使用默认精度）
 * @return 新的字符串类型 BigNum 指针，失败返回 NULL
 */
BigNum* bignum_number_to_string_type(const BigNum *num, int precision);

/* 
 * bignum_eval 已弃用 - 请使用 evaluator.h 中的 eval_to_string()
 * 该函数支持更多功能（布尔运算、比较运算等）
 */

#endif /* BIGNUM_H */

