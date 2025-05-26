#ifndef STREAM_HPP
#define STREAM_HPP
#include "mshare.hpp"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.6
Email:hj18914255909@outlook.com
*/

/*
STREAM流对象，创新性地使用mshare共享内存作为数据区
支持高效的多进程/多线程数据共享
*/

#define merr -1

class STREAM {
private:
    OFFSET offset;      // 数据在共享内存池中的偏移
    uint32_t size;      // 当前数据长度
    uint32_t capacity;  // 当前分配容量
    int state;          // 状态码

    int ensure_capacity(uint32_t mincap); // 自动扩容
public:
    STREAM(uint32_t initcap = 64); // 构造，指定初始容量
    STREAM(const STREAM& other); // 拷贝构造函数
    STREAM& operator=(const STREAM& other); // 拷贝赋值操作符
    ~STREAM();

    int append(const uint8_t* buf, uint32_t len); // 末尾追加
    int append_pos(uint32_t pos, const uint8_t* buf, uint32_t len); // 指定位置追加
    int get(uint32_t pos, uint32_t len, uint8_t* out); // 取数据
    int set(uint32_t pos, const uint8_t* buf, uint32_t len); // 设置数据
    int set_char(uint32_t pos, uint32_t len, uint8_t ch); // 指定位填充
    uint32_t len() const; // 获取流长度
    int iserr() const; // 错误检查
};

#endif