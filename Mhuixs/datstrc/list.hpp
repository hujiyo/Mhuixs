#include <stdint.h>

/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/

/*
LIST列表
列表库是我为了解决QUEUE库频繁申请内存而造成的性能问题而后开发的。
是第一个使用strmap库实现的数据结构。
在dirt/test-strmap-queue-list.20241213.c测试文件中，
strmap库的分配性能优异。
同时，由于LIST的内存是连续的，对象压缩也容易。
*/

#ifndef LIST_H
#define LIST_H

#include "stdstr.hpp"
#include "memap.hpp"

class LIST{
private:
    MEMAP strpool;//先声明一个数组内存池
    OFFSET head;//头节点偏移量
    OFFSET tail;//尾节点偏移量
    uint32_t num;//节点数量
    int state = 0;//对象状态,成员函数通过改变对象状态来表示对象的异常状态。
public:
    LIST();
    LIST(int block_size,int block_num);
    ~LIST();

    int lpush(str &s);
    int rpush(str &s);
    int lpop();
    int rpop();
    int iserr();
};

#endif