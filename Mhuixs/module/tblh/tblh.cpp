/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.1
Email:hj18914255909@outlook.com
*/
#include "tblh.hpp"

#define TENTATIVE 0
#define _max_( a , b ) ( ( a ) > ( b ) ? ( a ) : ( b ) )

TABLE::TABLE(str* table_name, FIELD* field, uint32_t field_num):
p_field(NULL),field_num(field_num),field_offset(),idle_map(NULL),
map_size(0),/*map_size是真实大小，不是数组中括号中的最大值*/p_data(NULL),
record_length(TENTATIVE),record_usage(TENTATIVE),record_num(0),line_index(),
data_ROM(begin_ROM),/*/TABLE数据区record条数初始容量为200条数*/
primary_key_i(NOPKEY),state(0)
{
	this->table_name=(table_name->string,table_name->len);//初始化table_name
	//检查field是否合法
	int if_exit_primary_key=0;//一个表只能存在一个主键,可以没有主键
	for(int i=0;i<field_num;i++){
		if(field[i].key_type==PRIMARY_KEY){
			if(if_exit_primary_key==1){
				state++;
				#ifdef tblh_debug
				printf("TABLE init err:PRIMARY_KEY is not unique!\n");
				#endif
				return;
			}
			if_exit_primary_key++;
			this->primary_key_i=i;
		}
	}
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
	this->idle_map = (IDLE_MAP*)malloc(sizeof(IDLE_MAP));//记录使用区的空位内存地图
	this->p_data = (uint8_t*)calloc(1,this->record_length * begin_ROM);//创建TABLE数据区
	if(!this->p_field+!this->idle_map+!this->p_data){
		free(this->p_field);
		free(this->idle_map);
		free(this->p_data);
		#ifdef tblh_debug
		printf("TABLE init err:calloc error\n");
		#endif
		state++;
		return;
	}
	//将字段的偏移量存入offsetofield队列中
	uint32_t ofsum = 0;
	for (uint32_t i = 0; i < field_num;this->p_field[i] = field[i],this->field_offset.rpush(ofsum),ofsum += sizeoftype(field[i].type),i++);
	return;
}

