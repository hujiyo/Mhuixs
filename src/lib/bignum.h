#ifndef BIGNUM_H
#define BIGNUM_H

#include <stddef.h>
#include <stdlib.h>

/* ========================================
 * BHS 类型定义（原 share/obj.h）
 * ======================================== */

/* BHS 相关常量定义 */
#define BIGNUM_SMALL_SIZE 32          /* 小数据内联存储大小 */

/* 类型标记 */
#define BIGNUM_TYPE_NULL    -1         /* 空类型 */
#define BIGNUM_TYPE_NUMBER  0         /* 数字类型 */
#define BIGNUM_TYPE_STRING  1         /* 字符串类型 */
#define BIGNUM_TYPE_BITMAP  2         /* 位图类型 */

#define BIGNUM_TYPE_LIST    3         /* 列表类型 */
#define BIGNUM_TYPE_TABLE   4         /* 表格类型 */
#define BIGNUM_TYPE_KVALOT  5         /* 键值对类型 */

#define BIGNUM_TYPE_HOOK    6         /* 钩子类型 */
#define BIGNUM_TYPE_KEY     7         /* 键类型 */

/* BHS 结构体定义 - 固定64字节 */
typedef struct {
    int type;                                 /* 类型标记（4字节） */
    int is_large;                             /* 是否使用大数据存储（4字节） */
    
    union {
        char small_data[BIGNUM_SMALL_SIZE];  /* 小数据内联存储（32字节） */
        char *large_data;                     /* 大数据动态分配指针（8字节） */
        /* 兼容 Mhuixs 类型的字段 */
        struct LIST *list;                    /* LIST类型指针 */
        /* TABLE *table; */
        /* HOOK *hook; */
    } data;                                   /* 32字节（联合体取最大） */    
    size_t capacity;                          /* 分配的容量（4字节） */    
    size_t length;                            /* 数字长度或字符串长度（4字节） */
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
} BHS, BigNum, basic_handle_struct, bhs;  /* BHS 是推荐的类型名，BigNum 保留兼容 */

typedef BHS* Obj;

/* ========================================
 * 依赖库引用
 * ======================================== */

#ifdef LOGEX_BUILD
/* Logex 简化模式 - 仅包含必要的头文件 */
#include "lib/bitmap.h"
#else
/* Mhuixs 完整模式 - 包含所有依赖 */
#include "lib/bitmap.h"
#include "tblh.h"
#include "list.h"
#endif

/**
 * BHS (Basic Handle Struct) - Mhuixs 统一数据类型操作库
 * 
 * BHS 是 Mhuixs 的核心数据结构，可以表示：
 * - NUMBER: 任意精度数值
 * - STRING: 字符串
 * - BITMAP: 位图
 * - LIST: 列表
 * - TABLE: 表格
 * - KVALOT: 键值对
 * 
 * 函数前缀 bignum_* 保留以保持 Logex 的可读性
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


/* 获取digits指针的辅助宏 */
#define BIGNUM_DIGITS(num) ((num)->is_large ? (num)->data.large_data : (char *)(num)->data.small_data)

/* 返回值宏 */
#define BIGNUM_SUCCESS  0      /* 成功 */
#define BIGNUM_ERROR   -1      /* 错误 */
#define BIGNUM_DIV_ZERO -2     /* 除零错误 */

/**
 * 创建并初始化一个新的 BHS（堆分配）
 * 
 * @return 新创建的 BHS 指针，失败返回 NULL
 */
BHS* bignum_create(void);

/**
 * 销毁 BHS 并释放所有内存（包括结构体本身）
 * 
 * @param num BHS 指针
 */
void bignum_destroy(BHS *num);

/**
 * 初始化大数为0（用于栈分配的 BHS，不推荐使用）
 * 
 * @param num BHS 结构
 * @deprecated 请使用 bignum_create() 代替
 */
void bignum_init(BHS *num);

/**
 * 清理大数内部数据（不释放结构体本身，不推荐使用）
 * 
 * @param num BHS 结构
 * @deprecated 请使用 bignum_destroy() 代替
 */
void bignum_free(BHS *num);

/**
 * 复制大数
 * 
 * @param src 源 BHS
 * @param dst 目标 BHS
 * @return 0 成功, -1 失败
 */
