#ifndef LIST_HPP
#define LIST_HPP
/*
#版权所有 (c) HUJI 2025
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.4
Email:hj18914255909@outlook.com
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <regex>
#include "nlohmann/json.hpp"
using namespace std;

#include "./../../lib/memap.hpp"
#include "./../../lib/uintdeque.hpp"
#include "./../../lib/str.hpp"

#define merr -1
#define bitmap_debug


/**********************************************************************
 *  \ LIST列表                                                   /
 *   列表库是我为了解决QUEUE库频繁申请内存而造成的性能问题而后开发的。
 *   最初QUEUE和LIST的唯一区别在于QUEUE库直接使用C提供内存分配函数，
 *   而LIST库使用MEMAP库进行内存提前申请，减少了内存分配时的性能消耗。
 *   后来由于LIST库的性能优异，于是便舍弃了QUEUE库。
 *   对于LIST库的优化除了预分配内存池外，还要想办法提高元素的随机访问性能。
 *   我来说说我原本的设计思路吧，我原本打算借鉴redis的quicklist和C++的deque
 *   初始状态下使用连续内存块：blocklist，当需要从中间插入时，如果
 *   blocklist足够小，则直接进行内存的位移，如果blocklist比较大，
 *   则直接从中间断开，形成两个blocklist，中间使用指针连起来
 *   但是这也太麻烦了，而且容易出错，所以我放弃了这种方法。
 *   我觉得还是使用正常的链表比较好，链表的优势在于首尾插入和删除的性能优异。
 *   缺点则是随机访问的性能较低。于是我打算使用一个deque动态数组来
 *   存储元素的偏移量索引,这不比quicklist还要遍历list要快吗？
 *   后来我发现deque的速度也不是特别高，于是我打算亲自实现一个专门用来
 *   储存uint32_t类型的deque,deque为了兼容不同的数据类型，其内部比较冗余。
 *   LIST是第一个使用memmap库实现的数据结构。
 *   在dirt/test-strmap-queue-list.20241213.c测试文件中，
 *   memap原名是strmap。它保证了LIST的内存是连续的，对象压缩更加容易。
 *  /                                                           \
***********************************************************************/

class LIST{
private:
    MEMAP strpool;//先声明一个数组内存池    
    UintDeque index;//元素偏移量索引
    int state;//对象状态,成员函数通过改变对象状态来表示对象的异常状态。
public:
    LIST();
    LIST(int block_size,int block_num);
    ~LIST();
    LIST(const LIST& other); // 拷贝构造函数
    LIST& operator=(const LIST& other); // 拷贝赋值运算符

    int lpush(str &s);
    int rpush(str &s);
    str lpop();
    str rpop();
    int insert(str &s, int64_t index);//在指定位置插入元素
    int update(str &s, int64_t index);//更新指定位置的元素
    int del(int64_t index);//删除指定位置的元素
    str get(int64_t index);//获取指定位置的元素
    uint32_t amount();//获取列表的元素数量
    int clear();//清空列表
    int64_t find(str &s);//查找元素
    int swap(int64_t idx1, int64_t idx2);//交换两个位置的元素

    int iserr();
    nlohmann::json get_all_info() const;
};

#endif