void TABLE::print_record(uint32_t j, uint32_t y){
	//打印第j条记录,在y行打印
	if (j >= this->record_num) {
		#ifdef tblh_debug
		printf("TABLE print_record err:record %d is not exist!\n",j);
		#endif
		return;
	}
	//初始化ptfmap得到打印地图
	uint32_t* ptfmap = (uint32_t*)malloc(sizeof(uint32_t) * this->field_num);
	uint32_t address = 0;
	for (uint32_t i = 0; i < this->field_num; i++) {
		ptfmap[i] = address;
		address += _max_(ptfsizeoftype(this->p_field[i].type), this->p_field[i].length);
	}
	temp_mem temp;
	for (uint32_t i = 0; i < this->field_num; i++) {
			gotoxy(ptfmap[i], y);//定位到打印位置			
			uint8_t* inf_addr = real_addr_of_lindex(j)+this->field_offset.get_index(i);
			memcpy(&temp, inf_addr, sizeoftype(this->p_field[i].type));
			switch (this->p_field[i].type) {
				case I1:printf("%d",temp.i1);break;
				case I2:printf("%d",temp.i2);break;
				case I4:case DATE:case TIME:
				if(this->p_field[i].type==I4) printf("%d", temp.i4);
				else if(this->p_field[i].type==DATE)printf("%d.%d.%d",temp.i4/10000, (temp.i4 / 100) % 100,temp.i4%100);
				else if(this->p_field[i].type==TIME)printf("%d:%d:%d",temp.i4/10000, (temp.i4 / 100) % 100,temp.i4%100); 
				break;
				case I8:printf("%lld", temp.i8);break;
				case UI1:printf("%d", temp.ui1);break;
				case UI2:printf("%d", temp.ui2);break;
				case UI4:printf("%d", temp.ui4);break; 
				case UI8:printf("%llu", temp.ui8);break;
				case F4:printf("%.7g", temp.f4);break;
				case F8:printf("%.15g", temp.f8);break;
				case STR://printf("%d",temp.str);break;
				if(temp.str==NULL){
					printf("NULL");break;
				}
				cout<<*temp.str;break;
				printf("%.10s", temp.str->c_str());printf("OK!");break;
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
		address += _max_(ptfsizeoftype(this->p_field[i].type), this->p_field[i].length);
	}
	for (uint32_t i = 0; i < this->field_num;i++){
		gotoxy(ptfmap[i], y);
		printf("%s", this->p_field[i].name);//打印字段名
	}
	y++;//换行
	for(uint32_t j = 0; j < this->record_num;this->print_record(j, y),y++,j++);//打印数据
	free(ptfmap);
}

TABLE::~TABLE(){
	//删除所有STR类型的字段
	for (uint32_t i = 0; i < this->field_num; i++) {
		if (this->p_field[i].type == STR) {
			this->rmv_field(i);
		}
	}
	//删除表，此时表必须make或者load之后才能使用
	this->field_num=0,this->map_size=0,this->record_length=0,this->record_usage=0,
	this->record_num=0,this->data_ROM=0,this->state=0;
	free(this->p_field);
	free(this->p_data);
	free(this->idle_map);
	this->p_field=NULL,this->p_data=NULL,this->idle_map=NULL;
	this->field_offset.clear();
	this->line_index.clear();
	return;
}

void TABLE::initFIELD(FIELD* field,const char* field_name, char type,char key_type)
{
	if(!isvalid_type(type) || !isvalid_keytype(key_type)){
		printf("initFIELD:illegal type err!");
		return;
	}
	field->type = type;
	field->key_type = key_type;
	memset(field->name, 0, format_name_length);//先归0
	memcpy(field->name, field_name, strlen(field_name) % format_name_length);
	return;
}

// TABLE拷贝构造函数
TABLE::TABLE(const TABLE& other)
    : p_field(nullptr), field_num(other.field_num), field_offset(other.field_offset),
      idle_map(nullptr), map_size(other.map_size), p_data(nullptr),
      record_length(other.record_length), record_usage(other.record_usage),
      record_num(other.record_num), line_index(other.line_index),
      data_ROM(other.data_ROM), primary_key_i(other.primary_key_i),
      table_name(other.table_name), state(other.state)
{
    // 拷贝字段区
    if (other.p_field && other.field_num > 0) {
        p_field = (FIELD*)calloc(other.field_num, sizeof(FIELD));
        for (uint32_t i = 0; i < other.field_num; ++i) {
            p_field[i] = other.p_field[i];
        }
    }
    // 拷贝空位内存地图
    if (other.idle_map && other.map_size > 0) {
        idle_map = (IDLE_MAP*)malloc(other.map_size * sizeof(IDLE_MAP));
        memcpy(idle_map, other.idle_map, other.map_size * sizeof(IDLE_MAP));
    }
    // 拷贝数据区
    if (other.p_data && other.data_ROM > 0 && other.record_length > 0) {
        p_data = (uint8_t*)calloc(1, other.record_length * other.data_ROM);
        memcpy(p_data, other.p_data, other.record_length * other.data_ROM);
        // 深拷贝STR类型字段内容
        for (uint32_t i = 0; i < other.field_num; ++i) {
            if (other.p_field[i].type == STR) {
                for (uint32_t j = 0; j < other.record_num; ++j) {
                    // 这里去除const限定
                    UintDeque& other_line_index = const_cast<UintDeque&>(other.line_index);
                    UintDeque& this_line_index = line_index;
                    UintDeque& other_field_offset = const_cast<UintDeque&>(other.field_offset);
                    UintDeque& this_field_offset = field_offset;
                    uint8_t* src_addr = other.p_data + other_line_index.get_index(j) * other.record_length + other_field_offset.get_index(i);
                    uint8_t* dst_addr = p_data + this_line_index.get_index(j) * record_length + this_field_offset.get_index(i);
                    string* src_str = nullptr;
                    memcpy(&src_str, src_addr, sizeof(string*));
                    if (src_str) {
                        string* dst_str = new string(*src_str);
                        memcpy(dst_addr, &dst_str, sizeof(string*));
                    } else {
                        string* dst_str = nullptr;
                        memcpy(dst_addr, &dst_str, sizeof(string*));
                    }
                }
            }
        }
    }
}

// TABLE赋值操作符
TABLE& TABLE::operator=(const TABLE& other)
{
    if (this == &other) return *this;
    // 先释放自身资源
    if (p_field) free(p_field);
    if (idle_map) free(idle_map);
    if (p_data) {
        // 释放STR类型字段内容
        for (uint32_t i = 0; i < field_num; ++i) {
            if (p_field && p_field[i].type == STR) {
                for (uint32_t j = 0; j < record_num; ++j) {
                    UintDeque& this_line_index = line_index;
                    UintDeque& this_field_offset = field_offset;
                    uint8_t* addr = p_data + this_line_index.get_index(j) * record_length + this_field_offset.get_index(i);
                    string* temp = nullptr;
                    memcpy(&temp, addr, sizeof(string*));
                    delete temp;
                }
            }
        }
        free(p_data);
    }
    // 拷贝基本成员
    field_num = other.field_num;
    field_offset = other.field_offset;
    map_size = other.map_size;
    record_length = other.record_length;
    record_usage = other.record_usage;
    record_num = other.record_num;
    line_index = other.line_index;
    data_ROM = other.data_ROM;
    primary_key_i = other.primary_key_i;
    table_name = other.table_name;
    state = other.state;
    // 拷贝字段区
    p_field = nullptr;
    if (other.p_field && other.field_num > 0) {
        p_field = (FIELD*)calloc(other.field_num, sizeof(FIELD));
        for (uint32_t i = 0; i < other.field_num; ++i) {
            p_field[i] = other.p_field[i];
        }
    }
    // 拷贝空位内存地图
    idle_map = nullptr;
    if (other.idle_map && other.map_size > 0) {
        idle_map = (IDLE_MAP*)malloc(other.map_size * sizeof(IDLE_MAP));
        memcpy(idle_map, other.idle_map, other.map_size * sizeof(IDLE_MAP));
    }
    // 拷贝数据区
    p_data = nullptr;
    if (other.p_data && other.data_ROM > 0 && other.record_length > 0) {
        p_data = (uint8_t*)calloc(1, other.record_length * other.data_ROM);
        memcpy(p_data, other.p_data, other.record_length * other.data_ROM);
        // 深拷贝STR类型字段内容
        for (uint32_t i = 0; i < other.field_num; ++i) {
            if (other.p_field[i].type == STR) {
                for (uint32_t j = 0; j < other.record_num; ++j) {
                    UintDeque& other_line_index = const_cast<UintDeque&>(other.line_index);
                    UintDeque& this_line_index = line_index;
                    UintDeque& other_field_offset = const_cast<UintDeque&>(other.field_offset);
                    UintDeque& this_field_offset = field_offset;
                    uint8_t* src_addr = other.p_data + other_line_index.get_index(j) * other.record_length + other_field_offset.get_index(i);
                    uint8_t* dst_addr = p_data + this_line_index.get_index(j) * record_length + this_field_offset.get_index(i);
                    string* src_str = nullptr;
                    memcpy(&src_str, src_addr, sizeof(string*));
                    if (src_str) {
                        string* dst_str = new string(*src_str);
                        memcpy(dst_addr, &dst_str, sizeof(string*));
                    } else {
                        string* dst_str = nullptr;
                        memcpy(dst_addr, &dst_str, sizeof(string*));
                    }
                }
            }
        }
    }
    return *this;
}