int bignum_copy(const BHS *src, BHS *dst);

/**
 * 从字符串创建大数（返回新分配的 BHS）
 * 
 * @param str 数字字符串（支持小数和负数，如 "-123.456"）
 * @return 新创建的 BHS 指针，失败返回 NULL
 */
BHS* bignum_from_string(const char *str);

/**
 * 从原始字符串创建字符串类型的 BHS（返回新分配的 BHS）
 * 
 * @param str 任意字符串
 * @return 新创建的 BHS 指针，失败返回 NULL
 */
BHS* bignum_from_raw_string(const char *str);

/**
 * 从字符串创建大数（旧版本 API，已弃用）
 * 
 * @param str 数字字符串
 * @param num 输出的 BHS 结构
 * @return 0 成功, -1 失败
 * @deprecated 请使用 bignum_from_string(str) 代替
 */
int bignum_from_string_legacy(const char *str, BHS *num);

/**
 * 从原始字符串创建字符串类型的 BHS（旧版本 API，已弃用）
 * 
 * @param str 任意字符串
 * @param num 输出的 BHS 结构
 * @return 0 成功, -1 失败
 * @deprecated 请使用 bignum_from_raw_string(str) 代替
 */
int bignum_from_raw_string_legacy(const char *str, BHS *num);

/**
 * 将大数格式化为 C 字符串（用于打印输出）
 * 
 * 注意：此函数仅用于格式化输出，不改变 BHS 的类型！
 * - 如果 BHS 是数字类型，格式化为 "123.456" 形式
 * - 如果 BHS 是字符串类型，格式化为 "\"hello\"" 形式（带引号）
 * - 输出到用户提供的 C 字符串缓冲区
 * 
 * 如果需要进行类型转换（数字→字符串类型的 BHS），请使用：
 *   bignum_number_to_string_type()
 * 
 * @param num BHS 结构
 * @param str 输出字符串缓冲区（C 字符串 char*）
 * @param max_len 缓冲区最大长度
 * @param precision 小数精度（-1表示使用默认精度）
 * @return 0 成功, -1 失败
 */
int bignum_to_string(const BHS *num, char *str, size_t max_len, int precision);

/**
 * 大数加法（返回新分配的 BHS）
 * 
 * @param a 被加数
 * @param b 加数
 * @return 结果 BHS 指针，失败返回 NULL
 */
BHS* bignum_add(const BHS *a, const BHS *b);

/**
 * 大数减法（返回新分配的 BHS）
 * 
 * @param a 被减数
 * @param b 减数
 * @return 结果 BHS 指针，失败返回 NULL
 */
BHS* bignum_sub(const BHS *a, const BHS *b);

/**
 * 大数乘法（返回新分配的 BHS）
 * 
 * @param a 被乘数
 * @param b 乘数
 * @return 结果 BHS 指针，失败返回 NULL
 */
BHS* bignum_mul(const BHS *a, const BHS *b);

/**
 * 大数除法（返回新分配的 BHS）
 * 
 * @param a 被除数
 * @param b 除数
 * @param precision 除法精度（小数位数，-1表示使用默认精度）
 * @return 结果 BHS 指针，失败返回 NULL（除零返回 NULL）
 */
BHS* bignum_div(const BHS *a, const BHS *b, int precision);

/**
 * 大数幂运算（返回新分配的 BHS）
 * 
 * @param base 底数
 * @param exponent 指数（必须为非负整数）
 * @param precision 计算精度（小数位数，-1表示使用默认精度）
 * @return 结果 BHS 指针，失败返回 NULL
 */
BHS* bignum_pow(const BHS *base, const BHS *exponent, int precision);

/**
 * 大数取模运算（返回新分配的 BHS）
 * 
 * @param a 被除数
 * @param b 除数（模数）
 * @return 结果 BHS 指针，失败返回 NULL（除零返回 NULL）
 */
BHS* bignum_mod(const BHS *a, const BHS *b);

/**
 * 判断 BHS 是否为数字类型
 * 
 * @param num BHS 结构
 * @return 1 是数字, 0 是字符串
 */
int bignum_is_number(const BHS *num);

