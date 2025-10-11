#ifndef STR_HPP
#define STR_HPP
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef char* mstring;
/*
str 字节流类型
使用len记录字节流的长度，防止字符串泄露
同时暴露所有成员，方便底层操作
str的目的不是封装，而是利用C++的特性将成员函数和str本身进行绑定
*/

struct str{
    mstring stream;
    str(const char* s);
    str(uint8_t* s, uint32_t len);
    str(const str& s);
    str(mstring ms, bool take_ownership = false); // 从 mstring 构造
    str(); // 默认构造函数
    ~str();
    str& operator=(const str& s); // 新增：赋值运算符重载
    str& operator=(const char* s); // 新增：支持直接赋值const char*
    void clear(); // 新增：清空内容
    char* string() const; // 新增：返回C风格字符串
    size_t len() const;
    bool operator==(const str& other) const; // 新增：判断两个str是否相等
    mstring release(); // 释放所有权，返回 mstring（调用者负责 free）
};

#endif