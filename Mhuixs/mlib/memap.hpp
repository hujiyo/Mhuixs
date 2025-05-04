#ifndef MEMAP_H
#define MEMAP_H
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define merr -1

/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/

typedef uint32_t OFFSET;//偏移量
#define default_block_size 32//默认页大小
#define default_block_num 128//默认页数量

#define add_block_num_base 128//每次扩容的页数的基数

#define NULL_OFFSET 0xffffffff

/*
memap：内存池
————掌管单字节数组中内存分配和释放
注意:
1.返回的是在memap中的偏移量而不是指针
2.分配的内存地址是否符合字节对齐要求取决于block_size的大小是否是对齐字节数的倍数
2025.5:强制要求block_size和block_num为32的倍数
3.4字节对齐
*/

/*
MEMAP内部结构定义:
0 ~ 3字节                           strpool_len的长度
4 ~ 7字节                           block_size的长度
8 ~ 11字节                          block_num的长度
12 ~ 12+(block_num/8+1)-1字节           位图区
12+(block_num/8+1) ~ strpool_len          数据区

关键位置:
位图区偏移量:12
数据区偏移量:12+ block_num/8
offset从0开始，返回的是在memap的数据区的偏移量
*/

struct MEMAP{
    uint8_t* strpool;
    MEMAP(uint32_t block_size,uint32_t block_num);//构造函数，block_size为页大小，block_num为页数量,强制要求block_size和block_num为32的倍数
    ~MEMAP();
    OFFSET smalloc(uint32_t len);//分配内存，建议len为block_size的整数倍以提高内存利用率
    void sfree(OFFSET offset,uint32_t len);//释放内存
    int iserr(OFFSET offset);//检查偏移量是否合法
    int iserr();//检查内存池是否出错
    uint8_t* addr(OFFSET offset) {        
        return strpool+12+(*(uint32_t*)(strpool + 8))/8 + offset;//通过偏移量获取指针地址,默认offset合法
    }
};


/*************************************************************************
**  由于功能实现的代码量比较短，不需要分成头文件和源文件，我就直接用这道注释隔开，
**  这道注释上面部分相当于是头文件，下面是函数的实现，相当于是源文件。
**************************************************************************/

MEMAP::MEMAP(uint32_t block_size,uint32_t block_num){
    //强制要求block_size和block_num为32的倍数
    if(block_size%32!=0 || block_num%32!=0){
        block_size=default_block_size;  
        block_num=default_block_num;
    }
    uint32_t strpool_len=12+block_num/8+block_size*block_num;//先计算strpool的所需长度
    strpool=(uint8_t*)calloc(1,strpool_len);
    if(strpool==NULL) {
        #ifdef bitmap_debug
        printf("MEMAP::MEMAP:MEMAP calloc error\n");
        #endif
        return;
    }
    *(uint32_t*)strpool= strpool_len;
    *(uint32_t*)(strpool+4)= block_size;
    *(uint32_t*)(strpool+8)= block_num;
}

MEMAP::~MEMAP(){
    free(strpool);
}

