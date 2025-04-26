#include "memap.hpp"
MEMAP::MEMAP(uint32_t block_size,uint32_t block_num){
    //先计算strpool的所需长度
    uint32_t strpool_len=12+block_num/8+1+block_size*block_num;
    //建立strpool
    strpool=(uint8_t*)calloc(1,strpool_len);
    if(strpool==NULL) {
        return;
    }
    memcpy(strpool, &strpool_len, sizeof(uint32_t));
    memcpy(strpool + 4, &block_size, sizeof(uint32_t));
    memcpy(strpool + 8, &block_num, sizeof(uint32_t));
}

MEMAP::~MEMAP(){
    free(strpool);
}

OFFSET MEMAP::smalloc(uint32_t len)
{
    //强烈建议len为block_size的整数倍,从而提高内存利用率
    if(len==0 || strpool==NULL || len > UINT32_MAX){
        return 0;
    }
    //获取block_size和block_num
    uint32_t block_size=0;
    uint32_t block_num=0;
    memcpy(&block_size,strpool+4,sizeof(uint32_t));
    memcpy(&block_num,strpool+8,sizeof(uint32_t));
    //计算位图区和数据区的偏移量
    uint8_t* bit_map=strpool+12;   
    uint8_t* mem=bit_map+block_num/8+1;

    uint32_t bite_num = block_num / 8;
    uint32_t bit_num_left = block_num % 8;
    /*
    bite_num是位图区前面占满部分的字节数,
    由于block_num不一定是8的倍数,所以还需要单独处理最后一个字节的位图
    */

    //计算需要的块数量，建议len为8的倍数。
    uint32_t need_block_num=len/block_size;
    if(len%block_size!=0)need_block_num++;
    /*
    STRmalloc的算法:
    遍历位图区，找到第一个足够大的连续block_num个空闲块
    找到后，把连续block_num个空闲块标记为1
    返回其偏移量
    */
    uint32_t k=0,start=0;
    for(uint32_t i=0;i<bite_num;i++){
        for(uint32_t j=0;j<8;j++){
            if((bit_map[i]&(1<<j))==0) k++;
            else k=0;
            if(k>=need_block_num){
                start=i*8+j-k+1;//包括start
                goto l;
            }
        }
    }
    for(uint32_t i=0;i<bit_num_left;i++){
        if((bit_map[bite_num]&(1<<i))==0) k++;
        else k=0;
        if(k>=need_block_num){
            start=bite_num*8+i-k+1;
            goto l;
        }
    }
    return 0;
    l://找到连续空间，把连续空间置为1
    for(uint32_t i=start;i<start+need_block_num;i++){
        bit_map[i/8]|=(1<< ( i % 8 ) );
    }
    return mem+start*block_size-strpool;//返回偏移量
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
    if (block_size == 0 || block_num == 0 ||stored_len <= 12) return merr;

    // 验证存储的长度是否合理
    uint32_t bitmap_size = block_num/8+1;
    uint32_t expected_len = 12 + bitmap_size + block_size * block_num;
    if (stored_len != expected_len) {printf("!");
        return merr;
    }
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