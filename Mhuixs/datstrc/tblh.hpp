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
//数据类型位 占用1字节
#define i1	'a'		// -128~127
#define i2	'b'		// -32768~32767
#define i4	'c'		// -2147483648~2147483647
#define i8	'd'		// -9223372036854775808~9223372036854775807
#define ui1	'e'		// 0~255
#define ui2	'f'		// 0~65535
#define ui4	'g'		// 0~4294967295
#define ui8	'h'		// 0~18446744073709551615
#define f4	'i'		
#define f8	'j'	
#define s50 'k'		
#define s300 'l'
#define d211		'm'		//* 年* 月* 日 4byte 2 + 1 + 1
#define t111		'n'		// * 时 * 分 * 秒 3byte 1 + 1 + 1
#define dt211111	'p'		// * 年 * 月 * 日 * 时 * 分 * 秒 7byte 2 + 1 + 1 + 1 + 1 + 1

typedef struct tp_d211 { 
	uint16_t year;
	uint8_t month;
	uint8_t day; 
} tp_d211, DATE;
typedef struct tp_t111 { 
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
} tp_t111, TIME;
typedef struct tp_dt211111 { 
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
}tp_dt211111, DAYTIME;


typedef struct IDLE_MAP IDLE_MAP;
typedef struct L_INDEX L_INDEX;

typedef struct FIELD FIELD;
void initFIELD(FIELD* field, char* field_name, char type);

class TABLE {
private:
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

    int state;
public:
    TABLE(char* table_name, FIELD* field, uint32_t field_num);
    ~TABLE();

    int64_t add_record(const char* record);
    int8_t rmv_record(uint32_t j);
    int8_t swap_record(uint32_t j1, uint32_t j2);
    uint8_t insert_record(const char* record, uint32_t j);

    uint32_t add_field(FIELD* field); 
    int8_t rmv_field(uint32_t i);
    int8_t swap_field(uint32_t i1_, uint32_t i2_);
    int8_t insert_field(FIELD* field, uint32_t i);

    int8_t getfrom_i_j(uint32_t i, uint32_t j, void* buffer);
    uint8_t reset_table_name(char* table_name);
    void printf_table(uint32_t start_line);
    void printf_record(uint32_t line_j,uint32_t start_line);
    //下面三个有bug，但是我找不到哪里有问题
    void save_table(char* file_path);
    int8_t load_table(char* file_path);
    int join_record(TABLE* table_join);
private:
    void flash_line_index(TABLE* table);
    uint8_t copymemfrom_i_j(TABLE* table, uint32_t i, uint32_t j, void* buffer);
    uint8_t* address_of_i_j(TABLE *table,uint32_t i,uint32_t j);
};

#endif