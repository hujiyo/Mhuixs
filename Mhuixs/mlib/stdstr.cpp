#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "stdstr.h"

/*
#版权所有 (c) HuJi 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/

#define end (str){NULL,0}
#define err -1

str::str(char* s):len(strlen(s)),state(0),string((uint8_t*)malloc(len)){
    if(string == NULL){
        #ifdef bitmap_debug
        printf("str init malloc error\n");
        #endif
        len = 0;
        state++;
    }
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
str::~str(){
    free(string);
}
int str::read(uint32_t pos, uint8_t *bitestream, uint32_t length)
{
    //从pos开始读取length个字节到bitestream中
    if (bitestream == NULL || pos >= len) {
        return err;
    }
    // 计算实际要读取的长度
    uint32_t available = stream->len - pos;
    uint32_t read_length = (length < available) ? length : available;
    // 复制数据到bitestream
    memcpy(bitestream, stream->string + pos, read_length);
    return read_length;
}
int swrite(str* stream, uint32_t pos, uint8_t *bitestream, uint32_t length)
{
    if(stream == NULL || bitestream == NULL || length == 0){
        return err;
    }
    if (pos > stream->len) {
        pos = stream->len; // 如果位置超出范围，则追加到末尾
    }
    // 如果超范围，则扩展内存以容纳新数据
    if (pos + length > stream->len) {
        uint8_t *new_string = (uint8_t *)realloc(stream->string, pos + length);
        if (new_string == NULL) {
            return err;
        }
        stream->string = new_string;
        stream->len = pos + length;
    }
    // 将新数据复制到指定位置
    memcpy(stream->string + pos, bitestream, length);
    stream->len = pos + length;//更新长度
    return pos;
}
int sset(str* stream, uint32_t st, uint32_t ed, uint8_t byte)
{
    if (stream->string == NULL || st > ed) {
        return err;
    }
    // 如果end超出当前长度，扩展字节流
    if (ed >= stream->len) {
        uint8_t *new_string = (uint8_t *)realloc(stream->string, ed + 1);
        if (new_string == NULL) {
            return err;
        }
        stream->string = new_string;
        stream->len = ed + 1;        
    }
    // 填充指定范围的字节
    memset(stream->string + st, byte, ed - st + 1);
    return stream->len;
}

uint8_t* strtos(str stream)
{
    swrite(&stream,stream.len,(uint8_t*)"\0",1);
    return (uint8_t*)stream.string;
}
int append(str* stream, uint8_t *bitestream, uint32_t length)
{
    if (stream->string == NULL || bitestream == NULL || length == 0) {
        return err;
    }
    // 扩展内存以容纳新数据
    uint8_t *new_string = (uint8_t *)realloc(stream->string, stream->len + length);
    if (new_string == NULL) {
        return err;
    }
    stream->string = new_string;
    stream->len += length;
    memcpy(stream->string + stream->len - length, bitestream, length);
    return stream->len;
}
void ptf(const str stream0,...){
    /*
    将多个字节流对象拼接成一个字节流对象,并输出到控制台
    sprint(&stream0,stream1,stream2,stream3,...,end);
    */
    va_list args;//定义一个va_list类型的变量args
    va_start(args,stream0);//初始化args,准备开始获取可变参数
    for(int i = 0;i < stream0.len;i++){
        printf("%c",stream0.string[i]); 
    }
    for(;;){
        str temp = va_arg(args,str);//获取可变参数
        if(temp.string == NULL && temp.len == 0){
            break;
        }
        for(int i = 0;i < temp.len;i++){
            printf("%c",temp.string[i]);
        }
    }
    va_end(args);//arg置为NULL
    return;
}
