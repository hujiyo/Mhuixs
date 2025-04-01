/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#ifndef BITMAP_H
#define BITMAP_H
#include "merr.h"

#define bitmap_debug

void bitcpy(uint8_t* destination, uint8_t dest_first_bit, const uint8_t* source, uint8_t source_first_bit, uint32_t len);
/*
redis支持位图这种数据结构用于存储大量比特数据，以节省内存和提高效率。
*/

/*
*   BITMAP (c) HUJI 2025.3
*   位图
*   该类提供了一个位图的基本功能,可以用于存储和操作二进制数据。//在bitmap_debug宏被定义的情况下，BITMAP会直接输出错误信息。
*   该类的基本功能和注意事项如下:
*       1. 初始化方法:
*           <1> BITMAP(uint32_t size): 
*               初始化一个大小为size的位图,size的单位为bit。注意size的大小不能超过堆区的大小。如果超过堆区的大小,
*               BITMAP会自动尝试初始化为size=0的位图，这对后续操作可能有影响，比如后续调用valid函数会抛出异常。
*           <2> BITMAP(BITMAP& otherbitmap):
*               初始化一个与otherbitmap相同的位图。确保otherbitmap是有效的，如果无效，BITMAP会尝试初始化为size=0，
*               这可能会影响后续操作。
*           <3> BITMAP(char* s,uint32_t len,uint8_t zerochar):
*               初始化一个大小为len的位图,并将s中的数据复制到位图中,如果s中的字符等于zerochar,则将对应位置的bit设置为0,
*               否则设置为1。请确保s是有效的,否则可能会导致未定义行为。
*       2. 赋值操作:
*           <1> BITMAP& operator=(BITMAP otherbitmap):
*               将otherbitmap赋值给当前的位图。确保otherbitmap是有效的，如果无效，当前位图会保留原始数据，但state状态会增加，
*               可通过检查state来判断操作是否异常。
*           <2> BITMAP& operator+=(BITMAP& otherbitmap):
*               将otherbitmap追加到当前位图的末尾。会重新分配内存以容纳合并后的位图。如果otherbitmap无效，当前位图会保留原始数据，
*               但state状态会增加。
*       3. 析构函数:
*           ~BITMAP():
*               释放位图所占用的内存，防止内存泄漏。
*       4. 下标操作:
*           BIT operator[](uint32_t offset):
*               返回一个代理类BIT的对象，用于访问和修改指定位偏移处的位。如果偏移量超过当前位图的大小，会尝试调用rexpand函数
*               进行扩容，若扩容失败，state状态会增加。
*       5. 代理类BIT:
*           该类用于代理位操作，使得可以像操作普通整数一样操作位图中的位。
*           <1> BIT(BITMAP& bitmap,uint32_t offset):
*               构造函数，初始化代理类对象，关联到位图和指定的偏移量。
*           <2> BIT& operator=(int value):
*               将指定位偏移处的位设置为value（0或1）。调用前确保位图有效且偏移量在合法范围内。
*           <3> operator int():
*               重载类型转换运算符，将代理类对象转换为整数，返回指定位偏移处的位值（0或1）。
*           <4> operator int() const:
*               常量版本的类型转换运算符，用于在常量对象上获取指定位偏移处的位值。
*       6. 其他成员函数:
*           <1> int iserr():
*               根据state状态判断检查位图是否有效，返回非0表示异常，返回0表示正常。
*           <2> int set(uint32_t offset,uint8_t value):
*               将指定位偏移处的位设置为value（0或1）。如果偏移量超出范围，可能会导致异常。
*           <3> int set(uint32_t offset, uint32_t len, uint8_t value):
*               从指定偏移量开始，连续设置len个位的值为value（0或1）。如果超出范围，可能会导致异常。
*           <4> int set(uint32_t offset,uint32_t len,const char* data_stream,char zero_value):
*               从指定偏移量开始，将data_stream中的数据复制到位图中，若字符等于zero_value，则对应位设为0，否则设为1。
*               确保data_stream有效且长度足够。
*           <5> uint32_t size():
*               返回位图的大小，单位为bit。
*           <6> uint32_t count(uint32_t st_offset,uint32_t ed_offset):
*               统计从st_offset到ed_offset范围内位值为1的数量。确保偏移量合法，否则可能导致未定义行为。
*           <7> int64_t find(uint8_t value,uint32_t start,uint32_t end):
*               从start到end范围内查找第一个值为value（0或1）的位的偏移量。若未找到，返回-1。
*           <8> void ptf():
*               可能用于打印位图的相关信息，具体实现取决于函数内部逻辑。
*           <9> int rexpand(uint32_t size):
*               私有成员函数，将位图的大小改变到size。执行失败返回merr（未定义常量，需确保其正确定义）。
*/

class BITMAP  {
    private:
        uint8_t* bitmap;
        class BIT;
        int state = 0;//对象状态,成员函数通过改变对象状态来表示对象的异常状态。
        int rexpand(uint32_t size);//该容函数将bitmap的大小改变到size,执行失败返回merr
    public:
        BITMAP();
        BITMAP(uint32_t size);
        BITMAP(const char* s);
        BITMAP(BITMAP& otherbitmap);
        BITMAP(char* s,uint32_t len,uint8_t zerochar);

        ~BITMAP();

        BITMAP& operator=(BITMAP otherbitmap);
        BITMAP& operator=(char* s);
        BITMAP& operator+=(BITMAP& otherbitmap);
        
        BIT operator[](uint32_t offset);

        int iserr();
        
        int set(uint32_t offset,uint8_t value);
        int set(uint32_t offset, uint32_t len, uint8_t value);
        int set(uint32_t offset,uint32_t len,const char* data_stream,char zero_value);
        uint32_t size();
        uint32_t count(uint32_t st_offset,uint32_t ed_offset);
        int64_t find(uint8_t value,uint32_t start,uint32_t end);
        void ptf();
};

#endif

/*
iserr使用示例
int main(){
    BITMAP bitmap(100);//初始化一个大小为100的位图
    if(bitmap.iserr()){//检查位图是否有效
        //错误处理代码
    }
    //正确使用的代码
}
*/