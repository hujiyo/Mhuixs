#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
#define err 1

typedef uint8_t* BITMAP;
//BITMAP 指向的前8个字节存放的是bitmap的大小，单位是bit

BITMAP getBITMAP(uint64_t size)//偏移量:0 ~ size-1
{
    if(size == 0)return NULL;
    BITMAP bitmap=(BITMAP)calloc(1,(size+7)/8 + sizeof(uint64_t));//bitmap的大小：(size+7)/8 + 8 = (size + 15)/8
    if(bitmap == NULL)return NULL;
    *(uint64_t*)bitmap = size;
    return bitmap;
}
int getBIT(BITMAP bitmap,uint64_t offset)//偏移量从0开始
{
    if(bitmap == NULL)return err;
    if(offset > *(uint64_t*)bitmap)return err;
    uint8_t* first = bitmap + sizeof(uint64_t);
    return (first[offset/8] >> (offset%8)) & 1;
}

int setBIT(BITMAP bitmap,uint64_t offset,uint8_t value)
{
    if(bitmap == NULL)return err;
    if(offset > *(uint64_t*)bitmap)return err;
    uint8_t* first = bitmap + sizeof(uint64_t);

    if(value == 0)first[offset/8] &= ~(1 << (offset%8));//与等于 将该位置为0
    else first[offset/8] |= 1 << (offset%8);//或等于 将该位置为1
    return 0;
}
int setBITs(BITMAP bitmap,const char* data_stream,uint64_t start,uint64_t end)//包括start和end
{
    if(bitmap == NULL || data_stream == NULL)return err;
    if(start > end || end > *(uint64_t*)bitmap || start > *(uint64_t*)bitmap)return err;
    uint8_t* first = bitmap + sizeof(uint64_t);
    for(;start <= end;start++){
        if(data_stream[start] == '0')first[start/8] &= ~(1 << (start%8));//与等于 将该位置为0
        else first[start/8] |= 1 << (start%8);//或等于 将该位置为1
    }
    return 0;
}
uint64_t countBIT(BITMAP bitmap,uint64_t start,uint64_t end)//包括start和end,数1的个数
{
    if(bitmap == NULL)return 0;
    if(start > end || end > *(uint64_t*)bitmap || start > *(uint64_t*)bitmap)return 0;
    uint8_t* first = bitmap + sizeof(uint64_t);
    uint64_t num = 0;
    for(;start <= end;start++){
        if(first[start/8] & (1 << (start%8)))num++;
    }
    return num;
}
uint64_t retuoffset(BITMAP bitmap,uint64_t start,uint64_t end)//包括start和end,返回第一个1的位置
{
    if(bitmap == NULL)return 0;
    if(start > end || end > *(uint64_t*)bitmap || start > *(uint64_t*)bitmap)return 0;
    uint8_t* first = bitmap + sizeof(uint64_t);
    for(;start <= end;start++){
        if(first[start/8] & (1 << (start%8)))return start;
    }
}
void printBITMAP(BITMAP bitmap)
{
    if(bitmap == NULL)return;
    uint64_t size = *(uint64_t*)bitmap;
    for(uint64_t i = 0;i < size;i++){
        printf("%d",getBIT(bitmap,i));
    }
    printf("\n");
}