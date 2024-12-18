#ifndef STRMAP_H
#define STRMAP_H
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
/*
数组地图————掌管单字节数组内存分配和释放
返回的是下标(也就是在STRPOOL中的偏移量)

//0-3字节                   strpool_len的长度
//4-7字节                   block_size的长度
//8-11字节                  block_num的长度
//12-12+(block_num/8+1)字节    位图
数据区偏移量: 12+block_num/8+1
*/
typedef uint8_t* STRPOOL;//字符串池指针
typedef uint32_t OFFSET;//偏移量
STRPOOL build_strpool(uint32_t block_size,uint32_t block_num)
{
    uint32_t strpool_len=12+block_num/8+1+block_size*block_num;
    STRPOOL strpool=(STRPOOL)calloc(strpool_len,1);
    if(strpool==NULL)return NULL;
    memcpy(strpool, &strpool_len, sizeof(uint32_t));
    memcpy(strpool + 4, &block_size, sizeof(uint32_t));
    memcpy(strpool + 8, &block_num, sizeof(uint32_t));
    return strpool;
}
OFFSET STRmalloc(STRPOOL strpool,uint32_t len)//建议len为block_size的整数倍,从而提高内存利用率
{
    if(len==0 || strpool==NULL)return 0;
    uint32_t block_size=0,block_num=0;
    uint8_t* bit_map=strpool+12;    
    memcpy(&block_size,strpool+4,sizeof(uint32_t));
    memcpy(&block_num,strpool+8,sizeof(uint32_t));
    uint8_t* mem=bit_map+block_num/8+1;
    uint32_t bite_num = block_num / 8;
    uint32_t bit_num_left = block_num % 8;
    uint32_t goal_block_num=len/block_size;
    if(len%block_size!=0)goal_block_num++;
    uint32_t k=0,start=0;
    for(uint32_t i=0;i<bite_num;i++){
        for(uint32_t j=0;j<8;j++){
            if((bit_map[i]&(1<<j))==0)k++;else k=0;
            if(k>=goal_block_num){
                start=i*8+j-k+1;//包括start
                goto l;
            }
        }
    }
    for(uint32_t i=0;i<bit_num_left;i++){
        if((bit_map[bite_num]&(1<<i))==0)k++;else k=0;        
        if(k>=goal_block_num){
            start=bite_num*8+i-k+1;
            goto l;
        }
    }
    return 0;
    l://找到连续空间，把连续空间置为1
    for(uint32_t i=start;i<start+goal_block_num;i++){
        bit_map[i/8]|=(1<< ( i % 8 ) );
    }
    return mem+start*block_size-strpool;//返回偏移量
}
void STRfree(STRPOOL strpool,OFFSET offset,uint32_t len)//通过偏移量释放内存
{
    uint8_t* p_mem= strpool + offset, *bit_map=strpool+12;
    if(len==0 || strpool==NULL)return;
    uint32_t block_size=0,block_num=0;
    memcpy(&block_size,strpool+4,sizeof(uint32_t));
    memcpy(&block_num,strpool+8,sizeof(uint32_t));
    uint8_t* mem=bit_map+block_num/8+1;
    uint32_t goal_block_num=len/block_size;
    if(len%block_size!=0)goal_block_num++;
    if((p_mem - mem)%block_size!=0)return;//如果你的释放地址不是块的倍数，直接返回
    uint32_t i = (p_mem - mem)/block_size;//i是块的编号 从0开始
    for(uint32_t j=0;j<goal_block_num;i++,j++){
        bit_map[i/8]&=~(1<<(i%8));
    }
}

#endif