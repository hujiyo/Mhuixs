/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
/*
这个内部头文件用于配置Tblh.h的基本参数
用户不需要包含这个头文件
*/
#include <stdint.h>
#ifndef TBLHDEF_H
#define TBLHDEF_H

#define long_record      400  //定义了多大才算是一个长记录
#define short_record_s_vacancy_rate 1.5  //短记录的留余空间占比
#define long_record_s_vacancy_rate 1.2	//长记录的留余空间占比
#define initial_ROM 200		//初始记录的总空间ROM
#define add_ROM 100			//每次扩展记录空间
#define record_add_ROM 300		//每次扩展每条记录的空间
#define name_max_size 50		//字段名 占用字节数

#define separater ','		//定义字段分隔符

#define TBL_HEAD_SIZE 100  //文件头HEAD的大小最大不超过100字节

#define short_string 50
#define long_string 300
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

typedef struct LINE_INDEX {
	uint32_t sequence;//LINE_INDEX[j].sequence中j表示(虚)顺序，sequence表示(实)顺序
	uint8_t* rcd_addr;//由于record_length相同，直接储存每一行的首地址会更快
}L_INDEX, LINE_INDEX;

typedef struct IDLE_MAP {
	uint32_t idle_size;//空位的可用大小
	uint32_t idle_offset;//空位的record偏移量
}IDLE_MAP;//“记录使用区”的“空位内存地图”

typedef struct FIELD {
	char type;
	char name[name_max_size];//字段名 注意\0结尾
}FIELD;

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

typedef struct TBL_HEAD{	
	uint32_t field_num;	
	uint32_t map_size;
	uint32_t record_length;
	uint32_t record_usage;
	uint32_t record_num;	
	uint32_t data_ROM;
	uint32_t offset_of_table_name;
	uint32_t offset_of_field;
	uint32_t offset_of_offsetofield;
	uint32_t offset_of_idle_map;
	uint32_t offset_of_line_index;
	uint32_t offset_of_data;
}TBL_HEAD;
/*
typedef struct Tblh {
	uint8_t (*make_table)(TABLE* table, char* table_name, FIELD* field, uint32_t field_num);

	uint32_t (*add_record)(TABLE* table, const char* record);
	uint8_t (*rmv_record)(TABLE* table, uint32_t j);
	uint8_t (*swap_record)(TABLE* table, uint32_t j1, uint32_t j2);
	uint8_t (*insert_record)(TABLE* table, const char* record, uint32_t j);

	uint32_t (*add_field)(TABLE* table, FIELD* field);
	uint8_t (*rmv_field)(TABLE* table, uint32_t i);
	uint8_t (*swap_field)(TABLE* table, uint32_t i1_, uint32_t i2_);
	uint8_t (*insert_field)(TABLE* table, FIELD* field, uint32_t i);

	uint8_t (*getfrom_i_j)(TABLE* table, uint32_t i, uint32_t j, void* buffer);
	uint8_t (*reset_table_name)(TABLE* table, char* table_name);
	void (*printf_table)(TABLE* table, uint32_t start_line);
	void (*printf_record)(TABLE* table, uint32_t line_j,uint32_t start_line);
	//下面三个有bug，但是我找不到哪里有问题
	void (*del_table)(TABLE* table);
	void (*save_table)(TABLE* table,char* file_path);
	uint8_t (*load_table)(TABLE* table,char* file_path);
}Tblh;
*/
#endif