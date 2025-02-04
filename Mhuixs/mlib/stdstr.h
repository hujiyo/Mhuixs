#include <stdint.h>
#include <stdarg.h>
/*
#版权所有 (c) HuJi 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
#ifndef STDSTR_H
#define STDSTR_H

#define end (str){NULL,0}


/*
str 字节流类型
参照C++中的string类：
使用len来记录字节流的长度
防止出现C语言中字符串泄露的问题
*/

typedef struct str {
    uint8_t *string;//STREAM:字节流的长度
    uint32_t len;//字节流的长度
} str,STREAM;

str* stostr(uint8_t *string, uint32_t len);
/*
获得一个字节流对象指针,内容为string,长度为len
stostr:将C字符串s变成to为str

返回的STREAM不接管C字符串的所有权
只是复制了全新的内存
*/
str splice(str stream0,...);
/*
将多个字节流对象拼接成一个字节流对象
splice(stream0,stream1,stream2,...,end);
返回一个字节流对象
*/
void sfree(str* stream,...);
/*
释放一个字节流对象
sfree(&stream0,&stream1,&stream2,&stream3,...,NULL);
*/
int sread(str* stream, uint32_t pos, uint8_t *bitestream, uint32_t length); // 修改: 参数改为指针
/*
从字节流中的pos位置读取长度为length的数据，并将数据存储到bitestream中
sread(object,pos,bitestream,length);
返回真实的读取长度
*/
int swrite(str* stream, uint32_t pos, uint8_t *bitestream, uint32_t length); // 修改: 参数改为指针
/*
将bitestream中的数据写入到字节流中的pos位置，长度为length
如果STRAEAM中没有足够的空间，则自动扩容
swrite(object,pos,bitestream,length);
返回真实的写入位置pos
*/
uint8_t* strtos(str stream); // 新增: 将str转换为C字符串

#endif