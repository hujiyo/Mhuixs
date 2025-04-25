#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/*
#版权所有 (c) HuJi 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
#ifndef STDSTR_H
#define STDSTR_H


#define merr -1
#define bitmap_debug

/*
str 字节流类型
参照C++中的string类：
使用len来记录字节流的长度
防止出现C语言中字符串泄露的问题
同时暴露所有成员，方便底层操作
str的目的不是封装，
而是利用C++的特性将成员函数和str本身进行绑定
*/

struct str{
    uint8_t *string;//STREAM:字节流的长度
    uint32_t len;//字节流的长度
    int state;//状态码
    str(char* s);
    str(uint8_t* s, uint32_t len);
    str(str& s);
    ~str();
};

str::str(char* s):len(strlen(s)),state(0),string((uint8_t*)malloc(strlen(s))){
    if(string == NULL){
        #ifdef bitmap_debug
        printf("str init malloc error\n");
        #endif
        len = 0;
        state++;
    }
    memcpy(string, s, len);
    return;
}

str::str(uint8_t *s, uint32_t len):len(len),state(0),string((uint8_t*)malloc(len)){
    if(string == NULL){
        #ifdef bitmap_debug
        printf("str init malloc error\n");
        #endif
        len = 0;
        state++;
        return;
    }
    memcpy(string, s, len);
    return;
}

str::str(str& s):len(s.len),state(s.state),string((uint8_t*)malloc(s.len)){
    if(string == NULL){
        #ifdef bitmap_debug
        printf("str init malloc error\n");
        #endif
        len = 0;
        state++;
        return;
    }
    memcpy(string, s.string, s.len);
    return;
}

str::~str(){
    free(string);
}



#endif