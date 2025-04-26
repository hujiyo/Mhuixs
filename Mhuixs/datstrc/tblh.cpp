#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <string>
using namespace std;
#include "time.hpp"

#define merr -1
#define TENTATIVE 0
#define _max_(a,b) ((a)>(b)?(a):(b))

#define long_record      400  //定义了多大才算是一个长记录
#define short_record_s_vacancy_rate 1.5  //短记录的留余空间占比
#define long_record_s_vacancy_rate 1.2	//长记录的留余空间占比
#define begin_ROM 200		//初始记录的总空间ROM
#define add_ROM 100			//每次扩展记录空间
#define record_add_ROM 300		//每次扩展每条记录的空间
#define format_name_length 50		//字段名 占用字节数

#define separater ','		//定义字段分隔符

#define short_string 50
#define long_string 300
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
uint8_t* TABLE::real_addr_of_lindex(uint32_t index){//index表示虚序列
	return p_data + line_index[index] * record_length;
}
static void cpstr(uint8_t* start, uint8_t* end, uint8_t* target){
	for (int i = 1; i < (end - start); *target = *(start + i), target++, i++);//复制start和end "中间" 的信息到target中
}
static void gotoxy(uint32_t x, uint32_t y){
	printf("\033[%d;%dH", ++y, ++x);//原点为（0，0）
}
static int sizeoftype(char type) { //返回某类型在p_data池中的占用的字节数	
	switch (type){
		case I1:case UI1:return 1;
		case I2:case UI2:return 2;
		case I4:case UI4:case F4:case STR:case DATE:case TIME:return 4;
		case I8:case UI8:case F8:return 8;
		default: return merr;
	}
}
static int ptfsizeoftype(char type){//根据数据类型返回打印时占用的字节数
	switch (type){
		case I1:return 4 + 1;//-128占用4个位数，加一表示至少1个空格
		case I2:return 6 + 1;//-32768占用6个位数
		case I4:return 11 + 1;//-2147483648占用11个位数
		case I8:return 20 + 1;//-9223372036854775808占用20个位数
		case UI1:return 3 + 1;//255占用3个位数
		case UI2:return 5 + 1;//65535占用5个位数
		case UI4:return 10 + 1;//4294967295占用10个位数
		case UI8:return 20 + 1;//18446744073709551615占用20个位数
		case F4:return 7 + 1;//总位数不超过7位
		case F8:return 15 + 1;//总位数不超过15位
		case STR:return 10 + 1;//打印前10个字符
		case DATE:return 10 + 1;//2024.10.10
		case TIME:return 8 + 1;//11:11:11
		default: return merr;
	}
}
static uint8_t store_fieldata(char* p_inputstr, uint8_t* p_storaddr, char type){
	/*
	tblh_add_record函数的底层储存函数
	把字符串类型p_inputdata按照type类型进行自动储存
	使用put_tobyte处理C语言标准数据的储存
	p_inputdata必须以\0结尾
	put_tobyte存储C语言标准数据类型
	storage_field_data存储SQlh标准数据类型
	*/
	switch (type){
		case I1:
			*p_storaddr = atoi((const char*)p_inputstr);//这里可以考虑使用更快速的函数atoi
			return 0;
		case I2: {
			int16_t i = atoi((const char*)p_inputstr);//在后期调试时，这里可以尝试改成atoi
			memcpy(p_storaddr,&i,sizeoftype(I2));
			return 0;
		}
		case I4:case DATE:case TIME: {
			int32_t i = atoi((const char*)p_inputstr);
			memcpy(p_storaddr,&i,sizeoftype(type));
			return 0;
		}		   
		case I8: {
			int64_t i = strtoll((const char*)p_inputstr, NULL, 10);//长字节使用strtoll
			memcpy(p_storaddr,&i,sizeoftype(I8));
			return 0;
		}			
		case UI1:
			*p_storaddr = atoi((const char*)p_inputstr);
			return 0;
		case UI2: {
			uint16_t i = atoi((const char*)p_inputstr);
			memcpy(p_storaddr,&i,sizeoftype(UI2));
			return 0;
		}				
		case UI4: {
			uint32_t i = strtoul((const char*)p_inputstr, NULL, 10);
			memcpy(p_storaddr,&i,sizeoftype(UI4));
			return 0;
		}				
		case UI8: {
			uint64_t i = strtoull((const char*)p_inputstr, NULL, 10);
			memcpy(p_storaddr,&i,sizeoftype(UI8));
			return 0;
		}				
		case F4: {
			float i = strtof((const char*)p_inputstr, NULL);
			memcpy(p_storaddr,&i,sizeoftype(F4));
			return 0;
		}			
		case F8: {
			double i = strtod((const char*)p_inputstr, NULL);
			memcpy(p_storaddr,&i,sizeoftype(F8));
			return 0;
		}
		case STR:{
			string* s = new string;
			s->assign(p_inputstr);
			memcpy(p_storaddr,s,sizeoftype(STR));
			return 0;
		}
	}
	return 1;
}
TABLE::TABLE(char* table_name, FIELD* field, uint32_t field_num):
p_field(NULL),field_num(field_num),offsetofield(NULL),idle_map(NULL),
map_size(0),/*map_size是真实大小，不是数组中括号中的最大值*/p_data(NULL),
record_length(TENTATIVE),record_usage(TENTATIVE),record_num(0),line_index(NULL),
data_ROM(begin_ROM),/*/TABLE数据区record条数初始容量为200条数*/table_name(NULL),
state(0){
	//初始化table基础信息
	uint32_t record_usage = 0;
	for (uint32_t i = 0; i < field_num; record_usage += sizeoftype(field[i].type), i++);
	this->record_usage = record_usage;
	this->record_length = 
	record_usage * (	//根据现有字段的占用长度合理的选择留白率
		(record_usage < long_record) ? short_record_s_vacancy_rate : long_record_s_vacancy_rate
	);
	// 创建p_head区,并创建对应的偏移量索引
	this->p_field = (FIELD*)calloc(field_num , sizeof(FIELD));//字段区域
	this->offsetofield = (uint32_t*)malloc(field_num * sizeof(uint32_t));//字段偏移量索引
	this->idle_map = (IDLE_MAP*)malloc(sizeof(IDLE_MAP));//记录使用区的空位内存地图
	this->p_data = (uint8_t*)calloc(1,this->record_length * begin_ROM);//创建TABLE数据区
	this->line_index = (uint32_t*)malloc(sizeof(uint32_t));//先建立一个line_index	
	if(!this->p_field+!this->offsetofield+!this->idle_map+!this->p_data+!this->line_index){
		free(this->p_field);free(this->offsetofield);
		free(this->idle_map);free(this->p_data);
		free(this->line_index);
		#ifdef tblh_debug
		printf("TABLE init err:calloc error\n");
		#endif
		state++;
		return;
	}
	//将字段的偏移量存入offsetofield数组中
	uint32_t ofsum = 0;	
	for (uint32_t i = 0; i < field_num;this->p_field[i] = field[i],this->offsetofield[i] = ofsum, 
		ofsum += sizeoftype(field[i].type),	i++	);
	
	this->table_name.assign(table_name);
	return;
}
int64_t TABLE::add_record(const char* record)//add总是在末尾追加
{
	//如果record为空，则增加一条空记录
	int isnullrecord = 0;
	if (record == NULL) isnullrecord = 1;
	/*
	tblh_add_record把新记录加到p_data的末尾（不用担心内存中存在空位 ：tblh_rmv_record将会把最后一个数据填补到删除的记录处）
	、line_index的索引组织，未来可能还要处理分页管理
	返回 虚序列号
	record的格式："字段1,字段2,字段3..."
	*/
	if (this->record_num == this->data_ROM) {
		//记录数容量满了:扩展
		//不用担心有空位没有利用，记录条数永远等于实际记录条数
		uint8_t* new_p_data = (uint8_t*)realloc(this->p_data, this->record_length * (this->data_ROM + add_ROM));
		if (new_p_data == NULL)	return merr;
		this->p_data=new_p_data;
		this->data_ROM += add_ROM;
	}
	/*
	定位本条待写入记录的首地址即p_data的末尾
	*/
	uint8_t* new_record_address = this->p_data + this->record_length * this->record_num;//更新record_num后定位地址
	this->record_num++;//记录数更新

	//重新创建line_index索引
	this->line_index = (uint32_t*)realloc(this->line_index, this->record_num * sizeof(uint32_t));
	if (this->line_index == NULL){
		free(this->p_data);
		return merr;
	}
	this->line_index[this->record_num - 1] = this->record_num - 1;//虚顺序也是表格的最后一个//序号都是从0开始，所以这里-1
	//this->line_index[this->record_num - 1].rcd_addr = new_record_address;//记录记录首地址
	//写入记录
	memset(new_record_address, 0, this->record_length);//注意！写入操作前先归0！！！！

	if (isnullrecord) return this->record_num - 1;//如果是空记录，则不写入数据	
	
	char* pointer = (char*)record;//初始化 输入字符串 的定位指针
	uint32_t field_num = this->field_num;//初始化 字段数量
	for (uint32_t i = 0; i < field_num; i++){
		if (i) pointer = strchr(pointer, separater) + 1;//如果不是第一个字段，则每次定位到,后都进一位
		if (pointer == (char*)1) return 0;//如果已经到record的结尾'\0'		

		uint8_t* start=(uint8_t*)(pointer - 1);
		uint8_t* end=NULL;
		if (i != field_num - 1) end=(uint8_t*)strchr(pointer, separater);
		else end=(uint8_t*)strchr(pointer, '\0');
		uint32_t len=end-start;
		char* result = (char*)malloc(len);
		memset(result, 0, len);//保证以\0结尾
		cpstr(start,end,(uint8_t*)result);

		store_fieldata(result,new_record_address + this->offsetofield[i], this->p_field[i].type);
	}
	return this->record_num - 1;
}
int8_t TABLE::rmv_record(uint32_t j){//删除虚序号及其对应的record
	if (j >= this->record_num)return merr;//j记录不存在
	/*
	本函数实际上处理的是末记录和删除记录的关系
	用最后一条实记录去补删除的记录留下的空位
	保证记录的连续性（记录条数永远等于实际记录条数）
	*/
	memset(real_addr_of_lindex(j), 0, this->record_length);
	//判断本j记录是否是末记录（实记录）
	this->record_num--;
	if (this->line_index[j] != this->record_num){
		//不是末记录就去找到sequence为record_num的记录（末记录）
		uint32_t j_mo = 0;
		for (; j_mo < this->record_num+1; j_mo++) {
			if (this->line_index[j_mo] == this->record_num) break;//找到末记录对应的虚序列
		}
		//把末记录复制到j处
		cpstr(real_addr_of_lindex(j_mo) - 1,real_addr_of_lindex(j_mo) + this->record_length,real_addr_of_lindex(j));
		memset(real_addr_of_lindex(j_mo), 0, this->record_length);//原来的末记录归0//想了很久还是增加这一步吧，省点力气
		//修改末记录对应的索引指向j地址
		this->line_index[j_mo] = this->line_index[j];
	}
	for (uint32_t i = j; i < this->record_num; this->line_index[i] = this->line_index[i + 1], i++);//索引进位
	return 0;
}
int8_t TABLE::swap_record(uint32_t j1, uint32_t j2)//函数用于交换两条记录的排序 返回0表示成功
{
	//tblh的实现方法是虚序列号对应的索引的交换
	if (j1 >= this->record_num || j2 >= this->record_num)return merr;//验证j1、j2是否存（j1，j2一定是正数）

	//交换j1和j2的索引
	uint32_t cache = this->line_index[j1];
	this->line_index[j1] = this->line_index[j2];
	this->line_index[j2] = cache;
	return 0;
}
uint8_t TABLE::insert_record(const char* record, uint32_t j)//向j处插入记录，原来及后面的记录全部退一位
{
	uint32_t sep = this->add_record(record);//这个地方调用了外部函数，不知道会不会影响性能
	if (j >= sep) return 0;//已经增加了一条记录，如果现在j比最后一条记录都大或等，那就直接return了
	
	uint32_t cache = this->line_index[sep];//先调用tblh_add_record并保存其索引
	for (uint32_t k = sep; k > j; this->line_index[k] = this->line_index[k - 1], k--);
	this->line_index[j] = cache;

	return 0;//执行完返回0
}
int8_t TABLE::rmv_field(uint32_t i){
	if (i >= this->field_num) return merr;//判断i是否合法
	/*
	由于字段的长度不一，再者对整列数据进行移动填补效率实在太低了
	所以空位就留下来，它所造成的内存浪费怎么解决呢？由以下机制来弥补：
	1.TABLE对象增加一个“记录使用区”的“空位内存地图”（idle_map）（注意：使用记录使用区的内存并不会改变record_usage）
	当下次增加字段的时候会优先查询idle_map中是否有适合大小的空位
	  只要新插入数据小于等于空位，就会占用空位，使空位变少
	2.使用tblh_del_idle(...)整理数据库字段，这个函数将删除所有空位并整理字段信息
	3.如果是删除最后一个字段，则不会增加map，而是直接修改record_usage
	*/
	//首先判断删除的是否是最后一个字段
	if (i != this->field_num - 1) {
		//增加this中的idle_map的大小
		this->idle_map = (IDLE_MAP*)realloc(this->idle_map, ++this->map_size * sizeof(IDLE_MAP));//map_size++;
		if(this->idle_map == NULL) return merr;
		this->idle_map[this->map_size - 1].idle_size = sizeoftype(this->p_field[i].type);//获得删除字段的大小
		this->idle_map[this->map_size - 1].idle_offset = this->offsetofield[i];//得到空闲位置的偏移量
	}
	else this->record_usage -= sizeoftype(this->p_field[i].type);//直接修改record_usage

	//清空删除字段对应的数据区中的数据以保证record_usage内部的空位绝对是归0的，对于record_length-record_usage中的区域，我称之为分配的到的未定义区域
	
	uint32_t cc_table_record_length = this->record_length;//缓存变量cache
	uint8_t* cc_table_p_a_i = this->p_data + this->offsetofield[i];
	uint32_t cc_clean_size = this->idle_map[this->map_size - 1].idle_size;
	for (uint32_t j = 0; j < this->record_num; j++) {
		memset(cc_table_p_a_i + cc_table_record_length * j, 0, cc_clean_size);
	}
	this->field_num--;//字段数-1
	if (i != this->field_num){
		//对field和offsetof_field中删除的字段后的字段进行进位
		for (uint32_t ii = i; ii < this->field_num;
		this->p_field[ii] = this->p_field[ii + 1],this->offsetofield[ii] = this->offsetofield[ii + 1],ii++);//索引进位
	}
	return 0;
}
uint32_t TABLE::add_field(FIELD* field){
	/*
	tblh_add_field先尝试遍历一遍idle_map,如果存在一个足够大小的空位，就把它写入空位，并重新修改idle_map
	把新字段对应的数据区映射到记录的末尾、field_index的索引组织
	返回 虚序列号
	*/
	// 扩展字段数组区
	FIELD* p_field_new = (FIELD*)realloc(this->p_field, (this->field_num+1) * sizeof(FIELD));
	if(!p_field_new) return merr;
	this->p_field[this->field_num++] = field[0];
	this->offsetofield = (uint32_t*)realloc(this->offsetofield, this->field_num * sizeof(uint32_t));
	//接下在p_data中为这个字段找合适的储存位置，先在idle_map里找
	uint32_t new_field_size = sizeoftype(field->type);
	uint32_t new_offset = this->record_usage;//默认偏移量
	uint32_t cc_map_size = this->map_size;
	IDLE_MAP* cc_idle_map = this->idle_map;
	//循环遍历找到合适的空位算法
	uint32_t idle_seq = 0;//记录之后填入的空位的序号
	for (uint32_t k = 0; k < cc_map_size; k++) {
		if (cc_idle_map[k].idle_size == new_field_size)	{
			new_offset = cc_idle_map[k].idle_offset;
			idle_seq = k;//记录填入的空位序号
			break;//如果有相等的空位，那不用多想，就这个了
		}
		if (cc_idle_map[k].idle_size > new_field_size && new_offset > cc_idle_map[k].idle_offset) {
			new_offset = cc_idle_map[k].idle_offset;
			idle_seq = k;
		}//如果有够装的空间，则以偏移量小的优先
	}
	this->offsetofield[this->field_num - 1] = new_offset;//得到偏移量了
	
	if (new_offset == this->record_usage){//接下来判断new_offset是否为record_usage外，如果是，还要判断record_length够不够
		if (this->record_length - this->record_usage <= new_field_size){//如果剩余空间不够，增加分配p_data的空间
			uint32_t old_record_length = this->record_length;//缓存旧的record_length
			this->record_length += record_add_ROM;//计算新的record_length
			uint8_t* new_p_data = (uint8_t*)realloc(this->p_data, this->record_length * this->data_ROM);//重新分配p_data
			if(new_p_data==NULL){
				this->field_num--;
				printf("p_data realloc error");
				return merr;
			}
			this->p_data=new_p_data;
			//接下来开始进行所有数据的位移
			if (this->record_num !=0){
				/*
				从末记录开始,每条记录向后移动record_add_ROM*k,注意record_usage之后的空间都是空闲的(此时this->record_usage仍未修改),
				保证移动过程中每条record_usage(包括idle)完全相同,对新字段空间的分配以及后续的归0.第一条记录无需移动所以执行到k=1就停止循环
				*/
				for (uint32_t k = this->record_num - 1; k > 0; k--) {					
					memmove(this->p_data + this->record_length * k, this->p_data + old_record_length * k, this->record_usage);					
				}
			}
		}
		this->record_usage += new_field_size;
		//清空新字段对应的数据区中的数据
		//使用空间record_usage已经增加,接下来进行归0操作,抄一下上面的代码。
		uint32_t cc_table_record_length = this->record_length;
		uint8_t* cc_table_p_a_i = this->p_data + this->offsetofield[this->field_num - 1];
		uint32_t cc_clean_size = new_field_size;
		for (uint32_t j = 0; j < this->record_num; j++)	{
			memset(cc_table_p_a_i + cc_table_record_length * j, 0, cc_clean_size);
		}
	}
	else {//如果new_offset在record_usage内部，则涉及idle_map的功能了
		this->idle_map[idle_seq].idle_size -= new_field_size;//之前的所有代码已经保证record_usage内部的所有空位都已经清空了，这里不需要再清空一遍
		
		if (this->idle_map[idle_seq].idle_size==0) //空位正好被占满，那么就直接用最后一个idle_map替换
		this->idle_map[idle_seq] = this->idle_map[--this->map_size];
		else 	//如果idle_map还有空位，那么修改offset就好了
		this->idle_map[idle_seq].idle_offset += new_field_size;
	}
	return this->field_num - 1;//返回虚序列号，序列号默认是最后一个
}

