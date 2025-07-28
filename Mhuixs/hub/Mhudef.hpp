#ifndef MHUDEF_HPP
#define MHUDEF_HPP
#include <stdint.h>
/*
#版权所有 (c) HUJI 2025
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.5
Email:hj18914255909@outlook.com
*/

#include "list.hpp"
#include "bitmap.hpp"
#include "kvalh.hpp"
#include "tblh.hpp"
#include "stream.hpp"
#include "hook.hpp"

/*
Mhuixs数据库支持的数据结构/数据操作对象:
1.表-table
2.键库-kvalot
3.列表-list
4.位图-bitmap
5.流-stream

注意:
1.数据结构是一种可以被直接引用和操作的东西<=>任何可以被hook引用的东西才是叫做数据结构
2.任何一个数据结构的使用之前为它创建一个引用hook可以增加其保护等级、链接灵活性
3.任何hook都必须挂载在Mhuixs上
*/

//下面是Mhuixs数据库的基本数据结构（操作对象）

union obj_struct //储存各种对象的描述符
{
    KVALOT *kvalot;
    LIST *list;
    TABLE *table;
    BITMAP *bitmap;
    STREAM *stream;
    KVALOT::KEY *key; //KVALOT的KEY对象
    HOOK *hook; //HOOK对象
};

enum obj_type //数据结构对象的类型
{
    M_NULL   =    '\0',

    M_KVALOT =    '1',
    M_LIST   =    '2',
    M_BITMAP =    '3',
    M_TABLE  =    '4',

    M_HOOK   =    '5',
    M_STREAM =    '6',
};

typedef struct basic_handle_struct
{
    obj_struct handle;//任意数据结构描述符
    obj_type type;//描述符的类型
    int make_self(obj_type type, void *parameter1, void *parameter2, void *parameter3){
        /*
        本函数在成功创建数据对象之前都不会对basic_handle_struct里的原数据进行任何修改
        */
        //检查type是否合法
        if(iserr_obj_type(type) || type == M_NULL){
            return merr;
        }
        switch (type){
            case M_STREAM: {
                STREAM *stream = (STREAM*)calloc(1,sizeof(STREAM)); 
                if(parameter1!=NULL) stream = new (stream) STREAM(*(uint32_t*)parameter1);//使用第一个参数作为STREAM的初始容量
                else stream = new (stream) STREAM();
                if(stream->iserr()){                    
                    printf("basic_handle_struct::make_self:Error: STREAM create error\n");
                    stream->~STREAM();                    
                    free(stream); //释放内存                    
                    return merr;                
                }
                handle.stream = stream;
                break;
            }
            case M_LIST:{
                LIST *list = (LIST*)calloc(1,sizeof(LIST));
                if(parameter1!=NULL && parameter2!=NULL) 
                list = new (list) LIST(*(uint32_t*)parameter1,*(uint32_t*)parameter2);//使用第一个参数作为LIST的大小，第二个参数作为LIST的元素类型
                else list = new (list) LIST();
                if(list->iserr()){                    
                    printf("basic_handle_struct::make_self:Error: LIST create error\n");
                    list->~LIST();                    
                    free(list);//释放内存          
                    return merr;
                }
                handle.list = list;
                break;
            }
            case M_BITMAP:{//使用第一个参数作为BITMAP的大小
                BITMAP *bitmap = (BITMAP*)calloc(1,sizeof(BITMAP));
                if(parameter1!=NULL) bitmap = new (bitmap) BITMAP(*(uint32_t*)parameter1);//使用第一个参数作为BITMAP的大小
                else bitmap = new (bitmap) BITMAP();
                if(bitmap->iserr()){                    
                    printf("basic_handle_struct::make_self:Error: BITMAP create error\n");
                    bitmap->~BITMAP();                    
                    free(bitmap);//释放内存 
                    return merr;
                }
                handle.bitmap = bitmap;
                break;
            }
            case M_TABLE:{
                TABLE *table = (TABLE*)calloc(1,sizeof(TABLE));
                if(parameter1!=NULL && parameter2!=NULL && parameter3!=NULL)
                    //使用第一个参数作为TABLE的字段信息，第二个参数作为TABLE的字段数量   
                    table = new (table) TABLE((str*)parameter1,(FIELD*)parameter2,*(uint32_t*)parameter3);
                else{
                    free(table);
                    printf("basic_handle_struct::make_self:Error:parameter err\n");
                    return merr;
                }
                if(table->iserr()){
                    printf("basic_handle_struct::make_self:Error: TABLE create error\n");
                    table->~TABLE();
                    free(table);
                    return merr;
                }
                handle.table = table;
                break;
            }
            case M_KVALOT:{
                KVALOT *kvalot = (KVALOT*)calloc(1,sizeof(KVALOT));
                if(parameter1!=NULL) kvalot = new (kvalot) KVALOT((str*)parameter1);//使用第一个参数作为KVALOT的名称                   
                else{
                    free(kvalot);
                    printf("basic_handle_struct::make_self:Error:parameter err\n");
                    return merr;
                }
                if(kvalot->iserr()){
                    printf("basic_handle_struct::make_self:Error: KVALOT create error\n");
                    kvalot->~KVALOT();
                    free(kvalot);
                    return merr;
                }
                handle.kvalot = kvalot;
                break;
            }
            /*
            case M_HOOK:{
                
            }
            */
            default://M_NULL
                printf("basic_handle_struct::add_key:Error:type error\n");
                return merr;
        }
        this->type = type;
        return 0;
    }
    void clear_self(){
        /*
        本函数会清空basic_handle_struct里的原数据、对象
        */
        switch (type){
           case M_KVALOT:
                handle.kvalot->~KVALOT();//调用析构函数
                free(handle.kvalot);
                break;
            case M_LIST:
                handle.list->~LIST();//调用析构函数
                free(handle.list);
                break;
            case M_BITMAP:
                handle.bitmap->~BITMAP();//调用析构函数
                free(handle.bitmap);
                break;
            case M_TABLE:
                handle.table->~TABLE();//调用析构函数
                free(handle.table);
                break;
            case M_STREAM:
                handle.stream->~STREAM();//调用析构函数
                free(handle.stream);
                break;
            //case M_HOOK:
            default:
                printf("basic_handle_struct::clear_self:Error:STRUCT WAS WRONG\n");
        }
        type = M_NULL;
        memset(&handle,0,sizeof(handle));//清空handle
        return;
    }
} basic_handle_struct;


