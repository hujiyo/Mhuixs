/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
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
#define M_TABLE      '0'
#define M_KEYLOT     '1'
#define M_STREAM     '2'
#define M_LIST       '3'
#define M_BITMAP     '4'
#define M_STACK      '5'
#define M_QUEUE      '6'
#define M_HOOK       '7'

/*
hook在Mhuixs中被用来：
1.链接所有需要有权限功能的独立"数据结构"
2.在一种数据结构中引用独立于自己的另一个数据结构
*/
typedef uint32_t OWNER_ID;//所有者ID
typedef uint32_t GROUP_ID;//组ID
typedef uint32_t HOOK_ID;//钩子ID

typedef uint8_t RANK;//保护等级
typedef uint8_t HOOKTYPE;//钩子类型（即钩子指向的数据结构类型）

#define RANK_Mhuixs      255     
#define RANK_root        250     
#define RANK_admin       200
#define RANK_user        150
#define RANK_aiadmin     100
#define RANK_aiworker    50
#define RANK_client      25
#define RANK_guest       0


typedef struct HOOK{
    void* handle;//指向任意数据结构描述符
    HOOKTYPE type;//描述符类型
    RANK rank;//保护等级
    OWNER_ID owner;
    GROUP_ID group;
    HOOK_ID hook_id;
    char* name;//狗子名
}HOOK;

#endif