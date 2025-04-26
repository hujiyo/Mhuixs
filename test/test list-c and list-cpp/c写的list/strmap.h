#ifndef STRMAP_H
#define STRMAP_H
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/

typedef uint8_t* STRPOOL;//字符串池指针
typedef uint32_t OFFSET;//偏移量

/*
strmap：string map 数组地图
————掌管单字节数组中内存分配和释放
注意:
1.返回的是在STRPOOL中的偏移量而不是指针
2.分配的内存地址是否符合字节对齐要求取决于block_size的大小是否是对齐字节数的倍数
*/

/*
STRPOOL内部结构定义:

0 ~ 3字节                           strpool_len的长度
4 ~ 7字节                           block_size的长度
8 ~ 11字节                          block_num的长度
12 ~ 12+(block_num/8+1)-1字节           位图区
12+(block_num/8+1) ~ strpool_len          数据区

关键位置:
位图区偏移量:12
数据区偏移量:12+(block_num/8+1)
*/

STRPOOL build_strpool(uint32_t block_size,uint32_t block_num);
OFFSET STRmalloc(STRPOOL strpool,uint32_t len);
void STRfree(STRPOOL strpool,OFFSET offset,uint32_t len);

#endif