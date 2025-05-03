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

/*
memap：内存池
————掌管单字节数组中内存分配和释放
注意:
1.返回的是在memap中的偏移量而不是指针
2.分配的内存地址是否符合字节对齐要求取决于block_size的大小是否是对齐字节数的倍数
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
数据区偏移量:12+(block_num/8+1)
*/

struct MEMAP{
    uint8_t* strpool;
    MEMAP(uint32_t block_size,uint32_t block_num);//构造函数，block_size为页大小，block_num为页数量
    ~MEMAP();
    OFFSET smalloc(uint32_t len);//分配内存，建议len为block_size的整数倍以提高内存利用率
    void sfree(OFFSET offset,uint32_t len);//释放内存
    int iserr(OFFSET offset);//检查偏移量是否合法
    int iserr();//检查内存池是否出错
    uint8_t* addr(OFFSET offset);//通过偏移量获取指针地址
private:
    bool expand_pool(uint32_t need_block_num);
};


/*************************************************************************
**  由于功能实现的代码量比较短，不需要分成头文件和源文件，我就直接用这道注释隔开，
**  这道注释上面部分相当于是头文件，下面是函数的实现，相当于是源文件。
**************************************************************************/


MEMAP::MEMAP(uint32_t block_size,uint32_t block_num){
    //先计算strpool的所需长度
    uint32_t strpool_len=12+block_num/8+1+block_size*block_num;
    //建立strpool
    strpool=(uint8_t*)calloc(1,strpool_len);
    if(strpool==NULL) {
        printf("MEMAP error\n");
        return;
    }
    memcpy(strpool, &strpool_len, sizeof(uint32_t));
    memcpy(strpool + 4, &block_size, sizeof(uint32_t));
    memcpy(strpool + 8, &block_num, sizeof(uint32_t));
}

MEMAP::~MEMAP(){
    free(strpool);
}

OFFSET MEMAP::smalloc(uint32_t len) {
    if (len == 0 || !strpool) return 0;

    uint32_t block_size, block_num;
    memcpy(&block_size, strpool + 4, sizeof(uint32_t));
    memcpy(&block_num, strpool + 8, sizeof(uint32_t));

    uint32_t need_block_num = (len + block_size - 1) / block_size;

    uint8_t* bit_map = strpool + 12;
    uint8_t* mem = bit_map + (block_num + 7) / 8;

    uint32_t k = 0, start = 0;
    bool found = false;
    uint32_t total_bits = block_num;
    uint32_t byte_count = total_bits / 8;
    uint32_t remaining_bits = total_bits % 8;

    // 搜索可用块
    for (uint32_t i = 0; i < byte_count; ++i) {
        for (uint8_t j = 0; j < 8; ++j) {
            if (!(bit_map[i] & (1 << j))) {
                if (++k >= need_block_num) {
                    start = i * 8 + j - k + 1;
                    found = true;
                    goto allocate;
                }
                continue;
            }
            k = 0;
        }
    }

    // 检查剩余位
    if (remaining_bits > 0) {
        for (uint8_t j = 0; j < remaining_bits; ++j) {
            if (!(bit_map[byte_count] & (1 << j))) {
                if (++k >= need_block_num) {
                    start = byte_count * 8 + j - k + 1;
                    found = true;
                    goto allocate;
                }
                continue;
            }
            k = 0;
        }
    }

    // 扩容并重试
    if (!found && expand_pool(need_block_num)) {
        // 更新参数
        memcpy(&block_num, strpool + 8, sizeof(uint32_t));
        bit_map = strpool + 12;
        mem = bit_map + (block_num + 7) / 8;
        total_bits = block_num;
        byte_count = total_bits / 8;
        remaining_bits = total_bits % 8;

        // 重新搜索
        k = 0;
        for (uint32_t i = 0; i < byte_count; ++i) {
            for (uint8_t j = 0; j < 8; ++j) {
                if (!(bit_map[i] & (1 << j))) {
                    if (++k >= need_block_num) {
                        start = i * 8 + j - k + 1;
                        found = true;
                        goto allocate;
                    }
                    continue;
                }
                k = 0;
            }
        }

        if (remaining_bits > 0) {
            for (uint8_t j = 0; j < remaining_bits; ++j) {
                if (!(bit_map[byte_count] & (1 << j))) {
                    if (++k >= need_block_num) {
                        start = byte_count * 8 + j - k + 1;
                        found = true;
                        goto allocate;
                    }
                    continue;
                }
                k = 0;
            }
        }
    }
    if (!found) return 0;
allocate:
    // 标记块为已用
    for (uint32_t i = start; i < start + need_block_num; ++i) {
        bit_map[i / 8] |= (1 << (i % 8));
    }
    return mem + start * block_size - strpool;
}

