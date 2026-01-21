#include "tblh.h"

TABLE* create_table(int* types, mstring* field_names, size_t field_num, mstring table_name){
    //先验证参数是否合法
    if(types==NULL || field_names==NULL || field_num==0 || table_name==NULL){
        return NULL;
    }
    
    //申请表结构内存
    TABLE* table = (TABLE*)malloc(sizeof(TABLE));
    if(table==NULL){
        return NULL;
    }
    
    table->name = table_name;//所有权转移 初始化表名
    
    //申请字段数组内存
    table->field = (FIELD*)malloc(sizeof(FIELD)*field_num);
    if(table->field==NULL){
        free(table->name);//所有权转移
        free(table);
        return NULL;
    }
    
    //初始化表的基本信息
    table->field_num = field_num;
    table->line_num = 0;//当前0行
    table->capacity = INCREASE_LINES_NUM;//预分配容量
    
    //申请索引数组内存
    table->logic_index = (size_t*)malloc(sizeof(size_t) * INCREASE_LINES_NUM);
    if(table->logic_index == NULL){
        free(table->field);
        free(table->name);
        free(table);
        return NULL;
    }
    
    //申请反向索引数组内存
    table->memory_index = (size_t*)malloc(sizeof(size_t) * INCREASE_LINES_NUM);
    if(table->memory_index == NULL){
        free(table->logic_index);
        free(table->field);
        free(table->name);
        free(table);
        return NULL;
    }
    
    //初始化每个字段，并预分配INCREASE_LINES_NUM行的内存
    for(size_t i=0; i<field_num; i++){
        table->field[i].type = types[i];
        table->field[i].name = field_names[i];//所有权转移
        table->field[i].column_index = i;
        
        //分配内存
        void* ptr = malloc(sizeof(Obj) * INCREASE_LINES_NUM);
        if(ptr == NULL){
            //如果分配失败，释放之前已分配的内存
            for(size_t j=0; j<i; j++){
                free(table->field[j].data);//union所有成员共享地址，释放任意一个即可
                free(table->field[j].name);//所有权转移
            }
            free(table->memory_index);
            free(table->logic_index);
            free(table->field);
            free(table->name);
            free(table);
            return NULL;
        }        
        table->field[i].data = (Obj*)ptr;
    }
    
    //初始化索引数组和反向索引数组，初始时逻辑顺序=物理顺序
    for(size_t i=0; i<INCREASE_LINES_NUM; i++){
        table->logic_index[i] = i;
        table->memory_index[i] = i;
    }
    return table;
}


int add_record(TABLE* table, Obj* values, size_t num){
    //参数验证 检查列数不能超过字段数
    if(table==NULL || values==NULL ||num > table->field_num){
        return -1;
    }
    
    //检查是否需要扩容
    if(table->line_num >= table->capacity){
        size_t new_capacity = table->capacity + INCREASE_LINES_NUM;

        //扩容索引数组
        size_t* temp_index = (size_t*)realloc(table->logic_index, sizeof(size_t) * new_capacity);
        if(temp_index == NULL){
            return -1;//扩容失败
        }
        table->logic_index = temp_index;
        
        //扩容反向索引数组
        size_t* temp_reverse = (size_t*)realloc(table->memory_index, sizeof(size_t) * new_capacity);
        if(temp_reverse == NULL){
            return -1;//扩容失败
        }
        table->memory_index = temp_reverse;
        
        //初始化新增的索引部分
        for(size_t i=table->capacity; i<new_capacity; i++){
            table->logic_index[i] = i;
            table->memory_index[i] = i;
        }
        
        //对每个字段进行扩容
        for(size_t i=0; i<table->field_num; i++){            
            //union所有成员共享地址，realloc使用任意一个即可
            void* temp = realloc(table->field[i].data, sizeof(Obj) * new_capacity);
            if(temp == NULL){
                //扩容失败，但原数据仍然有效
                return -1;
            }            
            //根据类型赋值给对应的union成员
            table->field[i].data = (Obj*)temp;
        }
        table->capacity = new_capacity;
    }
    
    //插入数据到当前行
    size_t current_line = table->line_num;
    for(size_t i=0; i<table->field_num; i++){
        if(i < num){
            //使用用户提供的值
            table->field[i].data[current_line] = values[i];
        }else{
            //超出num的部分赋值为NULL
            table->field[i].data[current_line] = NULL;
        }
    }
    
    //更新索引和反向索引
    table->logic_index[table->line_num] = current_line;//新的逻辑行指向新的物理行
    table->memory_index[current_line] = table->line_num;//新的物理行对应新的逻辑行
    
    table->line_num++;
    return 0;//成功
}


