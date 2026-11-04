#include "tblh.h"

/* 列索引的内部辅助（定义在文件末尾） */
static void table_index_clear(TABLE* table);
static void table_index_note_insert(TABLE* table, size_t logical_row);
static void table_index_note_remove(TABLE* table, size_t logical_row);
static void table_index_note_set(TABLE* table, size_t logical_row, size_t field);
static void table_index_note_swap(TABLE* table, size_t logic_index1, size_t logic_index2);

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
    table->idx = NULL;
    table->idx_num = 0;
    table->idx_cap = 0;
    
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
        /* 必须清零：单元格要么是有效 Obj 要么是 NULL。
         * 用 malloc 会让未使用的行留下野指针，
         * 而 free_table / table_copy 会遍历整个 capacity 区域。 */
        void* ptr = calloc(INCREASE_LINES_NUM, sizeof(Obj));
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


/*
深拷贝一张表：表名、字段（名字/类型/数据区）、索引数组，以及每个单元格的值。

注意：单元格值是深拷贝的（每个非 NULL 的 Obj 都会复制成新的）。
单元格里若装的是 LIST，BHS 层的 list_copy 也是深拷贝，整棵结构独立。

已知限制：若表内出现自引用（表套自己），深拷贝会无限递归。
当前没有产生这种结构的路径，先记录在此。

失败返回 NULL，且不泄漏已分配的部分。
*/
TABLE* table_copy(const TABLE* other){
    if(other == NULL) return NULL;

    TABLE* table = (TABLE*)malloc(sizeof(TABLE));
    if(table == NULL) return NULL;

    /* 全部置空，失败时统一走 free_table 清理，不会碰到野指针 */
    table->name = NULL;
    table->field = NULL;
    table->field_num = 0;
    table->line_num = 0;
    table->capacity = 0;
    table->logic_index = NULL;
    table->memory_index = NULL;
    table->idx = NULL;
    table->idx_num = 0;
    table->idx_cap = 0;
    /* 列索引不随深拷贝带走：副本从没有索引的状态开始，
     * 调用方需要时自己重建，避免复制一份可能已经 stale 的排序数组 */

    if(other->name != NULL){
        table->name = mstr_copy(other->name);
        if(table->name == NULL) goto fail;
    }

    table->field_num = other->field_num;
    table->capacity = other->capacity;
    table->line_num = other->line_num;

    if(other->field_num > 0){
        table->field = (FIELD*)calloc(other->field_num, sizeof(FIELD));
        if(table->field == NULL) goto fail;
    }

    for(size_t i=0; i<other->field_num; i++){
        const FIELD* src = &other->field[i];
        FIELD* dst = &table->field[i];

        dst->column_index = src->column_index;
        dst->type = src->type;
        dst->name = NULL;
        dst->data = NULL;

        if(src->name != NULL){
            dst->name = mstr_copy(src->name);
            if(dst->name == NULL) goto fail;
        }

        /* 数据区按 capacity 分配，逐格深拷贝 */
        dst->data = (Obj*)calloc(other->capacity, sizeof(Obj));
        if(dst->data == NULL) goto fail;

        for(size_t r=0; r<other->capacity; r++){
            Obj v = (src->data != NULL) ? src->data[r] : NULL;
            if(v == NULL) continue;

            Obj copy = bignum_create();
            if(copy == NULL) goto fail;
            if(bignum_copy(v, copy) != 0){
                bignum_destroy(copy);
                goto fail;
            }
            dst->data[r] = copy;
        }
    }

    if(other->capacity > 0){
        table->logic_index = (size_t*)malloc(sizeof(size_t) * other->capacity);
        if(table->logic_index == NULL) goto fail;
        table->memory_index = (size_t*)malloc(sizeof(size_t) * other->capacity);
        if(table->memory_index == NULL) goto fail;

        memcpy(table->logic_index, other->logic_index, sizeof(size_t) * other->capacity);
        memcpy(table->memory_index, other->memory_index, sizeof(size_t) * other->capacity);
    }

    return table;

fail:
    free_table(table);
    return NULL;
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
            /* 扩容出来的新区域必须清零，否则这些行是野指针，
             * free_table / table_copy 遍历到会崩溃 */
            memset(table->field[i].data + table->capacity, 0,
                   sizeof(Obj) * (new_capacity - table->capacity));
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
    /* 索引按新增的逻辑行增量插入；失败时该索引会退回 stale */
    table_index_note_insert(table, current_line);
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

    /* 表拥有单元格的值，删除行时必须释放它们。
     * 若待删行就是最后一行，直接销毁即可；
     * 否则随后会把最后一行的值搬进这个槽位，所以也要先销毁。 */
    for(size_t i=0; i<table->field_num; i++){
        Obj* slot = &table->field[i].data[physical_to_delete];
        if(*slot != NULL){
            bignum_destroy(*slot);
            *slot = NULL;
        }
    }

    //如果删除的不是最后一行，用最后一行的数据覆盖被删除行
    if(physical_to_delete != last_physical){
        //复制数据
        for(size_t i=0; i<table->field_num; i++){
            table->field[i].data[physical_to_delete] = table->field[i].data[last_physical];
            table->field[i].data[last_physical] = NULL;  /* 避免同一指针存在两份 */
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
    /* 索引里存的是逻辑行号：删掉被删行，后面的行号整体前移一位 */
    table_index_note_remove(table, logic_index);
    table->line_num--;//减少行数
    return 0;//成功
}


int rm_field(TABLE* table, size_t field_index){
    //参数验证 检查索引是否有效
    if(table == NULL || field_index >= table->field_num){
        return -1;
    }
    
    //释放要删除字段的内存
    /* 先释放该字段所有单元格里的值（表拥有它们） */
    if(table->field[field_index].data != NULL){
        for(size_t r=0; r<table->capacity; r++){
            if(table->field[field_index].data[r] != NULL){
                bignum_destroy(table->field[field_index].data[r]);
            }
        }
    }
    free(table->field[field_index].data);
    table->field[field_index].data = NULL;
    free(table->field[field_index].name);//所有权转移，需要释放
    table->field[field_index].name = NULL;
    
    //将后面的字段前移，覆盖被删除的字段
    for(size_t i=field_index; i<table->field_num-1; i++){
        table->field[i] = table->field[i+1];
        //更新字段的column_index
        table->field[i].column_index = i;
    }
    //减少字段数
    table->field_num--;

    /* 列下标在删除后会整体前移，旧索引记录的 field 已不再对应原列，
     * 直接全部丢弃，比逐个修正可靠 */
    table_index_clear(table);

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
    /* 同样必须清零：新字段的所有行初始都是"空单元格" */
    void* ptr = calloc(table->capacity, sizeof(Obj));
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
    table_index_note_swap(table, logic_index1, logic_index2);
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
    table_index_clear(table);
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
    Obj* slot = &table->field[idx_y].data[table->logic_index[idx_x]];
    /* 表拥有旧值，覆盖前先销毁，否则泄漏 */
    if(*slot != NULL){
        bignum_destroy(*slot);
    }
    *slot = content;  /* 所有权转移 */
    /* 只有被改的那一列需要重新定位，其他列的索引仍然有效 */
    table_index_note_set(table, idx_x, idx_y);
    return 0;
}

size_t get_field_index(TABLE* table, char* field_name, size_t len){
    if(table == NULL || field_name == NULL || len == 0){
        return FIELD_NOT_FOUND;
    }
    for(size_t i=0; i<table->field_num; i++){
        mstring name = table->field[i].name;
        if(name == NULL) continue;
        /* 原来这里写的是 *(size_t*)name != len，
         * 但 mstring 头是 [uint32 len][uint32 cap8]，按 size_t 读会变成
         * (uint64_t)(len | cap8<<32)，永远不等于 len，导致字段永远找不到。 */
        if(mstrlen(name) != len) continue;
        if(memcmp(mstr_cstr(name), field_name, len) == 0){
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
            //释放字段数据区里的每个单元格值（表拥有它们的所有权）
            if(table->field[i].data != NULL){
                for(size_t r=0; r<table->capacity; r++){
                    if(table->field[i].data[r] != NULL){
                        bignum_destroy(table->field[i].data[r]);
                    }
                }
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
    table_index_clear(table);
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
    /* 先释放原有单元格的值，否则直接置 NULL 会泄漏 */
    for(size_t i=0; i<table->field_num; i++){
        if(table->field[i].data == NULL) continue;
        for(size_t j=0; j<table->capacity; j++){
            if(table->field[i].data[j] != NULL){
                bignum_destroy(table->field[i].data[j]);
                table->field[i].data[j] = NULL;
            }
        }
    }
    table->line_num = 0;
    //初始化索引数组
    for(size_t i=0; i<table->capacity; i++){
        table->logic_index[i] = i;
        table->memory_index[i] = i;
    }
    table_index_clear(table);
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
            Obj* slot = &table->field[i].data[physical_line];
            if(*slot != NULL){
                bignum_destroy(*slot);  /* 覆盖前销毁旧值 */
            }
            *slot = values[i];
        }
    }
    /* 每个被覆盖的列各维护自己的索引 */
    for(size_t i=0; i<num; i++) table_index_note_set(table, logic_index, i);
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

/* ---------------- 列索引 ---------------- */

static int index_cmp(TABLE* table, size_t field, size_t a, size_t b)
{
    return bignum_compare(get_value(table, a, field), get_value(table, b, field));
}

/* 归并排序。qsort 的 comparator 拿不到"按哪一列比较"的上下文，
 * 用全局变量在多线程下就错了，所以这里自己实现一个带上下文的版本。 */
static void merge_sort_rows(TABLE* table, size_t field,
                            size_t* a, size_t* tmp, size_t n)
{
    if (n < 2) return;

    size_t mid = n / 2;
    merge_sort_rows(table, field, a, tmp, mid);
    merge_sort_rows(table, field, a + mid, tmp, n - mid);

    size_t i = 0, j = mid, k = 0;
    while (i < mid && j < n) {
        if (index_cmp(table, field, a[i], a[j]) <= 0) tmp[k++] = a[i++];
        else                                         tmp[k++] = a[j++];
    }
    while (i < mid) tmp[k++] = a[i++];
    while (j < n)   tmp[k++] = a[j++];

    memcpy(a, tmp, sizeof(size_t) * n);
}

/* 找某列的索引槽位；create 时没有就追加一个空槽 */
static TABLE_INDEX* index_slot(TABLE* table, size_t field, int create)
{
    for (size_t i = 0; i < table->idx_num; i++)
        if (table->idx[i].field == field) return &table->idx[i];

    if (!create) return NULL;

    if (table->idx_num == table->idx_cap) {
        size_t ncap = table->idx_cap ? table->idx_cap * 2 : 4;
        TABLE_INDEX* ni = (TABLE_INDEX*)realloc(table->idx, sizeof(TABLE_INDEX) * ncap);
        if (ni == NULL) return NULL;
        table->idx = ni;
        table->idx_cap = ncap;
    }

    TABLE_INDEX* ix = &table->idx[table->idx_num++];
    ix->field = field;
    ix->order = NULL;
    ix->n = 0;
    ix->cap = 0;
    ix->stale = 0;
    return ix;
}

/* 确保 order 能放下 need 个逻辑行号；失败返回 -1 */
static int index_reserve(TABLE_INDEX* ix, size_t need)
{
    if (need <= ix->cap) return 0;

    size_t ncap = ix->cap ? ix->cap * 2 : 8;
    while (ncap < need) ncap *= 2;

    size_t* no = (size_t*)realloc(ix->order, sizeof(size_t) * ncap);
    if (no == NULL) return -1;

    ix->order = no;
    ix->cap = ncap;
    return 0;
}

/* 在有序数组里找第一个"值大于 row"的位置（相同值排在新行之后） */
static size_t index_upper_bound_row(TABLE* table, const TABLE_INDEX* ix,
                                    size_t field, size_t row)
{
    size_t lo = 0, hi = ix->n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (index_cmp(table, field, ix->order[mid], row) <= 0) lo = mid + 1;
        else                                                      hi = mid;
    }
    return lo;
}

/* 追加一行后，把新的逻辑行号插进每个索引 */
static void table_index_note_insert(TABLE* table, size_t logical_row)
{
    if (table == NULL || table->idx == NULL) return;

    for (size_t i = 0; i < table->idx_num; i++) {
        TABLE_INDEX* ix = &table->idx[i];
        if (ix->stale) continue;

        if (index_reserve(ix, ix->n + 1) != 0) { ix->stale = 1; continue; }

        size_t pos = index_upper_bound_row(table, ix, ix->field, logical_row);
        memmove(&ix->order[pos + 1], &ix->order[pos],
                sizeof(size_t) * (ix->n - pos));
        ix->order[pos] = logical_row;
        ix->n++;
    }
}

/* 删除一行后：移走该逻辑行号，后面的行号整体减一 */
static void table_index_note_remove(TABLE* table, size_t logical_row)
{
    if (table == NULL || table->idx == NULL) return;

    for (size_t i = 0; i < table->idx_num; i++) {
        TABLE_INDEX* ix = &table->idx[i];
        if (ix->stale) continue;

        size_t pos = ix->n;
        for (size_t k = 0; k < ix->n; k++) {
            if (ix->order[k] == logical_row) { pos = k; break; }
        }
        if (pos == ix->n) { ix->stale = 1; continue; }   /* 索引与表不一致 */

        memmove(&ix->order[pos], &ix->order[pos + 1],
                sizeof(size_t) * (ix->n - pos - 1));
        ix->n--;

        for (size_t k = 0; k < ix->n; k++)
            if (ix->order[k] > logical_row) ix->order[k]--;
    }
}

/* 改写某列的一个单元格后：只维护该列的索引 */
static void table_index_note_set(TABLE* table, size_t logical_row, size_t field)
{
    if (table == NULL || table->idx == NULL) return;

    TABLE_INDEX* ix = index_slot(table, field, 0);
    if (ix == NULL || ix->stale) return;

    size_t pos = ix->n;
    for (size_t k = 0; k < ix->n; k++) {
        if (ix->order[k] == logical_row) { pos = k; break; }
    }
    if (pos == ix->n) { ix->stale = 1; return; }

    memmove(&ix->order[pos], &ix->order[pos + 1],
            sizeof(size_t) * (ix->n - pos - 1));
    ix->n--;

    size_t ins = index_upper_bound_row(table, ix, field, logical_row);
    if (index_reserve(ix, ix->n + 1) != 0) { ix->stale = 1; return; }

    memmove(&ix->order[ins + 1], &ix->order[ins],
            sizeof(size_t) * (ix->n - ins));
    ix->order[ins] = logical_row;
    ix->n++;
}

/* 交换两行的内容后：索引里对应的两个行号互换位置 */
static void table_index_note_swap(TABLE* table, size_t logic_index1, size_t logic_index2)
{
    if (table == NULL || table->idx == NULL || logic_index1 == logic_index2) return;

    for (size_t i = 0; i < table->idx_num; i++) {
        TABLE_INDEX* ix = &table->idx[i];
        if (ix->stale) continue;

        size_t p1 = ix->n, p2 = ix->n;
        for (size_t k = 0; k < ix->n; k++) {
            if      (ix->order[k] == logic_index1) p1 = k;
            else if (ix->order[k] == logic_index2) p2 = k;
        }
        if (p1 == ix->n || p2 == ix->n) { ix->stale = 1; continue; }

        ix->order[p1] = logic_index2;
        ix->order[p2] = logic_index1;
    }
}

static void table_index_clear(TABLE* table)
{
    if (table == NULL) return;

    if (table->idx != NULL) {
        for (size_t i = 0; i < table->idx_num; i++) free(table->idx[i].order);
        free(table->idx);
    }
    table->idx = NULL;
    table->idx_num = 0;
    table->idx_cap = 0;
}

int table_index_build(TABLE* table, size_t field)
{
    if (table == NULL || field >= table->field_num) return -1;

    size_t n = table->line_num;
    size_t* order = NULL;
    size_t* tmp = NULL;

    if (n > 0) {
        order = (size_t*)malloc(sizeof(size_t) * n);
        if (order == NULL) return -1;

        for (size_t i = 0; i < n; i++) order[i] = i;

        if (n > 1) {
            tmp = (size_t*)malloc(sizeof(size_t) * n);
            if (tmp == NULL) { free(order); return -1; }
            merge_sort_rows(table, field, order, tmp, n);
            free(tmp);
        }
    }

    TABLE_INDEX* ix = index_slot(table, field, 1);
    if (ix == NULL) { free(order); return -1; }

    free(ix->order);
    ix->order = order;
    ix->n = n;
    ix->cap = n;
    ix->stale = 0;
    return 0;
}

int table_index_drop(TABLE* table, size_t field)
{
    if (table == NULL) return -1;

    for (size_t i = 0; i < table->idx_num; i++) {
        if (table->idx[i].field != field) continue;

        free(table->idx[i].order);
        for (size_t j = i; j + 1 < table->idx_num; j++)
            table->idx[j] = table->idx[j + 1];
        table->idx_num--;
        return 0;
    }
    return -1;
}

void table_index_invalidate(TABLE* table)
{
    if (table == NULL || table->idx == NULL) return;
    for (size_t i = 0; i < table->idx_num; i++) table->idx[i].stale = 1;
}

TABLE_INDEX* table_index_get(TABLE* table, size_t field)
{
    if (table == NULL) return NULL;
    return index_slot(table, field, 0);
}

size_t table_index_count(const TABLE* table)
{
    return table == NULL ? 0 : table->idx_num;
}

TABLE_INDEX* table_index_at(TABLE* table, size_t i)
{
    if (table == NULL || i >= table->idx_num) return NULL;
    return &table->idx[i];
}
