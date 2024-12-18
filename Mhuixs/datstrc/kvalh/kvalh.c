#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../datstrc.h"
#include "kvalhdef.h"
#include "datstrc/tblh/tblh.h"//以Mhuixs为根目录
/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
static uint32_t murmurhash(const uint8_t *data, int len, uint32_t result_bits) {
    uint32_t seed=0;
    const int nblocks = len / 4;
    for (int i = 0; i < nblocks; i++) {
        uint32_t k1 = ((uint32_t)data[i*4]     ) |
                     ((uint32_t)data[i*4 + 1] <<  8) |
                     ((uint32_t)data[i*4 + 2] << 16) |
                     ((uint32_t)data[i*4 + 3] << 24); 
        k1 *= 0xcc9e2d51;k1 = (k1 << 15) | (k1 >> 17);k1 *= 0x1b873593; 
        seed ^= k1;
        seed = (seed << 13) | (seed >> 19);
        seed = seed * 5 + 0xe6546b64;
    } 
    const uint8_t *tail = (const uint8_t*)(data + nblocks*4); 
    uint32_t k1 = 0;
    switch(len & 3) {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] <<  8;
        case 1: k1 ^= tail[0];
                k1 *= 0xcc9e2d51;k1 = (k1 << 15) | (k1 >> 17);k1 *= 0x1b873593;
                seed ^= k1;
    } 
    seed ^= len;    
    seed ^= seed >> 16;
    seed *= 0x85ebca6b;
    seed ^= seed >> 13;
    seed *= 0xc2b2ae35;
    seed ^= seed >> 16; 
    return seed%result_bits;
}
int8_t kvalh_make_kvalot(KVALOT* kvalot,char* kvalot_name,uint8_t hash_tong_num)//只能是规定的数量
{
    kvalot->keypoolROM=init_ROM;//初始化池容量
    kvalot->numof_tong=hash_tong_num;//初始化哈希表数量
    kvalot->keynum=0;//初始化键数量
    kvalot->keypool=(KEY*)calloc(init_ROM*sizeof(KEY));//创建一个键值对池
    kvalot->hash_table=(HASH_TONG*)calloc(kvalot->numof_tong*sizeof(HASH_TONG));//创建一个哈希表    
    kvalot->kvalot_name=(char*)calloc(short_string,sizeof(char));//创建一个键值对池名称
    if(!kvalot->keypool||!kvalot->hash_table||!kvalot->kvalot_name){
        free(kvalot->keypool);
        free(kvalot->hash_table);
        free(kvalot->kvalot_name);
        return err;
    }
    strcpy(kvalot->kvalot_name,kvalot_name);//复制键值对池名称
    return 0;
}

