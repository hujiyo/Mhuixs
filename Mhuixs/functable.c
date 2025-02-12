#include "Mhudef.h"
#include "datstrc.h"

#include "strmap.h"

#define MAX_FUNC_NUM 10000

/*
给每个函数分配一个编号,通过编号即可调用函数
*/

int Mhuixscall(uint32_t funcseq,Obj* obj,uint32_t arg1,uint32_t arg2,uint32_t arg3,uint8_t* buffer,uint8_t* buffer2)
{

}
uint32_t* initfunctable(){
    void** functable = (void**)malloc(sizeof(void)*MAX_FUNC_NUM);
    if(functable == NULL){
       printf("initfunctable:malloc error\n");
       return NULL; 
    }

    for(int i = 0;i<1000;i++){
        functable[i] = NULL;
    }
    /*
    前1000个号保留
    */

    //基础功能
    uint8_t* run_get_obj(void);
    uint8_t* run_where(void);
    uint8_t* run_desc(void);
    uint8_t* run_hook(void);
    uint8_t* run_hook_make(void);
    uint8_t* run_hook_checkout(void);
    uint8_t* run_hook_del_obj(void);
    uint8_t* run_hook_clear(void);
    uint8_t* run_rank_obj(void);
    uint8_t* run_get_rank(void);
    uint8_t* run_get_type(void);
    uint8_t* run_temp_get(void);
    uint8_t* run_temp_del(void);
    uint8_t* run_temp_where(void);
    uint8_t* run_where_condition_temp(void);
    uint8_t* run_get_temp(void);

    functable[1000] = run_get_obj;
    functable[1001] = run_where;
    functable[1002] = run_desc;
    functable[1003] = run_hook;
    functable[1004] = run_hook_make;
    functable[1005] = run_hook_checkout;
    functable[1006] = run_hook_del_obj;
    functable[1007] = run_hook_clear;
    functable[1008] = run_rank_obj;
    functable[1009] = run_get_rank;
    functable[1010] = run_get_type;
    functable[1011] = run_temp_get;
    functable[1012] = run_temp_del;
    functable[1013] = run_temp_where;
    functable[1014] = run_where_condition_temp;
    functable[1015] = run_temp_get;

    //TABLE操作相关函数
    uint8_t* run_field_add(void);
    uint8_t* run_field_insert(void);
    uint8_t* run_field_swap(void);
    uint8_t* run_field_del(void);
    uint8_t* run_field_rename(void);
    uint8_t* run_field_set(void);
    uint8_t* run_line_add(void);
    uint8_t* run_line_insert(void);
    uint8_t* run_line_set(void);
    uint8_t* run_line_del(void);
    uint8_t* run_line_swap(void);
    uint8_t* run_line_get(void);
    uint8_t* run_pos_get(void);


    functable[2000] = run_field_add;
    functable[2001] = run_field_insert;
    functable[2002] = run_field_swap;
    functable[2003] = run_field_del;
    functable[2004] = run_field_rename;
    functable[2005] = run_field_set;
    functable[2006] = run_line_add;
    functable[2007] = run_line_insert;
    functable[2008] = run_line_set;
    functable[2009] = run_line_del;
    functable[2010] = run_line_swap;
    functable[2011] = run_line_get;
    functable[2012] = run_pos_get;

    //KVALOT操作相关函数
    uint8_t* run_key_exists(void);
    uint8_t* run_key_select(void);
    uint8_t* run_key_set(void);
    uint8_t* run_key_del(void);
    uint8_t* run_key_checkout(void);

    functable[3000] = run_key_exists;
    functable[3001] = run_key_select;
    functable[3002] = run_key_set;
    functable[3003] = run_key_del;
    functable[3004] = run_key_checkout;

    //STREAM操作相关函数
    uint8_t* run_stream_append(void);
    uint8_t* run_stream_append_pos(void);
    uint8_t* run_stream_get(void);
    uint8_t* run_stream_get_len(void);
    uint8_t* run_stream_set(void);
    uint8_t* run_stream_set_char(void);

    functable[4000] = run_stream_append;
    functable[4001] = run_stream_append_pos;
    functable[4002] = run_stream_get;
    functable[4003] = run_stream_get_len;
    functable[4004] = run_stream_set;
    functable[4005] = run_stream_set_char;

    //LIST操作相关函数
    uint8_t* run_list_lpush(void);
    uint8_t* run_list_rpush(void);
    uint8_t* run_list_lpop(void);
    uint8_t* run_list_rpop(void);
    uint8_t* run_list_get(void);
    uint8_t* run_list_del(void);
    uint8_t* run_list_get_index_len(void);
    uint8_t* run_list_insert(void);
    uint8_t* run_list_set(void);
    uint8_t* run_list_exists(void);

    functable[5000] = run_list_lpush;
    functable[5001] = run_list_rpush;
    functable[5002] = run_list_lpop;
    functable[5003] = run_list_rpop;
    functable[5004] = run_list_get;
    functable[5005] = run_list_del;
    functable[5006] = run_list_get_index_len;
    functable[5007] = run_list_insert;
    functable[5008] = run_list_set;
    functable[5009] = run_list_exists;

    //BITMAP操作相关函数
    uint8_t* run_bitmap_set(void);
    uint8_t* run_bitmap_set_range(void);
    uint8_t* run_bitmap_get(void);
    uint8_t* run_bitmap_get_range(void);
    uint8_t* run_bitmap_count(void);
    uint8_t* run_bitmap_count_range(void);

    functable[6000] = run_bitmap_set;
    functable[6001] = run_bitmap_set_range;
    functable[6002] = run_bitmap_get;
    functable[6003] = run_bitmap_get_range;
    functable[6004] = run_bitmap_count;
    functable[6005] = run_bitmap_count_range;

    return functable;    
}
