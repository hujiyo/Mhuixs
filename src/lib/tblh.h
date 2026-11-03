#ifndef TBLH_H
#define TBLH_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "mstring.h"  /* 提供 mstring 类型定义 */
#include "bignum.h"  /* 提供 BHS/Obj 类型定义 */

/*
tblh的思路
将数据的具体类型、内容与数据关系剥离
tblh只负责管理数据关系，不关心数据的具体类型、内容
*/

//代码规范：
//1.禁止小段内存余量申请，用多少申请多少。大段内存除外
//2.结构体、变量命名小写，中间加下划线，单词不宜超过4个
//3.宏命名大写
//4.严格遵守类似rust的所有权思想，指针传给谁就相当于送谁了
//5.禁止使用strlen这类不安全的函数
#define INCREASE_LINES_NUM 100//每次增加的行数
#define FIELD_NOT_FOUND SIZE_MAX//字段未找到的错误码

typedef struct {
    size_t column_index;//字段在表中的索引(从0开始)
    Obj* data;//数据区
    mstring name;//字段名
    int type;//字段类型(由外部设置、定义)
}FIELD;

/*
单列有序索引：按该列的值升序排好的逻辑行号。

只存行号（每行 8 字节），比较时回表取值，不复制单元格。
索引是**显式创建**的：任何写操作把它置为 stale，查询遇到 stale 直接报错，
由调用方重新建索引 —— 不自动重建，行为可预期。

为什么存逻辑行号而不是物理行号：逻辑行是调用方看到的行号；rm_record
会搬动物理行，但不改变"逻辑第 r 行"的含义，所以只要表没被写过，
逻辑行号一直是稳定的。
*/
typedef struct {
    size_t  field;   //列下标
    size_t* order;   //order[i] = 第 i 小的行的逻辑行号
    size_t  n;       //建索引时的行数
    int     stale;   //建好之后表被写过
}TABLE_INDEX;

typedef struct TABLE {
    mstring name;//表名
    FIELD* field;//字段区
    size_t field_num;//字段数
    size_t line_num;//当前实际数据行数
    size_t capacity;//已分配的内存容量(行数)
    size_t* logic_index;//索引数组，logic_index[逻辑行号]=物理行号
    size_t* memory_index;//反向索引，memory_index[物理行号]=逻辑行号

    TABLE_INDEX* idx;//已创建的列索引
    size_t idx_num;
    size_t idx_cap;
}TABLE;

//函数声明（对外接口使用 BHS*）
TABLE* create_table(int* types, mstring* field_names, size_t field_num, mstring table_name);
TABLE* table_copy(const TABLE* other);
int add_record(TABLE* table, Obj* values, size_t num);
int rm_record(TABLE* table, size_t logic_index);
int rm_field(TABLE* table, size_t field_index);
int add_field(TABLE* table, int type, mstring field_name);
int swap_record(TABLE* table, size_t logic_index1, size_t logic_index2);
int swap_field(TABLE* table, size_t field_index1, size_t field_index2);
Obj get_value(TABLE* table, size_t idx_x, size_t idx_y);
int set_value(TABLE* table, size_t idx_x, size_t idx_y, Obj content);
size_t get_field_index(TABLE* table, char* field_name, size_t len);
void free_table(TABLE* table);
void clear_table(TABLE* table);
size_t get_record_count(TABLE* table);
size_t get_field_count(TABLE* table);
int update_record(TABLE* table, size_t logic_index, Obj* values, size_t num);
Obj* get_record(TABLE* table, size_t logic_index);

/*
 * 列索引。建索引失败返回 -1（内存不足 / 列下标非法）；
 * 重复建同一列会重建并清掉 stale 标记。
 */
int table_index_build(TABLE* table, size_t field);
int table_index_drop(TABLE* table, size_t field);
void table_index_invalidate(TABLE* table);

/* 查某个列上的索引；没有返回 NULL。返回的指针随表结构变化失效。 */
TABLE_INDEX* table_index_get(TABLE* table, size_t field);

/* 遍历已有索引（tindexes 命令用） */
size_t table_index_count(const TABLE* table);
TABLE_INDEX* table_index_at(TABLE* table, size_t i);

#endif // TBLH_H
