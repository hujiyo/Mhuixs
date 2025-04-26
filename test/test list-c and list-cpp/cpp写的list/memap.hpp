#ifndef MEMAP_H
#define MEMAP_H
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define merr -1

/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/

typedef uint32_t OFFSET;//偏移量

/*
memap：string map 数组地图
————掌管单字节数组中内存分配和释放
注意:
1.返回的是在memap中的偏移量而不是指针
2.分配的内存地址是否符合字节对齐要求取决于block_size的大小是否是对齐字节数的倍数
*/

/*
MEMAP内部结构定义:

0 ~ 3字节                           strpool_len的长度
4 ~ 7字节                           block_size的长度
8 ~ 11字节                          block_num的长度
12 ~ 12+(block_num/8+1)-1字节           位图区
12+(block_num/8+1) ~ strpool_len          数据区

关键位置:
位图区偏移量:12
数据区偏移量:12+(block_num/8+1)
*/

struct MEMAP{
    uint8_t* strpool;
    MEMAP(uint32_t block_size,uint32_t block_num);//构造函数，block_size为页大小，block_num为页数量
    ~MEMAP();
    OFFSET smalloc(uint32_t len);//分配内存，建议len为block_size的整数倍以提高内存利用率
    void sfree(OFFSET offset,uint32_t len);//释放内存
    int iserr(OFFSET offset);//检查偏移量是否合法
    int iserr();//检查内存池是否出错
    uint8_t* addr(OFFSET offset);//通过偏移量获取指针地址
};


/*************************************************************************
**  由于功能实现的代码量比较短，不需要分成头文件和源文件，我就直接用这道注释隔开，
**  这道注释上面部分相当于是头文件，下面是函数的实现，相当于是源文件。
**************************************************************************/

#endif