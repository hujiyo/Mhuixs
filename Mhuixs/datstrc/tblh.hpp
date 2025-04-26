/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.1
Email:hj18914255909@outlook.com
*/
#ifndef TBLH_HPP
#define TBLH_HPP

#include <string>

#include <stdint.h>
#include "time.hpp"
using namespace std;

/*
这个库还没有完成
*/
/*
为了避免由于错误操作引发的问题，
table需要增添表格恢复功能
*/
//数据类型位 占用1字节
#define I1	'a'		// -128~127
#define I2	'b'		// -32768~32767
#define I4	'c'		// -2147483648~2147483647
#define I8	'd'		// -9223372036854775808~9223372036854775807
#define UI1	'e'		// 0~255
#define UI2	'f'		// 0~65535
#define UI4	'g'		// 0~4294967295
#define UI8	'h'		// 0~18446744073709551615
#define F4	'i'		
#define F8	'j'	
#define STR 'k'		
#define DATE		'l'		//* 年* 月* 日 4byte 2 + 1 + 1
#define TIME		'm'		// * 时 * 分 * 秒 3byte 1 + 1 + 1

#define format_name_length 50		//字段名 占用字节数

void initFIELD(FIELD* field, char* field_name, char type);

typedef struct FIELD {
	char type;
	char name[format_name_length];//字段名 注意\0结尾
}FIELD;

class TABLE {	
	struct IDLE_MAP {//“记录使用区”的“空位内存地图”
		uint32_t idle_size;//空位的可用大小
		uint32_t idle_offset;//空位的record偏移量
	};
    FIELD* p_field; //TABLE字段区地址（通过字段数确定边界！！！）//集成索引功能 删除FIELD_INDEX索引对象
    uint32_t field_num;//字段数
    uint32_t* offsetofield;//最新字段偏移量方案，p_p_field[i]表示第i个字段信息结构体，它的记录偏移量就是offsetofield[i],它将始终与对应的FIELD一一对应
    IDLE_MAP* idle_map;//“记录使用区”的“空位内存地图”
    uint32_t map_size;//空位的数量，这个数量实际应该不会特别大，除非你在天天删除字段但是却不增添字段，这显然不可能

    uint8_t* p_data;//TABLE数据区地
    uint32_t record_length;//记录长度（记录使用区大小+字段未使用区的大小）
    uint32_t record_usage;//记录长度（记录使用区大小：这里面包括字段删除留下的空位）
    uint32_t record_num;//记录条数（行数）	
    uint32_t* line_index;//行索引,LINE_INDEX[j]中j表示(虚)顺序，"LINE_INDEX[j]"这个值表示(实)顺序
	//虚顺序是指对外用户看到的顺序，实顺序是记录在内存中的真实顺序

    uint32_t data_ROM;//TABLE数据区record条数总容量
    string table_name;//table名

	int state;
	uint8_t* real_addr_of_lindex(uint32_t index);
	uint8_t copymemfrom_i_j(TABLE* table, uint32_t i, uint32_t j, void* buffer);
	uint8_t* address_of_i_j(TABLE *table,uint32_t i,uint32_t j);
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
	void reset_table_name(char* table_name);
	void print_table(uint32_t start_line);
	void print_record(uint32_t line_j,uint32_t start_line);	
};

#endif