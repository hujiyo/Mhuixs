#ifndef KVALH_HPP
#define KVALH_HPP
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <new>
#include <vector>
#include <string>

#include "Mhudef.hpp"
#include "memap.hpp"
#define merr -1
/*
#版权所有 (c) HUJI 2025
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.2
Email:hj18914255909@outlook.com
*/

//扩容时只会在一下几个数量级之间进行，当达到最高级时，不会再进行扩容而是直接返回溢出错误
#define hash_tong_1024ge        1024            //2^10
#define hash_tong_4096ge        4096            //2^12
#define hash_tong_16384ge       16384           //2^14
#define hash_tong_65536ge       65536           //2^16
#define hash_tong_262144ge      262144          //2^18
#define hash_tong_1048576ge     1048576         //2^20
#define hash_tong_4194304ge     4194304         //2^22
#define hash_tong_16777216ge    16777216        //2^24
#define hash_k 0.75  //哈希表装载因子
/*
哈希桶的数量决定了哈希表的大小，哈希桶的数量越大，在面对更多数据时，哈希表的性能就越好。
但是占用的内存空间也会越大。根据键的数量来决定哈希桶的数量。
一般来说，当 键值对数量 / 哈希桶数量 <= 0.75 时，哈希表的性能最好。
*/

class KVALOT{
    struct HASH_TONG{
        uint32_t numof_key;//桶内的key数量
        uint32_t* offsetof_key;//key偏移量数组,这里的偏移量是在keypool中的偏移量
    };
    //支持自辐射神经网状式连接结构的“键值对数据结构”--->单向连接+连接系数 特点：支持复杂的网络结构和关系查询，如社交网络分析、路径查找等。
    typedef struct KEY{
        basic_handle_struct bhs;//对象
        OFFSET name;//指向一个key名称存放的地址
        uint32_t hash_index;//当前key在哈希表中的索引
        int setname(str &key_name,MEMAP& key_name_pool){//设置键名
            OFFSET name_ = key_name_pool.smalloc(key_name.len + sizeof(uint32_t));
            if (name_ == NULL_OFFSET) {
                printf("KVALOT::KEY::setname:Error: Memory allocation failed for key name.\n");
                return merr;// 错误处理
            }
            *(uint32_t*)key_name_pool.addr(name_) = key_name.len;
            memcpy(key_name_pool.addr(name_) + sizeof(uint32_t), key_name.string, key_name.len);
            this->name = name_;
            return 0;
        }
        void clear_self(MEMAP& key_name_pool){
            bhs.clear_self();
            //获得键名的长度
            uint32_t len = *(uint32_t*)key_name_pool.addr(name);
            key_name_pool.sfree(name,len + sizeof(uint32_t));
            name = NULL_OFFSET;
            hash_index = NULL_OFFSET;
        }
    }KEY;
    
    vector<HASH_TONG> hash_table;//哈希桶表--->索引
    uint32_t numof_tong;//哈希桶数量

    vector<KEY> keypool;//键池    

    uint32_t keynum;//键数量
    MEMAP* key_name_pool;//键名池:键名的存放地址池

    string kvalot_name;//键值对池名称

    static uint32_t bits(uint32_t X);
    int rise_capacity();
    static uint32_t murmurhash(str& stream, uint32_t result_bits);
public:
    KVALOT(str* kvalot_name);
    //KVALOT(const KVALOT& kvalot);
    ~KVALOT();


    basic_handle_struct find_key(str* key_name);
    int  rmv_key(str* key_name);
    int  add_key(str* key_name, obj_type type,void* parameter1, void* parameter2,void* parameter3);
    str get_name();

    int iserr();
};


#endif