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

#include "list.h"
#include "bitmap.h"
#include "kvalh.hpp"
#include "tblh.h"
#include "hook.hpp"
#include "registry.hpp"
#include "usergroup.hpp"

#define PORT 18185

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

typedef char* mstring;//以size_t为长度前缀+字符串内容的类型






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
对长时间不操作的数据结构对象进行提高压缩等级的操作（刚创建时默认是lv0=0）。

其实简单来说，Mhuixs中压缩就是降低内存的使用，所以将数据存入硬盘这个行为本身就是一个极为有效的压缩算法
lv0: 不压缩              # 0延迟，立即响应
lv1: LZ4                 # ~1μs延迟
lv2: ZSTD -5/-7          # ~10μs延迟  
lv3: ZSTD -19/-22        # ~100-500μs延迟（模拟磁盘）
lv4 :直接存放于磁盘（然后返回磁盘索引）这个索引需要zslish自己定义，可能要包含文件路径、文件名、数据在文件中的偏移量等等
lv5 :使用mzstd库进行压缩存放于磁盘（然后返回索引）

typedef enum Cprs{
    lv0=0,    lv1=1,
    lv2=2,    lv3=3,
    lv4=4,    lv5=5
}Cprs;
*/

uint8_t islittlendian(){
    uint16_t a=1;//大端:00000000 00000001 小端:00000001 00000000
    return *(uint8_t*)&a;//返回1则是小端，返回0则是大端
}
extern int _IS_LITTLE_ENDIAN_; //全局变量:是否是小端 0-否 1-是

#endif