int8_t TABLE::swap_field(uint32_t i1_, uint32_t i2_){
	//tblh的独特设计使得字段交换成为非常简单的操作
	if (i1_ >= this->field_num || i2_ >= this->field_num) return merr;//验证11、12是否存

	//交换i1和i2的索引
	FIELD cache = this->p_field[i1_];
	this->p_field[i1_] = this->p_field[i2_];
	this->p_field[i2_] = cache;
	uint32_t cache_offset = this->offsetofield[i1_];
	this->offsetofield[i1_] = this->offsetofield[i2_];
	this->offsetofield[i2_] = cache_offset;
	return 0;
}
int8_t TABLE::insert_field(FIELD* field, uint32_t i){
	uint32_t sep = this->add_field(field);
	if (i >= sep) return merr;//如果想插入的位置i比末尾的序号还大或等，那就不叫插入，而是叫附加

	FIELD cache = this->p_field[sep];
	uint32_t cache_offset = this->offsetofield[sep];
	FIELD* cc_p_head = this->p_field;
	uint32_t* cc_offsetof_field = this->offsetofield;
	for (uint32_t k = sep; k > i; cc_p_head[k] = cc_p_head[k - 1], cc_offsetof_field[k] = cc_offsetof_field[k - 1], k--);
	this->p_field[i] = cache;
	this->offsetofield[i] = cache_offset;
	return 0;
}
int8_t TABLE::getfrom_i_j(uint32_t i, uint32_t j, void* buffer){
	/*
	从（i，j）虚拟位置获得数据,请保证buffer缓冲区足够容纳数据
	*/
	uint8_t* inf_addr = this->p_data + j * this->record_length + this->offsetofield[i];//使用公式找到（i,j）对应数据的指针地址
	//根据type类型进行格式化输出
	switch (this->p_field[i].type){ //获得第i个字段的类型信息，然后根据数据类型分情况处理数据
		case I1:case I2:case I4:case I8:			
		case UI1:case UI2:case UI4:case UI8:			
		case F4:case F8:case DATE:case TIME:
			memcpy(buffer, inf_addr, sizeoftype(this->p_field[i].type));
			return 0;
		case STR:
			string* p=(string*)inf_addr;
			memcpy(buffer,p->c_str(),p->length()); 
			return 0;
	}
	return merr;
}
uint8_t TABLE::copymemfrom_i_j(TABLE* table, uint32_t i, uint32_t j, void* buffer){
	/*
	从（i，j）虚拟位置获得数据
	内置函数，和tblh_getfrom_i_j相比，本函数直接在TABLE所管辖的数据内存区域直接复制到buffer中
	本函数不会根据数据类型自动返回相应的格式化数据
	*/
	//先获得第i个字段的类型、偏移量的信息
	char type = table->p_field[i].type;
	//找到（i,j）的地址
	uint8_t* inf_addr = table->p_data + j * table->record_length + table->offsetofield[i];
	memcpy(buffer,inf_addr,sizeoftype(type));

	return 0;
}
uint8_t* TABLE::address_of_i_j(TABLE *table,uint32_t i,uint32_t j){
	return table->p_data + j * table->record_length + table->offsetofield[i];
}
void TABLE::print_record(uint32_t j, uint32_t y){
	//初始化ptfmap得到打印地图
	uint32_t* ptfmap = (uint32_t*)malloc(sizeof(uint32_t) * this->field_num);
	uint32_t address = 0;
	for (uint32_t i = 0; i < this->field_num; i++) {
		ptfmap[i] = address;
		address += _max_(ptfsizeoftype(this->p_field[i].type), strlen(this->p_field[i].name) + 1);
	}
	for (uint32_t i = 0; i < this->field_num; i++) {
			gotoxy(ptfmap[i], y);//定位到打印位置			
			uint8_t* inf_addr = real_addr_of_lindex(j)+this->offsetofield[i];//确定数据位置
			switch (this->p_field[i].type) {
				case I1:
					printf("%d", *(int8_t*)inf_addr); 
					break;
				case I2: {
					int16_t tp; 
					memcpy(&tp, inf_addr, sizeoftype(I2));
					printf("%d", tp); 
					break;
				}
				case I4: {
					int32_t tp;
					memcpy(&tp, inf_addr,4);
					if(this->p_field[i].type==I4)printf("%d", tp);
					else if(this->p_field[i].type==DATE)printf("%d.%d.%d", ((Date)tp).year(), ((Date)tp).month(), ((Date)tp).day());
					else if(this->p_field[i].type==TIME)printf("%d:%d:%d", ((Time)tp).hour(), ((Time)tp).minute(), ((Time)tp).second()); 
					break;
				}
				case I8: {
					int64_t tp; 
					memcpy(&tp, inf_addr, sizeoftype(I8));
					printf("%lld" /*PRIx64*/, tp); 
					break;//"lld"报错，不知道为什么
				}
				case UI1:
					printf("%d", *(uint8_t*)inf_addr); 
					break;
				case UI2: {
					uint16_t tp; 
					memcpy(&tp, inf_addr, sizeoftype(UI2));
					printf("%d", tp);
					break;
				}
				case UI4: {
					uint32_t tp; 
					memcpy(&tp, inf_addr, sizeoftype(UI4));
					printf("%d", tp); 
					break;
				}
				case UI8: {
					uint64_t tp; 
					memcpy(&tp, inf_addr, sizeoftype(UI8));
					printf("%llu"/* PRIu64*/, tp); 
					break;
				}
				case F4: {
					float tp; 
					memcpy(&tp, inf_addr, sizeoftype(F4));
					printf("%.7g", tp); 
					break;
				}
				case F8: {
					double tp; 
					memcpy(&tp, inf_addr, sizeoftype(F8));
					printf("%.15g", tp); 
					break;
				}
				case STR:
					void* str_p;
					memcpy(str_p,inf_addr,sizeoftype(STR));
					printf("%.10s",((string*)str_p)->c_str()); 
					break;
			}
		}
	free(ptfmap);
}
void TABLE::print_table(uint32_t y){
	//初始化ptfmap得到打印地图
	uint32_t* ptfmap = (uint32_t*)malloc(sizeof(uint32_t) * this->field_num);
	if(ptfmap==NULL){
		printf("Err:failed to malloc ptfmap\n");
		return;
	}
	uint32_t address = 0;
	for (uint32_t i = 0; i < this->field_num; i++) {
		ptfmap[i] = address;
		address += _max_(ptfsizeoftype(this->p_field[i].type), strlen(this->p_field[i].name) + 1);
	}
	for (uint32_t i = 0; i < this->field_num;i++){
		gotoxy(ptfmap[i], y);
		printf("%s", this->p_field[i].name);//打印字段名
	}
	y++;//换行
	for(uint32_t j = 0; j < this->record_num;this->print_record(j, y),y++,j++);//打印数据
	free(ptfmap);
}
void TABLE::reset_table_name(char* table_name){
	this->table_name.assign(table_name);
}
TABLE::~TABLE(){
	//删除表，此时表必须make或者load之后才能使用
	this->field_num=0,this->map_size=0,this->record_length=0,this->record_usage=0,
	this->record_num=0,this->data_ROM=0,this->state=0;
	free(this->p_field);
	free(this->p_data);
	free(this->line_index);
	free(this->offsetofield);
	free(this->idle_map);
	this->p_field=NULL,this->p_data=NULL,this->line_index=NULL,
	this->offsetofield=NULL,this->idle_map=NULL;
	return;
}
void initFIELD(FIELD* field, char* field_name, char type){
	field->type = type;
	memset(field->name, 0, format_name_length);//先归0
	memcpy(field->name, field_name, strlen(field_name) % format_name_length);
}
/*
int main() {
    // 定义字段
    FIELD fields[3];
    initFIELD(&fields[0], "id", I4);       // 整数字段
    initFIELD(&fields[1], "name", STR);   // 字符串字段
    initFIELD(&fields[2], "salary", F4);  // 浮点数字段

    TABLE table("mytable", fields, 3); // 创建表，字段数量为3

	FIELD field[2];
	initFIELD(&field[0], "age", I4);       // 整数字段
    initFIELD(&field[1], "address", STR);   // 字符串字段

	//table.add_field(&field[0]);
	//table.add_field(&field[1]);
	//table.print_table(0);

    table.add_record("1,John Doe,50000,30,china"); // 添加记录
    table.add_record("2,Jane Smith,60000.00,30,china"); // 添加记录
    table.add_record("3,Michael Johnson,70000.00,40,china"); // 添加记录
    table.print_table(0); // 打印表

	//table.swap_field(2,4);//交换salary和address
	//table.swap_record(1,2);

	//table.print_table(7); // 打印表

    return 0;
}
*/