/**
 * 判断 BHS 是否为字符串类型
 * 
 * @param num BHS 结构
 * @return 1 是字符串, 0 是数字
 */
int bignum_is_string(const BHS *num);

/**
 * 判断 BHS 是否为位图类型
 * 
 * @param num BHS 结构
 * @return 1 是位图, 0 不是
 */
int bignum_is_bitmap(const BHS *num);

/**
 * 判断 BHS 是否为列表类型
 * 
 * @param num BHS 结构
 * @return 1 是列表, 0 不是
 */
int bignum_is_list(const BHS *num);

/**
 * 尝试将字符串类型的 BHS 转换为数字类型（返回新分配的 BHS）
 * 
 * @param str_num 字符串类型的 BHS
 * @return 新的数字类型 BHS 指针，失败返回 NULL
 */
BHS* bignum_string_to_number(const BHS *str_num);

/**
 * 将数字类型的 BHS 转换为字符串类型的 BHS（返回新分配的 BHS）
 * 
 * 注意：这是真正的类型转换函数！
 * - 输入：BIGNUM_TYPE_NUMBER (type=0)
 * - 输出：BIGNUM_TYPE_STRING (type=1)
 * - 内部流程：先格式化成 C 字符串，再创建字符串类型的 BHS
 * 
 * 用途：在 Logex 类型系统内进行数字→字符串的类型转换
 * 例如：package/string 中的 str() 函数就是调用这个函数实现的
 * 
 * 如果只是需要打印输出，请使用：bignum_to_string()
 * 
 * @param num 数字类型的 BHS (必须是 BIGNUM_TYPE_NUMBER)
 * @param precision 精度（-1 表示使用默认精度）
 * @return 新的字符串类型 BHS 指针，失败返回 NULL
 */
BHS* bignum_number_to_string_type(const BHS *num, int precision);

/**
 * 从二进制字符串创建位图类型的 BHS（简单包装bitmap_create_from_string）
 * 例如："10001000" -> 位图类型
 * 
 * @param str 二进制字符串（只包含 '0' 和 '1'）
 * @return 新的位图类型 BHS 指针，失败返回 NULL
 */
#define bignum_from_binary_string(str) bitmap_create_from_string(str)

/**
 * 位图运算函数 - 使用bitmap.h中的实现
 * 注意：移位运算需要将BigNum转换为uint64_t
 */
#define bignum_bitand(a, b)   bitmap_bitand(a, b)
#define bignum_bitor(a, b)    bitmap_bitor(a, b)
#define bignum_bitxor(a, b)   bitmap_bitxor(a, b)
#define bignum_bitnot(a)      bitmap_bitnot(a)

/* 移位运算需要转换接口（在bignum.c中实现） */
BHS* bignum_bitshl(const BHS *a, const BHS *shift);
BHS* bignum_bitshr(const BHS *a, const BHS *shift);

/* 类型转换函数（在type_package.c中实现，这里仅声明供内部使用） */
BHS* bignum_number_to_bitmap(const BHS *num);
BHS* bignum_bitmap_to_number(const BHS *bitmap);

/* LIST 类型相关函数 */
/**
 * 创建空的列表类型 BHS
 * 
 * @return 新的列表类型 BHS 指针，失败返回 NULL
 */
BHS* bignum_create_list(void);

/**
 * 从现有 LIST 创建列表类型 BHS
 * 
 * @param list 现有的 LIST 指针
 * @return 新的列表类型 BHS 指针，失败返回 NULL
 */
BHS* bignum_from_list(struct LIST *list);

/**
 * 获取列表类型 BHS 的底层 LIST 指针
 * 
 * @param num 列表类型的 BHS
 * @return LIST 指针，失败返回 NULL
 */
struct LIST* bignum_get_list(const BHS *num);

/**
 * 将 BHS 转换为 double（用于数值计算）
 * 
 * @param num BHS 指针
 * @return 转换后的 double 值
 */
double bignum_to_double(const BHS *num);

/* 
 * bignum_eval 已弃用 - 请使用 evaluator.h 中的 eval_to_string()
 * 该函数支持更多功能（布尔运算、比较运算等）
 */

#endif /* BIGNUM_H */

