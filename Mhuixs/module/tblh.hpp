/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.1
Email:hj18914255909@outlook.com
*/
#ifndef TBLH_HPP
#define TBLH_HPP
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <iostream>

#include <string>

#include "uintdeque.hpp"
#include "str.hpp"
#include "nlohmann/json.hpp"
using namespace std;
#define merr -1

#define tblh_debug

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

#define format_name_length 64		//字段名 占用字节数
#define long_record      400  //定义了多大才算是一个长记录
#define short_record_s_vacancy_rate 1.5  //短记录的留余空间占比
#define long_record_s_vacancy_rate 1.2	//长记录的留余空间占比
#define begin_ROM 200		//初始记录的总空间ROM
#define add_ROM 100			//每次扩展记录空间
#define record_add_ROM 300		//每次扩展每条记录的空间

#define PRIMARY_KEY '0'//主键:1.唯一性
#define FOREIGN_KEY '1'//外键
#define UNIQUE_KEY  '2'//唯一键:唯一性
#define INDEX_KEY   '3'//INDEX_KEY禁止提前声明
#define NOT_KEY		'4'//非键的普通字段

#define hub //核心函数标记

struct FIELD {
	char type;
	char key_type;
	uint8_t name[format_name_length];//字段名
	uint8_t length;//字段长度 单位byte
};

class TABLE {	
	struct IDLE_MAP {//"记录使用区"的"空位内存地图"
		uint32_t idle_size;//空位的可用大小
		uint32_t idle_offset;//空位的record偏移量
	};
	union temp_mem{
		int8_t i1;uint8_t ui1;int16_t i2;uint16_t ui2;
		int32_t i4;uint32_t ui4;int64_t i8;uint64_t ui8;
		float f4;double f8;string* str;
	};
    FIELD* p_field; //TABLE字段区地址（通过字段数确定边界！！！）//集成索引功能 删除FIELD_INDEX索引对象
    uint32_t field_num;//字段数
    UintDeque field_offset;//最新字段偏移量方案，p_p_field[i]表示第i个字段信息结构体，它的记录偏移量就是offsetofield[i],它将始终与对应的FIELD一一对应
    IDLE_MAP* idle_map;//"记录使用区"的"空位内存地图"
    uint32_t map_size;//空位的数量，这个数量实际应该不会特别大，除非你在天天删除字段但是却不增添字段，这显然不可能

    uint8_t* p_data;//TABLE数据区地
    uint32_t record_length;//记录长度（记录使用区大小+字段未使用区的大小）
    uint32_t record_usage;//记录长度（记录使用区大小：这里面包括字段删除留下的空位）
    uint32_t record_num;//记录条数（行数）	
    UintDeque line_index; // 用UintDeque替代原有的line_index数组
	//虚顺序是指对外用户看到的顺序，实顺序是记录在内存中的真实顺序

    uint32_t data_ROM;//TABLE数据区record条数总容量
	uint32_t primary_key_i;//主键序列
	#define NOPKEY 0xFFFFFFFF //表示无主键
    string table_name;//table名

	uint8_t* real_addr_of_lindex(uint32_t index);
	uint8_t* address_of_i_j(uint32_t i,uint32_t j);
	static uint8_t store_fieldata(char* input_str, uint8_t* stor_addr, char type);
	static int sizeoftype(char type);
	static int ptfsizeoftype(char type);
public:
	TABLE(str* table_name, FIELD* field, uint32_t field_num);
	~TABLE();

	hub int64_t add_record(size_t field_count, ...);
	hub int64_t add_record(initializer_list<char*> content);
	hub int8_t rmv_record(uint32_t j);
	int8_t swap_record(uint32_t j1, uint32_t j2);
	uint8_t insert_record(initializer_list<char*> contents, uint32_t j);
	
	hub uint32_t add_field(FIELD* field);
	hub int8_t rmv_field(uint32_t i);
	int8_t swap_field(uint32_t i1_, uint32_t i2_);
	int8_t insert_field(FIELD* field, uint32_t i);

	void reset_table_name(const char* table_name);
	void reset_field_name(uint32_t i,string& field_name);
	int8_t reset_field_key_type(uint32_t i,char key_type);
	void print_table(uint32_t start_line);
	void print_record(uint32_t line_j,uint32_t start_line);

	static int8_t isvalid_type(char type);
	static int8_t isvalid_keytype(char type);
	static void initFIELD(FIELD* field,const char* field_name, char type,char key_type);
	static void gotoxy(uint32_t x, uint32_t y);

	void debug_ram_inf_print(int y);

	int iserr();
	int state;

	TABLE(const TABLE& other);
	TABLE& operator=(const TABLE& other);

	nlohmann::json get_all_info() const;
};

/*
i,j ---> 表示对外的虚序列
x,y ---> 表示对内的实际序列
*/
/*
INDEX
1.B数索引
2.哈希索引
3.全文索引
4.空间索引
5.位图索引
*/

#endif