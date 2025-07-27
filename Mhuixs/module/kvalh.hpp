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
#include "merr.h"  // 引入merr库的错误处理机制
#include "stdstr.h" // 引入str结构支持
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
        uint32_t numof_key;      //桶内的key数量
        uint32_t capacity;       //桶内数组的容量（实际分配的大小）
        uint32_t* offsetof_key;  //key偏移量数组,这里的偏移量是在keypool中的偏移量
        
        // 构造函数，初始化为空桶
        HASH_TONG() : numof_key(0), capacity(0), offsetof_key(NULL) {}
    };
    static MEMAP g_memap;
public:
    struct KEY{
        basic_handle_struct bhs;//对象
        OFFSET name;//指向一个key名称存放的地址
        uint32_t hash_index;//当前key在哈希表中的索引
        int setname(str &key_name){//设置键名
            OFFSET name_ = g_memap.smalloc(key_name.len + sizeof(uint32_t));
            if (name_ == NULL_OFFSET) {
                report(merr, kvalot_module, "Memory allocation failed for key name");
                return merr;// 错误处理
            }
            *(uint32_t*)g_memap.addr(name_) = key_name.len;
            memcpy(g_memap.addr(name_) + sizeof(uint32_t), key_name.string, key_name.len);
            this->name = name_;
            return success;
        }
        void clear_self(){
            bhs.clear_self();
            //获得键名的长度
            if (name != NULL_OFFSET) {
                uint32_t len = *(uint32_t*)g_memap.addr(name);
                g_memap.sfree(name, len + sizeof(uint32_t));
                name = NULL_OFFSET;
            }
            hash_index = NULL_OFFSET;
        }
    };
private:
    vector<HASH_TONG> hash_table;//哈希桶表--->索引
    uint32_t numof_tong;//哈希桶数量

    vector<KEY> keypool;//键池    

    uint32_t keynum;//键数量

    string kvalot_name;//键值对池名称
    
    mrc state;//对象状态，使用merr库的错误码

    static uint32_t bits(uint32_t X);
    int rise_capacity();
    static uint32_t murmurhash(str& stream, uint32_t result_bits);
    
    // 辅助函数：C风格的内存管理
    int tong_ensure_capacity(HASH_TONG* tong, uint32_t needed_capacity);
    int validate_key_name(str* key_name);
    uint32_t find_key_in_tong(HASH_TONG* tong, str* key_name);
    int copy_bhs(basic_handle_struct* dest, const basic_handle_struct* src);
public:
    KVALOT(str* kvalot_name);
    KVALOT(const KVALOT& kvalot);
    ~KVALOT();


    basic_handle_struct find_key(str* key_name);
    int  rmv_key(str* key_name);
    int  add_key(str* key_name, obj_type type,void* parameter1, void* parameter2,void* parameter3);
    str get_name();

    mrc iserr();  // 返回merr库的错误码
    const char* get_error_msg(); // 获取错误信息
    
    // 调试和统计功能
    uint32_t get_key_count() const; // 获取键数量
    uint32_t get_bucket_count() const; // 获取桶数量
    float get_load_factor() const; // 获取装载因子
    void print_statistics() const; // 打印统计信息
};

MEMAP KVALOT::g_memap(1024,10240);

// C风格的便利函数声明
#ifdef __cplusplus
extern "C" {
#endif

// 验证哈希表完整性
int kvalot_validate_integrity(KVALOT* kvalot);

// C风格的KVALOT对象管理
KVALOT* kvalot_create(const char* name);
void kvalot_destroy(KVALOT* kvalot);

#ifdef __cplusplus
}
#endif

#endif