/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.1
Email:hj18914255909@outlook.com
*/
#include "tblh.hpp"

#define TENTATIVE 0
#define _max_(a,b) ((a)>(b)?(a):(b))

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

void initFIELD(FIELD* field,const char* field_name, char type){
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