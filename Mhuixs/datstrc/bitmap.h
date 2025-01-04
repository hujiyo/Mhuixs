/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
#ifndef BITMAP_H
#define BITMAP_H
/*
reids支持位图这种数据结构，
用于存储大量比特数据，以节省内存和提高效率。
*/
typedef uint8_t BITMAP;

BITMAP* initBITMAP(uint64_t size);
/*
创建一个位图对象，位图大小为size位。
BITMAP *bitmap = initBITMAP(size);
*/
void freeBITMAP(BITMAP* bitmap);
/*
释放一个位图对象。
freeBITMAP(bitmap);
*/
int getBIT(BITMAP* bitmap,uint64_t offset);
/*
获取位图中偏移量为offset的位的值。
返回值：0或1
*/
int setBIT(BITMAP* bitmap,uint64_t offset,uint8_t value);
/*
设置位图中偏移量为offset的位的值为value。
返回值：0：成功  -1：失败
*/
int setBITs(BITMAP* bitmap,const char* data_stream,uint64_t start,uint64_t end);
/*
将data_stream中的数据设置到位图中，从start偏移量到end偏移量。
返回值：0：成功  -1：失败
*/
uint64_t countBIT(BITMAP* bitmap,uint64_t start,uint64_t end);
/*
统计位图中从start偏移量到end偏移量之间1的个数。
返回值：1的个数
*/
uint64_t retuoffset(BITMAP* bitmap,uint64_t start,uint64_t end);
/*
返回位图中从start偏移量到end偏移量之间第一个为1的位的偏移量。
返回值：偏移量  -1：未找到
*/
void printBITMAP(BITMAP* bitmap);

#endif