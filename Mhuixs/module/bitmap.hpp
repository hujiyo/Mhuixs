#ifndef BITMAP_HPP
#define BITMAP_HPP
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

#include "bitcpy.h"

#define merr  -1
#define bitmap_debug

void bitcpy(uint8_t* destination, uint8_t dest_first_bit, const uint8_t* source, uint8_t source_first_bit, uint64_t len);

class BITMAP  {
    private:
        uint8_t* bitmap;
        class BIT;
        int state = 0;//对象状态,成员函数通过改变对象状态来表示对象的异常状态。
        int rexpand(uint64_t size);//该容函数将bitmap的大小改变到size,执行失败返回merr
    public:
        BITMAP();
        BITMAP(uint64_t size);
        BITMAP(const char* s);
        BITMAP(const BITMAP& otherbitmap);
        BITMAP(char* s,uint64_t len,uint8_t zerochar);

        ~BITMAP();

        BITMAP& operator=(const BITMAP& otherbitmap);
        BITMAP& operator=(char* s);
        BITMAP& operator+=(BITMAP& otherbitmap);
        
        BIT operator[](uint64_t offset);

        int iserr();
        
        int set(uint64_t offset,uint8_t value);
        int set(uint64_t offset, uint64_t len, uint8_t value);
        int set(uint64_t offset,uint64_t len,const char* data_stream,char zero_value);
        uint64_t size();
        uint64_t count(uint64_t st_offset,uint64_t ed_offset);
        int64_t find(uint8_t value,uint64_t start,uint64_t end);
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