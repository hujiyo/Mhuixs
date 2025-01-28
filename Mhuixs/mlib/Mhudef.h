/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
#include "datstrc.h"
#include "getid.h"
#include <stdint.h>
#ifndef MHUDEF_H
#define MHUDEF_H
/*
这个内部头文件是mhuixs最基本定义
*/
#define short_string 50
#define long_string 300
#define err -1

/*
Mhuixs数据库支持的数据结构/数据操作对象:
1.表-table
2.键库-kvalot
3.栈-stack
4.队列-queue
5.列表-list
6.位图-bitmap
7.流-stream

注意:
1.数据结构是一种可以被直接引用和操作的东西<=>任何可以被hook引用的东西才是叫做数据结构
2.任何一个数据结构的使用之前为它创建一个引用hook可以增加其保护等级、链接灵活性
3.任何hook都必须挂载在Mhuixs或其它
*/

//下面是Mhuixs数据库的基本数据结构（操作对象）
typedef enum OBJECTYPE{
    M_NULL   =    '0',
    M_KEYLOT =    '1',
    M_STREAM =    '2',
    M_LIST   =    '3',
    M_BITMAP =    '4',
    M_STACK  =    '5',
    M_QUEUE  =    '6',
    M_HOOK   =    '7',
    M_TABLE  =    '8',
}
OBJECTYPE,Obj_TYPE,OT,  //数据结构类型对象
HOOKTYPE;   //钩子类型对象

#define M_NULL       '0'
#define M_KEYLOT     '1'
#define M_STREAM     '2'
#define M_LIST       '3'
#define M_BITMAP     '4'
#define M_STACK      '5'
#define M_QUEUE      '6'
#define M_HOOK       '7'
#define M_TABLE      '8'

/*
hook在Mhuixs中被用来：
1.链接所有需要有权限功能的独立"数据结构"
2.在一种数据结构中引用独立于自己的另一个数据结构
*/
typedef struct basic_handle_struct{
    void* handle;//指向任意数据结构描述符
    OBJECTYPE type;//描述符类型
}basic_handle_struct,BHS;

typedef struct HOOK{
    void* handle;//指向任意数据结构描述符
    HOOKTYPE type;//描述符类型
    RANK rank;//保护等级
    cprs cprs_stage;//压缩级别
    userid_t owner;//对应所有者ID
    groupid_t group;//对应组ID
    hookid_t hook_id;
    char* name;//狗子名
}HOOK;

void* retkeyobject(void* bhs);
/*
    返回引用指向的数据结构对象的结构体指针
    KEY和HOOK都是数据结构对象的引用

    bhs:指向KEY或HOOK的指针
*/
OBJECTYPE retkeytype(void* bhs);
/*
    返回引用指向的数据结构对象的类型
    KEY和HOOK都是数据结构对象的引用

    bhs:指向KEY或HOOK的指针
*/
/*
Mhuixs权限管理建议：
对于人类用户：开启会话默认是guest,通过登录系统获得user权限，通过密码认证获得admin权限，再通过sudo获得root权限
对于AI用户：开启会话默认是guest,通过密码认证获得aiworker权限，通过密码获得aiadmin权限。
*/
typedef uint32_t userid_t;//用户ID 1-65535
//下面是ID的两个极端，ROOT是最高权限ID，VOID是最低权限ID
#define ROOT 0
#define VOID 65535
//ID分配规则草案：
//ADMIN ID：1-99
//HUMAN ID：100-999
//AI  ID：1000-9999
//GUEST ID：10000-
typedef uint32_t groupid_t;//组ID 0-65535
typedef uint32_t hookid_t;//钩子ID 0-65535



typedef uint8_t RANK;//保护等级
//下面是默认的保护等级
#define RANK_root        255
#define RANK_admin       250
#define RANK_user        150
#define RANK_aiadmin     100
#define RANK_aiworker    50
#define RANK_guest       0

/*
Mhuixs是基于内存的数据库，在这个寸土寸金的内存世界，内存压缩是数据库的核心
Mhuixs对于数据的操作是以数据结构为对象的，而所有数据对象是通过HOOK
结构体来操作的，在execute.c执行每一步标准命令前都需要使用zslish.h中的函数对HOOK对象进行一次解压操作。
此外，接入模块还要负责对长时间不操作的数据结构对象进行提高压缩等级的操作（刚创建时默认是lv0=0）。

其实简单来说，Mhuixs中压缩就是降低内存的使用，所以将数据存入硬盘这个行为本身就是一个极为有效的压缩算法
lv0 :不压缩
lv1 :LZF压缩算法
lv2 .snappy压缩
lv3 .zstd压缩
lv4 :直接存放于磁盘（然后返回磁盘索引）这个索引需要zslish自己定义，可能要包含文件路径、文件名、数据在文件中的偏移量等等
lv5 :使用mzstd库进行压缩存放于磁盘（然后返回索引）
*/
typedef enum cprs{
    lv0=0,    lv1=1,
    lv2=2,    lv3=3,
    lv4=4,    lv5=5
}cprs;



uint8_t islittlendian(){
    uint16_t a=1;//大端:00000000 00000001 小端:00000001 00000000
    return *(uint8_t*)&a;//返回1则是小端，返回0则是大端
}
extern int _IS_LITTLE_ENDIAN_; //全局变量:是否是小端 0-否 1-是

#define Threadsnum 4 //线程数量

#endif