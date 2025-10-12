#ifndef OBJ_H
#define OBJ_H
//#include "Mhudef.hpp"
#include <stdlib.h>
typedef void* Obj;

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


/* 大数结构体 - 固定64字节 */
typedef struct {
    int type;                                 /* 类型标记（4字节） */
    int is_large;                             /* 是否使用大数据存储（4字节） */
    
    union {
        char small_data[BIGNUM_SMALL_SIZE];  /* 小数据内联存储（32字节） */
        char *large_data;                     /* 大数据动态分配指针（8字节） */
        //下面是兼容Mhuixs类型的字段
        //KVALOT *kvalot;//这个库暂时有点问题，也是先注释掉
        //LIST *list;
        //TABLE *table;
        //HOOK *hook; //HOOK对象相关库还没写好，这里也注释掉
        //KEY
    } data;                                   /* 32字节（联合体取最大） */    
    size_t capacity;                             /* 分配的容量（4字节） */    
    size_t length;                               /* 数字长度或字符串长度（4字节） */
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
} BigNum,basic_handle_struct,bhs;  /* 总大小：32+4+4+4+4+8=56字节，填充到64字节 */



#endif