#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define err -1

/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
/*
BITMAP 结构
0~7             bit数
8~(bit/8+7)     数据区大小
*/

typedef uint8_t BITMAP;


BITMAP* initBITMAP(uint64_t size)//偏移量:0 ~ size-1
{
    if(size == 0){
        return NULL;
    }
    BITMAP* bitmap=(BITMAP*)calloc(1,(size+7)/8 + sizeof(uint64_t));//bitmap的大小：(size+7)/8 + 8 = (size + 15)/8
    if(bitmap == NULL){
        return NULL;
    }
    *(uint64_t*)bitmap = size; // 设置bitmap的大小

    return bitmap;
}
void freeBITMAP(BITMAP* bitmap){
    free(bitmap); // 释放bitmap内存
}
int getBIT(BITMAP* bitmap,uint64_t offset)//偏移量从0开始
{
    if(bitmap == NULL){
        return err;
    }
    if(offset > *(uint64_t*)bitmap){
        return err;
    }
    uint8_t* first = bitmap + sizeof(uint64_t); // 指向数据区
    return (first[offset/8] >> (offset%8)) & 1; // 获取指定位置的bit值
}
int setBIT(BITMAP* bitmap,uint64_t offset,uint8_t value)
{
    if(bitmap == NULL){
        return err;
    }
    if(offset > *(uint64_t*)bitmap){
        return err;
    }
    uint8_t* first = bitmap + sizeof(uint64_t); // 指向数据区
    if(value == 0){
        first[offset/8] &= ~(1 << (offset%8)); // 将该位置为0
    }
    else{ 
        first[offset/8] |= 1 << (offset%8); // 将该位置为1
    }
    return 0; // 成功设置，返回0
}
int setBITs(BITMAP* bitmap,const char* data_stream,uint64_t start,uint64_t end)//包括start和end
{
    if(bitmap == NULL || data_stream == NULL){
        return err; 
    }
    if(start > end || end > *(uint64_t*)bitmap || start > *(uint64_t*)bitmap){
        return err;
    }
    uint8_t* first = bitmap + sizeof(uint64_t); // 指向数据区
    for(;start <= end;start++){
        if(data_stream[start] == '0'){
            first[start/8] &= ~(1 << (start%8)); // 将该位置为0
        }
        else {
            first[start/8] |= 1 << (start%8); // 将该位置为1
        }
    }
    return 0; // 成功设置，返回0
}
uint64_t countBIT(BITMAP* bitmap,uint64_t start,uint64_t end)//包括start和end,数1的个数
{
    if(bitmap == NULL){
        return 0;
    }
    if(start > end || end > *(uint64_t*)bitmap || start > *(uint64_t*)bitmap){
        return 0;
    }
    uint8_t* first = bitmap + sizeof(uint64_t); // 指向数据区
    uint64_t num = 0; // 初始化计数器
    for(;start <= end;start++){
        if(first[start/8] & (1 << (start%8))){
            num++; // 如果该位为1，计数器加1
        }
    }
    return num; // 返回计数结果
}
int64_t retuoffset(BITMAP* bitmap,uint64_t start,uint64_t end)//包括start和end,返回范围内第一个1的位置
{
    if(bitmap == NULL){
        return 0;
    }
    if(start > end || end > *(uint64_t*)bitmap || start > *(uint64_t*)bitmap){
        return 0;
    }
    uint8_t* first = bitmap + sizeof(uint64_t); // 指向数据区
    if(end-start > 32){
        goto l;
    }
    //逐位查找
    for(;start <= end;start++){
        if(first[start/8] & (1 << (start%8))){
            return start; // 返回第一个1的位置
        }
    }
    return err; // 如果没有找到，则返回0
    l://逐字节查找
    //先找start所在的字节
    int st_bit = start%8;
    for(;st_bit < 8;st_bit++){
        if(first[start/8] & (128 >> st_bit)){//1000 0000 >> st_byte
            return (start/8)*8 + st_bit; // 返回第一个1的位置
        }
    }
    //再找中间的字节
    uint32_t st_byte = start/8+1;//开始字节
    uint32_t en_byte = end/8-1;//结束字节
    for(;st_byte <= en_byte;st_byte++){
        if(first[st_byte]!= 0){
            for(uint8_t i = 0;i < 8;i++){
                if(first[st_byte] & (128 >> i)){
                    return st_byte*8 + i; // 返回第一个1的位置
                }
            }
        }
    }
    //再找end所在的字节
    int en_byte = end%8;
    for(uint8_t i = 0;i <= en_byte;i++){
        if(first[end/8] & (128 >> i)){
            return (end/8)*8 + i; // 返回第一个1的位置
        }
    }
    return err; // 如果没有找到，则返回0
}

void printBITMAP(BITMAP* bitmap)
{
    if(bitmap == NULL){
        return;
    }
    uint64_t size = *(uint64_t*)bitmap; // 获取bitmap的大小
    for(uint64_t i = 0;i < size;i++){
        printf("%d",getBIT(bitmap,i)); // 打印每个bit的值
    }
    printf("\n"); // 默认换行
}