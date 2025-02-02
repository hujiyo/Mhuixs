#include <stdint.h>
/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
#ifndef LIST_H
#define LIST_H

#include "mlib/strmap.h"

/*
LIST列表
列表库是我为了解决QUEUE库频繁申请内存而造成的性能问题而后开发的。
是第一个使用strmap库实现的数据结构。
在dirt/test-strmap-queue-list.20241213.c测试文件中，
strmap库的分配性能优异。
同时，由于LIST的内存是连续的，对象压缩也容易。
*/

typedef struct LIST{
    STRPOOL strpool;//指向创建的内存池
    OFFSET head;//头元素偏移量
    OFFSET tail;//尾元素偏移量
    uint32_t num;//元素数量
}LIST;

LIST* makeLIST();
/*
创建一个LIST列表对象
LIST *object = makeLIST();
默认情况下，块大小为64字节，块数量为6400。
*/
LIST* makeLIST_AND_SET_BLOCK_SIZE_NUM(int block_size,int block_num);
/*
创建一个LIST列表对象
同时设置内存池的块大小和块数量。

如果块的大小平均大于LIST的元素大小时，strmap的性能会更好。
但是过大的话，内存碎片会较多。

所以，需要根据实际情况来进行平衡。
*/
void freeLIST(LIST* list);
int add_head(LIST* list, uint8_t* stream, uint32_t len);
/*
向LIST列表的头部添加元素

返回值：0：成功  -1：失败
*/
int add_tail(LIST* list, uint8_t* stream, uint32_t len);
/*
向LIST列表的尾部添加元素

返回值：0：成功  -1：失败
*/
int pop_head(LIST* list, uint8_t* stream, uint32_t len);
//从LIST列表的头部弹出元素
int pop_tail(LIST* list, uint8_t* stream, uint32_t len);
//从LIST列表的尾部弹出元素

uint32_t retLISTnum(LIST* list);

#endif