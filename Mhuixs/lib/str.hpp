#ifndef STR_HPP
#define STR_HPP
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
/*
str 字节流类型
使用len记录字节流的长度，防止字符串泄露
同时暴露所有成员，方便底层操作
str的目的不是封装，而是利用C++的特性将成员函数和str本身进行绑定
*/

struct str{
    uint8_t *string;//STREAM:字节流的长度
    uint32_t len;//字节流的长度
    int state;//状态码
    str(const char* s);
    str(uint8_t* s, uint32_t len);
    str(const str& s);
    ~str();
    str& operator=(const str& s); // 新增：赋值运算符重载
    str& operator=(const char* s); // 新增：支持直接赋值const char*
    void clear(); // 新增：清空内容
    char* c_str() const; // 新增：返回C风格字符串
    bool operator==(const str& other) const; // 新增：判断两个str是否相等
};

#endif