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
这个内部头文件用于mhuixs系列库的最基本定义
用户不需要包含这个头文件
*/
#define short_string 50
#define long_string 300
#define err -1

typedef uint8_t BITE;//字节流

typedef uint8_t RANK;//保护等级
#define RANK_Mhuixs      255     
#define RANK_root        250     
#define RANK_admin       200
#define RANK_user        150
#define RANK_aiadmin     100
#define RANK_aiworker    50
#define RANK_client      25
#define RANK_guest       0

typedef uint32_t OWNER_ID;//所有者ID
typedef uint32_t GROUP_ID;//组ID
typedef uint32_t HOOK_ID;//钩子ID

/*
Mhuixs数据库支持的数据结构:
1.表-table
2.键库-kvalot
3.栈-stack
4.队列-queue
5.列表-list
6.位图-bitmap
特殊链接型结构:
1.hook(被hook-tree或者其他数据结构引用)
2.hook-tree(依附于hook-tree)(可以被引用，但是不能直接得到数据结构进行直接操作)
3.key(依附于键库)
4.Mhuixs(本数据库的最高根hook-tree)
注意:
1.数据结构是一种可以被直接引用和操作的东西<=>任何可以被hook引用的东西才是叫做数据结构
2.任何一个数据结构的使用之前为它创建一个引用hook可以增加其保护等级、链接灵活性
3.Mhuixs是数据库的最高根hook-tree，任何hook都必须挂载在Mhuixs或其它
*/
//下面是Mhuixs数据库的基本数据结构
#define M_TABLE      'a'
#define M_KEYLOT     'b'
#define M_STACK      'c'
#define M_QUEUE      'd'
#define M_LIST       'e'
#define M_BITMAP     'f'
//下面是Mhuixs数据库路径结构
//#define HOOK         '1'
#define HOOK_TREE    '2'
/*
树钩对：tree-hook
键值对：key-value
*/









#endif