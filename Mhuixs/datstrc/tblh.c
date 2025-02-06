/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "tblhdef.h"
#define err -1

#define _max_(a,b) ((a)>(b)?(a):(b))
/*
使用宏时要小心，尤其是当宏参数是++、--时。
例如，max(a++, b++)可能会导致未定义行为，
因为两个自增操作都会被执行，而且顺序未定义。
unsigned int max(unsigned int a,unsigned int b){
	return (a>b)?a:b;
}
*/


static void gotoxy(uint32_t x, uint32_t y)
{
	//原点为（0，0）
	printf("\033[%d;%dH", ++y, ++x);
}
static int sizeoftype(char type) { //返回某类型在p_data池中的占用的字节数	
	switch (type){
		case i1:return 1;
		case i2:return 2;
		case i4:return 4;
		case i8:return 8;
		case ui1:return 1;
		case ui2:return 2;
		case ui4:return 4;
		case ui8:return 8;
		case f4:return 4;
		case f8:return 8;
		case s50:return 50;
		case s300:return 300;
		case d211:return 4;
		case t111:return 3;
		case dt211111:return 7;
		default: return err;
	}
}
static int ptfsizeoftype(char type){//根据数据类型返回打印时占用的字节数
	switch (type){
		case i1:return 4 + 1;//-128占用4个位数，加一表示至少1个空格
		case i2:return 6 + 1;//-32768占用6个位数
		case i4:return 11 + 1;//-2147483648占用11个位数
		case i8:return 20 + 1;//-9223372036854775808占用20个位数
		case ui1:return 3 + 1;//255占用3个位数
		case ui2:return 5 + 1;//65535占用5个位数
		case ui4:return 10 + 1;//4294967295占用10个位数
		case ui8:return 20 + 1;//18446744073709551615占用20个位数
		case f4:return 7 + 1;//总位数不超过7位
		case f8:return 15 + 1;//总位数不超过15位
		case s50:return 10 + 1;//打印前10个字符
		case s300:return 30 + 1;//打印前30个字符
		case d211:return 10 + 1;//2024.10.10
		case t111:return 8 + 1;//11:11:11
		case dt211111:return 19 + 1;//2024.10.10.11:11:11
	}
	return err;
}
static void cpstr(uint8_t* start, uint8_t* end, uint8_t* target)
{
	/*
	复制start和end "中间" 的信息到target中
	*/
	for (int i = 1; i < (end - start); *target = *(start + i), target++, i++);
}
static void flash_line_index(TABLE* table)
{
	/*
	当p_data或record_length改变
	调用此函数以更新line_index的快速指针
	*/
	//缓存数据
	uint8_t* new_p_data = table->p_data;
	uint32_t record_length = table->record_length;
	uint32_t record_num = table->record_num-1;
	L_INDEX* line_index = table->line_index;

	for(;record_num>0;
		line_index[record_num].rcd_addr = new_p_data + record_length * record_num,
		record_num--);

	line_index[0].rcd_addr = new_p_data;//第一行的地址
}
static void put_tobyte(void* void_inf, uint8_t* pbyte, uint8_t size)
{
	/*
	把字节数为size的void_inf存放在pbyte指向的size个字节中
	*/
	if (size == 1) { //void_inf必须本身是对齐的(原生C变量)，这样不会字节未对齐
		*pbyte = *(uint8_t*)void_inf;
	}
	else if (size == 2) {
		uint16_t inf = *(uint16_t*)void_inf;//把void_inf解释为指向无符号变量的地址，规避了算数位移而使用逻辑位移
		*(pbyte + 0) = inf;
		*(pbyte + 1) = inf >> 8;
	}
	else if (size == 4) {
		uint32_t inf = *(uint32_t*)void_inf;
		*(pbyte + 0) = inf;	
		*(pbyte + 1) = inf >> 8;
		*(pbyte + 2) = inf >> 16; 
		*(pbyte + 3) = inf >> 24;
	}
	else if (size == 8) {
		uint64_t inf = *(uint64_t*)void_inf;
		*(pbyte + 0) = inf;	
		*(pbyte + 1) = inf >> 8;
		*(pbyte + 2) = inf >> 16; 
		*(pbyte + 3) = inf >> 24;
		*(pbyte + 4) = inf >> 32; 
		*(pbyte + 5) = inf >> 40;
		*(pbyte + 6) = inf >> 48; 
		*(pbyte + 7) = inf >> 56;
	}
}
static void get_frombyte(void* void_inf, const uint8_t* pbyte, uint8_t size)
{
	/*
	读取i1,i2,i4,i8,ui1,ui2,ui4,ui8,f4,f8类型数据
	*/
	if (size == 1) {//注释请对照put_tobyte
		*(uint8_t*)void_inf = *pbyte;
	}
	else if (size == 2) {//tblh核心函数,无需考虑错误处理
		uint16_t inf = (*pbyte) |
		 (*(pbyte + 1) << 8);
		*(uint16_t*)void_inf = inf;
	}
	else if (size == 4) {
		uint32_t inf = (*pbyte) |
		(*(pbyte + 1) << 8) |
		(*(pbyte + 2) << 16) |
		(*(pbyte + 3) << 24);
		*(uint32_t*)void_inf = inf;
	}
	else if (size == 8) {
		uint64_t inf = ((int64_t)*pbyte) | 
		(((int64_t) * (pbyte + 1)) << 8) | 
		(((int64_t) * (pbyte + 2)) << 16) | 
		(((int64_t) * (pbyte + 3)) << 24) |
		(((int64_t) * (pbyte + 4)) << 32) | 
		(((int64_t) * (pbyte + 5)) << 40) | 
		(((int64_t) * (pbyte + 6)) << 48) | 
		(((int64_t) * (pbyte + 7)) << 56);
		*(uint64_t*)void_inf = inf;
	}
}
static uint8_t store_fieldata(char* p_inputstr, uint8_t* p_storaddr, char type)
{
	/*
	tblh_add_record函数的底层储存函数
	把字符串类型p_inputdata按照type类型进行自动储存
	使用put_tobyte处理C语言标准数据的储存
	p_inputdata必须以\0结尾
	put_tobyte存储C语言标准数据类型
	storage_field_data存储SQlh标准数据类型
	*/
	switch (type)
	{
		case i1:
			*p_storaddr = (int8_t)atoi((const char*)p_inputstr);//这里可以考虑使用更快速的函数atoi
			return 0;
		case i2: {//标签后不能直接声明变量，用大括号括起来
			int16_t i = (int16_t)atoi((const char*)p_inputstr);//在后期调试时，这里可以尝试改成atoi
			put_tobyte(&i, p_storaddr, sizeoftype(i2));
			return 0;
		}
		case i4: {
			int32_t i = (int32_t)atoi((const char*)p_inputstr);
			put_tobyte(&i, p_storaddr, sizeoftype(i4));
			return 0;
		}		   
		case i8: {
			int64_t i = (int64_t)strtoll((const char*)p_inputstr, NULL, 10);//长字节使用strtoll
			put_tobyte(&i, p_storaddr, sizeoftype(i8));
			return 0;
		}			
		case ui1:
			*p_storaddr = (int8_t)atoi((const char*)p_inputstr);
			return 0;
		case ui2: {
			uint16_t i = (int8_t)atoi((const char*)p_inputstr);
			put_tobyte(&i, p_storaddr, sizeoftype(ui2));
			return 0;
		}				
		case ui4: {
			uint32_t i = strtoul((const char*)p_inputstr, NULL, 10);
			put_tobyte(&i, p_storaddr, sizeoftype(ui4));
			return 0;
		}				
		case ui8: {
			uint64_t i = strtoull((const char*)p_inputstr, NULL, 10);
			put_tobyte(&i, p_storaddr, sizeoftype(ui8));
			return 0;
		}				
		case f4: {
			float i = strtof((const char*)p_inputstr, NULL);
			put_tobyte(&i, p_storaddr, sizeoftype(f4));
			return 0;
		}			
		case f8: {
			double i = strtod((const char*)p_inputstr, NULL);
			put_tobyte(&i, p_storaddr, sizeoftype(f8));
			return 0;
		}			
		case s50:
			for (int i = 0; i < short_string; i++) {
				if (*p_inputstr == separater || *p_inputstr == '\0'){
					return 0;//当遇到separater和'\0'就跳出循环
				}
				*p_storaddr = *p_inputstr;
				p_storaddr++; 
				p_inputstr++;
			}
			return 0;
		case s300:
			for (int i = 0; i < long_string; i++) 
			{
				if (*p_inputstr == separater || *p_inputstr == '\0'){
					return 0;
				}
				*p_storaddr = *p_inputstr;
				p_storaddr++; 
				p_inputstr++;
			}
			return 0;
		case d211:
			goto YEAR;
		case t111: 
			goto HOUR;
		case dt211111:
			goto YEAR;

		YEAR://年
		{
			uint32_t i = strtoul((const char*)p_inputstr, NULL, 10);//这里可以用更简单得atoi
			put_tobyte(&i, p_storaddr, sizeoftype(ui2));
			p_inputstr = strchr(p_inputstr, '.') + 1;
			p_storaddr += sizeoftype(ui2);
			//月
			*p_storaddr = strtoul((const char*)p_inputstr, NULL, 10);
			p_storaddr++;
			p_inputstr = strchr(p_inputstr, '.') + 1;
			//日
			*p_storaddr = strtoul((const char*)p_inputstr, NULL, 10);
			if (type == d211){
				return 0;
			}
			p_storaddr++;
			p_inputstr = strchr(p_inputstr, '.') + 1;
		HOUR://时
			*p_storaddr = strtoul((const char*)p_inputstr, NULL, 10);
			p_storaddr++;
			p_inputstr = strchr(p_inputstr, ':') + 1;
			//分
			*p_storaddr = strtoul((const char*)p_inputstr, NULL, 10);
			p_storaddr++;
			p_inputstr = strchr(p_inputstr, ':') + 1;
			//秒
			*p_storaddr = strtoul((const char*)p_inputstr, NULL, 10);
			return 0;
		}
		/*
		日期
		d211		*年*月*日 4byte 2+1+1
		t111		*时*分*秒 3byte 1+1+1
		dt211111	*年*月*日*时*分*秒 7byte 2+1+1+1+1+1
		例子：2024.10.23.17:56:45
		*/
	}
	return 1;
}
TABLE* makeTABLE(char* table_name, FIELD* field, uint32_t field_num)
{
	TABLE* table = (TABLE*)calloc(1, sizeof(TABLE));
	if(table == NULL){
		return NULL;
	}

	//初始化table基础信息
	table->field_num = field_num;
	table->record_num = 0;
	uint32_t record_usage = 0;
	for (uint32_t i = 0; i < field_num; record_usage += sizeoftype(field[i].type), i++);
	table->record_usage = record_usage;
	table->record_length = 
	record_usage * (	//根据现有字段的占用长度合理的选择留白率
		(record_usage < long_record) ? short_record_s_vacancy_rate : long_record_s_vacancy_rate
	);

	//创建p_head区,并创建对应的偏移量索引
	table->p_field = (FIELD*)calloc(field_num , sizeof(FIELD));
	if(table->p_field == NULL){
		free(table);
		return NULL;
	}
	table->offsetofield = (uint32_t*)malloc(field_num * sizeof(uint32_t*));
	if(table->offsetofield == NULL){
		free(table->p_field);
		free(table);
		return NULL;
	}
	uint32_t ofsum = 0;	
	for (uint32_t i = 0; i < field_num; 
		table->p_field[i] = field[i],
		table->offsetofield[i] = ofsum, 
		ofsum += sizeoftype(field[i].type), 
		i++
	);

	//没有空位
	table->idle_map = (IDLE_MAP*)malloc(sizeof(IDLE_MAP));
	if(table->idle_map == NULL){
		free(table->p_field);
		free(table->offsetofield);
		free(table);
		return NULL;
	}
	table->map_size = 0;//map_size是真实大小，不是数组中括号中的最大值

	//创建TABLE数据区
	table->data_ROM = initial_ROM;//TABLE数据区record条数初始容量为200条数
	uint8_t* p_data = (uint8_t*)calloc(1,table->record_length * initial_ROM);
	if(p_data == NULL){
		free(table->p_field);
		free(table->offsetofield);
		free(table->idle_map);
		free(table);
		return NULL;
	}
	table->p_data = p_data;

	//创建line_index索引
	table->line_index = (L_INDEX*)malloc(sizeof(L_INDEX));//先建立一个line_index
	if(table->line_index == NULL){
		free(table->p_field);
		free(table->offsetofield);
		free(table->idle_map);
		free(table->p_data);
		free(table);
		return NULL;
	}

	//创建table名区
	table->table_name = (char*)calloc(1,name_max_size);
	if(table->table_name == NULL){
		free(table->line_index);
		free(table->p_field);
		free(table->offsetofield);
		free(table->idle_map);
		free(table->p_data);
		free(table);
		return NULL;
	}
	memcpy(table->table_name, table_name, strlen(table_name)%name_max_size);

	return table;
}
int8_t tblh_make_table(TABLE* table, char* table_name, FIELD* field, uint32_t field_num)
{
	//初始化table基础信息
	table->field_num = field_num;
	table->record_num = 0;
	uint32_t record_usage = 0;
	for (uint32_t i = 0; i < field_num; record_usage += sizeoftype(field[i].type), i++);
	table->record_usage = record_usage;
	table->record_length = 
	record_usage * (
		(record_usage < long_record) ? short_record_s_vacancy_rate : long_record_s_vacancy_rate
	);//根据现有字段的占用长度合理的选择留白率
	
	//创建p_head区,并创建对应的偏移量索引
	table->p_field = (FIELD*)calloc(field_num , sizeof(FIELD));
	if(table->p_field == NULL){
		return err;
	}
	table->offsetofield = (uint32_t*)malloc(field_num * sizeof(uint32_t*));
	if(table->offsetofield == NULL){
		free(table->p_field);
		return err;
	}
	uint32_t ofsum = 0;
	for (uint32_t i = 0; i < field_num; table->p_field[i] = field[i], table->offsetofield[i] = ofsum, ofsum += sizeoftype(field[i].type), i++);
	
	//没有空位
	table->idle_map = (IDLE_MAP*)malloc(sizeof(IDLE_MAP));
	if(table->idle_map == NULL){
		free(table->p_field);
		free(table->offsetofield);
		return err;
	}
	table->map_size = 0;//map_size是真实大小，不是数组中括号中的最大值

	//创建TABLE数据区
	table->data_ROM = initial_ROM;//TABLE数据区record条数初始容量为200条数
	uint8_t* p_data = (uint8_t*)calloc(1,table->record_length * initial_ROM);
	if(p_data == NULL){
		free(table->p_field);
		free(table->offsetofield);
		free(table->idle_map);
		return err;
	}
	table->p_data = p_data;

	//创建line_index索引
	table->line_index = (L_INDEX*)malloc(sizeof(L_INDEX));//先建立一个line_index
	if(table->line_index == NULL){
		free(table->p_field);
		free(table->offsetofield);
		free(table->idle_map);
		free(table->p_data);
		return err;
	}

	//创建table名区
	table->table_name = (char*)calloc(1,name_max_size);
	memcpy(table->table_name, table_name, strlen(table_name)%name_max_size);

	return 0;
}
int64_t tblh_add_record(TABLE* table, const char* record)//add总是在末尾追加
{
	//如果record为空，则增加一条空记录
	int isnullrecord = 0;
	if (record == NULL){
		isnullrecord = 1;
	}

	/*
	tblh_add_record把新记录加到p_data的末尾、line_index的索引组织，未来可能还要处理分页管理
	函数返回 虚序列号
	*/

	if (table->record_num == table->data_ROM) 	{
		//记录数容量满了:扩展
		//不用担心有空位没有利用，记录条数永远等于实际记录条数
		table->p_data = (uint8_t*)realloc(table->p_data, table->record_length * (table->data_ROM + add_ROM));
		if (table->p_data == NULL){
			return err;
		}
		//更新p_data后要注意要刷新line_index#######!!!!!!!!!!!!!!!!!!!!!!###############################
		flash_line_index(table);
		table->data_ROM += add_ROM;
	}

	/*
	定位本条待写入记录的首地址即p_data的末尾    
	（不用担心内存中存在空位 ：tblh_rmv_record将会把最后一个数据填补到删除的记录处）
	*/
	uint8_t* new_record_address = table->p_data + table->record_length * table->record_num;//更新record_num后定位地址
	table->record_num++;//记录数更新

	//重新创建line_index索引
	table->line_index = (L_INDEX*)realloc(table->line_index, table->record_num * sizeof(L_INDEX));
	if (table->line_index == NULL){
		free(table->p_data);
		return err;
	}
	table->line_index[table->record_num - 1].sequence = table->record_num - 1;//虚顺序也是表格的最后一个//序号都是从0开始，所以这里-1
	table->line_index[table->record_num - 1].rcd_addr = new_record_address;//记录记录首地址
	//写入记录
	memset(new_record_address, 0, table->record_length);//注意！写入操作前先归0！！！！
	if (isnullrecord){
		return table->record_num - 1;//如果是空记录，则不写入数据
	}
	char* pointer = (char*)record;//初始化 输入字符串 的定位指针
	uint32_t field_num = table->field_num;//初始化 字段数量
	for (uint32_t i = 0; i < field_num; i++)
	{
		if (i){//如果不是第一个字段，则每次定位到,后都进一位
			pointer = strchr(pointer, separater) + 1;
		}
		if (pointer == (char*)1){
			return 0;//如果已经到record的结尾'\0'
		}

		char result[long_string];//足够的大小来储存(这里可能会不够)<----扯淡
		memset(result, 0, long_string);//保证以\0结尾

		if (i != field_num - 1){
			cpstr((uint8_t*)(pointer - 1), (uint8_t*)strchr(pointer, separater), (uint8_t*)result);//获得本字段对应的字符串
		}
		else{
			cpstr((uint8_t*)(pointer - 1), (uint8_t*)strchr(pointer, '\0'), (uint8_t*)result);
		}
		//store_fieldata自动将目标字符串按照type的种类存入p_data中
		store_fieldata(result,new_record_address + table->offsetofield[i], table->p_field[i].type);
	}

	return table->record_num - 1;
}
int8_t tblh_rmv_record(TABLE* table, uint32_t j)//删除虚序号及其对应的record
{
	/*
	本函数实际上处理的是末记录和删除记录的关系
	用最后一条实记录去补删除的记录留下的空位
	保证记录的连续性（记录条数永远等于实际记录条数）
	*/

	//判断j记录是否存在
	if (j >= table->record_num){
		return err;
	}
	memset(table->line_index[j].rcd_addr, 0, table->record_length);//清空j记录

	//判断本j记录是否是末记录（实记录）
	if (table->line_index[j].sequence != table->record_num - 1) 
	{
		//不是末记录就去找到sequence为record_num的记录（末记录）
		uint32_t j_mo = 0;
		for (; j_mo < table->record_num; j_mo++) 
		{
			if (table->line_index[j_mo].sequence == table->record_num - 1){
				break;//找到末记录对应的虚序列
			}
		}

		//把末记录复制到j处
		cpstr(table->line_index[j_mo].rcd_addr - 1, 
		table->line_index[j_mo].rcd_addr + table->record_length, 
		table->line_index[j].rcd_addr
		);
		memset(table->line_index[j_mo].rcd_addr, 0, table->record_length);//原来的末记录归0//想了很久还是增加这一步吧，省点力气
		
		//修改末记录对应的索引指向j地址
		table->line_index[j_mo] = table->line_index[j];
	}
	for (uint32_t i = j; i < table->record_num - 1; table->line_index[i] = table->line_index[i + 1], i++);//索引进位
	//缩小索引空间
	table->line_index = (L_INDEX*)realloc(table->line_index, sizeof(L_INDEX) * (--table->record_num));

	return 0;
}
int8_t tblh_swap_record(TABLE* table, uint32_t j1, uint32_t j2)//函数用于交换两条记录的排序 返回0表示成功
{
	//tblh的实现方法是虚序列号对应的索引的交换
	if (j1 >= table->record_num || j2 >= table->record_num){
		return err;//验证j1、j2是否存（j1，j2一定是正数）
	}
	//交换j1和j2的索引
	L_INDEX cache = table->line_index[j1];
	table->line_index[j1] = table->line_index[j2];
	table->line_index[j2] = cache;

	return 0;
}
uint8_t tblh_insert_record(TABLE* table, const char* record, uint32_t j)//向j处插入记录，原来及后面的记录全部退一位
{
	uint32_t sep = tblh_add_record(table, record);//这个地方调用了外部函数，不知道会不会影响性能
	if (j >= sep){
		return 0;//已经增加了一条记录，如果现在j比最后一条记录都大或等，那就直接return了
	}
	L_INDEX cache = table->line_index[sep];//先调用tblh_add_record并保存其索引
	for (uint32_t k = sep; k > j; table->line_index[k] = table->line_index[k - 1], k--);
	table->line_index[j] = cache;

	return 0;//执行完返回0
}
int8_t tblh_rmv_field(TABLE* table, uint32_t i)
{
	//判断i是否合法
	if (i >= table->field_num){
		return err;
	}
	/*
	由于字段的长度不一，再者对整列数据进行移动填补效率实在太低了
	所以空位就留下来，它所造成的内存浪费怎么解决呢？由以下机制来弥补：
	1.TABLE对象增加一个“记录使用区”的“空位内存地图”（idle_map）（注意：使用记录使用区的内存并不会改变record_usage）当下次增加字段的时候会首先查询idle_map中是否有适合大小的空位
	  只要新插入数据小于等于空位，就会占用空位，使空位变小或消失
	2.使用tblh_del_idle(...)整理数据库字段，这个函数将删除所有空位并整理字段信息
	3.如果是删除最后一个字段，则不会增加map，而是直接修改record_usage
	*/

	//首先判断删除的是否是最后一个字段
	if (i != table->field_num - 1) 
	{
		//增加table中的idle_map的大小
		table->idle_map = (IDLE_MAP*)realloc(table->idle_map, ++table->map_size * sizeof(IDLE_MAP));//map_size++;
		if(table->idle_map == NULL){
			return err;
		}
		table->idle_map[table->map_size - 1].idle_size = sizeoftype(table->p_field[i].type);//获得删除字段的大小
		table->idle_map[table->map_size - 1].idle_offset = table->offsetofield[i];//得到空闲位置的偏移量
	}
	else 	{
		table->record_usage -= sizeoftype(table->p_field[i].type);//直接修改record_usage
	}

	//清空删除字段对应的数据区中的数据	
	//这样可以保证record_usage内部的空位绝对是归0的，对于record_length-record_usage中的区域，我称之为分配的到的未定义区域

	//先缓存数据cache
	uint32_t cc_table_record_length = table->record_length;
	uint8_t* cc_table_p_a_i = table->p_data + table->offsetofield[i];
	uint32_t cc_clean_size = table->idle_map[table->map_size - 1].idle_size;
	for (uint32_t j = 0; j < table->record_num; j++) {
		memset(cc_table_p_a_i + cc_table_record_length * j, 0, cc_clean_size);
	}
	if (i != table->field_num - 1) 
	{
		//对field和offsetof_field中删除的字段后的字段进行进位
		for (uint32_t ii = i; ii < table->field_num - 1; 
		table->p_field[ii] = table->p_field[ii + 1], 
		table->offsetofield[ii] = table->offsetofield[ii + 1], 
		ii++
		);//索引进位

		table->field_num--;//字段数-1
		table->p_field = (FIELD*)realloc(table->p_field, table->field_num * sizeof(FIELD));//字段数-1		

		table->offsetofield = (uint32_t*)realloc(table->offsetofield, table->field_num * sizeof(uint32_t));
	}
	else {
		table->field_num--;//字段数-1
		table->p_field = (FIELD*)realloc(table->p_field, table->field_num * sizeof(FIELD));
	}
	return 0;
}
uint32_t tblh_add_field(TABLE* table, FIELD* field)
{
	/*
	tblh_add_field先尝试遍历一遍idle_map,如果存在一个足够大小的空位，就把它写入空位，并重新修改idle_map
	把新字段对应的数据区映射到记录的末尾、field_index的索引组织
	函数返回 虚序列号
	*/
	table->field_num++;
	table->p_field = (FIELD*)realloc(table->p_field, table->field_num * sizeof(FIELD));
	if(table->p_field == NULL){
		return err;
	}
	table->p_field[table->field_num - 1] = field[0];//加星号，field是一个结构体变量名(field[0]<=>*field)
	table->offsetofield = (uint32_t*)realloc(table->offsetofield, table->field_num * sizeof(uint32_t));

	//接下在p_data中为这个字段找合适的储存位置，先在idle_map里找
	uint32_t new_field_size = sizeoftype(field->type);
	uint32_t new_offset = table->record_usage;//默认偏移量
	uint32_t cc_map_size = table->map_size;
	IDLE_MAP* cc_idle_map = table->idle_map;

	//循环遍历找到合适的空位算法
	uint32_t idle_seq = 0;//记录之后填入的空位的序号
	for (uint32_t k = 0; k < cc_map_size; k++) {
		if (cc_idle_map[k].idle_size == new_field_size) 
		{
			new_offset = cc_idle_map[k].idle_offset;
			idle_seq = k;//记录填入的空位序号
			break;//如果有相等的空位，那不用多想，就这个了
		}
		if (cc_idle_map[k].idle_size > new_field_size && new_offset > cc_idle_map[k].idle_offset) 
		{
			new_offset = cc_idle_map[k].idle_offset;
			idle_seq = k;
		}//如果有够装的空间，则以偏移量小的优先
	}
	table->offsetofield[table->field_num - 1] = new_offset;//得到偏移量了
	//接下来判断new_offset是否为record_usage外，如果是，还要判断record_length够不够
	if (new_offset == table->record_usage) 
	{
		//如果不够，需要增加分配p_data的空间
		if (table->record_length - table->record_usage <= new_field_size) 
		{
			//如果剩余空间不够，增加分配p_data的空间
			uint32_t old_record_length = table->record_length;
			table->record_length += record_add_ROM;
			//重新分配p_data，注意：修改了p_data,字段索引的偏移量并不会改变，但是line_index的索引需要重新分配
			table->p_data = (uint8_t*)realloc(table->p_data, table->record_length * table->data_ROM);
			//接下来开始进行所有数据的位移
			if (table->record_num == 0){
				goto l;//uint32_t-1的情况
			}
			for (uint32_t k = table->record_num - 1; k > 0; k--) {
				//从最后一个记录开始,每条记录向后移动record_add_ROM*k,注意record_usage之后的空间都是空闲的(此时table->record_usage还没有进行修改)
				//首先要保证record_usage之中的空间是绝对统一的，然后才是对新字段空间的分配以及后续的归0
				memmove(table->p_data + table->record_length * k, table->p_data + old_record_length * k, table->record_usage);
				//只要执行到k=1,因为第一条记录不需要移动
			}
		l:
		flash_line_index(table);//刷新line_index，至此，扩容完毕		
		}
		table->record_usage += new_field_size;
		//清空新字段对应的数据区中的数据
		//使用空间record_usage已经增加,接下来进行归0操作,抄一下上面的代码。
		uint32_t cc_table_record_length = table->record_length;
		uint8_t* cc_table_p_a_i = table->p_data + table->offsetofield[table->field_num - 1];
		uint32_t cc_clean_size = new_field_size;
		for (uint32_t j = 0; j < table->record_num; j++) {
			memset(cc_table_p_a_i + cc_table_record_length * j, 0, cc_clean_size);
		}

		return table->field_num - 1;//返回虚序列号，序列号默认是最后一个
	}
	else {
		//如果new_offset在record_usage内部，则涉及idle_map的功能了
		//之前的所有代码已经保证record_usage内部的所有空位都已经清空了，这里不需要再清空一遍
		table->idle_map[idle_seq].idle_size -= new_field_size;
		if (table->idle_map[idle_seq].idle_size == 0) 
		{
			//如果这个空位正好被占满了，那么就把idle_map[idle_seq]删掉（用最后一个idle_map替换，无需担心如果只有一个（自己换自己））
			table->idle_map[idle_seq] = table->idle_map[--table->map_size];//天才！！之前怎么没想到替换
			table->idle_map = (IDLE_MAP*)realloc(table->idle_map, table->map_size * sizeof(IDLE_MAP));
			table->map_size--;
		}
		else {
			//如果idle_map还有空位，那么修改offset就好了
			table->idle_map[idle_seq].idle_offset += new_field_size;
		}
		return table->field_num - 1;
	}
}
int8_t tblh_swap_field(TABLE* table, uint32_t i1_, uint32_t i2_)
{
	if (i1_ >= table->field_num || i2_ >= table->field_num){
		return err;//验证11、12是否存
	}
	//交换i1和i2的索引
	FIELD cache = table->p_field[i1_];
	table->p_field[i1_] = table->p_field[i2_];
	table->p_field[i2_] = cache;
	uint32_t cache_offset = table->offsetofield[i1_];
	table->offsetofield[i1_] = table->offsetofield[i2_];
	table->offsetofield[i2_] = cache_offset;

	return 0;
}
int8_t tblh_insert_field(TABLE* table, FIELD* field, uint32_t i)
{
	uint32_t sep = tblh_add_field(table, field);
	if (i >= sep){
		return err;//如果非法插入还是默认在最后插入
	}
	FIELD cache = table->p_field[sep];
	uint32_t cache_offset = table->offsetofield[sep];
	FIELD* cc_p_head = table->p_field;
	uint32_t* cc_offsetof_field = table->offsetofield;
	for (uint32_t k = sep; k > i; cc_p_head[k] = cc_p_head[k - 1], cc_offsetof_field[k] = cc_offsetof_field[k - 1], k--);
	table->p_field[i] = cache;
	table->offsetofield[i] = cache_offset;

	return 0;
}
int8_t tblh_getfrom_i_j(TABLE* table, uint32_t i, uint32_t j, void* buffer)
{
	/*
	//使用这个函数前你就应当知道你读取的数据到底是什么类型,并保证buffer缓冲区足够容纳数据
	从（i，j）虚拟位置获得数据
	本函数会根据数据类型自动返回相应的格式化数据
	*/
	//先获得第i个字段的类型、偏移量的信息
	char type = table->p_field[i].type;
	//找到（i,j）的地址
	uint8_t* inf_addr = table->p_data + j * table->record_length + table->offsetofield[i];
	//根据type类型进行格式化输出
	switch (type)
	{
		//i1,i2,i4,i8,ui1,ui2,ui4,ui8,f4,f8直接对应C语言中的数据类型
/*******************************************
case i1:
	*(int8_t*)buffer = *(int8_t*)inf_addr; 
	return 0;
case i2:
	get_frombyte(buffer, inf_addr, 2); 
	return 0;
case i4:
	get_frombyte(buffer, inf_addr, 4); 
	return 0;
case i8:
	get_frombyte(buffer, inf_addr, 8); 
	return 0;
case ui1:
	*(uint8_t*)buffer = *(uint8_t*)inf_addr; 
	return 0;
case ui2:
	get_frombyte(buffer, inf_addr, 2); 
	return 0;
case ui4:
	get_frombyte(buffer, inf_addr, 4); 
	return 0;
case ui8:
	get_frombyte(buffer, inf_addr, 8); 
	return 0;
case f4:
	get_frombyte(buffer, inf_addr, 4); 
	return 0;
case f8:
	get_frombyte(buffer, inf_addr, 8); 
	return 0;
********************************************/
		case i1:case i2:case i4:case i8:			
		case ui1:case ui2:case ui4:case ui8:			
		case f4:case f8:
			get_frombyte(buffer, inf_addr, sizeoftype(type)); 
			return 0;
		case s50:
			memcpy(buffer, inf_addr, 50); 
			return 0;
		case s300:
			memcpy(buffer, inf_addr, 300); 
			return 0;
		/*********************************************************************************
		d211		'n'		// * 年* 月* 日 4byte 2 + 1 + 1
		t111		'p'		// * 时 * 分 * 秒 3byte 1 + 1 + 1
		dt211111	's'		// * 年 * 月 * 日 * 时 * 分 * 秒 7byte 2 + 1 + 1 + 1 + 1 + 1
		时间由特别结构体进行读取
		**********************************************************************************/
		case d211: {
			tp_d211 tp;
			get_frombyte(&tp.year, inf_addr, sizeof(uint16_t));//读取年
			tp.month = *(inf_addr + 2);
			tp.day = *(inf_addr + 3);
			*(tp_d211*)buffer = tp;

			return 0;
		}
		case t111: {
			tp_t111 tp;
			tp.hour = *(inf_addr + 0);
			tp.minute = *(inf_addr + 1);
			tp.second = *(inf_addr + 2);
			*(tp_t111*)buffer = tp;

			return 0;
		}
		case dt211111: {
			tp_dt211111 tp;
			get_frombyte(&tp.year, inf_addr, sizeof(uint16_t));
			tp.month = *(inf_addr + 2);
			tp.day = *(inf_addr + 3);
			tp.hour = *(inf_addr + 4);
			tp.minute = *(inf_addr + 5);
			tp.second = *(inf_addr + 6);
			*(tp_dt211111*)buffer = tp;

			return 0;
		}
	}
	return err;
}
static uint8_t copymemfrom_i_j(TABLE* table, uint32_t i, uint32_t j, void* buffer)
{
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
static uint8_t* address_of_i_j(TABLE *table,uint32_t i,uint32_t j)
{
	return table->p_data + j * table->record_length + table->offsetofield[i];
}
void tblh_printf_record(TABLE* table, uint32_t j, uint32_t y)
{
	//初始化ptfmap得到打印地图
	uint32_t* ptfmap = (uint32_t*)malloc(sizeof(uint32_t) * table->field_num);
	uint32_t address = 0;
	for (uint32_t i = 0; i < table->field_num; i++) {
		ptfmap[i] = address;
		address += _max_(ptfsizeoftype(table->p_field[i].type), strlen(table->p_field[i].name) + 1);
	}
	for (uint32_t i = 0; i < table->field_num; i++) {
			gotoxy(ptfmap[i], y);//定位到打印位置			
			uint8_t* inf_addr = table->line_index[j].rcd_addr+table->offsetofield[i];//确定数据位置
			switch (table->p_field[i].type) {
				case i1:
					printf("%d", *(int8_t*)inf_addr); 
					break;
				case i2: {
					int16_t tp; 
					get_frombyte(&tp, inf_addr, sizeoftype(i2));
					printf("%d", tp); 
					break;
				}
				case i4: {
					int32_t tp; 
					get_frombyte(&tp, inf_addr, sizeoftype(i4));
					printf("%d", tp); 
					break;
				}
				case i8: {
					int64_t tp; 
					get_frombyte(&tp, inf_addr, sizeoftype(i8));
					printf("%lld" /*PRIx64*/, tp); 
					break;//"lld"报错，不知道为什么
				}
				case ui1:
					printf("%d", *(uint8_t*)inf_addr); 
					break;
				case ui2: {
					uint16_t tp; 
					get_frombyte(&tp, inf_addr, sizeoftype(ui2));
					printf("%d", tp); 
					break;
				}
				case ui4: {
					uint32_t tp; 
					get_frombyte(&tp, inf_addr, sizeoftype(ui4));
					printf("%d", tp); 
					break;
				}
				case ui8: {
					uint64_t tp; 
					get_frombyte(&tp, inf_addr, sizeoftype(ui8));
					printf("%llu"/* PRIu64*/, tp); 
					break;
				}
				case f4: {
					float tp; 
					get_frombyte(&tp, inf_addr, sizeoftype(f4));
					printf("%.7g", tp); 
					break;
				}
				case f8: {
					double tp; 
					get_frombyte(&tp, inf_addr, sizeoftype(f8));
					printf("%.15g", tp); 
					break;
				}
				case s50:
					printf("%.10s", inf_addr); 
					break;
				case s300:
					printf("%.30s", inf_addr); 
					break;
				case d211: {
					tp_d211 tp; 
					tblh_getfrom_i_j(table, i, j, &tp);
					printf("%d.%d.%d", tp.year, tp.month, tp.day); 
					break;
				}
				case t111: {
					tp_t111 tp; 
					tblh_getfrom_i_j(table, i, j, &tp);
					printf("%d:%d:%d", tp.hour, tp.minute, tp.second); 
					break;
				}
				case dt211111: {
					tp_dt211111 tp; 
					tblh_getfrom_i_j(table, i, j, &tp);
					printf("%d.%d.%d.%d:%d:%d", tp.year, tp.month, tp.day, tp.hour, tp.minute, tp.second); 
					break;
				}
			}
		}
	free(ptfmap);
}
void tblh_printf_table(TABLE* table,uint32_t start_line)
{
	uint32_t y = start_line;

	//初始化ptfmap得到打印地图
	uint32_t* ptfmap = (uint32_t*)malloc(sizeof(uint32_t) * table->field_num);
	if(ptfmap==NULL){
		printf("Err:failed to malloc ptfmap\n");
		return;
	}
	uint32_t address = 0;
	for (uint32_t i = 0; i < table->field_num; i++) {
		ptfmap[i] = address;
		address += _max_(ptfsizeoftype(table->p_field[i].type), strlen(table->p_field[i].name) + 1);
	}

	//打印字段名
	for (uint32_t i = 0; i < table->field_num; i++) {
		gotoxy(ptfmap[i], y);
		printf("%s", table->p_field[i].name);
	}
	y++;

	//打印数据
	for (uint32_t j = 0; j < table->record_num; y++, j++) {
		tblh_printf_record(table, j, y);
	}
	free(ptfmap);
}
uint8_t tblh_reset_table_name(TABLE* table, char* table_name)
{
	memset(table->table_name, 0, name_max_size);//先归0
	memcpy(table->table_name, table_name, strlen(table_name));
	return 0;
}
void tblh_del_table(TABLE* table)
{
	//删除表，此时表必须make或者load之后才能使用
	table->field_num=0;	
	table->map_size=0;	
	table->record_length=0;
	table->record_usage=0;
	table->record_num=0;	
	table->data_ROM=0;

	if(table->p_field!=NULL){
		free(table->p_field);
	}
	if(table->p_data!=NULL){
		free(table->p_data);
	}
	if(table->line_index!=NULL){
		free(table->line_index);
	}
	if(table->offsetofield!=NULL){
		free(table->offsetofield);
	}
	if(table->idle_map!=NULL){
		free(table->idle_map);
	}
	if(table->table_name!=NULL){
		free(table->table_name);
	}
}
void tblh_save_table(TABLE* table,char* file_path){//保存表，不改变表的内容
	//只需要提供文件夹路径,文件名就是表名
	FILE* file=fopen(file_path,"wb");
	if(file==NULL){
		printf("Err:failed to open\"%s\"\n",file_path);return;
	}

	TBL_HEAD head;//先写入文件头
	head.field_num=table->field_num;
	head.map_size=table->map_size;
	head.record_length=table->record_length;
	head.record_usage=table->record_usage;
	head.record_num=table->record_num;
	head.data_ROM=table->data_ROM;
	head.offset_of_table_name=TBL_HEAD_SIZE;
	head.offset_of_field=head.offset_of_table_name+name_max_size;
	head.offset_of_offsetofield=head.offset_of_field+table->field_num*sizeof(FIELD);
	head.offset_of_idle_map=head.offset_of_offsetofield+table->field_num*sizeof(uint32_t);
	head.offset_of_line_index=head.offset_of_idle_map+table->map_size*sizeof(IDLE_MAP);
	head.offset_of_data=head.offset_of_line_index+table->record_num*sizeof(LINE_INDEX);

	fseek(file,0,SEEK_SET);
	fwrite(&head,sizeof(TBL_HEAD),1,file);

	//写入表名
	fseek(file,head.offset_of_table_name,SEEK_SET);
	fwrite(table->table_name,1,name_max_size,file);

	//写入字段
	fseek(file,head.offset_of_field,SEEK_SET);
	fwrite(table->p_field,sizeof(FIELD),table->field_num,file);

	//写入字段偏移
	fseek(file,head.offset_of_offsetofield,SEEK_SET);
	fwrite(table->offsetofield,sizeof(uint32_t),table->field_num,file);

	//写入空闲表
	fseek(file,head.offset_of_idle_map,SEEK_SET);
	fwrite(table->idle_map,sizeof(IDLE_MAP),table->map_size,file);

	//写入索引表
	fseek(file,head.offset_of_line_index,SEEK_SET);
	fwrite(table->line_index,sizeof(LINE_INDEX),table->record_num,file);

	//写入数据
	fseek(file,head.offset_of_data,SEEK_SET);
	fwrite(table->p_data,1,table->record_num *table->record_length,file);

	fclose(file);
}
int8_t tblh_load_table(TABLE* table,char* file_path)
{
	//加载表，删除原表内容
	//先释放原有空间
	tblh_del_table(table);

	FILE* file=fopen(file_path,"rb");
	if(file==NULL){
		printf("Err:failed to open\"%s\"\n",file_path);
		return err;
	}

	//读取文件头
	TBL_HEAD head;
	fseek(file,0,SEEK_SET);
	fread(&head,sizeof(TBL_HEAD),1,file);

	//初始化表
	table->field_num=head.field_num;
	table->map_size=head.map_size;
	table->record_length=head.record_length;
	table->record_usage=head.record_usage;
	table->record_num=head.record_num;
	table->data_ROM=head.data_ROM;

	//申请空间
	table->p_field = (FIELD*)calloc(head.field_num,sizeof(FIELD));
	table->offsetofield = (uint32_t*)calloc(head.field_num,sizeof(uint32_t) );
	table->idle_map = (IDLE_MAP*)calloc( head.map_size,sizeof(IDLE_MAP));
	table->line_index = (LINE_INDEX*)calloc( head.record_num,sizeof(LINE_INDEX) );
	table->p_data = (uint8_t*)calloc( head.data_ROM,sizeof(uint8_t) );
	table->table_name = (char*)calloc( name_max_size,sizeof(char) );

	if(table->p_field==NULL||
	table->offsetofield==NULL||
	table->idle_map==NULL||
	table->line_index==NULL||
	table->p_data==NULL||
	table->table_name==NULL){
		free(table->p_field);
		free(table->offsetofield);
		free(table->idle_map);
		free(table->line_index);
		free(table->p_data);
		free(table->table_name);
		printf("Err:failed to calloc\n");
		return err;
	}

	//读取表名
	fseek(file,head.offset_of_table_name,SEEK_SET);
	fread(table->table_name,1,name_max_size,file);

	//读取字段
	fseek(file,head.offset_of_field,SEEK_SET);
	fread(table->p_field,sizeof(FIELD),head.field_num,file);

	//读取字段偏移
	fseek(file,head.offset_of_offsetofield,SEEK_SET);
	fread(table->offsetofield,sizeof(uint32_t),head.field_num,file);

	//读取空闲表
	fseek(file,head.offset_of_idle_map,SEEK_SET);
	fread(table->idle_map,sizeof(IDLE_MAP),head.map_size,file);

	//读取索引表
	fseek(file,head.offset_of_line_index,SEEK_SET);
	fread(table->line_index,sizeof(LINE_INDEX),head.record_num,file);

	//读取数据
	fseek(file,head.offset_of_data,SEEK_SET);
	fread(table->p_data,1,table->record_length*table->record_num,file);

	fclose(file);
	return 0;
}

int tblh_join_record(TABLE* table, TABLE* table_join){//把join表加到table后面
	//先检查字段类型是否相同
	for (uint32_t i = 0; i < table->field_num; i++) {
		if (table->p_field[i].type != table_join->p_field[i].type) return err;
	}
	//不用检查字段名是否相同，直接把join表的内容加到table后面
	/*
	接下来我们来确定table的容量够不够
	*/
	if (table->record_num + table_join->record_num >= table->data_ROM) {	//记录数容量满了:扩展//不用担心有空位没有利用，记录条数永远等于实际记录条数
		table->p_data = (uint8_t*)realloc(table->p_data, table->record_length * (table->data_ROM + table_join->record_num));
		//更新p_data后要注意要刷新line_index
		flash_line_index(table);
		table->data_ROM += table_join->record_num;
	}
	memset(table->p_data + table->record_num*table->record_length ,0,table_join->record_num*table->record_length);
	//下面是一些临时变量的提前声明
	/*
	int8_t cc_i1 = 0;
	int16_t cc_i2 = 0;
	int32_t cc_i4 = 0;
	int64_t cc_i8 = 0;
	uint8_t cc_ui1 = 0;
	uint16_t cc_ui2 = 0;
	uint32_t cc_ui4 = 0;
	uint64_t cc_ui8 = 0;
	float cc_f4 = 0;
	double cc_f8 = 0;
	char cc_s50[50] = { 0 };
	char cc_s300[300] = { 0 };
	tp_d211 cc_d211;
	tp_t111 cc_t111;
	tp_dt211111 cc_dt211111;
	*/
	uint8_t* cc_buffer=(uint8_t*)calloc(75,4);//保证可以读取最长的s300
	uint32_t cc_table_join_record_num = table_join->record_num;
	uint32_t cc_table_join_field_num = table_join->field_num;
	uint32_t cc_table_record_num = table->record_num;

	for (uint32_t j = 0; j < cc_table_join_record_num; j++) 
	{
		for(uint32_t i = 0; i < cc_table_join_field_num; i++) {
			memcpy(address_of_i_j(table,i,j+cc_table_record_num),address_of_i_j(table_join,i,j),sizeof(table->p_field[i].type));
		}
	}

}
/*
void initFIELD(FIELD* field, char* field_name, char type)
{
	field->type = type;
	memset(field->name, 0, name_max_size);//先归0
	memcpy(field->name, field_name, strlen(field_name) % name_max_size);
}

void initTblh(Tblh* tblh)
{
	//将tqlh中的函数指针和函数进行关联	
	tblh->make_table = tblh_make_table;

	tblh->add_record = tblh_add_record;
	tblh->rmv_record = tblh_rmv_record;
	tblh->swap_record = tblh_swap_record;
	tblh->insert_record = tblh_insert_record;

	tblh->add_field = tblh_add_field;
	tblh->rmv_field = tblh_rmv_field;
	tblh->swap_field = tblh_swap_field;
	tblh->insert_field = tblh_insert_field;

	tblh->getfrom_i_j = tblh_getfrom_i_j;

	tblh->reset_table_name = tblh_reset_table_name;
	tblh->printf_table = tblh_printf_table;
	tblh->printf_record = tblh_printf_record;

	tblh->save_table = tblh_save_table;
	tblh->load_table = tblh_load_table;
	tblh->del_table = tblh_del_table;

	tblh->join_table = tblh_join_table;
}
*/