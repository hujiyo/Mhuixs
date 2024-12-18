/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
/*
这个内部头文件用于keval库的基本定义
用户不需要包含这个头文件
*/
#ifndef KVALHDEF_H
#define KVALHDEF_H

#include <stdint.h>
#include "../datstrc.h"
#define key_name_len 300

#define hash_tong_1024ge        1024            //2^10
#define hash_tong_16384ge       16384           //2^14
#define hash_tong_65536ge       65536           //2^16
#define hash_tong_1048576ge     1048576         //2^20
#define hash_tong_16777216ge    16777216        //2^24
#define hash_k 0.75                             //哈希表装载因子

#define init_ROM 300
#define add_ROM 200
/*
kvalh
与redis相比，keval库的键值对数据结构具有可构建关系的特性
*/
//1.泛化数据类型 用于储存数据，不属于数据结构
#define K_STREAM        'h'    //字节流串

//2.具象数据类型 用于储存另一个数据结构的引用
//表可单独存在，但是键不可以单独存在(它不是数据结构)，它必须依存于键库KVALOT,不能单独被引用
#define K_TABLE         M_TABLE         //表-->TABLE结构体              //对表进行引用
#define K_KEYLOT        M_KEYLOT        //键库-->KVALOT结构体           //对键库进行引用
#define K_STACK         M_STACK         //栈-->STACK结构体              //对栈进行引用
#define K_QUEUE         M_QUEUE         //队列-->QUEUE结构体            //对队列进行引用
#define K_LIST          M_LIST          //列表-->LIST结构体             //对列表进行引用
#define K_BITMAP        M_BITMAP        //位图-->BITMAP结构体           //对位图进行引用
//3.钩子
#define K_HOOK          'g'     //钩子-->HOOK结构体                     //对钩子进行引用

//支持自辐射神经网状式连接结构的“键值对数据结构”--->单向连接+连接系数 特点：支持复杂的网络结构和关系查询，如社交网络分析、路径查找等。
typedef struct KEY{
    char* key_name;//指向一个key名称存放的地址
    char type;//value数据类型
    void* val_addr;//value数据地址或链接
    uint32_t* linkey_offset;//数组,存放与当前key连接的key的偏移量
    uint32_t* linkey_coef;//数组,存放与当前key连接的key的连接系数
    uint32_t linkey_num;//与当前key连接的key数量
    uint32_t hash_index;//当前key在哈希表中的索引
}KEY;

typedef struct HASH_TONG{    
    uint32_t numof_key;//桶内的key数量
    uint32_t* offsetof_key;//桶内的key偏移量数组
}HASH_TONG;

typedef struct KVALOT{ //键值对池，它具有4个键值对分池
    HASH_TONG* hash_table;//哈希表--->索引
    uint32_t numof_tong;//哈希表数量

    KEY* keypool;//键池
    uint32_t keynum;//key数量
    uint32_t keypoolROM;//池总容量

    char* kvalot_name;//键值对池名称
}KVALOT;


typedef struct Kvalh{
    uint8_t (*make_kvalot)(KVALOT* kvalot);
}Kvalh;


#endif