int rm_record(TABLE* table, size_t logic_index){
    //参数验证 检查索引是否有效
    if(table == NULL || logic_index >= table->line_num){
        return -1;
    }   
    //获取要删除的物理行号和最后一个物理行号进行替换
    size_t physical_to_delete = table->logic_index[logic_index];
    size_t last_physical = table->logic_index[table->line_num - 1];
    
    //如果删除的不是最后一行，用最后一行的数据覆盖被删除行
    if(physical_to_delete != last_physical){
        //复制数据
        for(size_t i=0; i<table->field_num; i++){
            table->field[i].data[physical_to_delete] = table->field[i].data[last_physical];
        }
        //更新原本指向last_physical的逻辑行，让它指向physical_to_delete
        size_t logic_of_last = table->memory_index[last_physical];
        table->logic_index[logic_of_last] = physical_to_delete;
        table->memory_index[physical_to_delete] = logic_of_last;
    }
    
    //删除逻辑行：将后面的索引前移
    for(size_t i=logic_index; i<table->line_num-1; i++){
        table->logic_index[i] = table->logic_index[i+1];
        //同时更新反向索引
        table->memory_index[table->logic_index[i]] = i;
    }
    table->line_num--;//减少行数
    return 0;//成功
}


int rm_field(TABLE* table, size_t field_index){
    //参数验证 检查索引是否有效
    if(table == NULL || field_index >= table->field_num){
        return -1;
    }
    
    //释放要删除字段的内存
    free(table->field[field_index].data);
    free(table->field[field_index].name);//所有权转移，需要释放
    
    //将后面的字段前移，覆盖被删除的字段
    for(size_t i=field_index; i<table->field_num-1; i++){
        table->field[i] = table->field[i+1];
        //更新字段的column_index
        table->field[i].column_index = i;
    }
    //减少字段数
    table->field_num--;
    
    //如果字段数为0，释放字段数组
    if(table->field_num == 0){
        free(table->field);
        table->field = NULL;
    }else{
        //缩减字段数组内存（可选，遵循"用多少申请多少"原则）
        FIELD* temp = (FIELD*)realloc(table->field, sizeof(FIELD) * table->field_num);
        if(temp != NULL){
            table->field = temp;
        }
        //如果realloc失败，原内存仍然有效，继续使用
    }
    return 0;//成功
}


int add_field(TABLE* table, int type, mstring field_name){
    if(table == NULL || field_name == NULL){
        return -1;//参数验证
    }
    //扩展字段数组
    size_t new_field_num = table->field_num + 1;
    FIELD* temp = (FIELD*)realloc(table->field, sizeof(FIELD) * new_field_num);
    if(temp == NULL){
        return -1;//扩容失败
    }
    table->field = temp;
    
    //初始化新字段
    size_t new_index = table->field_num;
    table->field[new_index].type = type;
    table->field[new_index].name = field_name;//所有权转移
    table->field[new_index].column_index = new_index;
    
    //为新字段分配数据区内存
    void* ptr = malloc(sizeof(Obj) * table->capacity);
    if(ptr == NULL){
        //分配失败，恢复field_num（field数组已扩展但可以不用）
        return -1;
    }
    table->field[new_index].data = (Obj*)ptr;
    
    //初始化新字段的所有行为NULL
    for(size_t i=0; i<table->line_num; i++){
        size_t physical_line = table->logic_index[i];
        table->field[new_index].data[physical_line] = NULL;
    }
    //更新字段数
    table->field_num = new_field_num;
    return 0;//成功
}


int swap_record(TABLE* table, size_t logic_index1, size_t logic_index2){
    if(table == NULL || logic_index1 >= table->line_num || logic_index2 >= table->line_num){
        return -1;//参数验证
    }
    //交换逻辑索引数组中的映射
    size_t temp = table->logic_index[logic_index1];
    table->logic_index[logic_index1] = table->logic_index[logic_index2];
    table->logic_index[logic_index2] = temp;
    //更新反向索引
    table->memory_index[table->logic_index[logic_index1]] = logic_index1;
    table->memory_index[table->logic_index[logic_index2]] = logic_index2;
    return 0;
}


int swap_field(TABLE* table, size_t field_index1, size_t field_index2){
    if(table == NULL || field_index1 >= table->field_num || field_index2 >= table->field_num){
        return -1;
    }
    //交换两个字段的所有内容
    FIELD temp = table->field[field_index1];
    table->field[field_index1] = table->field[field_index2];
    table->field[field_index2] = temp;
    //更新交换后字段的column_index
    table->field[field_index1].column_index = field_index1;
    table->field[field_index2].column_index = field_index2;
    return 0;
}

