/* 
 * #版权所有 (c) Mhuixs-team 2024
 * #许可证协议:
 * #任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
 * start from 2025.1
 * Email:hj18914255909@outlook.com
 * 
 * Tblh_Base
 * 这里包含TABLE类的一些基础函数的实现方法
 * 
 */
#include "tblh.hpp"

int8_t TABLE::isvalid_type(char type){
	switch (type)
    {
		case I1:case UI1:case I2:case UI2:case I4:case UI4:case F4:
		case STR:case DATE:case TIME:case I8:case UI8:case F8:return 1;
		default:return 0;
	}
}

int8_t TABLE::isvalid_keytype(char type){
	switch(type)
	{
		case PRIMARY_KEY:case FOREIGN_KEY:
		case UNIQUE_KEY:case NOT_KEY:return 1;
		default:return 0;
	}
}

int TABLE::sizeoftype(char type) 
{
    /*
    根据数据类型返回数据在p_data池中的占用的字节数
    STR的本质是string类型指针,Mhuixs默认4字节对齐,指针大小为4
    Date,Time的本质是只有一个int成员的结构体，大小也为4
    */
	switch (type)
    {
		case I1:case UI1:return 1;
		case I2:case UI2:return 2;
		case I4:case UI4:case F4:case STR:case DATE:case TIME:return 4;
		case I8:case UI8:case F8:return 8;
		default:return merr;
	}
}

uint8_t* TABLE::real_addr_of_lindex(uint32_t j)//index是虚序列
{
    /*
    real_addr_of_lindex:接受记录的虚序列index,返回这条记录的首地址。
    原理：line_index是一个虚实序号映射数组，比如line_index[5]存储的是虚序列5对应的实际序列（假设是3，则说明序列为5
    的记录在内存中的实际序列是3，则 3*每条记录的占用长度 则为这条记录在p_data数据区的偏移量。）
    */
	return this->p_data + this->line_index[j] * this->record_length;
}

uint8_t* TABLE::address_of_i_j(uint32_t i,uint32_t j)
{
    /*
    address_of_i_j:直接根据虚坐标定位到p_data在内存中的位置，返回地址指针
    等价于：return real_addr_of_lindex(j) + this->offsetofield[i];
    */
	return this->p_data + this->line_index[j] * this->record_length + this->offsetofield[i];
}

int TABLE::ptfsizeoftype(char type)
{
    //根据数据类型返回打印时占用的字节数
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

uint8_t TABLE::store_fieldata(char* p_inputstr, uint8_t* p_storaddr, char type){
	/*
	tblh_add_record函数的底层储存函数
	把字符串类型p_inputdata按照type类型进行自动储存
	使用put_tobyte处理C语言标准数据的储存
	p_inputdata必须以\0结尾
	put_tobyte存储C语言标准数据类型
	storage_field_data存储SQlh标准数据类型
	*/
	temp_mem temp;
	int i;
	switch (type){
		case I1:
			*p_storaddr = atoi((const char*)p_inputstr);//这里可以考虑使用更快速的函数atoi
			return 0;
		case I2:
			temp.i2 = atoi((const char*)p_inputstr);//在后期调试时，这里可以尝试改成atoi
			memcpy(p_storaddr,&temp.i2,sizeoftype(I2));
			return 0;
		case I4:case DATE:case TIME:
			temp.i4 = atoi((const char*)p_inputstr);
			memcpy(p_storaddr,&temp.i4,sizeoftype(type));
			return 0; 
		case I8:
			temp.i8 = strtoll((const char*)p_inputstr, NULL, 10);//长字节使用strtoll
			memcpy(p_storaddr,&temp.i8,sizeoftype(I8));
			return 0;	
		case UI1:
			*p_storaddr = atoi((const char*)p_inputstr);
			return 0;
		case UI2:
			temp.ui2 = atoi((const char*)p_inputstr);
			memcpy(p_storaddr,&temp.ui2,sizeoftype(UI2));
			return 0;		
		case UI4:
			temp.ui4 = strtoul((const char*)p_inputstr, NULL, 10);
			memcpy(p_storaddr,&temp.ui4,sizeoftype(UI4));
			return 0;		
		case UI8:
			temp.ui8 = strtoull((const char*)p_inputstr, NULL, 10);
			memcpy(p_storaddr,&temp.ui8,sizeoftype(UI8));
			return 0;		
		case F4:
			temp.f4 = strtof((const char*)p_inputstr, NULL);
			memcpy(p_storaddr,&temp.f4,sizeoftype(F4));
			return 0;
		case F8:
			temp.f8 = strtod((const char*)p_inputstr, NULL);
			memcpy(p_storaddr,&temp.f8,sizeoftype(F8));
			return 0;
		case STR:
			temp.str = new string;
			temp.str->assign(p_inputstr);
			memcpy(p_storaddr,&temp.str,sizeoftype(STR));
			return 0;
	}
	return 1;
}

void TABLE::gotoxy(uint32_t x, uint32_t y)
{
	printf("\033[%d;%dH", ++y, ++x);//原点为（0，0）
    return;
}

void TABLE::debug_ram_inf_print(int y){
	//打印TABLE内部所有内存数据信息
	gotoxy(0,y);
}

