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

/*
这个库还没有完成
*/
/*
为了避免由于错误操作引发的问题，
table需要增添表格恢复功能
*/

typedef struct FIELD FIELD;
typedef struct TABLE TABLE;

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