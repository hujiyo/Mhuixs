#ifndef OBJ_H
#define OBJ_H
//#include "Mhudef.hpp"

typedef void* Obj;

#define BIGNUM_SMALL_SIZE 32          /* 小数据内联存储大小 */
/* 大数结构体 - 固定64字节 */
typedef struct {
    int type;                                 /* 类型标记（4字节） */
    int length;                               /* 数字长度或字符串长度（4字节） */
    union {
        char small_data[BIGNUM_SMALL_SIZE];  /* 小数据内联存储（32字节） */
        char *large_data;                     /* 大数据动态分配指针（8字节） */
        //下面是兼容Mhuixs类型的字段
        //KVALOT *kvalot;//这个库暂时有点问题，也是先注释掉
        //LIST *list;
        //TABLE *table;
        //BITMAP *bitmap;//bitmap后期考虑直接将BITMAP结构体用BigNum结构体代替,因为底层都是char*,所以这里注释掉
        //HOOK *hook; //HOOK对象相关库还没写好，这里也注释掉
        //KEY
    } data;                                   /* 32字节（联合体取最大） */    
    int capacity;                             /* 分配的容量（4字节） */    
    int is_large;                             /* 是否使用大数据存储（4字节） */
    union {
        struct {
            int decimal_pos;                  /* 小数点位置（从右边数）（4字节） */
            int is_negative;                  /* 是否为负数（4字节） */
        } num;                                /* 数字类型专有字段 */
        struct {
            int reserved1;                    /* 保留字段1（4字节） */
            int reserved2;                    /* 保留字段2（4字节） */
        } str;                                /* 字符串类型专有字段（未来扩展） */
        char padding[8];                      /* 确保联合体为8字节 */
    } type_data;                              /* 8字节（类型特定数据） */
} BigNum,basic_handle_struct,bhs;  /* 总大小：32+4+4+4+4+8=56字节，填充到64字节 */



void free_Obj(Obj p){
    //本函数会释放Obj
    
    //BigNum:调整变量表
    if((*(int*)p) < 100){
        //说明底层是BigNum结构体
    }
    else if((*(int*)p) < 200){
        basic_handle_struct *handle = (basic_handle_struct*)p;
        //说明底层是bhs结构体
        switch (*(obj_type*)p){
            case M_KVALOT:
                handle.kvalot->~KVALOT();//调用析构函数
                free(handle.kvalot);
                break;
            case M_LIST:
                free_list(handle.list);
                break;
            case M_BITMAP:
                free_bitmap(handle.bitmap);
                break;
            case M_TABLE:
                free_table(handle.table);
                break;
            case M_STREAM:
                free(handle.stream);
                break;
            case M_HOOK:
            default:
                printf("basic_handle_struct::clear_self:Error:STRUCT WAS WRONG\n");
        }
        type = M_NULL;
        memset(&handle,0,sizeof(handle));//清空handle
        return;
        
    }
    else {
        //说明底层是hook或key结构体，需要谨慎处理
    }
    return;
}



#endif