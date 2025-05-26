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

int8_t TABLE::reset_field_key_type(uint32_t i,char key_type)
{
	if(!isvalid_keytype(key_type)) return merr;//检查i,key_type是否合法
	if(i>=this->field_num) return merr;

	if(key_type!=PRIMARY_KEY){
		if(i!=this->primary_key_i)//如果操作的不是当前主键且修改过后也不是主键，那就直接修改
		{	
			this->p_field[i].key_type=key_type;
		}
		else//把当前主键修改为非主键
		{
			this->primary_key_i=NOPKEY;//没有主键了
			this->p_field[i].key_type=key_type;
		}
	}
	else{
		if(this->primary_key_i==NOPKEY){
			this->primary_key_i=i;
			this->p_field[i].key_type=key_type;
		}
		else{
			printf("warning:table already has a primary key!");
		}
	}
	return 0;	
}


int8_t TABLE::insert_field(FIELD* field, uint32_t i)
{
	uint32_t sep = this->add_field(field);
	if (i >= sep) return merr;//如果想插入的位置i比末尾的序号还大或等，那就不叫插入，而是叫附加

	FIELD cache = this->p_field[sep];
	uint32_t cache_offset = this->field_offset.get_index(sep);
	FIELD* cc_p_head = this->p_field;
	for (uint32_t k = sep; k > i; cc_p_head[k] = cc_p_head[k - 1], this->field_offset.set_index(k, this->field_offset.get_index(k - 1)), k--);
	this->p_field[i] = cache;
	this->field_offset.set_index(i, cache_offset);
	return 0;
}

int8_t TABLE::rmv_field(uint32_t i)
{
	if (i >= this->field_num) return merr;//判断i是否合法
	if (i != this->field_num - 1) {
		this->idle_map = (IDLE_MAP*)realloc(this->idle_map, ++this->map_size * sizeof(IDLE_MAP));//map_size++;
		if(this->idle_map == NULL) return merr;
		this->idle_map[this->map_size - 1].idle_size = sizeoftype(this->p_field[i].type);//获得删除字段的大小
		this->idle_map[this->map_size - 1].idle_offset = this->field_offset.get_index(i);//得到空闲位置的偏移量
	}
	else this->record_usage -= sizeoftype(this->p_field[i].type);//直接修改record_usage

	uint32_t cc_table_record_length = this->record_length;//缓存变量cache
	uint8_t* cc_table_p_a_i = this->p_data + this->field_offset.get_index(i);
	uint32_t cc_clean_size = sizeoftype(this->p_field[i].type);

	if(this->p_field[i].type==STR){
		for (uint32_t j = 0; j < this->record_num; j++) {
			string* temp;
			memcpy((uint8_t*)&temp, cc_table_p_a_i + cc_table_record_length * j, sizeoftype(STR));
			delete temp;
			memset(cc_table_p_a_i + cc_table_record_length * j, 0, cc_clean_size);
		}
	}
	else{
		for (uint32_t j = 0; j < this->record_num; j++) {
			memset(cc_table_p_a_i + cc_table_record_length * j, 0, cc_clean_size);
		}
	}

	this->field_num--;//字段数-1
	if (i != this->field_num){
		for (uint32_t ii = i; ii < this->field_num;
		this->p_field[ii] = this->p_field[ii + 1],this->field_offset.set_index(ii, this->field_offset.get_index(ii + 1)),ii++);//索引进位
	}
	this->field_offset.rm_index(this->field_num); // 删除最后一个
	return 0;
}

uint32_t TABLE::add_field(FIELD* field)
{
	FIELD* p_field_new = (FIELD*)realloc(this->p_field, (this->field_num+1) * sizeof(FIELD));
	if(!p_field_new){ 
		printf("TABLE::add_field:p_field realloc error");
		return merr;
	}
	this->p_field = p_field_new;
	this->p_field[this->field_num++] = field[0];
	// offsetofield扩容由UintDeque自动管理
	uint32_t new_field_size = sizeoftype(field->type);
	uint32_t new_offset = this->record_usage;//默认偏移量
	uint32_t cc_map_size = this->map_size;
	IDLE_MAP* cc_idle_map = this->idle_map;

	uint32_t idle_seq = 0;//记录之后填入的空位的序号
	for (uint32_t k = 0; k < cc_map_size; k++) {
		if (cc_idle_map[k].idle_size == new_field_size) {
			new_offset = cc_idle_map[k].idle_offset;
			idle_seq = k;
			break;
		}
		if (cc_idle_map[k].idle_size > new_field_size && new_offset > cc_idle_map[k].idle_offset) {
			new_offset = cc_idle_map[k].idle_offset;
			idle_seq = k;
		}
	}
	this->field_offset.rpush(new_offset);//得到偏移量了

	if (new_offset == this->record_usage){
		if (this->record_length - this->record_usage <= new_field_size){
			uint32_t old_record_length = this->record_length;
			this->record_length += record_add_ROM;
			uint8_t* new_p_data = (uint8_t*)realloc(this->p_data, this->record_length * this->data_ROM);
			if(new_p_data==NULL){
				this->field_num--;
				printf("p_data realloc error");
				return merr;
			}
			this->p_data=new_p_data;
			if (this->record_num !=0){
				for (uint32_t k = this->record_num - 1; k > 0; k--) {
					memmove(this->p_data + this->record_length * k, this->p_data + old_record_length * k, this->record_usage);
				}
			}
		}
		this->record_usage += new_field_size;
		uint32_t cc_table_record_length = this->record_length;
		uint8_t* cc_table_p_a_i = this->p_data + this->field_offset.get_index(this->field_num - 1);
		uint32_t cc_clean_size = new_field_size;
		for (uint32_t j = 0; j < this->record_num; j++) {
			memset(cc_table_p_a_i + cc_table_record_length * j, 0, cc_clean_size);
		}
	}
	else {
		this->idle_map[idle_seq].idle_size -= new_field_size;
		if (this->idle_map[idle_seq].idle_size==0)
			this->idle_map[idle_seq] = this->idle_map[--this->map_size];
		else
			this->idle_map[idle_seq].idle_offset += new_field_size;
	}
	return this->field_num - 1;
}

int8_t TABLE::swap_field(uint32_t i1_, uint32_t i2_)
{
	if (i1_ >= this->field_num || i2_ >= this->field_num) return merr;
	FIELD cache = this->p_field[i1_];
	this->p_field[i1_] = this->p_field[i2_];
	this->p_field[i2_] = cache;
	uint32_t cache_offset = this->field_offset.get_index(i1_);
	this->field_offset.set_index(i1_, this->field_offset.get_index(i2_));
	this->field_offset.set_index(i2_, cache_offset);
	return 0;
}