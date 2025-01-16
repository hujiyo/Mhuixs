#include <stdint.h>
/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.1
Email:hj18914255909@outlook.com
*/
#ifndef STREAM_H
#define STREAM_H

/*
STREAM 字节流
参照C++中的string类：
使用len来记录字节流的长度
防止出现C语言中字符串泄露的问题
*/

typedef struct STREAM {
    uint8_t *string;
    /*
    STREAM:字节流的长度
    */
    uint32_t len;
    /*
    字节流的长度
    */
} STREAM;

STREAM* makeSTREAM();
/*
获得一个字节流对象
STREAM *object = makeSTREAM();
*/

void freeSTREAM(STREAM *stream);
/*
释放一个字节流对象
freeSTREAM(object);
*/
int readSTREAM(STREAM *stream, uint32_t pos, uint8_t *bitestream, uint32_t length);
/*
从字节流中的pos位置读取长度为length的数据，并将数据存储到bitestream中
readSTREAM(object,pos,bitestream,length);
返回真实的读取长度
*/
int writeSTREAM(STREAM *stream, uint32_t pos, uint8_t *bitestream, uint32_t length);
/*
将bitestream中的数据写入到字节流中的pos位置，长度为length
如果STRAEAM中没有足够的空间，则自动扩容
writeSTREAM(object,pos,bitestream,length);
返回真实的写入位置pos
*/
#endif