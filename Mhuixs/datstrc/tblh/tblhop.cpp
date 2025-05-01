/* 
 * #版权所有 (c) Mhuixs-team 2024
 * #许可证协议:
 * #任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
 * start from 2025.1
 * Email:hj18914255909@outlook.com
 * 
 * Tblh_Base
 * 这里包含TABLE类的一些操作函数的方法实现
 * 
 */
#include "tblh.hpp"
#include <cstdarg>  // for va_list, va_start, va_end

int64_t TABLE::add_record(size_t field_count, ...)
{
	if(field_count!=this->field_num)return merr;
    // 如果字段数量为0，则增加一条空记录
    int isnullrecord = 0;
    if (field_count == 0) {
        isnullrecord = 1;
    }
    /*
    tblh_add_record把新记录加到p_data的末尾（不用担心内存中存在空位 ：tblh_rmv_record将会把最后一个数据填补到删除的记录处）
    、line_index的索引组织，未来可能还要处理分页管理
    返回 虚序列号
    */
    if (this->record_num == this->data_ROM) {
        // 记录数容量满了:扩展
        uint8_t* new_p_data = (uint8_t*)realloc(this->p_data, this->record_length * (this->data_ROM + add_ROM));
        if (new_p_data == NULL) return merr;
        this->p_data = new_p_data;
        this->data_ROM += add_ROM;
    }

    /*
    定位本条待写入记录的首地址即p_data的末尾
    */
    uint8_t* new_record_address = this->p_data + this->record_length * this->record_num; // 更新record_num后定位地址
    this->record_num++; // 记录数更新

    // 重新创建line_index索引
    this->line_index = (uint32_t*)realloc(this->line_index, this->record_num * sizeof(uint32_t));
    if (this->line_index == NULL) {
        free(this->p_data);
        return merr;
    }
    this->line_index[this->record_num - 1] = this->record_num - 1; // 虚顺序也是表格的最后一个

    // 写入记录前清零
    memset(new_record_address, 0, this->record_length);

    if (isnullrecord) return this->record_num - 1; // 如果是空记录，则不写入数据

    va_list args;
    va_start(args, field_count);

    size_t idx = 0;
    for (; idx < field_count && idx < this->field_num; ++idx) {
        char* field = va_arg(args, char*);
        store_fieldata(field, new_record_address + this->offsetofield[idx], this->p_field[idx].type);
    }

    va_end(args);
    return this->record_num - 1;
}
int64_t TABLE::add_record(std::initializer_list<char*> contents)
{
    // 如果fields为空，则增加一条空记录
    int isnullrecord = 0;
    if (contents.size() == 0) isnullrecord = 1;

    /*
    tblh_add_record把新记录加到p_data的末尾（不用担心内存中存在空位 ：tblh_rmv_record将会把最后一个数据填补到删除的记录处）
    、line_index的索引组织，未来可能还要处理分页管理
    返回 虚序列号
    record的格式："字段1,字段2,字段3..."
    */
    if (this->record_num == this->data_ROM) {
        // 记录数容量满了:扩展
        uint8_t* new_p_data = (uint8_t*)realloc(this->p_data, this->record_length * (this->data_ROM + add_ROM));
        if (new_p_data == NULL) return merr;
        this->p_data = new_p_data;
        this->data_ROM += add_ROM;
    }

    /*
    定位本条待写入记录的首地址即p_data的末尾
    */
    uint8_t* new_record_address = this->p_data + this->record_length * this->record_num; // 更新record_num后定位地址
    this->record_num++; // 记录数更新

    // 重新创建line_index索引
    this->line_index = (uint32_t*)realloc(this->line_index, this->record_num * sizeof(uint32_t));
    if (this->line_index == NULL) {
        free(this->p_data);
        return merr;
    }
    this->line_index[this->record_num - 1] = this->record_num - 1; // 虚顺序也是表格的最后一个

    // 写入记录前清零
    memset(new_record_address, 0, this->record_length);

    if (isnullrecord) return this->record_num - 1; // 如果是空记录，则不写入数据

    size_t idx = 0;
    for (auto it = contents.begin(); it != contents.end() && idx < this->field_num; ++it, ++idx) {
        char* field = *it;
        store_fieldata(field, new_record_address + this->offsetofield[idx], this->p_field[idx].type);
    }

    return this->record_num - 1;
}
/*
int64_t TABLE::add_record(const char* record)//add总是在末尾追加
{
	//如果record为空，则增加一条空记录
	int isnullrecord = 0;
	if (record == NULL) isnullrecord = 1;
	///*
	tblh_add_record把新记录加到p_data的末尾（不用担心内存中存在空位 ：tblh_rmv_record将会把最后一个数据填补到删除的记录处）
	、line_index的索引组织，未来可能还要处理分页管理
	返回 虚序列号
	record的格式："字段1,字段2,字段3..."
	///
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
*/

int8_t TABLE::rmv_record(uint32_t j)
{
    //删除虚序号及其对应的record
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