struct COMMEND{
    uint32_t command; // 命令码
    obj_type objtype; // 对象名
    void* param1;      // 参数1
    void* param2;      // 参数2
    void* param3;      // 参数3
    void* param4;      // 参数4
    void* param5;      // 参数5
    void* param6;      // 参数6
};

struct SECTION{
    basic_handle_struct obj;//操作对象的描述符
    COMMEND commend;//操作命令
};

bool iserr_obj_type(obj_type type);

/*
Mhuixs是基于内存的数据库，在这个寸土寸金的内存世界，内存压缩是数据库的核心
Mhuixs对于数据的操作是以数据结构为对象的，而所有数据对象是通过HOOK
结构体来操作的，在execute.c执行每一步标准命令前都需要使用zslish.h中的函数对HOOK对象进行一次解压操作。
此外，接入模块还要负责对长时间不操作的数据结构对象进行提高压缩等级的操作（刚创建时默认是lv0=0）。

其实简单来说，Mhuixs中压缩就是降低内存的使用，所以将数据存入硬盘这个行为本身就是一个极为有效的压缩算法
lv0: 不压缩              # 0延迟，立即响应
lv1: LZ4                 # ~1μs延迟
lv2: ZSTD -5/-7          # ~10μs延迟  
lv3: ZSTD -19/-22        # ~100-500μs延迟（模拟磁盘）
lv4 :直接存放于磁盘（然后返回磁盘索引）这个索引需要zslish自己定义，可能要包含文件路径、文件名、数据在文件中的偏移量等等
lv5 :使用mzstd库进行压缩存放于磁盘（然后返回索引）
*/
typedef enum Cprs{
    lv0=0,    lv1=1,
    lv2=2,    lv3=3,
    lv4=4,    lv5=5
}Cprs;



uint8_t islittlendian(){
    uint16_t a=1;//大端:00000000 00000001 小端:00000001 00000000
    return *(uint8_t*)&a;//返回1则是小端，返回0则是大端
}
extern int _IS_LITTLE_ENDIAN_; //全局变量:是否是小端 0-否 1-是

#endif