OFFSET MEMAP::smalloc(uint32_t len){
    //强烈建议len为block_size的整数倍,从而提高内存利用率
    if(len==0 || strpool==NULL) return NULL_OFFSET;

    /*
    smalloc的算法:
    遍历位图区，找到第一个足够大的连续block_num个空闲块
    找到后，把连续block_num个空闲块标记为1
    返回其偏移量
    */
    
    //获取block_size和block_num
    uint32_t block_size = *(uint32_t*)(strpool + 4);
    uint32_t block_num = *(uint32_t*)(strpool + 8);

    uint8_t* bit_map=strpool+12;//位图区指针
    uint32_t bitmap_size = block_num / 8;//位图区的字节数
    uint32_t need_block_num = (len + block_size - 1) / block_size;//计算需要的块数量.    
    
    uint32_t k=0,start=0;//k为连续空闲块的数量，start为连续空闲块的起始位置
    for(uint32_t i=0;i<bitmap_size;i++){
        for(uint32_t j=0;j<8;j++){
            if((bit_map[i]&(1<<j))==0) k++;
            else k=0;
            if(k==need_block_num){
                start=i*8+j-need_block_num+1;//包括start
                //找到连续空间，把连续空间置为1
                for(uint32_t ii=start;ii<start+need_block_num;ii++){
                    bit_map[ii>>3]|=(1<< ( ii &7 ) );//ii/8可以用右移3位来代替,ii%8可以用ii&7来代替
                }
                return start*block_size;//返回偏移量
            }
        }
    }

    //如果没有找到足够大的连续block_num个空闲块，则扩容
    uint32_t need_block=need_block_num - k;//需要的块数量
    //向上取整到add_block_num_base的倍数
    uint32_t add_block_num= ((need_block + add_block_num_base - 1) / add_block_num_base)*add_block_num_base;    
    uint32_t new_block_num = block_num + add_block_num;//新的块数量
    uint32_t new_bitmap_size = new_block_num / 8;//新的位图区的字节数
    uint32_t new_strpool_len = 12 + new_bitmap_size + new_block_num * block_size;//新的strpool的长度
    
    uint8_t* new_strpool = (uint8_t*)calloc(1,new_strpool_len);
    if(new_strpool==NULL) {
        printf("MEMAP::smalloc:MEMAP re calloc error\n");
        return NULL_OFFSET;
    }
    //复制数据
    memcpy(new_strpool, strpool, 12 + bitmap_size);//复制HEAD和位图区
    memcpy(new_strpool + 12 + new_bitmap_size, bit_map + bitmap_size, block_num * block_size);//复制数据区
    //释放旧内存
    free(strpool);
    //更新strpool
    strpool = new_strpool;
    *(uint32_t*)new_strpool = new_strpool_len;//更新strpool_len
    *(uint32_t*)(new_strpool+8) = new_block_num;//更新block_num

    //更新位图区指针后继续搜索
    bit_map=new_strpool+12;    
    for(uint32_t i=bitmap_size;i<new_bitmap_size;i++){//初始化新的位图区
        for(uint32_t j=0;j<8;j++){
            if((bit_map[i]&(1<<j))==0) k++;
            else k=0;
            if(k==need_block_num){
                start=i*8+j-need_block_num+1;//包括start
                for(uint32_t ii=start;ii<start+need_block_num;ii++){
                    bit_map[ii>>3]|=(1<< ( ii &7 ) ); 
                }
                return start*block_size;//返回偏移量
            }
        }
    }
    return NULL_OFFSET;
}

void MEMAP::sfree(OFFSET offset,uint32_t len)//通过偏移量释放内存
{
    /*
    STRfree的算法:直接把块对应的位清零
    使用STRfree时必须保证offset是由STRmalloc正确分配的。
    同时，必须保证len是用STRmalloc分配内存时的len。
    */
    if( !len || !strpool ) return;

    uint32_t block_size=*(uint32_t*)(strpool+4);
    uint8_t *bit_map = strpool + 12;//位图区指针

    uint32_t need_block_num=( len + block_size - 1) / block_size;//计算需要标记的块数量.

    if(offset%block_size!=0){
        printf("MEMAP::sfree:error,p_mem is not aligned to block_size\n");
        return;//如果你的释放地址不是块的倍数，直接返回
    }

    uint32_t i = offset/block_size;//i是块的编号 从0开始

    for(uint32_t j=0;j<need_block_num;i++,j++){
        bit_map[i>>3]&=~(1<<(i&7));
    }
}

int MEMAP::iserr(){
    if (!strpool) return merr; // 内存未分配

    uint32_t stored_len = *(uint32_t*)strpool;
    uint32_t block_size=*(uint32_t*)(strpool+4);
    uint32_t block_num=*(uint32_t*)(strpool+8);

    // 检查参数有效性
    if (block_size%32 != 0 || block_num%32 != 0 || stored_len < 12) return merr;

    // 验证存储的长度是否合理
    if (stored_len != 12 + block_num / 8 + block_size * block_num ) return merr;
    return 0;
}

int MEMAP::iserr(OFFSET offset){
    uint32_t block_size = *(uint32_t*)(strpool + 4);
    uint32_t block_num = *(uint32_t*)(strpool + 8);
    /*
    offset永远都是block_size的整数倍,比如6400个块,每个块64字节,
    则offset的取值范围是0,1,....6399
    */
    if(offset==NULL_OFFSET || offset%block_size!=0 || offset>=block_num) return merr;
    // 接下来就是检查offset是否已经分配
    //6400个块,每个块占用1bit,则需要6400/8=800字节
 
    if( ((strpool+12)[offset/8]&(1<<(offset&7))) ) return 0;//分配的合法偏移量

    return 1;//未分配的合法偏移量
}

#endif