int8_t kvalh_add_key(KVALOT* kvalot,const char* key_name,uint8_t type,void* value)
{
    //先判断哈希桶是不是太少了
    if(kvalot->keynum+1>=kvalot->numof_tong*hash_k)return err;//如果键数量大于等于哈希表数量*加载因子,增加失败
    //再判断键名池有没有满，满了就对keypool_ROM进行扩容
    if(kvalot->keynum+1>=kvalot->keypoolROM){//如果键值对池容量不足,增加keypool容量
        KEY* cc_keypool=kvalot->keypool;
        kvalot->keypool=(KEY*)realloc(kvalot->keypool,(kvalot->keypoolROM+add_ROM)*sizeof(KEY));
        if(kvalot->keypool==NULL){
            kvalot->keypool=cc_keypool;//恢复原来的键值对池
            return err;
        }
        memset(kvalot->keypool+kvalot->keypoolROM,0,add_ROM*sizeof(KEY));//初始化新增的键值对池
        kvalot->keypoolROM+=add_ROM;
    }
    //对键名进行哈希，找到存储对应的哈希桶hash_index并保存    
    uint32_t hash_index=murmurhash((const uint8_t*)key_name,strlen(key_name),kvalot->numof_tong);//计算哈希表索引
    //hash_index先不要动，我们先试着把value和key都存放进去之后最后设置桶内参数
    //默认都是存放在键池keypool内的最后一个里
    //先为KEY成员keyname申请一段内存
    kvalot->keypool[kvalot->keynum].key_name=(char*)calloc(strlen(key_name)+1,sizeof(char));//创建一个键名//键名不可更改
    if(kvalot->keypool[kvalot->keynum].key_name==NULL)return err;
    strcpy(kvalot->keypool[kvalot->keynum].key_name,key_name);//复制键名
    //为KEY其他成员初始化    
    kvalot->keypool[kvalot->keynum].linkey_num=0;//初始化键连接数量
    kvalot->keypool[kvalot->keynum].linkey_coef=NULL;//初始化键连接系数数组
    kvalot->keypool[kvalot->keynum].linkey_offset=NULL;//初始化键连接偏移量数组
    kvalot->keypool[kvalot->keynum].hash_index=hash_index;//复制哈希表索引
    kvalot->keypool[kvalot->keynum].type=type;//复制键类型

    //之后我们要根据type的类型为KEY的val_addr分别处理
    //处理结束

    /*
    //1.泛化数据类型
    #define K_STRING 'a'    //字符串
    #define K_ARRAY   'b'   //数组
    #define K_STRUCT 'c'    //结构体
    //2.具象数据类型
    #define K_TABLE 'd'     //表-->结构体
    #define K_TABLOT 'e'    //表库-->结构体数组
    #define K_KEY   'f'     //键-->数据库/某键库/某键-->
    #define K_KEYLOT 'g'    //键库
    #define K_FILE  'h'     //文件
    //3.其它数据类型
    #define K_LINKLIST 'i'  //链表
    #define K_TREE   'j'   //链树
    #define K_DICT  'k'    //字典
    #define K_SET  'l'    //集合
    */
    switch (type){
        case K_STRING://这是最简单的数据类型
            kvalot->keypool[kvalot->keynum].val_addr=(char*)calloc(strlen((char*)value)+1,sizeof(char));//创建一个字符串
            if(kvalot->keypool[kvalot->keynum].val_addr==NULL)goto ERR;
            strcpy(kvalot->keypool[kvalot->keynum].val_addr,(char*)value);//复制字符串
            break;
        case K_ARRAY:
        case K_STRUCT:
        case K_TABLE:
        case K_TABLOT:
        case K_KEY:
        case K_KEYLOT:
        case K_FILE:
        case K_LINKLIST:
        case K_TREE:
        case K_DICT:
        case K_SET:
        default:goto ERR;
    }

    //接下来才是更新哈希桶内的数据
    kvalot->hash_table[hash_index].numof_key++;//哈希表中对应的哈希桶内键数量+1    
    //为哈希桶中键的偏移量数组申请更多内存
    uint32_t* cc_offsetof_key=kvalot->hash_table[hash_index].offsetof_key;
    kvalot->hash_table[hash_index].offsetof_key=(uint32_t*)realloc(kvalot->hash_table[hash_index].offsetof_key,(kvalot->hash_table[hash_index].numof_key)*sizeof(uint32_t));//增加键连接系数数组
    if(kvalot->hash_table[hash_index].offsetof_key==NULL){
        kvalot->hash_table[hash_index].offsetof_key=cc_offsetof_key;
        kvalot->hash_table[hash_index].numof_key--;//哈希表中对应的哈希桶内键数量还原
        goto ERR;
    }
    //记录这个KEY的偏移量 ————> keypool[偏移量]
    kvalot->hash_table[hash_index].offsetof_key[kvalot->hash_table[hash_index].numof_key-1]=kvalot->keynum;
    kvalot->keynum++;//键数量+1
    return 0;ERR:{
        //清空新增KEY
        free(kvalot->keypool[kvalot->keynum].key_name);
        memset(&kvalot->keypool[kvalot->keynum],0,sizeof(KEY));        
        return err;
    }
}
uint8_t kvalh_expand_kvalot(KVALOT* kvalot,uint8_t size)
{

}
uint8_t initKvalh(Kvalh* kvalh){
    kvalh->make_kvalot=kvalh_make_kvalot;

}





/*

int main() {
    KVALOT kvalot;//定义一个 键值对池 对象

    Kvalh kvalh;//定义一个操作键值对池的函数集
    initKvalh(&kvalh);//初始化函数集

    kvalh.make_kvalot(&kvalot);//创建一个键值对池

}
*/