Obj get_value(TABLE* table, size_t idx_x, size_t idx_y){
    if(table == NULL || idx_x >= table->line_num || idx_y >= table->field_num){
        return NULL;
    }
    return table->field[idx_y].data[table->logic_index[idx_x]];
}

int set_value(TABLE* table, size_t idx_x, size_t idx_y, Obj content){
    if(table == NULL || idx_x >= table->line_num || idx_y >= table->field_num){
        return -1;
    }
    table->field[idx_y].data[table->logic_index[idx_x]] = content;
    return 0;
}

size_t get_field_index(TABLE* table, char* field_name, size_t len){
    if(table == NULL || field_name == NULL || len == 0){
        return FIELD_NOT_FOUND;
    }
    for(size_t i=0; i<table->field_num; i++){
        mstring name = table->field[i].name;
        if(*(size_t*)name != len) continue;
        if(memcmp(name + sizeof(size_t), field_name, len) == 0){
            return i;
        }
    }
    return FIELD_NOT_FOUND;
}

void free_table(TABLE* table){
    if(table == NULL) return;

    //释放所有字段的内存
    if(table->field != NULL){
        for(size_t i=0; i<table->field_num; i++){
            //释放字段数据区
            if(table->field[i].data != NULL){
                free(table->field[i].data);
            }
            //释放字段名（所有权转移）
            if(table->field[i].name != NULL){
                free(table->field[i].name);
            }
        }
        free(table->field);
    }
    //释放索引数组
    if(table->logic_index != NULL){
        free(table->logic_index);
    }
    if(table->memory_index != NULL){
        free(table->memory_index);
    }
    //释放表名（所有权转移）
    if(table->name != NULL){
        free(table->name);
    }
    //释放表结构本身
    free(table);
}

//清空表中所有数据但保留结构
void clear_table(TABLE* table){
    if(table == NULL) return;
    //缩减到初始容量
    if(table->capacity > INCREASE_LINES_NUM){
        //缩减索引数组
        size_t* temp_logic = (size_t*)realloc(table->logic_index, sizeof(size_t) * INCREASE_LINES_NUM);
        if(temp_logic != NULL){
            table->logic_index = temp_logic;
        }
        size_t* temp_memory = (size_t*)realloc(table->memory_index, sizeof(size_t) * INCREASE_LINES_NUM);
        if(temp_memory != NULL){
            table->memory_index = temp_memory;
        }
        //缩减每个字段的数据区
        for(size_t i=0; i<table->field_num; i++){
            void* temp = realloc(table->field[i].data, sizeof(Obj) * INCREASE_LINES_NUM);
            if(temp != NULL){
                table->field[i].data = (Obj*)temp;
            }
        }
        table->capacity = INCREASE_LINES_NUM;
    }
    
    //初始化所有数据为NULL
    for(size_t i=0; i<table->field_num; i++){
        for(size_t j=0; j<table->capacity; j++){
            table->field[i].data[j] = NULL;
        }
    }
    table->line_num = 0;
    //初始化索引数组
    for(size_t i=0; i<table->capacity; i++){
        table->logic_index[i] = i;
        table->memory_index[i] = i;
    }
}

size_t get_record_count(TABLE* table){
    if(table == NULL) return 0;
    return table->line_num;
}

size_t get_field_count(TABLE* table){
    if(table == NULL) return 0;
    return table->field_num;
}

int update_record(TABLE* table, size_t logic_index, Obj* values, size_t num){
    if(table == NULL || values == NULL || logic_index >= table->line_num || num > table->field_num){
        return -1;
    }
    size_t physical_line = table->logic_index[logic_index];
    for(size_t i=0; i<table->field_num; i++){
        if(i < num){
            //使用用户提供的值（所有权转移）
            table->field[i].data[physical_line] = values[i];
        }
    }
    return 0;//成功
}

Obj* get_record(TABLE* table, size_t logic_index){
    if(table == NULL || logic_index >= table->line_num){
        return NULL;
    }
    Obj* record = (Obj*)malloc(sizeof(Obj) * table->field_num);
    if(record == NULL){
        return NULL;
    }
    size_t physical_line = table->logic_index[logic_index];
    for(size_t i=0; i<table->field_num; i++){
        record[i] = table->field[i].data[physical_line];
    }
    return record;
}
