#ifndef BIGNUM_H
#define BIGNUM_H

#include <stddef.h>

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
#define BIGNUM_SMALL_SIZE 32          /* 小数据内联存储大小 */

/* 类型标记 */
#define BIGNUM_TYPE_NUMBER  0         /* 数字类型 */
#define BIGNUM_TYPE_STRING  1         /* 字符串类型 */

/* 大数结构体 - 固定64字节 */
typedef struct {
    union {
        char small_data[BIGNUM_SMALL_SIZE];  /* 小数据内联存储（32字节） */
        char *large_data;                     /* 大数据动态分配指针（8字节） */
    } data;                                   /* 32字节（联合体取最大） */
    int length;                               /* 数字长度或字符串长度（4字节） */
    int capacity;                             /* 分配的容量（4字节） */
    int type;                                 /* 类型标记（4字节） */
    int is_large;                             /* 是否使用大数据存储（4字节） */
    union {
        struct {
            int decimal_pos;                  /* 小数点位置（从右边数）（4字节） */
            int is_negative;                  /* 是否为负数（4字节） */
        } num;                                /* 数字类型专有字段 */
        struct {
            int reserved1;                    /* 保留字段1（4字节） */
            int reserved2;                    /* 保留字段2（4字节） */
        } str;                                /* 字符串类型专有字段（未来扩展） */
        char padding[8];                      /* 确保联合体为8字节 */
    } type_data;                              /* 8字节（类型特定数据） */
} BigNum;  /* 总大小：32+4+4+4+4+8=56字节，填充到64字节 */

/* 获取digits指针的辅助宏 */
#define BIGNUM_DIGITS(num) ((num)->is_large ? (num)->data.large_data : (char *)(num)->data.small_data)

/* 返回值宏 */
#define BIGNUM_SUCCESS  0      /* 成功 */
#define BIGNUM_ERROR   -1      /* 错误 */
#define BIGNUM_DIV_ZERO -2     /* 除零错误 */

/**
 * 初始化大数为0
 * 
 * @param num 大数结构
 */
void bignum_init(BigNum *num);

/**
 * 清理大数（释放动态内存）
 * 
 * @param num 大数结构
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
 * 从字符串创建大数
 * 
 * @param str 数字字符串（支持小数和负数，如 "-123.456"）
 * @param num 输出的大数结构
 * @return 0 成功, -1 失败
 */
int bignum_from_string(const char *str, BigNum *num);

/**
 * 从原始字符串创建字符串类型的 BigNum
 * 
 * @param str 任意字符串
 * @param num 输出的大数结构
 * @return 0 成功, -1 失败
 */
int bignum_from_raw_string(const char *str, BigNum *num);

/**
 * 将大数转换为字符串
 * 
 * @param num 大数结构
 * @param str 输出字符串缓冲区
 * @param max_len 缓冲区最大长度
 * @param precision 小数精度（-1表示使用默认精度）
 * @return 0 成功, -1 失败
 */
int bignum_to_string(const BigNum *num, char *str, size_t max_len, int precision);

/**
 * 大数加法
 * 
 * @param a 被加数
 * @param b 加数
 * @param result 结果
 * @return 0 成功, -1 失败
 */
int bignum_add(const BigNum *a, const BigNum *b, BigNum *result);

/**
 * 大数减法
 * 
 * @param a 被减数
 * @param b 减数
 * @param result 结果
 * @return 0 成功, -1 失败
 */
int bignum_sub(const BigNum *a, const BigNum *b, BigNum *result);

/**
 * 大数乘法
 * 
 * @param a 被乘数
 * @param b 乘数
 * @param result 结果
 * @return 0 成功, -1 失败
 */
int bignum_mul(const BigNum *a, const BigNum *b, BigNum *result);

/**
 * 大数除法
 * 
 * @param a 被除数
 * @param b 除数
 * @param result 结果
 * @param precision 除法精度（小数位数，-1表示使用默认精度）
 * @return 0 成功, -1 失败, -2 除零错误
 */
int bignum_div(const BigNum *a, const BigNum *b, BigNum *result, int precision);

/**
 * 大数幂运算
 * 
 * @param base 底数
 * @param exponent 指数（必须为非负整数）
 * @param result 结果
 * @param precision 计算精度（小数位数，-1表示使用默认精度）
 * @return 0 成功, -1 失败
 */
int bignum_pow(const BigNum *base, const BigNum *exponent, BigNum *result, int precision);

/**
 * 大数取模运算
 * 
 * @param a 被除数
 * @param b 除数（模数）
 * @param result 结果（余数）
 * @return 0 成功, -1 失败, -2 除零错误
 */
int bignum_mod(const BigNum *a, const BigNum *b, BigNum *result);

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
 * 尝试将字符串类型的 BigNum 转换为数字类型
 * 
 * @param str_num 字符串类型的 BigNum
 * @param num_result 输出的数字类型 BigNum
 * @return 0 成功, -1 失败（不是有效数字字符串）
 */
int bignum_string_to_number(const BigNum *str_num, BigNum *num_result);

/**
 * 将数字类型的 BigNum 转换为字符串类型
 * 
 * @param num 数字类型的 BigNum
 * @param str_result 输出的字符串类型 BigNum
 * @param precision 精度（-1 表示使用默认精度）
 * @return 0 成功, -1 失败
 */
int bignum_number_to_string_type(const BigNum *num, BigNum *str_result, int precision);

/* 
 * bignum_eval 已弃用 - 请使用 evaluator.h 中的 eval_to_string()
 * 该函数支持更多功能（布尔运算、比较运算等）
 */

#endif /* BIGNUM_H */

