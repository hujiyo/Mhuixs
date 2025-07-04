#include "kvalh.hpp"

int KVALOT::rise_capacity() {
    // 1. 确定桶数量
    uint32_t new_numof_tong = 0;
    if (numof_tong < hash_tong_4096ge) new_numof_tong = hash_tong_4096ge;
    else if (numof_tong < hash_tong_16384ge) new_numof_tong = hash_tong_16384ge;
    else if (numof_tong < hash_tong_65536ge) new_numof_tong = hash_tong_65536ge;
    else if (numof_tong < hash_tong_262144ge) new_numof_tong = hash_tong_262144ge;
    else if (numof_tong < hash_tong_1048576ge) new_numof_tong = hash_tong_1048576ge;
    else if (numof_tong < hash_tong_4194304ge) new_numof_tong = hash_tong_4194304ge;
    else if (numof_tong < hash_tong_16777216ge) new_numof_tong = hash_tong_16777216ge;
    else return merr;// 已经到最大桶数量

    // 2. 新建哈希桶表
    std::vector<HASH_TONG> new_hash_table(new_numof_tong);//已经保证初始化了

    // 3. 重新分配所有 key 到新桶
    for (uint32_t i = 0; i < keypool.size(); ++i) {
        KEY& key = keypool[i];
        // 获取key名称用于重新计算hash
        uint32_t key_len = *(uint32_t*)g_memap.addr(key.name);
        str key_str((uint8_t*)g_memap.addr(key.name) + sizeof(uint32_t), key_len);
        
        // 重新计算 hash_index
        uint32_t new_hash_index = murmurhash(key_str, bits(new_numof_tong));
        HASH_TONG& tong = new_hash_table[new_hash_index];
        
        // 检查是否需要分配或扩容桶内数组
        if(tong.numof_key == 0) {
            // 初次分配
            tong.offsetof_key = (uint32_t*)malloc(sizeof(uint32_t) * 4);
            if(!tong.offsetof_key) {
                // 释放已分配内存
                for (uint32_t j = 0; j < new_numof_tong; ++j) {
                    free(new_hash_table[j].offsetof_key);
                }
                return merr;
            }
            tong.capacity = 4;
        } else if(tong.numof_key >= tong.capacity) {
            // 需要扩容
            uint32_t new_capacity = tong.capacity * 2;
            uint32_t* new_offsetof_key = (uint32_t*)realloc(tong.offsetof_key, sizeof(uint32_t) * new_capacity);
            if (!new_offsetof_key) {
                // 释放已分配内存
                for (uint32_t j = 0; j < new_numof_tong; ++j) {
                    free(new_hash_table[j].offsetof_key);
                }
                return merr;
            }
            tong.offsetof_key = new_offsetof_key;
            tong.capacity = new_capacity;
        }
        
        // 添加key到桶中
        tong.offsetof_key[tong.numof_key] = i;
        tong.numof_key++;
        key.hash_index = new_hash_index;
    }

    // 4. 释放旧桶内存
    for (uint32_t i = 0; i < hash_table.size(); ++i) {
        free(hash_table[i].offsetof_key);
    }

    // 5. 替换哈希表和桶数量
    hash_table = std::move(new_hash_table);
    numof_tong = new_numof_tong;

    return 0;
}

uint32_t KVALOT::bits(uint32_t X){
    /*
    功能：
    1.把哈希桶的数量转化为二进制位数
    无效输入返回0
    */
    switch (X)    {
    //哈希桶数量映射到二进制位数
    case hash_tong_1024ge:return 10;
    case hash_tong_4096ge:return 12;
    case hash_tong_16384ge:return 14;
    case hash_tong_65536ge:return 16;
    case hash_tong_262144ge:return 18;
    case hash_tong_1048576ge:return 20;
    case hash_tong_4194304ge:return 22;
    case hash_tong_16777216ge:return 24;
    default:return bits(hash_tong_65536ge);//默认返回65536的位数
    }
}

uint32_t KVALOT::murmurhash(str& stream, uint32_t result_bits) 
{
    int len = stream.len;
    const uint8_t *data = (const uint8_t*)stream.string;
    /*
    这个哈希算法叫murmurhash,这个算法我是找的网上的，并和几个大模型联合完成
    由Austin Appleby在2008年发明的。
    result_bits是指哈希值的位数，这个位数对应哈希表的大小。最大可以设置为32位。
    */
    uint32_t seed=0x9747b28c;
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

    return seed & ((1 << result_bits) - 1);
}