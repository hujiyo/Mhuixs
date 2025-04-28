#include "tblh.hpp"

void TABLE::reset_table_name(const char* table_name)
{
    //设置表名
	this->table_name.assign(table_name);
}

void TABLE::reset_field_name(uint32_t i,string& field_name)
{
	//设置第i个字段名
	memset(this->p_field[i].name,0,format_name_length);
	memcpy(this->p_field[i].name,field_name.c_str(),strlen(field_name.c_str())%format_name_length);
	return;
}


int8_t TABLE::insert_field(FIELD* field, uint32_t i)
{
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

int8_t TABLE::rmv_field(uint32_t i)
{
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

uint32_t TABLE::add_field(FIELD* field)
{
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

int8_t TABLE::swap_field(uint32_t i1_, uint32_t i2_)
{
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