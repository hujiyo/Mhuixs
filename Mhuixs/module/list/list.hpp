#ifndef LIST_H
#define LIST_H
/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <regex>

using namespace std;

#include "./../../mlib/memap.hpp"
#include "./../../mlib/uintdeque.hpp"

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
    struct str{
        uint8_t *string;//STREAM:字节流的长度
        uint32_t len;//字节流的长度
        int state;//状态码
        str(const char* s);
        str(uint8_t* s, uint32_t len);
        str(const str& s);
        ~str();
    };
    LIST();
    LIST(int block_size,int block_num);
    ~LIST();

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

    /*  
    //int sort();//排序
    */
    int iserr();
};

#endif

/************************************
 * 下面是std::list和std::deque的测试代码
 * 这里直接放总结：
 * LIST对比std::list的优势在于
 * 1.参数自由，可根据需求设置LIST以提高性能上限
 * 2.提前的内存分配，在对象操作过程中性能更好
 * 3.通过MEMAP管理的独立内存池，易于进行整块内存的对象压缩，内存占用更可控
 ************************************/

/*
 void print(LIST::str s){
    for(uint32_t i = 0;i < s.len;i++){
        printf("%c",s.string[i]);
    }
}
//下面是LIST的测试代码
#include <time.h>
int main(){
    LIST list(32,32);   

    //测试代码
    time_t start, end;
    start = clock();

    for(int i = 0;i < 10000;i++){
        LIST::str s1("hello1########################");
        LIST::str s2("hello2########################");
        LIST::str s3("hello3########################");
        LIST::str s4("hello4########################");
        LIST::str s5("hello5########################");
        list.lpush(s1);
        list.lpush(s2);
        list.lpush(s3);
        list.lpush(s4);
        list.lpush(s5);
        list.lpush(s1);
        list.lpush(s2);
        list.lpush(s3);
        list.lpush(s4);
        list.lpush(s5);
        for(int i = 0;i < 10;i++){
            list.insert(s1,i);
        }
        for(int i = 0;i > -10;i--){
            list.insert(s2,i);
        }
    }
    end = clock();
    #define CLOCKS_PER_SEC 1000
    printf("LIST耗时: %.3f秒\n", (end - start) / (double)CLOCKS_PER_SEC);
    printf("end\n");
    return 0;
}
*/

//下面是std::list的测试代码
/*
#include <list>
#include <string>
int main(){
    for(int i = 0;i < 1000000;i++){
        std::list<std::string> list;
        std::string s1("hello1########################");
        std::string s2("hello2########################");
        std::string s3("hello3########################");
        std::string s4("hello4########################");
        std::string s5("hello5########################");
        list.push_front(s1);
        list.push_front(s2);
        list.push_front(s3);
        list.push_front(s4);
        list.push_front(s5);
        for(int i = 0;i < 5;i++){
            list.pop_front();
        }
        list.push_back(s1);
        list.push_back(s2);
        list.push_back(s3);
        list.push_back(s4);
        list.push_back(s5);
        for(int i = 0;i < 5;i++){
            list.pop_back();
        }
        list.push_back(s1);
        list.push_front(s2);
        list.push_back(s3);
        list.push_front(s4);
        list.push_back(s5);
        for(int i = 0;i < 5;i++){
            list.pop_front();
        }
    }
    printf("end\n");
    return 0;
}
*/
//下面是std::deque的测试代码
/*
#include <deque>
#include <string>
int main(){
    for(int i = 0;i < 1000000;i++){
        std::deque<std::string> list;
        std::string s1("hello1");
        std::string s2("hello2");
        std::string s3("hello3");
        std::string s4("hello4");
        std::string s5("hello5");
        list.push_front(s1);
        list.push_front(s2);
        list.push_front(s3);
        list.push_front(s4);
        list.push_front(s5);
        for(int i = 0;i < 5;i++){
            list.pop_front();
        }
        list.push_back(s1);
        list.push_back(s2);
        list.push_back(s3);
        list.push_back(s4);
        list.push_back(s5);
        for(int i = 0;i < 5;i++){
            list.pop_back();
        }
        list.push_back(s1);
        list.push_front(s2);
        list.push_back(s3);
        list.push_front(s4);
        list.push_back(s5);
        for(int i = 0;i < 5;i++){
            list.pop_front();
        } 
    } 
    printf("end\n");
    return 0;
}
*/