void MEMAP::sfree(OFFSET offset,uint32_t len)//通过偏移量释放内存
{
    /*
    STRfree的算法:直接把块对应的位清零
    使用STRfree时必须保证offset是由STRmalloc正确分配的。
    同时，必须保证len是用STRmalloc分配内存时的len。
    */
    if(len==0 || strpool==NULL || len > UINT32_MAX){
        return;
    }
    uint8_t *p_mem = strpool + offset;
    uint8_t *bit_map = strpool+12;
    
    uint32_t block_size=0;
    uint32_t block_num=0;    
    memcpy(&block_size,strpool+4,sizeof(uint32_t));
    memcpy(&block_num,strpool+8,sizeof(uint32_t));

    uint8_t* mem=bit_map+block_num/8+1;

    uint32_t need_block_num=len/block_size;
    if(len%block_size!=0)need_block_num++;

    if((p_mem - mem)%block_size!=0){
        return;//如果你的释放地址不是块的倍数，直接返回
    }
    uint32_t i = (p_mem - mem)/block_size;//i是块的编号 从0开始

    for(uint32_t j=0;j<need_block_num;i++,j++){
        bit_map[i/8]&=~(1<<(i%8));
    }
}

int MEMAP::iserr(){
    if (!strpool) return merr; // 内存未分配

    uint32_t stored_len, block_size, block_num;
    memcpy(&stored_len, strpool, sizeof(uint32_t));
    memcpy(&block_size, strpool +4, sizeof(uint32_t));
    memcpy(&block_num, strpool +8, sizeof(uint32_t));

    // 检查参数有效性
    if (block_size == 0 || block_num == 0) return merr;
    if (stored_len < 12) return merr; // 最小头部长度不足

    // 验证存储的长度是否合理
    uint32_t bitmap_size = (block_num +7) / 8;
    uint32_t expected_len = 12 + bitmap_size + block_size * block_num;
    if (stored_len != expected_len) return merr;

    return 0;
}

int MEMAP::iserr(OFFSET offset){
    if (strpool == NULL) return merr;

    uint32_t strpool_len, block_size, block_num;
    memcpy(&strpool_len, strpool, sizeof(uint32_t));
    memcpy(&block_size, strpool + 4, sizeof(uint32_t));
    memcpy(&block_num, strpool + 8, sizeof(uint32_t));

    // 计算数据区的起始和结束位置
    uint32_t bitmap_size = (block_num + 7) / 8; // 向上取整到字节
    uint32_t data_start = 12 + bitmap_size;
    uint32_t data_end = data_start + block_size * block_num;

    // 检查偏移是否在数据区内
    if (offset < data_start || offset >= data_end) return merr;
    // 检查是否对齐到block_size的倍数
    if ((offset - data_start) % block_size != 0) return merr;

    // 计算块号
    uint32_t block = (offset - data_start) / block_size;

    // 验证块号是否在有效范围内
    if (block >= block_num) return false;

    // 验证位图是否标记为已分配
    uint8_t* bit_map = strpool + 12; // 位图起始地址
    if (!(bit_map[block / 8] & (1 << (block % 8)))) {
        return false; // 未分配
    }
    return 0;
}

uint8_t* MEMAP::addr(OFFSET offset) {
    // 内存池未初始化或分配失败 偏移量无效
    if (strpool == NULL || iserr(offset) ) return NULL;  
    return strpool + offset;
}

bool MEMAP::expand_pool(uint32_t need_block_num) {
    uint32_t block_size, block_num;
    memcpy(&block_size, strpool + 4, sizeof(uint32_t));
    memcpy(&block_num, strpool + 8, sizeof(uint32_t));

    // 计算需要新增的块数
    uint32_t current_blocks_needed = need_block_num;
    uint32_t added_blocks = 200;
    while (added_blocks < current_blocks_needed) {
        added_blocks += 200;
    }
    uint32_t new_block_num = block_num + added_blocks;

    // 计算新内存池大小
    uint32_t new_bitmap_size = (new_block_num + 7) / 8;
    uint32_t new_data_size = new_block_num * block_size;
    uint32_t new_strpool_len = 12 + new_bitmap_size + new_data_size;

    uint8_t* new_strpool = (uint8_t*)calloc(1, new_strpool_len);
    if (!new_strpool) return false;

    // 设置新头部
    memcpy(new_strpool, &new_strpool_len, sizeof(uint32_t));
    memcpy(new_strpool + 4, &block_size, sizeof(uint32_t));
    memcpy(new_strpool + 8, &new_block_num, sizeof(uint32_t));

    // 复制位图区
    uint32_t old_bitmap_size = (block_num + 7) / 8;
    uint8_t* old_bit_map = strpool + 12;
    uint8_t* new_bit_map = new_strpool + 12;
    memcpy(new_bit_map, old_bit_map, old_bitmap_size);

    // 复制数据区
    uint8_t* old_mem = old_bit_map + old_bitmap_size;
    uint8_t* new_mem = new_bit_map + new_bitmap_size;
    memcpy(new_mem, old_mem, block_num * block_size);

    // 替换旧内存池
    free(strpool);
    strpool = new_strpool;
    return true;
}

#endif
