/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.1
Email:hj18914255909@outlook.com
*/
#ifndef TBLH_H
#define TBLH_H

#include <stdint.h>
#include "tblhdef.h"

/*
这个库还没有完成
*/
/*
为了避免由于错误操作引发的问题，
table需要增添表格恢复功能
*/

typedef struct FIELD {
    char type;
    char name[name_max_size];
} FIELD;

typedef struct IDLE_MAP {
    uint32_t idle_offset;
    uint32_t idle_size;
} IDLE_MAP;

typedef struct L_INDEX {
    uint32_t sequence;
    uint8_t* rcd_addr;
} L_INDEX;

typedef struct TABLE {
    FIELD* p_field; //TABLE字段区地址（通过字段数确定边界！！！）//集成索引功能 删除FIELD_INDEX索引对象
    uint32_t field_num;//字段数
    uint32_t* offsetofield;//最新字段偏移量方案，p_p_field[i]表示第i个字段信息结构体，它的记录偏移量就是offsetofield[i],它将始终与对应的FIELD一一对应
    IDLE_MAP* idle_map;//“记录使用区”的“空位内存地图”
    uint32_t map_size;//空位的数量，这个数量实际应该不会特别大，除非你在天天删除字段但是却不增添字段，这显然不可能

    uint8_t* p_data;//TABLE数据区地
    uint32_t record_length;//记录长度（记录使用区大小+字段未使用区的大小）
    uint32_t record_usage;//记录长度（记录使用区大小：这里面包括字段删除留下的空位）
    uint32_t record_num;//记录条数（行数）	
    L_INDEX* line_index;//行索引

    uint32_t data_ROM;//TABLE数据区record条数总容量
    char* table_name;//table名
} TABLE;

TABLE*      makeTABLE(char* table_name, FIELD* field, uint32_t field_num);
uint8_t     tblh_make_table     (TABLE* table, char* table_name, FIELD* field, uint32_t field_num);
uint8_t     tblh_add_record     (TABLE* table, const char* record);
uint8_t     tblh_rmv_record     (TABLE* table, uint32_t j);
uint8_t     tblh_swap_record    (TABLE* table, uint32_t j1, uint32_t j2);
uint8_t     tblh_insert_record  (TABLE* table, const char* record, uint32_t j);
uint8_t     tblh_rmv_field      (TABLE* table, uint32_t i);
uint32_t    tblh_add_field      (TABLE* table, FIELD* field);
uint8_t     tblh_swap_field     (TABLE* table, uint32_t i1_, uint32_t i2_);
uint8_t     tblh_insert_field   (TABLE* table, FIELD* field, uint32_t i);
uint8_t     tblh_getfrom_i_j    (TABLE* table, uint32_t i, uint32_t j, void* buffer);
void        tblh_printf_record  (TABLE* table, uint32_t j, uint32_t y);
void        tblh_printf_table   (TABLE* table, uint32_t start_line);
uint8_t     tblh_reset_table_name(TABLE* table, char* table_name);
void        tblh_del_table      (TABLE* table);
void        tblh_save_table     (TABLE* table, char* file_path);
uint8_t     tblh_load_table     (TABLE* table, char* file_path);
int         tblh_join_record    (TABLE* table, TABLE* table_join);

#endif