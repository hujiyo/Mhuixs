#ifndef TBLH_H
#define TBLH_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../lib/mstring.h"

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

typedef void* Obj;

typedef struct FIELD FIELD;
struct FIELD {
    size_t column_index;//字段在表中的索引(从0开始)
    Obj* data;//数据区
    mstring name;//字段名
    int type;//字段类型(由外部设置、定义)
};

typedef struct TABLE TABLE;
struct TABLE {
    mstring name;//表名
    FIELD* field;//字段区
    size_t field_num;//字段数
    size_t line_num;//当前实际数据行数
    size_t capacity;//已分配的内存容量(行数)
    size_t* logic_index;//索引数组，logic_index[逻辑行号]=物理行号
    size_t* memory_index;//反向索引，memory_index[物理行号]=逻辑行号
};

//函数声明
TABLE* create_table(int* types, mstring* field_names, size_t field_num, mstring table_name);
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

#endif // TBLH_H
